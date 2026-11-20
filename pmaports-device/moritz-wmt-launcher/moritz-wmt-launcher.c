// SPDX-License-Identifier: GPL-2.0
/*
 * moritz-wmt-launcher - Ersatz fuer MediaTeks Android-Daemons wmt_loader und
 * wmt_launcher (combo_tool) auf dem MT8168 (CONNAC/SOC, Firmware soc1_0_*).
 *
 *   1. /dev/wmtdetect: SOC-Chip-ID holen, setzen, Modul-Init (wie wmt_loader)
 *   2. /dev/stpwmt:    WMT-Config-Name + STP-Modus setzen und danach die
 *      Anfragen des Kernels ("srh_patch", "srh_rom_patch", ...) beantworten
 *      (wie wmt_launcher -p <firmware-dir>).
 *
 * Header-Auswertung nach Disassembly des Original-wmt_launcher (Android 10,
 * Ilium Pad X8): Offset 0x16 = HW-Version, 0x18..0x1b = patch info;
 * patch num = info[0] >> 4, seq = info[0] & 0xf, Adresse = info mit [0] = 0.
 * ROM-Patch: Typ = Byte 0x1f, Adresse = 00 b[0x19] b[0x1a] b[0x1b]
 * (vgl. Evergo-Mainline/initramfs, GPL-2.0).
 */
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <unistd.h>

#define WMT_DETECT_IOC_MAGIC        'w'
#define COMBO_IOCTL_SET_CHIP_ID     _IOW(WMT_DETECT_IOC_MAGIC, 1, int)
#define COMBO_IOCTL_GET_SOC_CHIP_ID _IOR(WMT_DETECT_IOC_MAGIC, 3, int)
#define COMBO_IOCTL_DO_MODULE_INIT  _IOR(WMT_DETECT_IOC_MAGIC, 4, int)

#define WMT_IOC_MAGIC                0xa0
#define WMT_IOCTL_SET_STP_MODE       _IOW(WMT_IOC_MAGIC, 5, int)
#define WMT_IOCTL_WMT_CFG_NAME       _IOWR(WMT_IOC_MAGIC, 21, char *)
#define WMT_IOCTL_SET_LAUNCHER_KILL  _IOW(WMT_IOC_MAGIC, 13, int)
#define WMT_IOCTL_SET_PATCH_NUM      _IOW(WMT_IOC_MAGIC, 14, int)
#define WMT_IOCTL_SET_PATCH_INFO     _IOW(WMT_IOC_MAGIC, 15, char *)
#define WMT_IOCTL_SET_ROM_PATCH_INFO _IOW(WMT_IOC_MAGIC, 31, char *)

struct wmt_patch_info {          /* WMT_PATCH_INFO (wmt_lib.h) */
	unsigned int seq;
	unsigned char addr[4];
	char name[256];
};

struct wmt_rom_patch_info {      /* struct wmt_rom_patch_info (wmt_lib.h) */
	unsigned int type;
	unsigned char addr[4];
	char name[256];
};

static const char *fwdir = "/lib/firmware";
static const char *prefix = "soc1_0";
static int patch_info_sent;

static void logk(const char *fmt, ...)
{
	char buf[256];
	va_list ap;
	int n, fd;

	va_start(ap, fmt);
	n = vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);
	fprintf(stderr, "%s", buf);
	fd = open("/dev/kmsg", O_WRONLY);
	if (fd >= 0) {
		char k[300];
		int m = snprintf(k, sizeof(k), "moritz-wmt: %s", buf);
		(void)write(fd, k, m);
		close(fd);
	}
	(void)n;
}

/* /dev-Knoten anlegen, falls kein devtmpfs/mdev ihn erzeugt hat */
static void ensure_node(const char *name)
{
	char path[128], buf[32];
	unsigned int maj, min;
	int fd, n;

	snprintf(path, sizeof(path), "/dev/%s", name);
	if (access(path, F_OK) == 0)
		return;
	snprintf(path, sizeof(path), "/sys/class/%s/%s/dev", name, name);
	fd = open(path, O_RDONLY);
	if (fd < 0)
		return;
	n = read(fd, buf, sizeof(buf) - 1);
	close(fd);
	if (n <= 0)
		return;
	buf[n] = 0;
	if (sscanf(buf, "%u:%u", &maj, &min) != 2)
		return;
	snprintf(path, sizeof(path), "/dev/%s", name);
	if (mknod(path, S_IFCHR | 0660, makedev(maj, min)) == 0)
		logk("mknod %s %u:%u\n", path, maj, min);
}

