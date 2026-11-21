// SPDX-License-Identifier: MIT
/*
 * moritz-v5x-touch: uebersetzt den Typ-A-Multitouch-Treiber mtk-tpd in ein
 * sauberes Einzelberuehrungs-Geraet (uinput). Bei Beruehrungsbeginn wird
 * zuerst die Position (eigener SYN-Frame) und erst danach BTN_TOUCH gemeldet,
 * damit Xorg den Klick an der richtigen Stelle ausfuehrt.
 */
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/input.h>
#include <linux/uinput.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <sys/select.h>
#include <time.h>
#include <stdlib.h>

#define NAME "moritz-v5x-touch"
#define NSLOT 5

static int find_src(char *path, size_t n)
{
	DIR *d = opendir("/sys/class/input");
	struct dirent *e;
	char p[600], name[128];
	if (!d)
		return -1;
	while ((e = readdir(d))) {
		FILE *f;
		if (strncmp(e->d_name, "event", 5))
			continue;
		snprintf(p, sizeof(p), "/sys/class/input/%s/device/name", e->d_name);
		f = fopen(p, "r");
		if (!f)
			continue;
		if (fgets(name, sizeof(name), f) && !strncmp(name, "mtk-tpd", 7)) {
			fclose(f);
			snprintf(path, n, "/dev/input/%s", e->d_name);
			closedir(d);
			return 0;
		}
		fclose(f);
	}
	closedir(d);
	return -1;
}

static void emit(int fd, int type, int code, int val)
{
	struct input_event ev;
	memset(&ev, 0, sizeof(ev));
	ev.type = type;
	ev.code = code;
	ev.value = val;
	if (write(fd, &ev, sizeof(ev)) < 0)
		perror("write");
}

static void abs_setup(int fd, int code, int max)
{
	struct uinput_abs_setup a;
	memset(&a, 0, sizeof(a));
	a.code = code;
	a.absinfo.minimum = 0;
	a.absinfo.maximum = max;
	ioctl(fd, UI_ABS_SETUP, &a);
}


/* ---- Gestenerkennung -------------------------------------------------
 * IDLE: kein Finger. PENDING: erster Finger liegt < HOLD_MS, noch keine
 * Entscheidung (nichts an X gemeldet). TOUCH: normale Beruehrung (MT-Slots
 * laufen). SCROLL: zwei Finger -> Mausrad. LONG: Langdruck -> Rechtsklick,
 * Rest der Beruehrung wird verschluckt. */
#define HOLD_MS 120
#define LONG_MS 600
#define MOVE_TOL 18
#define WHEEL_STEP 45
enum { IDLE, PENDING, TOUCH, SCROLL, LONG };
static int out = -1, gfd = -1, twofinger = 1, state = IDLE;
static int fx[NSLOT], fy[NSLOT], fid[NSLOT], nf;
static int sid[NSLOT], sx[NSLOT], sy[NSLOT], active, next_tid = 1, cur_slot = -1;
static long t_down; static int x0, y0, moved, accy, accx, lasty, lastx;

static long ms(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec * 1000L + t.tv_nsec / 1000000L; }
static void gemit(int type, int code, int val) { if (gfd >= 0) emit(gfd, type, code, val); }

/* aktuelle Kontakte (fx/fy/nf) als MT-B an X melden */
static void push_slots(void)
{
	int k, j, was = active, used[NSLOT] = {0};
	for (k = 0; k < NSLOT; k++) {
		int hit = -1;
		if (sid[k] < 0) continue;
		for (j = 0; j < nf; j++) if (!used[j] && fid[j] == sid[k] >> 16) { hit = j; break; }
		if (cur_slot != k) { emit(out, EV_ABS, ABS_MT_SLOT, k); cur_slot = k; }
		if (hit < 0) { emit(out, EV_ABS, ABS_MT_TRACKING_ID, -1); sid[k] = -1; active--; }
		else {
			used[hit] = 1;
			if (fx[hit] != sx[k]) emit(out, EV_ABS, ABS_MT_POSITION_X, sx[k] = fx[hit]);
			if (fy[hit] != sy[k]) emit(out, EV_ABS, ABS_MT_POSITION_Y, sy[k] = fy[hit]);
		}
	}
	for (j = 0; j < nf; j++) {
		if (used[j]) continue;
		for (k = 0; k < NSLOT && sid[k] >= 0; k++);
		if (k == NSLOT) break;
		sid[k] = (fid[j] << 16) | (next_tid & 0xffff);
		if (cur_slot != k) { emit(out, EV_ABS, ABS_MT_SLOT, k); cur_slot = k; }
		emit(out, EV_ABS, ABS_MT_TRACKING_ID, next_tid++ & 0xffff);
		emit(out, EV_ABS, ABS_MT_POSITION_X, sx[k] = fx[j]);
		emit(out, EV_ABS, ABS_MT_POSITION_Y, sy[k] = fy[j]);
		active++;
	}
	for (k = 0; k < NSLOT && sid[k] < 0; k++);
	if (k < NSLOT) { emit(out, EV_ABS, ABS_X, sx[k]); emit(out, EV_ABS, ABS_Y, sy[k]); }
	if (!was && active) emit(out, EV_KEY, BTN_TOUCH, 1);
	if (was && !active) emit(out, EV_KEY, BTN_TOUCH, 0);
	emit(out, EV_SYN, SYN_REPORT, 0);
}

