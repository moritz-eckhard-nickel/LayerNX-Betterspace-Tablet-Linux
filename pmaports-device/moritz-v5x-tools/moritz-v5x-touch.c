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

#define NAME "moritz-v5x-touch"

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

int main(void)
{
	char src[300];
	int in, out, i;
	int maxx = 1280, maxy = 800;
	struct input_absinfo ai;
	struct uinput_setup us;

	for (i = 0; i < 50 && find_src(src, sizeof(src)); i++)
		usleep(200000);
	in = open(src, O_RDONLY);
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

	out = open("/dev/uinput", O_WRONLY | O_NONBLOCK);
	if (out < 0) {
		perror("/dev/uinput");
		return 1;
	}
	ioctl(out, UI_SET_EVBIT, EV_KEY);
	ioctl(out, UI_SET_KEYBIT, BTN_TOUCH);
	ioctl(out, UI_SET_EVBIT, EV_ABS);
	ioctl(out, UI_SET_ABSBIT, ABS_X);
	ioctl(out, UI_SET_ABSBIT, ABS_Y);
	ioctl(out, UI_SET_PROPBIT, INPUT_PROP_DIRECT);
	abs_setup(out, ABS_X, maxx);
	abs_setup(out, ABS_Y, maxy);
	memset(&us, 0, sizeof(us));
	us.id.bustype = BUS_VIRTUAL;
	us.id.vendor = 0x4d56; /* "MV" */
	us.id.product = 0x0001;
	snprintf(us.name, sizeof(us.name), NAME);
	if (ioctl(out, UI_DEV_SETUP, &us) || ioctl(out, UI_DEV_CREATE)) {
		perror("uinput setup");
		return 1;
	}
	fprintf(stderr, NAME ": %s -> uinput (%dx%d)\n", src, maxx, maxy);

	{
		struct input_event ev;
		int fx = -1, fy = -1, cx = -1, cy = -1, contacts = 0;
		int touching = 0, lastx = -1, lasty = -1, released = 0;

		while (read(in, &ev, sizeof(ev)) == sizeof(ev)) {
			if (ev.type == EV_ABS) {
				if (ev.code == ABS_MT_POSITION_X) cx = ev.value;
				else if (ev.code == ABS_MT_POSITION_Y) cy = ev.value;
				else if (ev.code == ABS_X && fx < 0) fx = ev.value;
				else if (ev.code == ABS_Y && fy < 0) fy = ev.value;
			} else if (ev.type == EV_SYN && ev.code == SYN_MT_REPORT) {
				if (cx >= 0 && cy >= 0) {
					if (contacts == 0) { fx = cx; fy = cy; }
					contacts++;
				}
				cx = cy = -1;
			} else if (ev.type == EV_KEY && ev.code == BTN_TOUCH) {
				if (!ev.value) released = 1;
			} else if (ev.type == EV_SYN && ev.code == SYN_REPORT) {
				int down = !released && (contacts > 0 || (fx >= 0 && fy >= 0));
				if (down) {
					if (fx != lastx || fy != lasty) {
						emit(out, EV_ABS, ABS_X, fx);
						emit(out, EV_ABS, ABS_Y, fy);
						emit(out, EV_SYN, SYN_REPORT, 0);
						lastx = fx; lasty = fy;
					}
					if (!touching) {
						emit(out, EV_KEY, BTN_TOUCH, 1);
						emit(out, EV_SYN, SYN_REPORT, 0);
						touching = 1;
					}
				} else if (touching) {
					emit(out, EV_KEY, BTN_TOUCH, 0);
					emit(out, EV_SYN, SYN_REPORT, 0);
					touching = 0;
				}
				fx = fy = cx = cy = -1;
				contacts = 0;
				released = 0;
			}
		}
	}
	ioctl(out, UI_DEV_DESTROY);
	return 1;
}
