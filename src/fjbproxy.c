/*
 * fjbtndrv dbus proxy daemon
 *
 * Copyright (C) 2011 Robert Gerlach <khnz@users.sourceforge.net>
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

#include <stdlib.h>
#include <sys/ioctl.h>
#include <linux/input.h>

#include "fjbtndrv.h"
#include "fjbtndrv-proxy.h"

#define BIT(n) (1UL << n)

typedef struct input_event InputEvent;

static GMainLoop *mainloop;

static void
switch_event(InputEvent *event, FjbtndrvProxy *proxy)
{
	g_assert(event);
	g_assert(proxy);

	switch (event->code) {
	case SW_TABLET_MODE:
		fjbtndrv_proxy_set_tablet_mode(proxy, event->value);
		break;

	case SW_DOCK:
		fjbtndrv_proxy_set_dock_state(proxy, event->value);
		break;
	}
}


static void
on_device_lost(void)
{
	debug("device lost");
	g_main_loop_quit(mainloop);
}

static gboolean
on_device_event(GIOChannel *source, GIOCondition condition, gpointer data)
{
	FjbtndrvProxy *proxy = (FjbtndrvProxy*) data;
	InputEvent event;
	GIOStatus status;
	GError *error = NULL;

	g_assert(proxy);

	status = g_io_channel_read_chars(source, (gchar*) &event, sizeof(event),
			NULL, &error);

	switch (status) {
	case G_IO_STATUS_NORMAL:
		debug("input_event_dispatcher: timestamp=%lu.%lu  type=%04d code=%04d value=%d",
				event.time.tv_sec, event.time.tv_usec, event.type, event.code, event.value);

		switch (event.type) {
		case EV_SW:
			switch_event(&event, proxy);
			break;
		}

		return TRUE;

	case G_IO_STATUS_ERROR:
		g_warning("%s", error->message);
		g_error_free(error);
	
	default:
		return FALSE;
	}
}

static GIOChannel*
open_device(GError **error)
{
	char *devname;
	GIOChannel *gioc;

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

	g_io_channel_set_encoding(gioc, NULL, error);

	return gioc;
}

int
main(int argc, char *argv[])
{
	FjbtndrvProxy *daemon;
	GIOChannel *device;
	GError *error = NULL;

	g_type_init();

	/* XXX: g_die() or {
		GLogLevelFlags mask;
		mask = g_log_set_always_fatal(G_LOG_FATAL_MASK);
		mask |= G_LOG_LEVEL_CRITICAL;
		g_log_set_always_fatal(mask);
	} */


	debug(" * initialization");

	mainloop = g_main_loop_new(NULL, FALSE);

	daemon = fjbtndrv_proxy_new();
	g_assert(daemon);

	device = open_device(&error);
	if (error) {
		g_error("%s", error->message);
		debug("open_device failed");
		return error->code;
	}

	/* get and set initial switch states */
	{
		gulong switches = 0;
		gint fd = g_io_channel_unix_get_fd(device);

		if (ioctl(fd, EVIOCGSW(sizeof(switches)), &switches) >= 0) {
			fjbtndrv_proxy_set_tablet_mode(daemon,
					(switches & BIT(SW_TABLET_MODE)) >> SW_TABLET_MODE);
			fjbtndrv_proxy_set_dock_state(daemon,
					(switches & BIT(SW_DOCK)) >> SW_DOCK);
		}
	}

	g_io_add_watch_full(device, 0, G_IO_IN|G_IO_ERR|G_IO_HUP,
			(GIOFunc) on_device_event, daemon,
			(GDestroyNotify) on_device_lost);


	debug(" * start");

	g_main_loop_run(mainloop);
	

	debug(" * shutdown");

	if (mainloop)
		g_main_loop_unref(mainloop);
	if (device)
		g_io_channel_unref(device);
	if (daemon)
		g_object_unref(daemon);

	return 0;
}

