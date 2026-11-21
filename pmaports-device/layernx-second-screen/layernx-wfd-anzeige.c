// SPDX-License-Identifier: MIT
/*
 * layernx-wfd-anzeige: Anzeige fuer den Miracast-Empfaenger von LayerNX-Tablet-Linux
 *
 *   layernx-wfd-anzeige RTP-PORT   (nur Bild, kein Ton: der bleibt am PC)
 *
 * - empfaengt RTP/MPEG-TS (H.264) auf RTP-PORT, dekodiert mit GStreamer
 * - nimmt immer nur das NEUESTE Bild (appsink drop=true, max-buffers=1): haengt die Anzeige
 *   hinterher, werden Bilder ausgelassen statt gestaut -> keine wachsende Verzoegerung
 * - I420 -> RGBA und 90°-Drehung in einem Durchgang, gekachelt, auf allen Kernen, direkt in /dev/fb0
 *   (Panel nativ 800x1280 hochkant, Bild 1280x800 quer: fb_x = Y, fb_y = 1279 - X)
 * - legt waehrenddessen ein schwarzes Fenster ueber X, damit die Oberflaeche nicht durchblitzt;
 *   beim Beenden verschwindet es und X zeichnet alles neu
 */
#include <errno.h>
#include <fcntl.h>
#include <gst/app/gstappsink.h>
#include <gst/gst.h>
#include <gst/video/video.h>
#include <linux/fb.h>
#include <pthread.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <X11/Xlib.h>

#define W 1280
#define H 800
#define FW 800              /* Framebuffer-Breite (hochkant) */
#define FH 1280
#define THREADS 4
#define KACHEL 32

static volatile sig_atomic_t ende;
static uint32_t *fb;
static int fb_stride;       /* in Pixeln */
static int fbfd = -1;
static struct fb_var_screeninfo var;

static const uint8_t *py, *pu, *pv;
static int sy, su, sv;
static pthread_barrier_t start_b, fertig_b;

static inline uint8_t klemme(int v) { return v < 0 ? 0 : v > 255 ? 255 : v; }

/* BT.601 begrenzter Bereich, Festkomma */
static inline uint32_t yuv_rgba(int y, int u, int v)
{
	int c = (y - 16) * 298, d = u - 128, e = v - 128;
	int r = (c + 409 * e + 128) >> 8;
	int g = (c - 100 * d - 208 * e + 128) >> 8;
	int b = (c + 516 * d + 128) >> 8;
	return klemme(r) | (uint32_t)klemme(g) << 8 | (uint32_t)klemme(b) << 16 | 0xff000000u;
}

/* Thread t bearbeitet die Bildspalten-Kacheln X in [t*W/THREADS, (t+1)*W/THREADS) */
static void *arbeiter(void *arg)
{
	int t = (int)(intptr_t)arg;
	int x0 = t * W / THREADS, x1 = (t + 1) * W / THREADS;
	for (;;) {
		pthread_barrier_wait(&start_b);
		if (ende)
			return NULL;
		for (int bx = x0; bx < x1; bx += KACHEL)
			for (int by = 0; by < H; by += KACHEL) {
				int ex = bx + KACHEL < x1 ? bx + KACHEL : x1;
				int ey = by + KACHEL < H ? by + KACHEL : H;
				for (int y = by; y < ey; y++) {
					const uint8_t *ly = py + y * sy, *lu = pu + (y >> 1) * su, *lv = pv + (y >> 1) * sv;
					for (int x = bx; x < ex; x++)
						fb[(FH - 1 - x) * fb_stride + y] = yuv_rgba(ly[x], lu[x >> 1], lv[x >> 1]);
				}
			}
		pthread_barrier_wait(&fertig_b);
	}
}

static void stopp(int s) { (void)s; ende = 1; }

static Display *dpy;
static Window abdeckung;
static void abdecken(void)
{
	setenv("DISPLAY", getenv("DISPLAY") ? getenv("DISPLAY") : ":0", 0);
	dpy = XOpenDisplay(NULL);
	if (!dpy) {
		fprintf(stderr, "layernx-wfd-anzeige: kein X (Oberflaeche kann durchblitzen)\n");
		return;
	}
	XSetWindowAttributes a = { .override_redirect = True, .background_pixel = BlackPixel(dpy, DefaultScreen(dpy)) };
	abdeckung = XCreateWindow(dpy, DefaultRootWindow(dpy), 0, 0, W, H, 0, CopyFromParent, InputOutput,
		CopyFromParent, CWOverrideRedirect | CWBackPixel, &a);
	XMapRaised(dpy, abdeckung);
	XSync(dpy, False);
}
static void aufdecken(void)
{
	if (!dpy)
		return;
	XDestroyWindow(dpy, abdeckung);   /* X bekommt Expose fuer alles darunter -> neu zeichnen */
	XSync(dpy, False);
	XCloseDisplay(dpy);
}