static int read_hdr(const char *name, unsigned char *hdr, int len)
{
	char path[512];
	int fd, n;

	snprintf(path, sizeof(path), "%s/%s", fwdir, name);
	fd = open(path, O_RDONLY);
	if (fd < 0) {
		logk("open %s: %s\n", path, strerror(errno));
		return -1;
	}
	n = read(fd, hdr, len);
	close(fd);
	return n == len ? 0 : -1;
}

/* Dateien <prefix>_<kind>_*_hdr.bin im Firmware-Verzeichnis, sortiert */
static int list_files(const char *kind, char out[][256], int max)
{
	char pre[64];
	struct dirent **nl;
	int n, i, cnt = 0;

	snprintf(pre, sizeof(pre), "%s_%s_", prefix, kind);
	n = scandir(fwdir, &nl, NULL, alphasort);
	if (n < 0)
		return 0;
	for (i = 0; i < n; i++) {
		const char *d = nl[i]->d_name;
		size_t l = strlen(d);

		if (cnt < max && !strncmp(d, pre, strlen(pre)) && l > 8 &&
		    !strcmp(d + l - 8, "_hdr.bin"))
			snprintf(out[cnt++], 256, "%s", d);
		free(nl[i]);
	}
	free(nl);
	return cnt;
}

static int answer_srh_patch(int fd)
{
	char files[8][256];
	int n = list_files("patch", files, 8), i, num = 0;

	for (i = 0; i < n; i++) {
		unsigned char hdr[0x1c];
		struct wmt_patch_info pi;

		if (read_hdr(files[i], hdr, sizeof(hdr)))
			continue;
		if (!patch_info_sent && num == 0) {
			num = hdr[0x18] >> 4;
			logk("patch num = %d (%s)\n", num, files[i]);
			if (ioctl(fd, WMT_IOCTL_SET_PATCH_NUM, num) < 0)
				logk("SET_PATCH_NUM: %s\n", strerror(errno));
		}
		memset(&pi, 0, sizeof(pi));
		pi.seq = hdr[0x18] & 0x0f;
		memcpy(pi.addr, hdr + 0x18, 4);
		pi.addr[0] = 0;
		snprintf(pi.name, sizeof(pi.name), "%s", files[i]);
		logk("patch seq %u addr %02x %02x %02x %02x %s\n", pi.seq,
		     pi.addr[0], pi.addr[1], pi.addr[2], pi.addr[3], pi.name);
		if (ioctl(fd, WMT_IOCTL_SET_PATCH_INFO, &pi) < 0)
			logk("SET_PATCH_INFO: %s\n", strerror(errno));
	}
	if (n > 0)
		patch_info_sent = 1;
	return n > 0;
}

static int answer_srh_rom_patch(int fd)
{
	char files[8][256];
	int n = list_files("ram", files, 8), i, ok = 0;

	for (i = 0; i < n; i++) {
		unsigned char hdr[0x20];
		struct wmt_rom_patch_info ri;

		if (read_hdr(files[i], hdr, sizeof(hdr)))
			continue;
		memset(&ri, 0, sizeof(ri));
		ri.type = hdr[0x1f];
		ri.addr[0] = 0;
		ri.addr[1] = hdr[0x19];
		ri.addr[2] = hdr[0x1a];
		ri.addr[3] = hdr[0x1b];
		snprintf(ri.name, sizeof(ri.name), "%s", files[i]);
		logk("rom patch type %u addr %02x %02x %02x %02x %s\n", ri.type,
		     ri.addr[0], ri.addr[1], ri.addr[2], ri.addr[3], ri.name);
		if (ioctl(fd, WMT_IOCTL_SET_ROM_PATCH_INFO, &ri) < 0)
			logk("SET_ROM_PATCH_INFO: %s\n", strerror(errno));
		else
			ok++;
	}
	return ok;
}

