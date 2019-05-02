/*
 * fbutils.h
 *
 * Headers for utility routines for framebuffer interaction
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

#pragma once
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <fcntl.h>

#include <linux/vt.h>
#include <linux/kd.h>
#include <linux/fb.h>

#include "common.h"

/* This constant, being ORed with the color index tells the library
 * to draw in exclusive-or mode (that is, drawing the same second time
 * in the same place will remove the element leaving the background intact).
 */
#define XORMODE	0x80000000

extern uint32_t xres, yres;
extern uint32_t xres_orig, yres_orig;
extern int8_t rotation;
extern int8_t alternative_cross;

int open_framebuffer(void);
void close_framebuffer(void);
