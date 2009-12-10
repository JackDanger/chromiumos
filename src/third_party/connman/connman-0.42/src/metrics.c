/*
 *
 *  Connection Manager Metrics - summarize information on the DBus for
 *  metrics usage.
 *
 *  This file initially created by Google, Inc.
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

#include <stdlib.h>
#include <gdbus.h>

#include "connman.h"

#define	_DBG_METRICS(fmt, arg...)	DBG(DBG_METRICS, fmt, ## arg)
#define CONNMAN_METRICS_STATE_CHANGED_SIGNAL "ConnectionStateChanged"

static DBusConnection *connection;

static GDBusMethodTable metrics_methods[] = {
	{ },
};

static GDBusSignalTable metrics_signals[] = {
	{ CONNMAN_METRICS_STATE_CHANGED_SIGNAL,    "s"  },
	{ },
};

void __connman_metrics_state_changed(const char *state_name)
{
	DBusMessage *signal;
	DBusMessageIter entry, value;
	const char *key = "ConnectionState";

	signal = dbus_message_new_signal(CONNMAN_METRICS_PATH,
					 CONNMAN_METRICS_INTERFACE,
                                         CONNMAN_METRICS_STATE_CHANGED_SIGNAL);
	if (signal == NULL)
		return;

	dbus_message_iter_init_append(signal, &entry);

	dbus_message_iter_append_basic(&entry, DBUS_TYPE_STRING, &key);

	dbus_message_iter_open_container(&entry, DBUS_TYPE_VARIANT,
					DBUS_TYPE_STRING_AS_STRING, &value);
	dbus_message_iter_append_basic(&value, DBUS_TYPE_STRING, &state_name);
	dbus_message_iter_close_container(&entry, &value);

	g_dbus_send_message(connection, signal);
}

int __connman_metrics_init(void)
{
	_DBG_METRICS("");

	connection = connman_dbus_get_connection();
	if (connection == NULL)
		return -1;

	g_dbus_register_interface(connection, CONNMAN_METRICS_PATH,
					CONNMAN_METRICS_INTERFACE,
					metrics_methods,
					metrics_signals, NULL, NULL, NULL);

	return 0;
}

void __connman_metrics_cleanup(void)
{
	_DBG_METRICS("");

	if (connection == NULL)
		return;

	g_dbus_unregister_interface(connection, CONNMAN_METRICS_PATH,
						CONNMAN_METRICS_INTERFACE);

	dbus_connection_unref(connection);
}
