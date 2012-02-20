/*
 * fjbtndrv dbus proxy daemon
 *
 * Copyright (C) 2012 Robert Gerlach <khnz@users.sourceforge.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <locale.h>
#include <glib.h>
#include <gio/gio.h>
#include <linux/input.h>
#include <sys/ioctl.h>
#include <syslog.h>

#include "fjbtndrv.h"

#define BIT(n) (1UL << n)

static gboolean no_daemonize = FALSE;
static gchar *device_file = NULL;

static const GOptionEntry options[] = {
#ifdef DEBUG
	{ "no-daemonize", 'f', 0, G_OPTION_ARG_NONE, &no_daemonize,
	  "Do not daemonize, run in foreground", NULL },
#endif
	{ "device", 'd', 0, G_OPTION_ARG_FILENAME, &device_file,
	  "input device file", NULL },
	{ NULL }
};

static const gchar introspection_xml[] =
	"<node>"
	"  <interface name='" FJBTNDRV_DBUS_SERVICE_INTERFACE "'>"
	"    <property type='b' name='TabletMode' access='read' />"
	"    <signal name='TabletModeChanged'>"
	"      <arg direction='out' name='value' type='b' />"
	"    </signal>"
	"    <property type='b' name='DockState' access='read' />"
	"    <signal name='DockStateChanged'>"
	"      <arg direction='out' name='value' type='b' />"
	"    </signal>"
	"  </interface>"
	"</node>";

static struct FjbtndrvSwitchStates {
	gboolean tablet_mode;
	gboolean dock_state;
} state;

static GDBusNodeInfo *introspection_data = NULL;
static GDBusConnection *dbus;
static GMainLoop *mainloop;


static void
dbus_emit_signal(const char *name, GVariant *parameters)
{
	GError *error = NULL;

	g_return_if_fail (dbus);

	debug("fjbtndrv_proxy_emit_signal: signal=%s parameters=%s",
			name, g_variant_print(parameters, TRUE));

	g_dbus_connection_emit_signal(
			dbus,
			NULL,
			FJBTNDRV_DBUS_SERVICE_PATH,
			FJBTNDRV_DBUS_SERVICE_INTERFACE,
			name,
			parameters,
			&error);
	if (error) {
		g_warning("%s", error->message);
		g_error_free(error);
	}
}

static GVariant *
dbus_get_property(GDBusConnection *connection, const gchar *sender, const gchar *object_path, const gchar *interface_name, const gchar *property_name, GError **error, gpointer user_data)
{
	GVariant *value = NULL;

	debug("handle_get_property: sender=%s path=%s interface=%s name=%s",
			sender, object_path, interface_name, property_name);

	if (g_strcmp0 (property_name, "TabletMode") == 0) {
		value = g_variant_new_boolean(state.tablet_mode);
	}
	else if (g_strcmp0 (property_name, "DockState") == 0) {
		value = g_variant_new_boolean(state.dock_state);
	}

	return value;
}

static const GDBusInterfaceVTable fjbtndrv_proxy_vtable = {
	NULL,
	dbus_get_property,
	NULL,
};


void
set_tablet_mode(gboolean value)
{
	debug("fjbtndrv_proxy_set_tablet_mode: value=%d", value);

	GVariant *v_value = g_variant_new("(b)", value);

	state.tablet_mode = value;
	dbus_emit_signal("TabletModeChanged", v_value);

	g_variant_unref(v_value);
}

void
set_dock_state(gboolean value)
{
	debug("fjbtndrv_proxy_set_dock_state: value=%d", value);

	GVariant *v_value = g_variant_new("(b)", value);

	state.dock_state = value;
	dbus_emit_signal("DockStateChanged", v_value);

	g_variant_unref(v_value);
}


static void
on_switch_event(struct input_event *event)
{
	switch (event->code) {
	case SW_TABLET_MODE:
		set_tablet_mode(event->value);
		break;

	case SW_DOCK:
		set_dock_state(event->value);
		break;
	}
}

static gboolean
on_event(GIOChannel *source, GIOCondition condition, gpointer user_data)
{
	struct input_event event;
	GIOStatus status;
	GError *error = NULL;

	status = g_io_channel_read_chars(source, (gchar*) &event, sizeof(event),
			NULL, &error);

	switch (status) {
	case G_IO_STATUS_NORMAL:
		debug("input_event_dispatcher: timestamp=%lu.%lu  type=%04d code=%04d value=%d",
				event.time.tv_sec, event.time.tv_usec, event.type, event.code, event.value);

		switch (event.type) {
		case EV_SW:
			on_switch_event(&event);
			break;
		}

		return TRUE;

	case G_IO_STATUS_ERROR:
		g_error("%s", error->message);
		g_error_free(error);
	
	default:
		return FALSE;
	}
}

static void
on_bus_acquired (GDBusConnection *connection, const gchar *name, gpointer user_data)
{
	GError *error = NULL;

	debug("on_bus_acquired: name=%s", name);

	guint id = g_dbus_connection_register_object(
			connection, 
			FJBTNDRV_DBUS_SERVICE_PATH,
			introspection_data->interfaces[0],
			&fjbtndrv_proxy_vtable,
			NULL, NULL, &error);
	if (error)
		g_error("%s", error->message);

	g_assert (id > 0);
}

static void
on_name_acquired (GDBusConnection *connection, const gchar *name, gpointer user_data)
{
	debug("on_name_acquired: name=%s", name);
	dbus = connection;
}

static void
on_name_lost (GDBusConnection *connection, const gchar *name, gpointer user_data)
{
	debug("on_name_lost: name=%s", name);
	dbus = NULL;
	// TODO: reconnect
	g_main_loop_quit(mainloop);
}

/*
static void
on_device_lost(void)
{
	debug("device lost");
	g_main_loop_quit(mainloop);
}
*/


