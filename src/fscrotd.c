/* FSC Tablet Buttons Helper Daemon (hal connector)
 * Copyright (C) 2008 Robert Gerlach
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
/******************************************************************************/

#ifdef HAVE_CONFIG_H
#  include "../config.h"
#endif

#include "fscd-wacom.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <X11/Xlib.h>
#include <X11/extensions/Xrandr.h>
#include <dbus/dbus.h>
#include <hal/libhal.h>

#ifdef DEBUG
#include <stdarg.h>
void debug(const char *format, ...)
{
	va_list a;

	va_start(a, format);
	vfprintf(stderr, format, a);
	putc('\n', stderr);
	va_end(a);
}
#else
#define debug(...) do {} while(0)
#endif

static Display *display;
static int get_tablet_sw(void);
static int get_tablet_orientation(int);

typedef struct {
	LibHalContext *hal;
	char *device;
	char *panel;
} hal_connection_t;
hal_connection_t global;

#define HAL_SIGNAL_FILTER "type='signal', sender='org.freedesktop.Hal', interface='org.freedesktop.Hal.Device', member='PropertyModified', path='%s'"


static char* find_script(const char *name)
{
	struct stat s;
	int error;
	char *path, *homedir;

	homedir = getenv("HOME");
	if(homedir) {
		path = malloc(strlen(homedir) + strlen(name) + 8);
		sprintf(path, "%s/.fjbtndrv/%s", homedir, name);

		error = stat(path, &s);
		debug("%s: %d", path, error);
		if((!error) &&
		   (((s.st_mode & S_IFMT) == S_IFREG) ||
		    ((s.st_mode & S_IFMT) == S_IFLNK)))
			return path;
		else
			free(path);
	}

	path = malloc(sizeof(SCRIPTDIR) + strlen(name) + 2);
	sprintf(path, "%s/%s", SCRIPTDIR, name);

	error = stat(path, &s);
	debug("%s: %d", path, error);
	if((!error) &&
	   (((s.st_mode & S_IFMT) == S_IFREG) ||
	    ((s.st_mode & S_IFMT) == S_IFLNK)))
		return path;
	else
		free(path);

	return NULL;
}

static int run_script(const char *name)
{
	int error;
       	char *path = find_script(name);
	if(!path) {
		debug("script %s not found.", name);
		return 0;
	}

	error = system(path) << 8;
	free(path);
	debug("returncode: %d", error);
	return error;
}

int handle_display_rotation(int mode)
{
	Window rw;
	XRRScreenConfiguration *sc;
	Rotation cr, rr;
	SizeID sz;
	int error;

	//TODO: if(settings.lock_rotate == UL_UNLOCKED)
	//        return 0

	rw = DefaultRootWindow(display);
	sc = XRRGetScreenInfo(display, rw);
	if(!sc)
		return -1;

	sz = XRRConfigCurrentConfiguration(sc, &cr);
	debug("current rotation: %d", cr);

	rr = get_tablet_orientation(mode);
	if(!rr)
		return -1;
	debug(" target rotation: %d", rr);

	if(rr != cr & 0xf) {
		error = run_script(mode ? "fscrotd-pre-rotate-tablet"
		                        : "fscrotd-pre-rotate-normal");
		if(error)
			goto err;

		error = XRRSetScreenConfig(display, sc, rw, sz,
				rr | (cr & ~0xf),
				CurrentTime);
		if(error)
			goto err;

#ifdef ENABLE_WACOM
		wacom_rotate(rr);
#endif
	
		error = run_script(mode ? "fscrotd-rotate-tablet"
		                        : "fscrotd-rotate-normal");
		if(error)
			goto err;

		//TODO: screen_rotated();
	}

  err:
	XRRFreeScreenConfigInfo(sc);
}

