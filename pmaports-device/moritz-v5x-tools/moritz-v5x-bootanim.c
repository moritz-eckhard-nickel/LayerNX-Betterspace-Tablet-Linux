// SPDX-License-Identifier: MIT
/*
 * moritz-v5x-bootanim: spielt eine Bildfolge (PNG, 1280x800 quer) auf dem
 * mtkfb-Framebuffer (800x1280 hochkant, RGBA) ab.
 *  - mindestens ein kompletter Durchlauf, danach Schleife
 *  - beendet sich, sobald nach dem ersten Durchlauf die Datei STOPFILE existiert
 *    (oder SIGTERM kommt, ebenfalls erst nach dem ersten Durchlauf)
 * Aufruf: moritz-v5x-bootanim <bildordner> [fps]
 * Abbildung Querformat (X,Y) -> Hochkant-Puffer: fb_x = Y, fb_y = (H_fb-1) - X
 */
#include <dirent.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <png.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>

#define STOPFILE "/run/moritz-v5x-bootanim.stop"
#define DONEFILE "/run/moritz-v5x-bootanim.firstloop"

static volatile sig_atomic_t stop_req;
static void on_term(int s) { (void)s; stop_req = 1; }

static int cmpstr(const void *a, const void *b)
{
	return strcmp(*(char *const *)a, *(char *const *)b);
}

static unsigned char *load_png(const char *path, int *w, int *h)
{
	png_image im;
	unsigned char *buf;

	memset(&im, 0, sizeof(im));
	im.version = PNG_IMAGE_VERSION;
	if (!png_image_begin_read_from_file(&im, path))
		return NULL;
	im.format = PNG_FORMAT_RGBA;
	buf = malloc(PNG_IMAGE_SIZE(im));
	if (!buf || !png_image_finish_read(&im, NULL, buf, 0, NULL)) {
		free(buf);
		png_image_free(&im);
		return NULL;
	}
	*w = im.width;
	*h = im.height;
	return buf;
}

static long now_ms(void)
{
	struct timespec t;
	clock_gettime(CLOCK_MONOTONIC, &t);
	return t.tv_sec * 1000L + t.tv_nsec / 1000000L;
}

int main(int argc, char **argv)
{
	const char *dir = argc > 1 ? argv[1] : "/usr/share/layernx/bootanim";
	int fps = argc > 2 ? atoi(argv[2]) : 30;
	struct fb_var_screeninfo v;
	struct fb_fix_screeninfo f;
	char **names = NULL;
	int n = 0, cap = 0, fd, i, first_done = 0;
	unsigned char *fbm;
	DIR *d;
	struct dirent *e;

	signal(SIGTERM, on_term);
	signal(SIGINT, on_term);
	if (fps < 1 || fps > 60)
		fps = 30;
	d = opendir(dir);
	if (!d) {
		perror(dir);
		return 1;
	}
	while ((e = readdir(d))) {
		size_t l = strlen(e->d_name);
		if (l < 5 || strcmp(e->d_name + l - 4, ".png"))
			continue;
		if (n == cap) {
			cap = cap ? cap * 2 : 256;
			names = realloc(names, cap * sizeof(*names));
		}
		names[n] = malloc(strlen(dir) + l + 2);
		sprintf(names[n++], "%s/%s", dir, e->d_name);
	}
	closedir(d);
	if (!n) {
		fprintf(stderr, "keine PNGs in %s\n", dir);
		return 1;
	}
	qsort(names, n, sizeof(*names), cmpstr);

	fd = open("/dev/fb0", O_RDWR);
	if (fd < 0 || ioctl(fd, FBIOGET_VSCREENINFO, &v) || ioctl(fd, FBIOGET_FSCREENINFO, &f)) {
		perror("/dev/fb0");
		return 1;
	}
	fbm = mmap(NULL, f.line_length * v.yres, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (fbm == MAP_FAILED) {
		perror("mmap");
		return 1;
	}
	unlink(DONEFILE);

	for (;;) {
		for (i = 0; i < n; i++) {
			long t0 = now_ms(), dt;
			int w, h, x, y;
			unsigned char *img = load_png(names[i], &w, &h);

			if (img) {
				/* Querformat-Bild auf Hochkant-Puffer drehen (wie Xorg Rotate CCW) */
				for (y = 0; y < h && y < (int)v.xres; y++) {
					const unsigned char *src = img + (size_t)y * w * 4;
					for (x = 0; x < w && x < (int)v.yres; x++) {
						unsigned char *dst = fbm + (size_t)(v.yres - 1 - x) * f.line_length + (size_t)y * 4;
						dst[0] = src[x * 4 + 0];
						dst[1] = src[x * 4 + 1];
						dst[2] = src[x * 4 + 2];
						dst[3] = 0xff;
					}
				}
				free(img);
				v.xoffset = 0;
				v.yoffset = 0;
				ioctl(fd, FBIOPAN_DISPLAY, &v);
			}
			if (first_done && (stop_req || access(STOPFILE, F_OK) == 0))
				return 0;
			dt = 1000 / fps - (now_ms() - t0);
			if (dt > 0)
				usleep(dt * 1000);
		}
		if (!first_done) {
			first_done = 1;
			close(open(DONEFILE, O_CREAT | O_WRONLY, 0644));
		}
		if (stop_req || access(STOPFILE, F_OK) == 0)
			return 0;
	}
}
