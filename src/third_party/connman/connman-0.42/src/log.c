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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdarg.h>
#include <syslog.h>

#include <gdbus.h>

#include "connman.h"

static volatile unsigned debug_enabled = 0;
static gchar *debug_enabled_str = NULL;

/**
 * connman_info:
 * @format: format string
 * @Varargs: list of arguments
 *
 * Output general information
 */
void connman_info(const char *format, ...)
{
	va_list ap;

	va_start(ap, format);

	vsyslog(LOG_INFO, format, ap);

	va_end(ap);
}

/**
 * connman_warn:
 * @format: format string
 * @Varargs: list of arguments
 *
 * Output warning messages
 */
void connman_warn(const char *format, ...)
{
	va_list ap;

	va_start(ap, format);

	vsyslog(LOG_WARNING, format, ap);

	va_end(ap);
}

/**
 * connman_error:
 * @format: format string
 * @varargs: list of arguments
 *
 * Output error messages
 */
void connman_error(const char *format, ...)
{
	va_list ap;

	va_start(ap, format);

	vsyslog(LOG_ERR, format, ap);

	va_end(ap);
}

/**
 * connman_debug:
 * @mask: debug message mask
 * @format: format string
 * @varargs: list of arguments
 *
 * Output debug message
 *
 * The actual output of the debug message is controlled via a command line
 * switch. If not enabled, these messages will be ignored.
 */
void connman_debug(unsigned mask, const char *format, ...)
{
	va_list ap;

	if ((debug_enabled & mask) == 0)
		return;

	va_start(ap, format);

	vsyslog(LOG_DEBUG, format, ap);

	va_end(ap);
}

unsigned __connman_debug_setmask(unsigned debugmask)
{
	unsigned omask = debug_enabled;

	debug_enabled = debugmask;
	g_free(debug_enabled_str);
	debug_enabled_str = g_strdup_printf("0x%x", debug_enabled);

	return omask;
}

unsigned __connman_debug_getmask(void)
{
	return debug_enabled;
}

const gchar *__connman_debug_getmask_str(void)
{
	return debug_enabled_str;
}

gboolean __connman_debug_enabled(unsigned debugmask)
{
	return (debug_enabled & debugmask) ? TRUE : FALSE;
}

int __connman_log_init(gboolean detach, unsigned debugmask)
{
	int option = LOG_NDELAY | LOG_PID;

	if (detach == FALSE)
		option |= LOG_PERROR;

	openlog("connmand", option, LOG_DAEMON);

	syslog(LOG_INFO, "Connection Manager version %s", VERSION);

	__connman_debug_setmask(debugmask);

	return 0;
}

void __connman_log_cleanup(void)
{
	syslog(LOG_INFO, "Exit");

	closelog();

	g_free(debug_enabled_str);
	debug_enabled_str = NULL;
}
