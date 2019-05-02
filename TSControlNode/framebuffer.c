/*
 * fbutils-linux.c
 *
 * Utility routines for framebuffer interaction
 *
 * Copyright 2002 Russell King and Doug Lowder
 *
 * This file is placed under the GPL.  Please see the
 * file COPYING for details.
 *
 * SPDX-License-Identifier: GPL-2.0+
 */

 /*
  * Modified to only include opening and closing blank framebuffer
  */

#include "framebuffer.h"

union multiptr {
	uint8_t *p8;
	uint16_t *p16;
	uint32_t *p32;
};

static int32_t con_fd, last_vt = -1;
static struct fb_fix_screeninfo fix;
static struct fb_var_screeninfo var;
static unsigned char *fbuffer;
static unsigned char **line_addr;
static int32_t fb_fd;
static int32_t bytes_per_pixel;
static uint32_t transp_mask;
static uint32_t colormap[256];
uint32_t xres, yres;
uint32_t xres_orig, yres_orig;
int8_t rotation;
int8_t alternative_cross;

static char *defaultfbdevice = "/dev/fb0";
static char *defaultconsoledevice = "/dev/tty";
static char *fbdevice;
static char *consoledevice;
 
#define VTNAME_LEN 128

int open_framebuffer(void)
{
	struct vt_stat vts;
	char vtname[VTNAME_LEN];
	int32_t fd, nr;
	uint32_t y, addr;

	if ((fbdevice = getenv("TSLIB_FBDEVICE")) == NULL)
		fbdevice = defaultfbdevice;

	if ((consoledevice = getenv("TSLIB_CONSOLEDEVICE")) == NULL)
		consoledevice = defaultconsoledevice;

	if (strcmp(consoledevice, "none") != 0) {
		if (strlen(consoledevice) >= VTNAME_LEN)
			return -1;

		sprintf(vtname, "%s%d", consoledevice, 1);
		fd = open(vtname, O_WRONLY);
		if (fd < 0) {
			perror("open consoledevice");
			return -1;
		}

		if (ioctl(fd, VT_OPENQRY, &nr) < 0) {
			close(fd);
			perror("ioctl VT_OPENQRY");
			return -1;
		}
		close(fd);

		sprintf(vtname, "%s%d", consoledevice, nr);

		con_fd = open(vtname, O_RDWR | O_NDELAY);
		if (con_fd < 0) {
			perror("open tty");
			return -1;
		}

		if (ioctl(con_fd, VT_GETSTATE, &vts) == 0)
			last_vt = vts.v_active;

		if (ioctl(con_fd, VT_ACTIVATE, nr) < 0) {
			perror("VT_ACTIVATE");
			close(con_fd);
			return -1;
		}

#ifndef TSLIB_NO_VT_WAITACTIVE
		if (ioctl(con_fd, VT_WAITACTIVE, nr) < 0) {
			perror("VT_WAITACTIVE");
			close(con_fd);
			return -1;
		}
#endif

		if (ioctl(con_fd, KDSETMODE, KD_GRAPHICS) < 0) {
			perror("KDSETMODE");
			close(con_fd);
			return -1;
		}

	}

	fb_fd = open(fbdevice, O_RDWR);
	if (fb_fd == -1) {
		perror("open fbdevice");
		return -1;
	}

	if (ioctl(fb_fd, FBIOGET_FSCREENINFO, &fix) < 0) {
		perror("ioctl FBIOGET_FSCREENINFO");
		close(fb_fd);
		return -1;
	}

	if (ioctl(fb_fd, FBIOGET_VSCREENINFO, &var) < 0) {
		perror("ioctl FBIOGET_VSCREENINFO");
		close(fb_fd);
		return -1;
	}

	xres_orig = var.xres;
	yres_orig = var.yres;

	if (rotation & 1) {
		/* 1 or 3 */
		y = var.yres;
		yres = var.xres;
		xres = y;
	} else {
		/* 0 or 2 */
		xres = var.xres;
		yres = var.yres;
	}

	fbuffer = mmap(NULL,
		       fix.smem_len,
		       PROT_READ | PROT_WRITE, MAP_FILE | MAP_SHARED,
		       fb_fd,
		       0);

	if (fbuffer == (unsigned char *)-1) {
		perror("mmap framebuffer");
		close(fb_fd);
		return -1;
	}
	memset(fbuffer, 0, fix.smem_len);

	bytes_per_pixel = (var.bits_per_pixel + 7) / 8;
	transp_mask = ((1 << var.transp.length) - 1) <<
		var.transp.offset; /* transp.length unlikely > 32 */
	line_addr = malloc(sizeof(*line_addr) * var.yres_virtual);
	addr = 0;
	for (y = 0; y < var.yres_virtual; y++, addr += fix.line_length)
		line_addr[y] = fbuffer + addr;

	return 0;
}

void close_framebuffer(void)
{
	memset(fbuffer, 0, fix.smem_len);
	munmap(fbuffer, fix.smem_len);
	close(fb_fd);

	if (strcmp(consoledevice, "none") != 0) {
		if (ioctl(con_fd, KDSETMODE, KD_TEXT) < 0)
			perror("KDSETMODE");

		if (last_vt >= 0)
			if (ioctl(con_fd, VT_ACTIVATE, last_vt))
				perror("VT_ACTIVATE");

		close(con_fd);
	}

	free(line_addr);

	xres = 0;
	yres = 0;
	rotation = 0;
}