//{{{ HAL stuff
static int hal_init(void)
{
	DBusConnection *dbus;
	DBusError dbus_error;
	char **devices;
	char *buffer;
	int count;

	dbus_error_init(&dbus_error);

	dbus = dbus_bus_get(DBUS_BUS_SYSTEM, &dbus_error);
	if(!dbus || dbus_error_is_set(&dbus_error)) {
		fprintf(stderr, "Failed to connect to the D-BUS daemon: %s\n",
				dbus_error.message);
		goto err;
	}

	global.hal = libhal_ctx_new();
	if(!global.hal) {
		fprintf(stderr, "libhal_ctx_new failed\n");
		goto err;
	}

	libhal_ctx_init(global.hal, &dbus_error);
	if(dbus_error_is_set(&dbus_error)) {
		fprintf(stderr, "init hal ctx failed - %s\n",
				dbus_error.message);
		goto err_free_ctx;
	}

	libhal_ctx_set_dbus_connection(global.hal, dbus);

	devices = libhal_find_device_by_capability(global.hal,
			"input.switch", &count, &dbus_error);
	if(dbus_error_is_set(&dbus_error)) {
		fprintf(stderr, "find_device_by_capability - %s\n",
				dbus_error.message);
		goto err_free_ctx;
	}

	if((devices == NULL) || (count <= 0)) {
		fprintf(stderr, "switch not found\n");
		goto err_free_devices;
	}

	debug("%d input.switch device(s) found:", count);
	while(count-- && devices[count]) {
		char *type;
		debug("  check device %s", devices[count]);

		type = libhal_device_get_property_string(global.hal,
				devices[count], "button.type",
				&dbus_error);
		if(dbus_error_is_set(&dbus_error)) {
			fprintf(stderr, "prop get failed - %s\n",
					dbus_error.message);
			goto err_input_next_device;
		}

		if(type && !strcmp("tablet_mode", type)) {
			global.device = strdup(devices[count]);
			debug("tablet mode device found: %s", global.device);
			buffer = malloc(sizeof(HAL_SIGNAL_FILTER) + strlen(global.device));
			if(buffer) {
				sprintf(buffer, HAL_SIGNAL_FILTER, global.device);
				debug("filter: %s.", buffer);
				dbus_bus_add_match(dbus, buffer, &dbus_error);
//				dbus_connection_add_filter(dbus, dbus_prop_modified, NULL, NULL);
				free(buffer);
			}
			break;
		}

 err_input_next_device:
		libhal_free_string(type);
	}
	libhal_free_string_array(devices);

/*
	devices = libhal_find_device_by_capability(global.hal,
			"tablet_panel", &count, &dbus_error);
	if(!dbus_error_is_set(&dbus_error)) {
		if((devices != NULL) || (count > 0))
			global.panel = strdup(devices[0]);
		debug("laptop panel device found: %s", global.panel);
		libhal_free_string_array(devices);
	}
*/

	return 0;

 err_free_devices:
	libhal_free_string_array(devices);
 err_free_ctx:
	libhal_ctx_free(global.hal);
 err:
	dbus_error_free(&dbus_error);
	return -1;
}

static void hal_exit(void)
{
	if(global.hal)
		libhal_ctx_free(global.hal);
}

static int get_tablet_sw(void)
{
	dbus_bool_t tablet_mode;
	DBusError dbus_error;

	dbus_error_init(&dbus_error);

	tablet_mode = libhal_device_get_property_bool(global.hal, global.device,
			"button.state.value", &dbus_error);
	if(dbus_error_is_set(&dbus_error)) {
		fprintf(stderr, "query button state failed - %s\n",
				dbus_error.message);
		dbus_error_free(&dbus_error);
		return -1;
	}

	return (tablet_mode == TRUE);
}

static int get_tablet_orientation(int mode)
{
	char propname[40];
	char *orientation;
	int orientation_id;
	DBusError dbus_error;

	debug("get_tablet_orientation: mode=%d", mode);

	if(!global.panel)
		return (mode == 0 ? RR_Rotate_0 : RR_Rotate_270);

	dbus_error_init(&dbus_error);

	snprintf(propname, 39, "tablet_panel.orientation.%s",
			(mode == 0 ? "normal" : "tablet_mode"));

	orientation = libhal_device_get_property_string(global.hal,
			global.panel, propname, &dbus_error);
	if(dbus_error_is_set(&dbus_error)) {
		fprintf(stderr, "query orientation failed - %s\n",
				dbus_error.message);
		dbus_error_free(&dbus_error);
		return -1;
	}

	if(!orientation)
		return -1;

	debug("get_tablet_orientation: orientation=%s", orientation);

	if((orientation[0] == 'n') || (orientation[0] == 'N')) {
		orientation_id = RR_Rotate_0;
	} else if((orientation[0] == 'l') || (orientation[0] == 'L')) {
		orientation_id = RR_Rotate_90;
	} else if((orientation[0] == 'i') || (orientation[0] == 'I')) {
		orientation_id = RR_Rotate_180;
	} else if((orientation[0] == 'r') || (orientation[0] == 'R')) {
		orientation_id = RR_Rotate_270;
	} else
		orientation_id = -1;
	
	libhal_free_string(orientation);

	debug("get_tablet_orientation: id=%d", orientation_id);
	return orientation_id;	
}