static int loader(void)
{
	int fd, chip, r, tries;

	for (tries = 0; tries < 50; tries++) {
		ensure_node("wmtdetect");
		fd = open("/dev/wmtdetect", O_RDWR | O_NOCTTY);
		if (fd >= 0)
			break;
		usleep(200000);
	}
	if (fd < 0) {
		logk("/dev/wmtdetect: %s\n", strerror(errno));
		return -1;
	}
	chip = ioctl(fd, COMBO_IOCTL_GET_SOC_CHIP_ID, 0);
	logk("soc chip id 0x%x\n", chip);
	if (chip <= 0) {
		close(fd);
		return -1;
	}
	r = ioctl(fd, COMBO_IOCTL_SET_CHIP_ID, chip);
	logk("set chip id: %d\n", r);
	r = ioctl(fd, COMBO_IOCTL_DO_MODULE_INIT, chip);
	logk("module init: %d\n", r);
	close(fd);
	return r;
}

static void launcher(void)
{
	char cfg[256], cmd[256];
	int fd = -1, tries, r;

	for (tries = 0; tries < 50; tries++) {
		ensure_node("stpwmt");
		fd = open("/dev/stpwmt", O_RDWR | O_NOCTTY);
		if (fd >= 0)
			break;
		usleep(200000);
	}
	if (fd < 0) {
		logk("/dev/stpwmt: %s\n", strerror(errno));
		exit(1);
	}
	memset(cfg, 0, sizeof(cfg));
	snprintf(cfg, sizeof(cfg), "WMT_SOC.cfg");
	if (ioctl(fd, WMT_IOCTL_WMT_CFG_NAME, cfg) < 0)
		logk("CFG_NAME: %s\n", strerror(errno));
	/* STP_BTIF_FULL (3) | WMT_FM_COMM (2) << 4 */
	if (ioctl(fd, WMT_IOCTL_SET_STP_MODE, 0x23) < 0)
		logk("SET_STP_MODE: %s\n", strerror(errno));
	ioctl(fd, WMT_IOCTL_SET_LAUNCHER_KILL, 0);
	logk("launcher bereit (fw %s, prefix %s)\n", fwdir, prefix);

	for (;;) {
		fd_set rfds;
		struct timeval tv = { 5, 0 };

		FD_ZERO(&rfds);
		FD_SET(fd, &rfds);
		r = select(fd + 1, &rfds, NULL, NULL, &tv);
		if (r <= 0)
			continue;
		memset(cmd, 0, sizeof(cmd));
		r = read(fd, cmd, sizeof(cmd) - 1);
		if (r <= 0) {
			usleep(100000);
			continue;
		}
		logk("kernel fragt: %s\n", cmd);
		if (!strcmp(cmd, "srh_patch")) {
			r = answer_srh_patch(fd);
			(void)write(fd, r ? "ok" : "fail", r ? 2 : 4);
		} else if (!strcmp(cmd, "srh_rom_patch")) {
			r = answer_srh_rom_patch(fd);
			(void)write(fd, r ? "ok" : "fail", r ? 2 : 4);
		} else {
			(void)write(fd, "ok", 2);
		}
	}
}

int main(int argc, char **argv)
{
	int o;

	while ((o = getopt(argc, argv, "p:x:")) != -1) {
		if (o == 'p')
			fwdir = optarg;
		else if (o == 'x')
			prefix = optarg;
		else {
			fprintf(stderr, "usage: %s [-p fwdir] [-x prefix] loader|launcher\n", argv[0]);
			return 2;
		}
	}
	if (optind >= argc) {
		fprintf(stderr, "usage: %s [-p fwdir] [-x prefix] loader|launcher\n", argv[0]);
		return 2;
	}
	signal(SIGPIPE, SIG_IGN);
	if (!strcmp(argv[optind], "loader"))
		return loader() < 0 ? 1 : 0;
	if (!strcmp(argv[optind], "launcher"))
		launcher();
	return 2;
}
