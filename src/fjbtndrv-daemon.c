/*******************************************************************************
 * fjbtndrv uinput daemon
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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/ioctl.h>

#include <linux/input.h>
#include <linux/uinput.h>

#include <glib.h>
#include <glib-object.h>
#include <dbus/dbus-glib.h>
#include <dbus/dbus-glib-lowlevel.h>

#include "fjbtndrv-daemon.h"
#include "fjbtndrv-daemon-glue.h"

#define DMI_PRODUCT_NAME "/sys/devices/virtual/dmi/id/product_name"

#define UINPUT_DEVNAME "/dev/uinput"

#define debug(msg, a...) g_printf(msg "\n", ##a)

#define tv_msec(tv) ((tv.tv_sec * 1000) + (tv.tv_usec / 1000))


typedef struct input_event InputEvent;

typedef enum {
	PROP_0,
	PROP_STICKY_TIMEOUT,
	PROP_LAST
} FjbtndrvProperty;

typedef enum {
	SIGNAL_CHANGED,
	SIGNAL_STICKY_TIMEOUT_CHANGED,
	SIGNAL_TABLET_MODE_CHANGED,
	SIGNAL_LAST
} FjbtndrvSignal;


static GMainLoop *mainloop;
static FjbtndrvDaemon *fjbtndrv;
static guint signals[SIGNAL_LAST];

// TODO:
typedef struct {
	guint key;
	guint time;
} FjbtndrvDaemonPrivate;
static FjbtndrvDaemonPrivate *private;

static void stop_mainloop(gpointer);
static void fjbtndrv_daemon_get_property(GObject*, guint, GValue*, GParamSpec*);
static void fjbtndrv_daemon_set_property(GObject*, guint, const GValue*, GParamSpec*);

static KeymapEntry keymap_none[] = {
	{ 0 }
};

static KeymapEntry keymap_t4010[] = {
	{ KEY_SCROLLDOWN,     KEY_A, KEY_PROG1 },
	{ KEY_SCROLLUP,       KEY_B, KEY_PROG2 },
	{ KEY_DIRECTION,      KEY_C, KEY_PROG3 },
	{ KEY_FN,                 0, KEY_PROG4 },
	{ KEY_SLEEP,              0, KEY_POWER },
	{ 0 }
};

G_DEFINE_TYPE(FjbtndrvDaemon, fjbtndrv_daemon, G_TYPE_OBJECT);

static void
fjbtndrv_daemon_class_init(FjbtndrvDaemonClass *klass)
{
	GObjectClass *object = G_OBJECT_CLASS(klass);

	g_assert(klass != NULL);
	g_assert(object != NULL);

	object->get_property = fjbtndrv_daemon_get_property,
	object->set_property = fjbtndrv_daemon_set_property,
	//object->finalize = fjbtndrv_daemon_finalize,

	signals[SIGNAL_CHANGED] =
		g_signal_new ("changed",
			G_OBJECT_CLASS_TYPE (klass),
			G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
			0, NULL, NULL,
			g_cclosure_marshal_VOID__VOID,
			G_TYPE_NONE, 0);

	signals[SIGNAL_TABLET_MODE_CHANGED] =
		g_signal_new("tablet-mode-changed",
			G_OBJECT_CLASS_TYPE(klass),
			G_SIGNAL_RUN_LAST,
			0, NULL, NULL,
			g_cclosure_marshal_VOID__BOOLEAN,
			G_TYPE_NONE,
			1, G_TYPE_BOOLEAN);

/*
	signals[SIGNAL_STICKY_TIMEOUT_CHANGED] =
		g_signal_new("sticky-timeout-changed",
			G_OBJECT_CLASS_TYPE(klass),
			G_SIGNAL_RUN_LAST,
			0, NULL, NULL,
			g_cclosure_marshal_VOID__INT,
			G_TYPE_NONE,
			1, G_TYPE_INT);
*/

	dbus_g_object_type_install_info(FJBTNDRV_TYPE_DAEMON, &dbus_glib_fjbtndrv_daemon_object_info);

	g_object_class_install_property(object,
			PROP_STICKY_TIMEOUT,
			g_param_spec_uint("sticky-timeout",
				NULL, NULL,
				0, 30000, 1400,
				G_PARAM_READWRITE));

}

