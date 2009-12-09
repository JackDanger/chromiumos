/*
 *
 *  Connection Manager
 *
 *  Copyright (C) 2007-2009  Intel Corporation. All rights reserved.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#ifndef __CONNMAN_LOG_H
#define __CONNMAN_LOG_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * SECTION:log
 * @title: Logging premitives
 * @short_description: Functions for logging error and debug information
 */

void connman_info(const char *format, ...)
				__attribute__((format(printf, 1, 2)));
void connman_warn(const char *format, ...)
				__attribute__((format(printf, 1, 2)));
void connman_error(const char *format, ...)
				__attribute__((format(printf, 1, 2)));
void connman_debug(unsigned mask, const char *format, ...)
				__attribute__((format(printf, 2, 3)));

/**
 * DBG:
 * @mask: bit mask of debug classes
 * @fmt: format string
 * @arg...: list of arguments
 *
 * Simple macro around connman_debug() which also include the function
 * name it is called in.
 */
#define DBG(mask, fmt, arg...) connman_debug(mask, "%s:%s() " fmt, __FILE__, __FUNCTION__ , ## arg)

enum {
	DBG_AGENT	= 0x00000001,
	DBG_CONNECTION	= 0x00000002,
	DBG_DEVICE	= 0x00000004,
	DBG_ELEMENT	= 0x00000008,
	DBG_INET	= 0x00000010,
	DBG_MANAGER	= 0x00000020,
	DBG_NETWORK	= 0x00000040,
	DBG_NOTIFIER	= 0x00000080,
	DBG_PROFILE	= 0x00000100,
	DBG_RESOLV	= 0x00000200,
	DBG_RFKILL	= 0x00000400,
	DBG_RTNL	= 0x00000800,
	DBG_SECURITY	= 0x00001000,
	DBG_SERVICE	= 0x00002000,
	DBG_STORAGE	= 0x00004000,
	DBG_TASK	= 0x00008000,
	DBG_TEST	= 0x00010000,
	DBG_UDEV	= 0x00020000,
	/* 0x000c0000 available */

	DBG_PLUGIN	= 0x00100000,
	/* NB: plugins start at DBG_PLUGIN */
	DBG_BLUETOOTH	= 0x00200000,
	DBG_DHCLIENT	= 0x00400000,
	DBG_DNSPROXY	= 0x00800000,
	DBG_ETHERNET	= 0x01000000,
	DBG_IWMX	= 0x02000000,
	DBG_PPPD	= 0x04000000,
	DBG_UDHCP	= 0x08000000,
	DBG_WIFI	= 0x10000000
	/* 0xe0000000 available */
};
#define	DBG_ANY		0xffffffff

#ifdef __cplusplus
}
#endif

#endif /* __CONNMAN_LOG_H */