static void end_touch(void) { int n = nf; nf = 0; push_slots(); nf = n; }

static void right_click(void)
{
	/* Zeiger steht schon an der Position (absolutes Geraet), Rechtsklick ueber Gestengeraet */
	gemit(EV_KEY, BTN_RIGHT, 1); gemit(EV_SYN, SYN_REPORT, 0);
	gemit(EV_KEY, BTN_RIGHT, 0); gemit(EV_SYN, SYN_REPORT, 0);
}

static void move_pointer_only(int x, int y)
{
	emit(out, EV_ABS, ABS_X, x); emit(out, EV_ABS, ABS_Y, y); emit(out, EV_SYN, SYN_REPORT, 0);
}

/* wird bei jedem Frame aufgerufen */
static void frame(void)
{
	long now = ms();
	switch (state) {
	case IDLE:
		if (nf >= 1) { state = PENDING; t_down = now; x0 = fx[0]; y0 = fy[0]; moved = 0;
			if (nf >= 2 && twofinger) { state = SCROLL; lasty = (fy[0]+fy[1])/2; lastx = (fx[0]+fx[1])/2; accy = accx = 0; } }
		break;
	case PENDING:
		if (nf == 0) {           /* kurzer Tipp: sofort komplett melden */
			int n = 1; nf = n; fx[0] = x0; fy[0] = y0; push_slots(); nf = 0; push_slots(); state = IDLE; break;
		}
		if (nf >= 2 && twofinger) { state = SCROLL; lasty = (fy[0]+fy[1])/2; lastx = (fx[0]+fx[1])/2; accy = accx = 0; break; }
		if (abs(fx[0]-x0) > MOVE_TOL || abs(fy[0]-y0) > MOVE_TOL) moved = 1;
		if (moved || now - t_down >= HOLD_MS) { state = TOUCH; push_slots(); }
		break;
	case TOUCH:
		if (abs(fx[0]-x0) > MOVE_TOL || abs(fy[0]-y0) > MOVE_TOL) moved = 1;
		push_slots();
		if (nf == 0) state = IDLE;
		break;
	case SCROLL:
		if (nf >= 2) {
			int my = (fy[0]+fy[1])/2, mx = (fx[0]+fx[1])/2;
			accy += my - lasty; accx += mx - lastx; lasty = my; lastx = mx;
			move_pointer_only(mx, my);
			while (accy >= WHEEL_STEP) { gemit(EV_REL, REL_WHEEL, 1); gemit(EV_SYN, SYN_REPORT, 0); accy -= WHEEL_STEP; }
			while (accy <= -WHEEL_STEP) { gemit(EV_REL, REL_WHEEL, -1); gemit(EV_SYN, SYN_REPORT, 0); accy += WHEEL_STEP; }
			while (accx >= WHEEL_STEP) { gemit(EV_REL, REL_HWHEEL, -1); gemit(EV_SYN, SYN_REPORT, 0); accx -= WHEEL_STEP; }
			while (accx <= -WHEEL_STEP) { gemit(EV_REL, REL_HWHEEL, 1); gemit(EV_SYN, SYN_REPORT, 0); accx += WHEEL_STEP; }
		}
		if (nf == 0) state = IDLE;
		break;
	case LONG:
		if (nf == 0) state = IDLE;
		break;
	}
}

/* Zeitgesteuert (ohne neue Events): PENDING-Ablauf und Langdruck */
static void tick(void)
{
	long now = ms();
	if (state == PENDING && now - t_down >= HOLD_MS) { state = TOUCH; push_slots(); }
	if (state == TOUCH && !moved && nf == 1 && now - t_down >= LONG_MS) {
		end_touch();             /* laufende Beruehrung beenden ... */
		move_pointer_only(x0, y0);
		right_click();           /* ... und Rechtsklick an der Stelle */
		state = LONG;
	}
}

