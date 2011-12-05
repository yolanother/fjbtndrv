/*******************************************************************************
 * fjbtndrv dbus proxy daemon
 * Copyright (C) 2011 Robert Gerlach <khnz@users.sourceforge.net>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA
 */

#ifdef HAVE_CONFIG_H
#  include "../config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <glib.h>
#include <gio/gio.h>
#include <linux/input.h>

#include "fjbtndrv-daemon.h"

#ifdef DEBUG
#define debug(msg, a...) g_debug(msg, ##a)
#else
#define debug(msg, a...) do {} while(0)
#endif

static const gchar introspection_xml[] =
	"<node>"
	"  <interface name='" FJBTNDRV_DAEMON_SERVICE_INTERFACE "'>"
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

static GDBusNodeInfo *introspection_data = NULL;

typedef struct input_event InputEvent;

typedef enum {
	PROP_0 = 0,
	PROP_TABLET_MODE,
	PROP_LAST
} FjbtndrvProperty;

static GMainLoop *mainloop;

// TODO:
static struct {
	gboolean tablet_mode;
	gboolean dock_state;
} FjbtndrvState;


static void stop_mainloop(gpointer);

G_DEFINE_TYPE(FjbtndrvDaemon, fjbtndrv_daemon, G_TYPE_OBJECT);


FjbtndrvDaemon*
fjbtndrv_daemon_new(void)
{
	return FJBTNDRV_DAEMON(g_object_new(FJBTNDRV_TYPE_DAEMON, NULL));
}

static void
fjbtndrv_daemon_init(FjbtndrvDaemon *daemon)
{
}

static void
fjbtndrv_daemon_class_init(FjbtndrvDaemonClass *klass)
{
}

/******************************************************************************/

static GVariant *
fjbtndrv_daemon_handle_get_property(GDBusConnection *connection, const gchar *sender, const gchar *object_path, const gchar *interface_name, const gchar *property_name, GError **error, gpointer user_data)
{
	GVariant *value = NULL;
	FjbtndrvDaemon *daemon = user_data;

	debug("handle_get_property: sender=%s path=%s interface=%s name=%s",
			sender, object_path, interface_name, property_name);

	if (g_strcmp0 (property_name, "TabletMode") == 0) {
		value = g_variant_new_boolean(FjbtndrvState.tablet_mode);
	}
	else if (g_strcmp0 (property_name, "DockState") == 0) {
		value = g_variant_new_boolean(FjbtndrvState.dock_state);
	}

	return value;
}

static const GDBusInterfaceVTable fjbtndrv_daemon_vtable = {
	NULL,
	fjbtndrv_daemon_handle_get_property,
	NULL,
};

static void
on_bus_acquired (GDBusConnection *connection, const gchar *name, gpointer user_data)
{
	FjbtndrvDaemon *daemon = user_data;
	GError *error = NULL;

	debug("on_bus_acquired: name=%s", name);

	guint id = g_dbus_connection_register_object(
			connection, 
			FJBTNDRV_DAEMON_SERVICE_PATH,
			introspection_data->interfaces[0],
			&fjbtndrv_daemon_vtable,
			daemon, NULL, &error);
	if (error)
		g_error("%s", error->message);

	g_assert (id > 0);
}

static void
on_name_acquired (GDBusConnection *connection, const gchar *name, gpointer user_data)
{
	FjbtndrvDaemon *daemon = user_data;

	debug("on_name_acquired: name=%s", name);

	daemon->dbus = connection;
}

static void
on_name_lost (GDBusConnection *connection, const gchar *name, gpointer user_data)
{
	FjbtndrvDaemon *daemon = user_data;

	debug("on_name_lost: name=%s", name);

	daemon->dbus = NULL;

	stop_mainloop(NULL);
}

/******************************************************************************/

static GIOChannel*
fjbtndrv_daemon_input_open_device(GError **error)
{
	char *devname;
	GIOChannel *gioc;
	int fd;

	*error = NULL;

	devname = getenv("DEVNAME");
	if (devname == NULL) {
		g_set_error(error, 0, 1, "DEVNAME not set.");
		return NULL;
	}

	debug("device file: %s", devname);

	gioc = g_io_channel_new_file(devname, "r", error);
	if (*error)
		return NULL;

	return g_io_channel_ref(gioc);
}