int main(int argc, char **argv)
{
	int port = argc > 1 ? atoi(argv[1]) : 1028;
	struct fb_fix_screeninfo fix;
	pthread_t th[THREADS];

	gst_init(&argc, &argv);
	signal(SIGINT, stopp);
	signal(SIGTERM, stopp);

	fbfd = open("/dev/fb0", O_RDWR | O_CLOEXEC);
	if (fbfd < 0 || ioctl(fbfd, FBIOGET_VSCREENINFO, &var) || ioctl(fbfd, FBIOGET_FSCREENINFO, &fix)) {
		perror("/dev/fb0");
		return 1;
	}
	if (var.bits_per_pixel != 32 || var.xres != FW || var.yres != FH) {
		fprintf(stderr, "unerwarteter Framebuffer %ux%u@%u\n", var.xres, var.yres, var.bits_per_pixel);
		return 1;
	}
	fb_stride = fix.line_length / 4;
	fb = mmap(NULL, fix.line_length * var.yres_virtual, PROT_READ | PROT_WRITE, MAP_SHARED, fbfd, 0);
	if (fb == MAP_FAILED) {
		perror("mmap");
		return 1;
	}
	fb += (size_t)var.yoffset * fb_stride;   /* sichtbarer Puffer */

	gchar *desc = g_strdup_printf(
		"udpsrc port=%d buffer-size=4194304 caps=\"application/x-rtp,media=video,clock-rate=90000,encoding-name=MP2T\" "
		"! rtpjitterbuffer latency=50 drop-on-latency=true ! rtpmp2tdepay ! tsdemux name=d "
		"d. ! queue max-size-buffers=64 ! h264parse ! avdec_h264 max-threads=4 output-corrupt=false "
		"! videoscale ! video/x-raw,format=I420,width=%d,height=%d "
		"! appsink name=bild sync=false async=false max-buffers=1 drop=true",
		port, W, H);
	GError *err = NULL;
	GstElement *pipe = gst_parse_launch(desc, &err);
	g_free(desc);
	if (!pipe) {
		fprintf(stderr, "Pipeline: %s\n", err ? err->message : "?");
		return 1;
	}
	GstAppSink *sink = GST_APP_SINK(gst_bin_get_by_name(GST_BIN(pipe), "bild"));

	pthread_barrier_init(&start_b, NULL, THREADS + 1);
	pthread_barrier_init(&fertig_b, NULL, THREADS + 1);
	for (int i = 0; i < THREADS; i++)
		pthread_create(&th[i], NULL, arbeiter, (void *)(intptr_t)i);

	abdecken();
	gst_element_set_state(pipe, GST_STATE_PLAYING);

	GstBus *bus = gst_element_get_bus(pipe);
	guint64 n = 0;
	gint64 t0 = g_get_monotonic_time();
	while (!ende) {
		GstMessage *m = gst_bus_pop_filtered(bus, GST_MESSAGE_ERROR | GST_MESSAGE_EOS);
		if (m) {
			if (GST_MESSAGE_TYPE(m) == GST_MESSAGE_ERROR) {
				GError *e = NULL;
				gst_message_parse_error(m, &e, NULL);
				fprintf(stderr, "GStreamer: %s\n", e ? e->message : "?");
				g_clear_error(&e);
			}
			gst_message_unref(m);
			break;
		}
		GstSample *s = gst_app_sink_try_pull_sample(sink, 200 * GST_MSECOND);
		if (!s) {
			if (getenv("LAYERNX_WFD_DEBUG")) {
				GstState st, pe;
				gst_element_get_state(pipe, &st, &pe, 0);
				fprintf(stderr, "kein Bild (Zustand %s/%s)\n", gst_element_state_get_name(st), gst_element_state_get_name(pe));
			}
			continue;
		}
		if (n == 0)
			fprintf(stderr, "erstes Bild\n");
		GstVideoInfo vi;
		GstVideoFrame f;
		if (gst_video_info_from_caps(&vi, gst_sample_get_caps(s)) &&
		    gst_video_frame_map(&f, &vi, gst_sample_get_buffer(s), GST_MAP_READ)) {
			py = GST_VIDEO_FRAME_PLANE_DATA(&f, 0); sy = GST_VIDEO_FRAME_PLANE_STRIDE(&f, 0);
			pu = GST_VIDEO_FRAME_PLANE_DATA(&f, 1); su = GST_VIDEO_FRAME_PLANE_STRIDE(&f, 1);
			pv = GST_VIDEO_FRAME_PLANE_DATA(&f, 2); sv = GST_VIDEO_FRAME_PLANE_STRIDE(&f, 2);
			pthread_barrier_wait(&start_b);
			pthread_barrier_wait(&fertig_b);
			ioctl(fbfd, FBIOPAN_DISPLAY, &var);   /* mtkfb aktualisiert das Panel nur beim Pan */
			gst_video_frame_unmap(&f);
			if (++n % 150 == 0) {
				gint64 t = g_get_monotonic_time();
				fprintf(stderr, "%.1f Bilder/s angezeigt\n", 150e6 / (double)(t - t0));
				t0 = t;
			}
		}
		gst_sample_unref(s);
	}

	ende = 1;
	pthread_barrier_wait(&start_b);
	for (int i = 0; i < THREADS; i++)
		pthread_join(th[i], NULL);
	gst_element_set_state(pipe, GST_STATE_NULL);
	aufdecken();
	return 0;
}