static void
fjbtndrv_daemon_init(FjbtndrvDaemon *daemon)
{
	g_assert(daemon != NULL);

	FjbtndrvDaemonClass* klass = FJBTNDRV_DAEMON_GET_CLASS(daemon);
	g_assert(klass != NULL);

	daemon->sticky_timeout = 1400;

	daemon->skey = 0;
	daemon->stime = 0;
}

FjbtndrvDaemon*
fjbtndrv_daemon_new(void)
{
	FjbtndrvDaemon *daemon;
	daemon = FJBTNDRV_DAEMON(g_object_new(FJBTNDRV_TYPE_DAEMON, NULL));
	return daemon;
}

/*
static void
fjbtndrv_daemon_finalize(GObject *object)
{
	G_OBJECT_CLASS(fjbtndrv_daemon_parent_class)->finalize(object);
}
*/

static gboolean
fjbtndrv_daemon_set_config(FjbtndrvDaemon *daemon, GError **error)
{
	GIOChannel *gioc;
	GIOStatus gios;
	gsize len, tpos;

	gioc = g_io_channel_new_file(DMI_PRODUCT_NAME, "r", error);
	if (*error)
		return FALSE;

	gios = g_io_channel_read_line(gioc, &daemon->product, &len, &tpos, error);
	g_io_channel_close(gioc);
	if (gios != G_IO_STATUS_NORMAL)
		return FALSE;

	if (tpos > 0)
		daemon->product[tpos] = 0;

	debug("DMI PRODUCT: %s", daemon->product);

	if (strcmp(daemon->product, "LIFEBOOK T4010") == 0) {
		debug("USING KEYMAP: t4010");
		daemon->keymap = keymap_t4010;
	}
	else {
		debug("USING KEYMAP: none");
		daemon->keymap = keymap_none;
	}

	return TRUE;
}

gboolean
fjbtndrv_daemon_bind_dbus(FjbtndrvDaemon *daemon, DBusGConnection *dbus, GError **error)
{
	gboolean success;
	gint result;

	error = NULL;

	daemon->proxy = dbus_g_proxy_new_for_name(dbus,
			DBUS_SERVICE_DBUS, DBUS_PATH_DBUS, DBUS_INTERFACE_DBUS);
	g_assert(daemon->proxy != NULL);

//	success = org_freedesktop_DBus_request_name(daemon->proxy,
//			FJBTNDRV_DAEMON_SERVICE_NAME, 0, &result, error);
	success =  dbus_g_proxy_call(daemon->proxy, "RequestName", error,
			G_TYPE_STRING, FJBTNDRV_DAEMON_SERVICE_NAME,
			G_TYPE_UINT, 0,
			G_TYPE_INVALID,
			G_TYPE_UINT, &result,
			G_TYPE_INVALID);
	if ((!success) || (error))
		return FALSE;

	g_assert(result == DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER);

	dbus_g_connection_register_g_object(dbus,
			FJBTNDRV_DAEMON_SERVICE_PATH, G_OBJECT(daemon));

	return TRUE;
}

gboolean
fjbtndrv_daemon_register_callbacks(FjbtndrvDaemon *daemon, GMainLoop *mainloop)
{
	GSource *source;

	source = g_io_create_watch(daemon->input, G_IO_IN);
	g_source_set_callback(source,
			(GSourceFunc) fjbtndrv_daemon_input_event_dispatcher, daemon,
			(GDestroyNotify) stop_mainloop);
	g_source_attach(source, g_main_loop_get_context(mainloop));
	g_source_unref(source);

	return TRUE;
}

gboolean
fjbtndrv_daemon_input_event_dispatcher(GIOChannel *source, GIOCondition condition, gpointer data)
{
	FjbtndrvDaemon *daemon = (FjbtndrvDaemon*) data;
	InputEvent event;
	int l;

	g_assert(daemon != NULL);

	l = read(g_io_channel_unix_get_fd(daemon->input),
			(char*)&event, sizeof(event));
	if (l != sizeof(event)) {
		debug("read failed (%d)", l);
		return FALSE;
	}

	debug("InputEvent: timestamp=%lu  type=%04d code=%04d value=%08d",
			tv_msec(event.time), event.type, event.code, event.value);

	switch (event.type) {
	case EV_SW:
		fjbtndrv_daemon_input_handle_switch(daemon, &event);
		break;

	case EV_KEY:
		fjbtndrv_daemon_input_handle_key(daemon, &event);
		break;
	}

	return TRUE;
}

