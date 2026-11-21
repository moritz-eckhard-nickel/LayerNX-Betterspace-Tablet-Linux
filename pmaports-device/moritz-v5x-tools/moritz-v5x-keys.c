// SPDX-License-Identifier: MIT
/*
 * moritz-v5x-keys: Power-Taste des Betterspace V5x (mtk-kpd)
 *  - kurz druecken: Display an/aus (layernx-display toggle)
 *  - halten (>= HOLD_MS): Power-Menue, nur wenn das Display an ist;
 *    bei ausgeschaltetem Display weckt jeder Druck nur den Bildschirm.
 */
#include <dirent.h>
#include <fcntl.h>
#include <linux/input.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <time.h>
#include <unistd.h>

#define HOLD_MS 800

static long ms(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec * 1000L + t.tv_nsec / 1000000L; }

static int display_on(void)
{
	FILE *f = fopen("/sys/class/leds/lcd-backlight/brightness", "r");
	int v = 0;
	if (f) { if (fscanf(f, "%d", &v) != 1) v = 0; fclose(f); }
	return v > 0;
}

static int open_kpd(void)
{
	DIR *d = opendir("/sys/class/input");
	struct dirent *e;
	char p[600], name[64];
	while (d && (e = readdir(d))) {
		FILE *f;
		if (strncmp(e->d_name, "event", 5)) continue;
		snprintf(p, sizeof(p), "/sys/class/input/%s/device/name", e->d_name);
		if (!(f = fopen(p, "r"))) continue;
		if (fgets(name, sizeof(name), f) && !strncmp(name, "mtk-kpd", 7)) {
			fclose(f);
			snprintf(p, sizeof(p), "/dev/input/%s", e->d_name); /* vor closedir! */
			closedir(d);
			return open(p, O_RDONLY | O_CLOEXEC); /* CLOEXEC: sonst erben Kindprozesse den Grab */
		}
		fclose(f);
	}
	if (d) closedir(d);
	return -1;
}

static void run(const char *c) { if (system(c) < 0) perror("system"); }

int main(void)
{
	int fd = -1, i, down = 0, held = 0, was_on = 0, count = 0;
	long t0 = 0, first = 0;
	struct input_event ev;

	for (i = 0; i < 50 && (fd = open_kpd()) < 0; i++) usleep(200000);
	if (fd < 0) { perror("mtk-kpd"); return 1; }
	ioctl(fd, EVIOCGRAB, 1);
	for (;;) {
		fd_set rf; struct timeval tv = { 0, 50000 };
		FD_ZERO(&rf); FD_SET(fd, &rf);
		if (select(fd + 1, &rf, NULL, NULL, &tv) > 0) {
			if (read(fd, &ev, sizeof(ev)) != sizeof(ev)) break;
			if (ev.type != EV_KEY || ev.code != KEY_POWER) continue;
			if (ev.value == 1) { down = 1; held = 0; t0 = ms(); was_on = display_on(); }
			else if (ev.value == 0 && down) {
				long now = ms();
				down = 0;
				if (!held) {
					if (now - first > 2500) { first = now; count = 0; }
					if (++count >= 5) { count = 0; run("/usr/bin/layernx-powerkey multi5 &"); }
					else run("/usr/bin/layernx-powerkey short &");
				}
			}
		}
		if (down && !held && ms() - t0 >= HOLD_MS) {
			held = 1;
			run("/usr/bin/layernx-powerkey hold &");
		}
	}
	return 1;
}