DBusHandlerResult dbus_prop_modified(DBusConnection *dbus, DBusMessage *msg, void *data)
{
	DBusMessageIter iter;

	debug("dbus_prop_modified(%p, %s)", msg, (char*)data);

	//org.freedesktop.Hal.Device/PropertyModified
	if(dbus_message_is_signal(msg, "org.freedesktop.Hal.Device", "PropertyModified")) {
		//int mode;

		/*
		err = dbus_message_get_args(msg, NULL, DBUS_TYPE_INT32, &i);
		if(err) {
			debug(" DBUS: dbus_prop_modified: broken signal");
			return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
		}
		debug(" DBUS: dbus_prop_modified: %d entries", i);
		*/

		dbus_message_iter_init(msg, &iter);
		while(dbus_message_iter_has_next(&iter)) {
			debug(" DBUS: dbus_prop_modified: %d", dbus_message_iter_get_arg_type(&iter));
			dbus_message_iter_next(&iter);
		}

		handle_display_rotation(get_tablet_sw());

		debug(" DBUS: dbus_prop_modified: signal handled");
		return DBUS_HANDLER_RESULT_HANDLED;
	}

	debug("dbus_prop_modified: bad signal resived (%s/%s)",
			dbus_message_get_interface(msg), dbus_message_get_member(msg));
	return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}
//}}} 

int main(int argc, char **argv)
{
//	Display *display;
	int major, minor;
	int error;

	if((geteuid() == 0) && (getuid() > 0)) {
		fprintf(stderr, " *** suid is no longer needed ***\n");
		sleep(5);
		seteuid(getuid());
	}

	display = XOpenDisplay(NULL);
	if(!display) {
		fprintf(stderr, "x11 initalisation failed\n");
		return 1;
	}

	if( !XRRQueryVersion(display, &major, &minor) ) {
		fprintf(stderr, "RandR extension missing\n");
		XCloseDisplay(display);
		return 1;
	} else
		debug("RANDR: major=%d minor=%d", major, minor);

#ifdef ENABLE_WACOM
	error = wacom_init(display);
	if(error) {
		fprintf(stderr, "wacom initalisation failed\n");
		XCloseDisplay(display);
		return 1;
	}
#endif

	error = hal_init();
	if(error) {
		fprintf(stderr, "hal initalisation failed\n");
		goto hal_failed;
	}

	handle_display_rotation(get_tablet_sw());

// TODO: Display IO-Error?
	while(1) {
		DBusConnection *dbus = libhal_ctx_get_dbus_connection(global.hal);
		DBusMessage *msg;

		dbus_connection_read_write(dbus, -1);
		msg = dbus_connection_pop_message(dbus);
		debug("dbus_connection_read_write returned... pop(%p)", msg);
		debug("  I:%s M:%s P:%s",
				dbus_message_get_interface(msg),
				dbus_message_get_member(msg),
				dbus_message_get_path(msg));
		if(dbus_message_is_signal(msg, "org.freedesktop.Hal.Device", "PropertyModified") &&
		   dbus_message_has_path(msg, global.device))
			handle_display_rotation(get_tablet_sw());
		else
			debug("no or wrong signal (%s, %s)",
					dbus_message_get_interface(msg),
					dbus_message_get_path(msg));

		dbus_message_unref(msg);
	}

	hal_exit();
hal_failed:
#ifdef ENABLE_WACOM
	wacom_exit();
#endif
	XCloseDisplay(display);
	return 0;
}