uint
fjbtndrv_daemon_get_modified_keycode(FjbtndrvDaemon *daemon, InputEvent *event)
{
	int i;

	if (daemon->skey == 0)
		return event->code;

	for (i=0; daemon->keymap[i].normal; i++) {
		if (daemon->keymap[i].normal != event->code) {
			switch (daemon->skey) {
			case KEY_FN:    return daemon->keymap[i].fn;
			case KEY_SLEEP: return daemon->keymap[i].alt;
			}
		}
	}

	return event->code;
}

void
fjbtndrv_daemon_uinput_send_key(FjbtndrvDaemon *daemon, InputEvent *event)
{
	if (event->code > 0) {
		int l = write(g_io_channel_unix_get_fd(daemon->uinput),
				event, sizeof(InputEvent));
		if (l != sizeof(InputEvent)) {
			debug("write failed (%d)", l);
			stop_mainloop(NULL);
		}
		debug(" uinput event %d/%d with value %d send",
				event->type, event->code, event->value);
	}
}

void
fjbtndrv_daemon_sticky_key_interval_destroy(FjbtndrvDaemon *daemon)
{
	if (daemon->sinterval != NULL) {
		debug("destroying sticky interval");
		g_source_destroy(daemon->sinterval);
		daemon->sinterval = NULL;
		daemon->stime = 0;
	}
}

void
fjbtndrv_daemon_sticky_key_interval_destroyed(gpointer data)
{
	FjbtndrvDaemon *daemon = (FjbtndrvDaemon*) data;
	g_assert( daemon != NULL);

	debug("interval destroyed");

	g_source_unref(daemon->sinterval);
	daemon->sinterval = NULL;
}

gboolean
fjbtndrv_daemon_sticky_key_interval(FjbtndrvDaemon *daemon)
{
	struct timeval tv;

	g_assert(daemon != NULL);

	debug("interval - %u", daemon->stime);

	if (daemon->stime > 0) {
		daemon->stime--;
	}
	else {
		debug("timeout");
		fjbtndrv_daemon_sticky_key_interval_destroy(daemon);
		daemon->skey = 0;
	}

	return TRUE;
}

void
fjbtndrv_daemon_sticky_key(FjbtndrvDaemon *daemon, gint keycode)
{
	fjbtndrv_daemon_sticky_key_interval_destroy(daemon);

	daemon->skey = keycode;
	daemon->stime = daemon->sticky_timeout / 100;

	daemon->sinterval = g_timeout_source_new(100);
	g_source_set_callback(daemon->sinterval,
			(GSourceFunc)    fjbtndrv_daemon_sticky_key_interval, daemon,
			(GDestroyNotify) fjbtndrv_daemon_sticky_key_interval_destroyed);
	g_source_attach(daemon->sinterval, g_main_loop_get_context(mainloop));
	debug("sticky interval added");
}

