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

#include <unistd.h>
#include <sys/wait.h>
#include <glib/gstdio.h>

#include <errno.h>
#include <string.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <net/route.h>

#define CONNMAN_API_SUBJECT_TO_CHANGE
#include <connman/plugin.h>
#include <connman/driver.h>
#include <connman/inet.h>
#include <connman/dbus.h>
#include <connman/log.h>
#include <connman/resolver.h>

#define	_DBG_DHCLIENT(fmt, arg...)	DBG(DBG_DHCLIENT, fmt, ## arg)

#define DHCLIENT_INTF "org.isc.dhclient"
#define DHCLIENT_PATH "/org/isc/dhclient"

static const char *busname;

struct dhclient_task {
	GPid pid;
	gboolean killed;
	int ifindex;
	gchar *ifname;
	struct connman_element *element;
	struct dhclient_task *pending;
};

static GSList *task_list = NULL;

static struct dhclient_task *find_task_by_pid(GPid pid)
{
	GSList *list;

	for (list = task_list; list; list = list->next) {
		struct dhclient_task *task = list->data;

		if (task->pid == pid)
			return task;
	}

	return NULL;
}

static struct dhclient_task *find_task_by_index(int index)
{
	GSList *list;

	for (list = task_list; list; list = list->next) {
		struct dhclient_task *task = list->data;

		if (task->ifindex == index)
			return task;
	}

	return NULL;
}

static void kill_task(struct dhclient_task *task)
{
	_DBG_DHCLIENT("task %p name %s pid %d", task, task->ifname, task->pid);

	if (task->killed == TRUE)
		return;

	if (task->pid > 0) {
		task->killed = TRUE;
		kill(task->pid, SIGTERM);
	}
}

static void unlink_task(struct dhclient_task *task)
{
	gchar *pathname;

	_DBG_DHCLIENT("task %p name %s pid %d", task, task->ifname, task->pid);

	pathname = g_strdup_printf("%s/dhclient.%s.pid",
						STATEDIR, task->ifname);
	g_unlink(pathname);
	g_free(pathname);

	pathname = g_strdup_printf("%s/dhclient.%s.leases",
						STATEDIR, task->ifname);
	g_unlink(pathname);
	g_free(pathname);
}

static int start_dhclient(struct dhclient_task *task);

static void task_died(GPid pid, gint status, gpointer data)
{
	struct dhclient_task *task = data;

	if (WIFEXITED(status))
		_DBG_DHCLIENT("exit status %d for %s", WEXITSTATUS(status), task->ifname);
	else
		_DBG_DHCLIENT("signal %d killed %s", WTERMSIG(status), task->ifname);

	g_spawn_close_pid(pid);
	task->pid = 0;

	task_list = g_slist_remove(task_list, task);

	unlink_task(task);

	if (task->pending != NULL)
		start_dhclient(task->pending);

	g_free(task->ifname);
	g_free(task);
}

static void task_setup(gpointer data)
{
	struct dhclient_task *task = data;

	_DBG_DHCLIENT("task %p name %s", task, task->ifname);

	task->killed = FALSE;
}

static int start_dhclient(struct dhclient_task *task)
{
	char *argv[16], *envp[1], address[128], pidfile[PATH_MAX];
	char leases[PATH_MAX], config[PATH_MAX], script[PATH_MAX];

	snprintf(address, sizeof(address) - 1, "BUSNAME=%s", busname);
	snprintf(pidfile, sizeof(pidfile) - 1,
			"%s/dhclient.%s.pid", STATEDIR, task->ifname);
	snprintf(leases, sizeof(leases) - 1,
			"%s/dhclient.%s.leases", STATEDIR, task->ifname);
	snprintf(config, sizeof(config) - 1, "%s/dhclient.conf", SCRIPTDIR);
	snprintf(script, sizeof(script) - 1, "%s/dhclient-script", SCRIPTDIR);

	argv[0] = DHCLIENT;
	argv[1] = "-d";
	argv[2] = "-q";
	argv[3] = "-e";
	argv[4] = address;
	argv[5] = "-pf";
	argv[6] = pidfile;
	argv[7] = "-lf";
	argv[8] = leases;
	argv[9] = "-cf";
	argv[10] = config;
	argv[11] = "-sf";
	argv[12] = script;
	argv[13] = task->ifname;
	argv[14] = "-n";
	argv[15] = NULL;

	envp[0] = NULL;

	if (g_spawn_async(NULL, argv, envp, G_SPAWN_DO_NOT_REAP_CHILD,
				task_setup, task, &task->pid, NULL) == FALSE) {
		connman_error("Failed to spawn dhclient");
		return -1;
	}

	task_list = g_slist_append(task_list, task);

	g_child_watch_add(task->pid, task_died, task);

	_DBG_DHCLIENT("executed %s with pid %d", DHCLIENT, task->pid);

	return 0;
}