static void
fjbtndrv_daemon_input_handle_switch(FjbtndrvDaemon *daemon, InputEvent *event)
{
	GError *error = NULL;
	char *event_name = NULL;

	g_assert(daemon);
	g_assert(event);

	switch (event->code) {
	case SW_TABLET_MODE:
		FjbtndrvState.tablet_mode = event->value;
		event_name = "TabletModeChanged";
		break;

	case SW_DOCK:
		FjbtndrvState.dock_state = event->value;
		event_name = "DockStateChanged";
		break;
	}

	if ((daemon->dbus) && (event_name)) {
		debug("fjbtndrv_daemon_input_handle_switch: signal=%s value=%s",
				event_name, (event->value ? "true" : "false"));

		g_dbus_connection_emit_signal(
				daemon->dbus,
				NULL,
				FJBTNDRV_DAEMON_SERVICE_PATH,
				FJBTNDRV_DAEMON_SERVICE_INTERFACE,
				event_name,
				g_variant_new("(b)", event->value),
				&error);
		if (error) {
			g_warning("%s", error->message);
			g_error_free(error);
		}
	}
}

static gboolean
fjbtndrv_daemon_input_event_dispatcher(GIOChannel *source, GIOCondition condition, gpointer data)
{
	FjbtndrvDaemon *daemon = (FjbtndrvDaemon*) data;
	InputEvent event;
	int l;

	g_assert(daemon);

	l = read(g_io_channel_unix_get_fd(daemon->input),
			(char*)&event, sizeof(event));
	if (l != sizeof(event)) {
		debug("read failed (%d)", l);
		return FALSE;
	}

	debug("input_event_dispatcher: timestamp=%lu.%lu  type=%04d code=%04d value=%08d",
			event.time.tv_sec, event.time.tv_usec, event.type, event.code, event.value);

	switch (event.type) {
	case EV_SW:
		fjbtndrv_daemon_input_handle_switch(daemon, &event);
		break;
	}

	return TRUE;
}

static inline void
fjbtndrv_daemon_register_callbacks(FjbtndrvDaemon *daemon, GMainLoop *mainloop)
{
	GSource *source = g_io_create_watch(daemon->input, G_IO_IN);
	g_source_set_callback(source,
			(GSourceFunc) fjbtndrv_daemon_input_event_dispatcher, daemon,
			(GDestroyNotify) stop_mainloop);
	g_source_attach(source, g_main_loop_get_context(mainloop));
	g_source_unref(source);
}


/******************************************************************************/

static void
stop_mainloop(gpointer data)
{
	if (mainloop)
		g_main_loop_quit(mainloop);
}

int
main(int argc, char *argv[])
{
	FjbtndrvDaemon *daemon;
	guint dbus_owner_id;
	GError *error;
	gboolean success;

	g_type_init();

	introspection_data = g_dbus_node_info_new_for_xml (introspection_xml, NULL);
	g_assert (introspection_data);

	/* XXX: g_die() or {
		GLogLevelFlags mask;
		mask = g_log_set_always_fatal(G_LOG_FATAL_MASK);
		mask |= G_LOG_LEVEL_CRITICAL;
		g_log_set_always_fatal(mask);
	} */

	error = NULL;

	daemon = fjbtndrv_daemon_new();
	g_assert(daemon);

	daemon->input = fjbtndrv_daemon_input_open_device(&error);
	if (error)
		g_error("%s", error->message);

	g_assert(daemon->input);

	dbus_owner_id = g_bus_own_name (
			G_BUS_TYPE_SYSTEM,
			FJBTNDRV_DAEMON_SERVICE_NAME,
			G_BUS_NAME_OWNER_FLAGS_NONE,
			on_bus_acquired,
			on_name_acquired,
			on_name_lost,
			daemon,
			NULL);

	g_assert(dbus_owner_id > 0);

	mainloop = g_main_loop_new(NULL, FALSE);

	fjbtndrv_daemon_register_callbacks(daemon, mainloop);

	g_main_loop_run(mainloop);

out:

	if (mainloop)
		g_main_loop_unref(mainloop);
	if (dbus_owner_id > 0)
		g_bus_unown_name(dbus_owner_id);
	if (daemon->input)
		g_object_unref(daemon->input);
	if (daemon)
		g_object_unref(daemon);

	return 0;
}