gboolean
fjbtndrv_daemon_input_handle_key(FjbtndrvDaemon *daemon, InputEvent *event)
{
	KeymapEntry *ke;

	FjbtndrvDaemonClass *klass = FJBTNDRV_DAEMON_GET_CLASS(daemon);

	g_assert(daemon != NULL);
	g_assert(klass != NULL);
	g_assert(event != NULL);

	debug("fjbtndrv_daemon_input_handle_key: skey=%u  stime=%u  sinterval=%p",
			daemon->skey, daemon->stime, daemon->sinterval);

	/* value: 0 = KeyRelease, 1 = KeyPress, 2 = KeyRepeat */
	if (event->value == 1) {
		debug("KeyPress:");
		if ((daemon->skey == 0) || (daemon->skey == event->code)) {
			debug(" no sticked key");
			switch (event->code) {
			case KEY_FN:
				fjbtndrv_daemon_sticky_key(daemon, event->code);
				// TODO: dbus signal - sticky-fn
				debug(" new sticked key - FN");
				return TRUE;

			case KEY_SLEEP:
				fjbtndrv_daemon_sticky_key(daemon, event->code);
				// TODO: dbus signal
				debug(" new sticked key - ALT");
				return TRUE;
			}
		}
		else {
			debug(" sticky key");
			fjbtndrv_daemon_sticky_key_interval_destroy(daemon);
			event->code = fjbtndrv_daemon_get_modified_keycode(daemon, event);
			debug(" modify key to %04d", event->code);
		}
	}
	else if (event->value == 2) {
		debug("KeyRepeat:");
		switch (event->code) {
		case KEY_FN:
		case KEY_SLEEP:
			return TRUE;

		default:
			if (daemon->skey) {
				event->code = fjbtndrv_daemon_get_modified_keycode(daemon, event);
				debug(" modify key to %04d", event->code);
			}
		}
	}
	else if (event->value == 0) {
		debug("KeyRelease:");
		if (daemon->skey == event->code) {
			debug(" sticked key released");
			if (daemon->stime == 0) {
				debug(" sticked key was lp");
				event->code = fjbtndrv_daemon_get_modified_keycode(daemon, event);
				debug(" modify key to %04d", event->code);
				event->value = 1;
				// TODO timeval
				fjbtndrv_daemon_uinput_send_key(daemon, event);
				event->value = 0;
				// TODO timeval
				fjbtndrv_daemon_uinput_send_key(daemon, event);
			}
			return TRUE;
		}
		else if (daemon->skey) {
			fjbtndrv_daemon_sticky_key_interval_destroy(daemon);
			event->code = fjbtndrv_daemon_get_modified_keycode(daemon, event);
			daemon->skey = 0;
			daemon->stime = 0;
			debug(" modify key to %04d", event->code);
		}
	}

	fjbtndrv_daemon_uinput_send_key(daemon, event);

	debug("KeyHandlerDone");
	return TRUE;
}

gboolean
fjbtndrv_daemon_input_handle_switch(FjbtndrvDaemon *daemon, InputEvent *event)
{
	FjbtndrvDaemonClass *klass = FJBTNDRV_DAEMON_GET_CLASS(daemon);

	g_assert(daemon != NULL);
	g_assert(klass != NULL);
	g_assert(event != NULL);

	debug("EmitSignal: tablet-mode-changed: %d\n", event->value);
	g_signal_emit(daemon, signals[SIGNAL_TABLET_MODE_CHANGED], 0, event->value);

	return TRUE;
}

/*
gboolean
fjbtndrv_daemon_get_sticky_timeout(FjbtndrvDaemon *daemon, gint *msec, DBusGMethodInvocation *context)
{
	g_assert(daemon != NULL);
	g_assert(msec != NULL);


	*msec = daemon->sticky_timeout;

	return TRUE;
}

gboolean
fjbtndrv_daemon_set_sticky_timeout(FjbtndrvDaemon *daemon, gint msec, DBusGMethodInvocation *context)
{
	GError *error = NULL;
	FjbtndrvDaemonClass *klass = FJBTNDRV_DAEMON_GET_CLASS(daemon);

	g_assert(daemon != NULL);
	g_assert(klass != NULL);
	g_assert(context != NULL);

	debug("DBusCall: setStickyTimeout(%d)", msec);

	if ((msec < 0) || (msec > 9999)) {
		g_set_error(&error, FJBTNDRV_DAEMON_ERROR, 0, "Out of Range.");
		dbus_g_method_return_error(context, error);
		return FALSE;
	}

	if (msec != daemon->sticky_timeout) {
		daemon->sticky_timeout = msec;
		debug("EmitSignal: sticky-timeout-changed: %d\n", msec);
		g_signal_emit(daemon, signals[SIGNAL_STICKY_TIMEOUT_CHANGED], 0, msec);
	}

	return TRUE;
}
*/

static void
fjbtndrv_daemon_get_property(GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
	FjbtndrvDaemon *daemon = FJBTNDRV_DAEMON(object);

	debug("DbusCall: get_property(%d)", prop_id);

	switch (prop_id) {
		case PROP_STICKY_TIMEOUT:
			g_value_set_uint(value, daemon->sticky_timeout);
			break;

		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;
	}

}

static void
fjbtndrv_daemon_set_property(GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
	FjbtndrvDaemon *daemon = FJBTNDRV_DAEMON(object);

	debug("DbusCall: set_property(%d, %d)", prop_id, g_value_get_uint(value));

	switch (prop_id) {
		case PROP_STICKY_TIMEOUT:
			// TODO: min/max check?
			daemon->sticky_timeout = g_value_get_uint(value);
			break;

		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;
	}
}