static int dhclient_probe(struct connman_element *element)
{
	struct dhclient_task *task, *previous;
	_DBG_DHCLIENT("element %p name %s", element, element->name);

	if (access(DHCLIENT, X_OK) < 0)
		return -errno;

	task = g_try_new0(struct dhclient_task, 1);
	if (task == NULL)
		return -ENOMEM;

	task->ifindex = element->index;
	task->ifname  = connman_inet_ifname(element->index);
	task->element = element;

	if (task->ifname == NULL) {
		g_free(task);
		return -ENOMEM;
	}

	previous= find_task_by_index(element->index);
	if (previous != NULL) {
		previous->pending = task;
		kill_task(previous);
		return 0;
	}

	return start_dhclient(task);
}

static void dhclient_remove(struct connman_element *element)
{
	struct dhclient_task *task;

	_DBG_DHCLIENT("element %p name %s", element, element->name);

	task = find_task_by_index(element->index);
	if (task == NULL)
		return;

	_DBG_DHCLIENT("release %s", task->ifname);

	kill_task(task);
}

static void dhclient_change(struct connman_element *element)
{
	_DBG_DHCLIENT("element %p name %s", element, element->name);

	if (element->state == CONNMAN_ELEMENT_STATE_ERROR)
		connman_element_set_error(element->parent,
					CONNMAN_ELEMENT_ERROR_DHCP_FAILED);
}

static struct connman_driver dhclient_driver = {
	.name		= "dhclient",
	.type		= CONNMAN_ELEMENT_TYPE_DHCP,
	.probe		= dhclient_probe,
	.remove		= dhclient_remove,
	.change		= dhclient_change,
};

static int add_hostroute(char *ifname, const char *ipaddr, const char *gateway)
{
	struct rtentry rt;
	struct sockaddr_in addr;
	int sk, err;

	_DBG_DHCLIENT("ifname %s ipaddr %s gateway %s", ifname, ipaddr, gateway);

	sk = socket(PF_INET, SOCK_DGRAM, 0);
	if (sk < 0) {
		connman_error("socket failed (%s)", strerror(errno));
		return -1;
	}

	memset(&rt, 0, sizeof(rt));
	rt.rt_flags = RTF_UP | RTF_HOST | RTF_GATEWAY;

	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = inet_addr(ipaddr);
	memcpy(&rt.rt_dst, &addr, sizeof(rt.rt_dst));

	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = inet_addr(gateway);
	memcpy(&rt.rt_gateway, &addr, sizeof(rt.rt_gateway));

	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = INADDR_ANY;
	memcpy(&rt.rt_genmask, &addr, sizeof(rt.rt_genmask));

	rt.rt_dev = ifname;

	err = ioctl(sk, SIOCADDRT, &rt);
	if (err < 0)
		connman_error("Adding host route for DNS server %s failed "
				"(%s gateway %s)", ipaddr, gateway,
							strerror(errno));
	close(sk);

	return err;
}

static void dhclient_add_dnsproxys(struct dhclient_task *task,
					const gchar *server_spec,
					gchar *domain_name)
{
	char *ifname;
	gchar **servers;
	int i;

	_DBG_DHCLIENT("index %d server_spec %s domain_name %s",
	    task->ifindex, server_spec, domain_name);

	ifname = connman_inet_ifname(task->ifindex);
	if (ifname == NULL) {
		connman_error("No interface with index %d", task->ifindex);
		return;
	}
	if (server_spec == NULL || *server_spec == '\0') {
		connman_error("No nameservers for %s defined", ifname);
		goto done;
	}
	/* TODO(sleffler): max 5 servers accepted */
	servers = g_strsplit(server_spec, " ", 5);
	for (i = 0; servers[i] != NULL; i++) {
		/*
		 * Add resolver and host route to reach server as
		 * DNS proxy does SO_BINDTODEVICE on socket used to
		 * fwd requests which bypasses the routing table.
		 */
		connman_resolver_append(ifname, domain_name, servers[i]);
		add_hostroute(ifname, servers[i], task->element->ipv4.gateway);
	}
	g_strfreev(servers);
	if (i == 0)
		connman_error("Empty server_spec \"%s\" for %s",
		    server_spec, ifname);
done:
	g_free(ifname);
}

