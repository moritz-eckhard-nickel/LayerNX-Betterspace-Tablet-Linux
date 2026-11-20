// SPDX-License-Identifier: MIT
/*
 * moritz-v5x-fbrefresh: mtkfb uebertraegt den Framebuffer nur bei FBIOPAN_DISPLAY
 * aufs Panel (unter Android macht das der HW-Composer). Daher periodisch pannen.
 */
#include <fcntl.h>
#include <linux/fb.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <unistd.h>

int main(int argc, char **argv)
{
	int hz = argc > 1 ? atoi(argv[1]) : 30;
	int fd = open("/dev/fb0", O_RDWR);
	struct fb_var_screeninfo v;

	if (hz < 1 || hz > 120)
		hz = 30;
	if (fd < 0) {
		perror("/dev/fb0");
		return 1;
	}
	for (;;) {
		if (!ioctl(fd, FBIOGET_VSCREENINFO, &v)) {
			v.xoffset = 0;
			v.yoffset = 0;
			ioctl(fd, FBIOPAN_DISPLAY, &v);
		}
		usleep(1000000 / hz);
	}
}