GIOChannel*
open_input_device(GError **error)
{
	char *devname;
	GIOChannel *gioc;
	int fd;

	*error = NULL;

	devname = getenv("DEVNAME");
	if (devname == NULL) {
		g_set_error(error, FJBTNDRV_DAEMON_ERROR, 1,
				"DEVNAME not set.");
		return NULL;
	}

	debug("DEVICE FILE: %s", devname);

	gioc = g_io_channel_new_file(devname, "r", error);
	if (*error)
		return NULL;

	fd = g_io_channel_unix_get_fd(gioc);

	if (ioctl(fd, EVIOCGRAB, 1) < 0) {
		g_set_error(error, FJBTNDRV_DAEMON_ERROR, errno,
				"Failed to grab input device");
		return NULL;
	}

	return g_io_channel_ref(gioc);
}

GIOChannel*
create_uinput_device_for(GIOChannel *gioc_src, GError **error)
{
	GIOChannel *gioc;
	int fd, fd_src;
	struct uinput_user_dev uinput_dev;
	int i;

	*error = NULL;

	fd_src = g_io_channel_unix_get_fd(gioc_src);

	gioc = g_io_channel_new_file(UINPUT_DEVNAME, "w", error);
	if (*error)
		return NULL;

	fd = g_io_channel_unix_get_fd(gioc);

	ioctl(fd, UI_SET_EVBIT, EV_KEY);
	ioctl(fd, UI_SET_EVBIT, EV_REP);
	ioctl(fd, UI_SET_EVBIT, EV_SW);

	// TODO: nur benutzte keycodes
	for (i=0; i < KEY_MAX; i++) {
		ioctl(fd, UI_SET_KEYBIT, i);
	}

	ioctl(fd, UI_SET_SWBIT, SW_TABLET_MODE);

	memset(&uinput_dev, 0, sizeof(uinput_dev));
	ioctl(fd_src, EVIOCGNAME(UINPUT_MAX_NAME_SIZE), uinput_dev.name);
	ioctl(fd_src, EVIOCGID, &uinput_dev.id);

	debug("DEVICE NAME: %s", uinput_dev.name);

	i = write(fd, &uinput_dev, sizeof(uinput_dev));
	if (i != sizeof(uinput_dev)) {
		g_set_error(error, FJBTNDRV_DAEMON_ERROR, errno,
				"Failed to write to uinput device file");
		return NULL;
	}

	if (ioctl(fd, UI_DEV_CREATE) < 0) {
		g_set_error(error, FJBTNDRV_DAEMON_ERROR, errno,
				"Failed to create uinput device");
		return NULL;
	}

	return g_io_channel_ref(gioc);
}

static void
stop_mainloop(gpointer data)
{
	if (mainloop)
		g_main_loop_quit(mainloop);
}

int
main(int argc, char *argv[])
{
	DBusGConnection *bus;
	GError *error;
	gboolean success;

	g_type_init();
	error = NULL;

	fjbtndrv = fjbtndrv_daemon_new();
	g_assert(fjbtndrv != NULL);

	success = fjbtndrv_daemon_set_config(fjbtndrv, &error);
	if (!success) {
		fprintf(stderr, "%s\n", error->message);
		return error->code;
	}

	fjbtndrv->input = open_input_device(&error);
	if (error) {
		fprintf(stderr, "%s\n", error->message);
		return error->code;
	}
	g_assert(fjbtndrv->input != NULL);

	fjbtndrv->uinput = create_uinput_device_for(fjbtndrv->input, &error);
	if (error) {
		fprintf(stderr, "%s\n", error->message);
		return error->code;
	}
	g_assert(fjbtndrv->uinput != NULL);

	bus = dbus_g_bus_get(DBUS_BUS_SYSTEM, &error);
	if (error) {
		fprintf(stderr, "Failed to open connection to system bus: %s\n", error->message);
		return error->code;
	}
	g_assert(bus != NULL);

	success = fjbtndrv_daemon_bind_dbus(fjbtndrv, bus, &error);
	if ((!success) || (error)) {
		fprintf(stderr, "Unable to register service: %s\n", error->message);
		return error->code;
	}

	mainloop = g_main_loop_new(NULL, FALSE);
	fjbtndrv_daemon_register_callbacks(fjbtndrv, mainloop);
	g_main_loop_run(mainloop);

	return EXIT_FAILURE;
}