int main(void)
{
	char src[300];
	int in, i;
	int maxx = 1280, maxy = 800;
	struct input_absinfo ai;
	struct uinput_setup us;

	for (i = 0; i < 50 && find_src(src, sizeof(src)); i++)
		usleep(200000);
	in = open(src, O_RDONLY | O_CLOEXEC);
	if (in < 0) {
		perror("mtk-tpd");
		return 1;
	}
	if (!ioctl(in, EVIOCGABS(ABS_MT_POSITION_X), &ai) && ai.maximum > 0)
		maxx = ai.maximum;
	if (!ioctl(in, EVIOCGABS(ABS_MT_POSITION_Y), &ai) && ai.maximum > 0)
		maxy = ai.maximum;
	if (ioctl(in, EVIOCGRAB, 1))
		perror("EVIOCGRAB");

	out = open("/dev/uinput", O_WRONLY | O_NONBLOCK | O_CLOEXEC);
	if (out < 0) {
		perror("/dev/uinput");
		return 1;
	}
	ioctl(out, UI_SET_EVBIT, EV_KEY);
	ioctl(out, UI_SET_KEYBIT, BTN_TOUCH);
	ioctl(out, UI_SET_EVBIT, EV_ABS);
	ioctl(out, UI_SET_ABSBIT, ABS_X);
	ioctl(out, UI_SET_ABSBIT, ABS_Y);
	ioctl(out, UI_SET_ABSBIT, ABS_MT_SLOT);
	ioctl(out, UI_SET_ABSBIT, ABS_MT_TRACKING_ID);
	ioctl(out, UI_SET_ABSBIT, ABS_MT_POSITION_X);
	ioctl(out, UI_SET_ABSBIT, ABS_MT_POSITION_Y);
	ioctl(out, UI_SET_PROPBIT, INPUT_PROP_DIRECT);
	abs_setup(out, ABS_X, maxx);
	abs_setup(out, ABS_Y, maxy);
	abs_setup(out, ABS_MT_POSITION_X, maxx);
	abs_setup(out, ABS_MT_POSITION_Y, maxy);
	abs_setup(out, ABS_MT_SLOT, NSLOT - 1);
	abs_setup(out, ABS_MT_TRACKING_ID, 65535);
	memset(&us, 0, sizeof(us));
	us.id.bustype = BUS_VIRTUAL;
	us.id.vendor = 0x4d56; /* "MV" */
	us.id.product = 0x0002;
	snprintf(us.name, sizeof(us.name), NAME);
	if (ioctl(out, UI_DEV_SETUP, &us) || ioctl(out, UI_DEV_CREATE)) {
		perror("uinput setup");
		return 1;
	}
	fprintf(stderr, NAME ": %s -> uinput MT-B (%dx%d, %d Slots)\n", src, maxx, maxy, NSLOT);

	/* zweites Geraet fuer Gesten: Rechtsklick + Mausrad */
	gfd = open("/dev/uinput", O_WRONLY | O_NONBLOCK | O_CLOEXEC);
	if (gfd >= 0) {
		struct uinput_setup gs;
		ioctl(gfd, UI_SET_EVBIT, EV_KEY);
		ioctl(gfd, UI_SET_KEYBIT, BTN_LEFT);
		ioctl(gfd, UI_SET_KEYBIT, BTN_RIGHT);
		ioctl(gfd, UI_SET_EVBIT, EV_REL);
		ioctl(gfd, UI_SET_RELBIT, REL_X);
		ioctl(gfd, UI_SET_RELBIT, REL_Y);
		ioctl(gfd, UI_SET_RELBIT, REL_WHEEL);
		ioctl(gfd, UI_SET_RELBIT, REL_HWHEEL);
		memset(&gs, 0, sizeof(gs));
		gs.id.bustype = BUS_VIRTUAL; gs.id.vendor = 0x4d56; gs.id.product = 0x0003;
		snprintf(gs.name, sizeof(gs.name), NAME "-gesten");
		if (ioctl(gfd, UI_DEV_SETUP, &gs) || ioctl(gfd, UI_DEV_CREATE)) { close(gfd); gfd = -1; }
	}
	twofinger = !getenv("MORITZ_TOUCH_NO_2F_SCROLL");

	{
		struct input_event ev;
		int cx = -1, cy = -1, cid = -1, released = 0;
		int k;

		for (k = 0; k < NSLOT; k++) sid[k] = -1;
		for (;;) {
			fd_set rf;
			struct timeval tv = { 0, 20000 };
			FD_ZERO(&rf); FD_SET(in, &rf);
			if (select(in + 1, &rf, NULL, NULL, &tv) <= 0) { tick(); continue; }
			if (read(in, &ev, sizeof(ev)) != sizeof(ev)) break;
			if (ev.type == EV_ABS) {
				if (ev.code == ABS_MT_POSITION_X) cx = ev.value;
				else if (ev.code == ABS_MT_POSITION_Y) cy = ev.value;
				else if (ev.code == ABS_MT_TRACKING_ID) cid = ev.value;
			} else if (ev.type == EV_SYN && ev.code == SYN_MT_REPORT) {
				if (cx >= 0 && cy >= 0 && nf < NSLOT) {
					fx[nf] = cx; fy[nf] = cy; fid[nf] = cid >= 0 ? cid : nf; nf++;
				}
				cx = cy = cid = -1;
			} else if (ev.type == EV_KEY && ev.code == BTN_TOUCH) {
				if (!ev.value) released = 1;
			} else if (ev.type == EV_SYN && ev.code == SYN_REPORT) {
				if (released) nf = 0;
				frame();
				tick();
				nf = 0; released = 0;
			}
		}
	}
	ioctl(out, UI_DEV_DESTROY);
	return 1;
}
