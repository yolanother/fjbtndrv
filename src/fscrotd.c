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

#ifdef DEBUG
void debug(const char *tag, const char *format, ...);
#else
#define debug(...) do {} while(0)
#endif

#include <stdio.h>
#ifdef ENABLE_DYNAMIC 
#  include <dlfcn.h>
#  define DLOPEN(info, name) \
	((info)->hdnl = dlopen(name, RTLD_NOW))
#  define DLSYM(info, func) \
	((info)->func = dlsym((info)->hdnl, #func))
#  define DLCLOSE(info) \
	(((info)->hdnl) \
		? (dlclose((info)->hdnl) ? (int)((info)->hdnl=NULL) : -1) \
		: 0)
#  define DLCALL(info, func, args...) \
	(((info)->hdnl && (info)->func) ? (info)->func(args) : 0)
#else
#  define DLCALL(info, func, args...) \
	func(args)
#endif

static unsigned keep_running = 1;

//XXX: to fscd-base.h ?
static int get_tablet_sw(void);
static int get_tablet_orientation(int mode);

#ifdef DEBUG
#include <stdarg.h>
void debug(const char *tag, const char *format, ...)
{
	va_list a;
	char buffer[256];

	va_start(a, format);
	snprintf(buffer, 255, "%s: %s\n", tag, format);
	vfprintf(stderr, buffer, a);
	va_end(a);
}
#endif

static char* find_script(const char *name)
{
	struct stat s;
	int error;
	char *path, *homedir;

	homedir = getenv("HOME");
	if(homedir) {
		path = malloc(strlen(homedir) + strlen(name) + 8);
		sprintf(path, "%s/.fscd/%s", homedir, name);

		error = stat(path, &s);
		debug("XXX", "%s: %d", path, error);
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
	debug("XXX", "%s: %d", path, error);
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
		debug("XXX", "script %s not found.", name);
		return 0;
	}

	error = system(path) << 8;
	free(path);
	debug("XXX", "returncode: %d", error);
	return error;
}

//{{{ X11 stuff
#include <X11/Xlib.h>
#include <X11/extensions/Xrandr.h>
static Display *display;
static Window root;
static int x11_error(Display*, XErrorEvent*);
static int x11_ioerror(Display*);

Bool x11_check_extension(const char *name)
{
	int opcode, event, error;
#ifdef DEBUG
	int major, minor;
#endif

	Bool found = XQueryExtension(display, name,
			&opcode, &event, &error);
	if(found) {
#ifdef DEBUG
		XTestQueryExtension(display,
				&event, &error,
				&major, &minor);
		debug("X11", "Found %s %d.%d extension (%d, %d, %d)",
			name, major, minor,
			opcode, event, error);
#endif
	} else
		fprintf(stderr, "No %s extension\n", name);

	return found;
}

Display* x11_init(void)
{
	Bool randr;

	display = XOpenDisplay(NULL);
	if(!display)
		return NULL;

#ifdef DEBUG
	XSynchronize(display, True);
#endif

	root = XDefaultRootWindow(display);
	if(!root) {
		XCloseDisplay(display);
		return NULL;
	}

	XSetErrorHandler(x11_error);
	XSetIOErrorHandler(x11_ioerror);

	randr  = x11_check_extension("RANDR");
	if(!randr) {
		XCloseDisplay(display);
		return NULL;
	}

	return display;
}

static void x11_exit(void)
{
	XSync(display, True);
	XCloseDisplay(display);
}

static int x11_error(Display *dpy, XErrorEvent *ee)
{
	char buffer[256];
	XGetErrorText(dpy, ee->error_code, buffer, 255);
	fprintf(stderr, "%s\n", buffer);
	return 0;
}

static int x11_ioerror(Display *dpy)
{
	fprintf(stderr, "X11 IO Error.\n");
	return keep_running = 0;
}

static void rotate_screen(int mode)
{
	Window rwin;
	XRRScreenConfiguration *sc;
	Rotation rotation, current_rotation;
	SizeID size;

	rwin = DefaultRootWindow(display);
	sc = XRRGetScreenInfo(display, rwin);
	if(!sc)
		return;

	size = XRRConfigCurrentConfiguration(sc, &current_rotation);
	debug("TRACE", "XRRRotations: current_rotation=%d", current_rotation);

	if(mode == -1) {	/* toggle orientation */
		register int normal = get_tablet_orientation(0);
		register int tablet = get_tablet_orientation(1);

		debug("XXX", "toggle rotatation %d (%d/%d)",
				current_rotation, normal, tablet);

		if(current_rotation & normal)
			rotation = tablet;
		else
			rotation = normal;

		debug("XXX", "target rotation: %d", rotation);
	} else
		rotation = get_tablet_orientation(mode);

	if(rotation < 0)
		goto err;

	rotation |= current_rotation & ~0xf;

	if(rotation != current_rotation) {
		int error;

		if(mode)
			error = run_script("fscd-pre-rotate-tablet");
		else
			error = run_script("fscd-pre-rotate-normal");
		if(error)
			goto err;

		error = XRRSetScreenConfig(display, sc, rwin, size,
				rotation, CurrentTime);
		if(error)
			goto err;

#ifdef ENABLE_WACOM
		wacom_rotate(rotation);
#endif
	
		if(mode)
			error = run_script("fscd-rotate-tablet");
		else
			error = run_script("fscd-rotate-normal");
		if(error)
			goto err;

		//TODO: screen_rotated();

	}

  err:
	XRRFreeScreenConfigInfo(sc);
}
//}}}

//{{{ HAL stuff
#include <dbus/dbus.h>
#include <hal/libhal.h>
static DBusConnection *dbus;
static DBusError dbus_error;
static LibHalContext *hal;
static char *laptop_panel;
static char *fsc_tablet_device;

#define HAL_SIGNAL_FILTER "type='signal', sender='org.freedesktop.Hal', interface='org.freedesktop.Hal.Device', member='PropertyModified', path='%s'"
DBusHandlerResult dbus_prop_modified(DBusConnection *dbus, DBusMessage *msg, void *data);

static int hal_init(void)
{
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

	hal = libhal_ctx_new();
	if(!hal) {
		fprintf(stderr, "libhal_ctx_new failed\n");
		goto err;
	}

	libhal_ctx_init(hal, &dbus_error);
	if(dbus_error_is_set(&dbus_error)) {
		fprintf(stderr, "init hal ctx failed - %s\n",
				dbus_error.message);
		goto err_free_ctx;
	}

	libhal_ctx_set_dbus_connection(hal, dbus);

	/* search panel */
	devices = libhal_find_device_by_capability(hal,
			"laptop_panel", &count, &dbus_error);
	if(dbus_error_is_set(&dbus_error)) {
		fprintf(stderr, "find_device_by_capability - %s\n",
				dbus_error.message);
		goto err_free_devices;
	}

	if((devices) || (count > 0))
		laptop_panel = strdup(devices[0]);

	libhal_free_string_array(devices);

	/* search fsc_btns driver */
	devices = libhal_find_device_by_capability(hal,
			"input.switch", &count, &dbus_error);
	if(dbus_error_is_set(&dbus_error)) {
		fprintf(stderr, "find_device_by_capability - %s\n",
				dbus_error.message);
		goto err_free_devices;
	}

	if((devices == NULL) || (count < 0)) {
		fprintf(stderr, "no tablet device found\n");
		goto err_free_devices;
	}

	if(count == 0) {
		fprintf(stderr, "no devices found\n");
		goto err_free_devices;
	}

	debug("HAL", "%d input.switch device(s) found:", count);
	while(count-- && devices[count]) {
		char *type;
		debug("HAL", "check device %s", devices[count]);

		type = libhal_device_get_property_string(hal,
				devices[count], "button.type",
				&dbus_error);
		if(dbus_error_is_set(&dbus_error)) {
			fprintf(stderr, "prop get failed - %s\n",
					dbus_error.message);
			goto err_input_next_device;
		}

		if(type && !strcmp("tablet_mode", type)) {
			debug("HAL", "tablet mode device found: %s",
					devices[count]);

			fsc_tablet_device = strdup(devices[count]);

			buffer = malloc(sizeof(HAL_SIGNAL_FILTER) + strlen(fsc_tablet_device));
			if(buffer) {
				sprintf(buffer, HAL_SIGNAL_FILTER, fsc_tablet_device);
				debug("HAL", "filter: %s.", buffer);
				dbus_bus_add_match(dbus, buffer, &dbus_error);
				dbus_connection_add_filter(dbus, dbus_prop_modified, NULL, NULL);
				free(buffer);
			}
			break;
		}

 err_input_next_device:
		libhal_free_string(type);
	}
	libhal_free_string_array(devices);

	return 0;

 err_free_devices:
	libhal_free_string_array(devices);
 err_free_ctx:
	libhal_ctx_free(hal);
 err:
	dbus_error_free(&dbus_error);
	return -1;
}

static void hal_exit(void)
{
	if(hal) {
		libhal_ctx_free(hal);
		dbus_error_free(&dbus_error);
	}
}

static int get_tablet_sw(void)
{
	dbus_bool_t tablet_mode;

	tablet_mode = libhal_device_get_property_bool(hal, fsc_tablet_device,
			"button.state.value", &dbus_error);
	if(dbus_error_is_set(&dbus_error)) {
		fprintf(stderr, "query button state failed - %s\n",
				dbus_error.message);
		return -1;
	}

	return (tablet_mode == TRUE);
}

static int get_tablet_orientation(int mode)
{
	char propname[40];
	char *orientation;
	int orientation_id;

	debug("TRACE", "get_tablet_orientation: mode=%d", mode);

	snprintf(propname, 39, "tablet_panel.orientation.%s",
			(mode == 0 ? "normal" : "tablet_mode"));

	orientation = libhal_device_get_property_string(hal, laptop_panel, propname,
			&dbus_error);
	if(dbus_error_is_set(&dbus_error)) {
		fprintf(stderr, "query orientation failed - %s\n",
				dbus_error.message);
		return -1;
	}

	if(!orientation)
		return -1;

	debug("HAL", "get_tablet_orientation: orientation=%s", orientation);

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

	debug("HAL", "get_tablet_orientation: id=%d", orientation_id);
	return orientation_id;	
}

DBusHandlerResult dbus_prop_modified(DBusConnection *dbus, DBusMessage *msg, void *data)
{
	DBusMessageIter iter;
	//int i, err;

	debug("DBUS", "dbus_prop_modified(%p, %s)", msg, (char*)data);

	//org.freedesktop.Hal.Device/PropertyModified
	if(dbus_message_is_signal(msg, "org.freedesktop.Hal.Device", "PropertyModified")) {
		//int mode;

		/*
		err = dbus_message_get_args(msg, NULL, DBUS_TYPE_INT32, &i);
		if(err) {
			debug("XXX", " DBUS: dbus_prop_modified: broken signal");
			return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
		}
		debug("XXX", " DBUS: dbus_prop_modified: %d entries", i);
		*/

		dbus_message_iter_init(msg, &iter);
		while(dbus_message_iter_has_next(&iter)) {
			debug("XXX", " DBUS: dbus_prop_modified: %d", dbus_message_iter_get_arg_type(&iter));
			dbus_message_iter_next(&iter);
		}

		// XXX: callback
		handle_display_rotation(get_tablet_sw());

		debug("XXX", " DBUS: dbus_prop_modified: signal handled");
		return DBUS_HANDLER_RESULT_HANDLED;
	}

	debug("DBUS", "dbus_prop_modified: bad signal resived (%s/%s)",
			dbus_message_get_interface(msg), dbus_message_get_member(msg));
	return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}
//}}} 

int handle_display_rotation(int mode)
{
	int error;
		
	if(mode)
		error = run_script("fscd-pre-mode-tablet");
	else
		error = run_script("fscd-pre-mode-normal");
	if(error)
		return -1;

	//TODO: if(settings.lock_rotate == UL_UNLOCKED)
		rotate_screen(mode);

	if(mode)
		error = run_script("fscd-mode-tablet");
	else
		error = run_script("fscd-mode-normal");

	return error;
}

int main(int argc, char **argv)
{
	int error;

	if((geteuid() == 0) && (getuid() > 0)) {
		fprintf(stderr, " *** suid is no longer needed ***\n");
		sleep(5);
		seteuid(getuid());
	}

	error = hal_init();
	if(error) {
		fprintf(stderr, "hal initalisation failed\n");
		goto hal_failed;
	}

	Display *display = x11_init();
	if(!display) {
		fprintf(stderr, "x11 initalisation failed\n");
		goto x_failed;
	}

#ifdef ENABLE_WACOM
	error = wacom_init(display);
	if(error) {
		fprintf(stderr, "wacom initalisation failed\n");
	}
#endif

	debug("INFO", "\n *** Please report bugs to " PACKAGE_BUGREPORT " ***\n");

	handle_display_rotation(get_tablet_sw());

	while(keep_running) {
		dbus_connection_read_write_dispatch(dbus, 10000);	// timeout in msec
	}

#ifdef ENABLE_WACOM
	wacom_exit();
#endif
	x11_exit();
 x_failed:
	hal_exit();
 hal_failed:
	return 0;
}