static GIOChannel*
open_device(const char* devname, GError **error)
{
	GIOChannel *gioc;

	*error = NULL;

#if 0
	char *devname;

	devname = getenv("DEVNAME");
	if (devname == NULL) {
		g_set_error(error, 0, 1, "DEVNAME not set.");
		return NULL;
	}
#endif

	syslog(LOG_DEBUG, "device file: %s", devname);

	gioc = g_io_channel_new_file(devname, "r", error);
	if (*error)
		return NULL;

	g_io_channel_set_encoding(gioc, NULL, error);

	return gioc;
}

int
main(int argc, char *argv[])
{
	GOptionContext *context;
	GIOChannel *device = NULL;
	GError *error = NULL;

	setlocale (LC_ALL, "");

	g_type_init();

	context = g_option_context_new ("fjbtndrv dbus proxy daemon");
	g_option_context_add_main_entries (context, options, NULL);
	g_option_context_parse (context, &argc, &argv, NULL);

	if (!device_file) {
		fprintf(stderr, "Syntax: %s --device <DEVICE>\n", argv[0]);
		return 1;
	}

	g_option_context_free (context);

	if (!no_daemonize)
		if (daemon(0, 0) < 0)
			return 0;

	/* XXX: g_die() or {
		GLogLevelFlags mask;
		mask = g_log_set_always_fatal(G_LOG_FATAL_MASK);
		mask |= G_LOG_LEVEL_CRITICAL;
		g_log_set_always_fatal(mask);
	} */

	openlog("fjbproxy", LOG_PID | LOG_CONS, LOG_DAEMON);

	debug(" * initialization");

	introspection_data = g_dbus_node_info_new_for_xml (introspection_xml, NULL);

	(void)g_bus_own_name (
			G_BUS_TYPE_SYSTEM,
			FJBTNDRV_DBUS_SERVICE_NAME,
			G_BUS_NAME_OWNER_FLAGS_NONE,
			on_bus_acquired,
			on_name_acquired,
			on_name_lost,
			NULL,
			NULL);


	mainloop = g_main_loop_new(NULL, FALSE);

	device = open_device(device_file, &error);
	if (error) {
		syslog(LOG_ERR, "failed to open device - %s", error->message);
		goto out;
	}

	/* get and set initial switch states */
	{
		gulong switches = 0;
		gint fd = g_io_channel_unix_get_fd(device);

		if (ioctl(fd, EVIOCGSW(sizeof(switches)), &switches) >= 0) {
			set_tablet_mode((switches & BIT(SW_TABLET_MODE)) >> SW_TABLET_MODE);
			set_dock_state((switches & BIT(SW_DOCK)) >> SW_DOCK);
		}
	}

	g_io_add_watch(device, G_IO_IN|G_IO_ERR|G_IO_HUP,
			(GIOFunc) on_event, NULL);

	debug(" * start");

	g_main_loop_run(mainloop);
	

out:
	debug(" * shutdown");

	if (mainloop)
		g_main_loop_unref(mainloop);
	if (device)
		g_io_channel_unref(device);

	closelog();

	return 0;
}