static DBusHandlerResult dhclient_filter(DBusConnection *conn,
						DBusMessage *msg, void *data)
{
	DBusMessageIter iter, dict;
	dbus_uint32_t pid;
	struct dhclient_task *task;
	const char *text, *key, *value;
	gchar *name_servers = NULL;
	gchar *domain_name = NULL;

	if (dbus_message_is_method_call(msg, DHCLIENT_INTF, "notify") == FALSE)
		return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

	dbus_message_iter_init(msg, &iter);

	dbus_message_iter_get_basic(&iter, &pid);
	dbus_message_iter_next(&iter);

	dbus_message_iter_get_basic(&iter, &text);
	dbus_message_iter_next(&iter);

	_DBG_DHCLIENT("change %d to %s", pid, text);

	task = find_task_by_pid(pid);

	if (task == NULL) {
		connman_error("No task for pid %d", pid);
		return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
	}

	dbus_message_iter_recurse(&iter, &dict);

	while (dbus_message_iter_get_arg_type(&dict) == DBUS_TYPE_DICT_ENTRY) {
		DBusMessageIter entry;

		dbus_message_iter_recurse(&dict, &entry);
		dbus_message_iter_get_basic(&entry, &key);
		dbus_message_iter_next(&entry);
		dbus_message_iter_get_basic(&entry, &value);

		_DBG_DHCLIENT("%s = %s", key, value);

		if (g_ascii_strcasecmp(key, "new_ip_address") == 0) {
			g_free(task->element->ipv4.address);
			task->element->ipv4.address = g_strdup(value);
		}

		if (g_ascii_strcasecmp(key, "new_subnet_mask") == 0) {
			g_free(task->element->ipv4.netmask);
			task->element->ipv4.netmask = g_strdup(value);
		}

		if (g_ascii_strcasecmp(key, "new_routers") == 0) {
			g_free(task->element->ipv4.gateway);
			task->element->ipv4.gateway = g_strdup(value);
		}

		if (g_ascii_strcasecmp(key, "new_network_number") == 0) {
			g_free(task->element->ipv4.network);
			task->element->ipv4.network = g_strdup(value);
		}

		if (g_ascii_strcasecmp(key, "new_broadcast_address") == 0) {
			g_free(task->element->ipv4.broadcast);
			task->element->ipv4.broadcast = g_strdup(value);
		}

		if (g_ascii_strcasecmp(key, "new_domain_name_servers") == 0) {
			g_free(name_servers);
			name_servers = g_strdup(value);
		}

		if (g_ascii_strcasecmp(key, "new_domain_name") == 0) {
			g_free(domain_name);
			domain_name = g_strdup(value);
		}

		if (g_ascii_strcasecmp(key, "new_domain_search") == 0) {
		}

		if (g_ascii_strcasecmp(key, "new_host_name") == 0) {
		}

		dbus_message_iter_next(&dict);
	}

	if (g_ascii_strcasecmp(text, "PREINIT") == 0) {
	} else if (g_ascii_strcasecmp(text, "BOUND") == 0 ||
				g_ascii_strcasecmp(text, "REBOOT") == 0) {
		struct connman_element *element;
		element = connman_element_create(NULL);
		element->type = CONNMAN_ELEMENT_TYPE_IPV4;
		element->index = task->ifindex;
		connman_element_update(task->element);
		if (connman_element_register(element, task->element) < 0)
			connman_element_unref(element);
		else
			dhclient_add_dnsproxys(task, name_servers, domain_name);
	} else if (g_ascii_strcasecmp(text, "RENEW") == 0 ||
				g_ascii_strcasecmp(text, "REBIND") == 0) {
		connman_element_update(task->element);
	} else if (g_ascii_strcasecmp(text, "FAIL") == 0) {
		connman_element_set_error(task->element,
						CONNMAN_ELEMENT_ERROR_FAILED);
	} else {
	}

	g_free(name_servers);
	g_free(domain_name);

	return DBUS_HANDLER_RESULT_HANDLED;
}

static DBusConnection *connection;

static const char *dhclient_rule = "path=" DHCLIENT_PATH
						",interface=" DHCLIENT_INTF;

static int dhclient_init(void)
{
	int err;

	connection = connman_dbus_get_connection();

	busname = dbus_bus_get_unique_name(connection);
	busname = CONNMAN_SERVICE;

	dbus_connection_add_filter(connection, dhclient_filter, NULL, NULL);

	dbus_bus_add_match(connection, dhclient_rule, NULL);

	err = connman_driver_register(&dhclient_driver);
	if (err < 0) {
		dbus_connection_unref(connection);
		return err;
	}

	return 0;
}

static void dhclient_exit(void)
{
	GSList *list;

	for (list = task_list; list; list = list->next) {
		struct dhclient_task *task = list->data;

		_DBG_DHCLIENT("killing process %d", task->pid);

		kill_task(task);
		unlink_task(task);
	}

	g_slist_free(task_list);

	connman_driver_unregister(&dhclient_driver);

	dbus_bus_remove_match(connection, dhclient_rule, NULL);

	dbus_connection_remove_filter(connection, dhclient_filter, NULL);

	dbus_connection_unref(connection);
}

CONNMAN_PLUGIN_DEFINE(dhclient, "ISC DHCP client plugin", VERSION,
		CONNMAN_PLUGIN_PRIORITY_DEFAULT, dhclient_init, dhclient_exit)
