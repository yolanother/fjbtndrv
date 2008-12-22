/*
 * Copyright (C) 2007-2008 Robert Gerlach
 *
 * You can redistribute and/or modify this program under the terms of the
 * GNU General Public License version 3 as published by the Free Software
 * Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifdef HAVE_CONFIG_H
#  include "../config.h"
#endif

#include "wacom.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
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

#define HAL_SIGNAL_FILTER "type='signal', sender='org.freedesktop.Hal', interface='org.freedesktop.Hal.Device', member='PropertyModified', path='%s'"


static int keep_running = 1;

static char* find_script(const char *name)
{
	struct stat s;
	int error;
	char *path, *homedir;

	homedir = getenv("HOME");
	if(homedir) {
		path = malloc(strlen(homedir) + strlen(PACKAGE) + strlen(name) + 4);
		if(path) {
			sprintf(path, "%s/." PACKAGE "/%s", homedir, name);

			error = stat(path, &s);
			if((!error) &&
			   (((s.st_mode & S_IFMT) == S_IFREG) ||
			    ((s.st_mode & S_IFMT) == S_IFLNK)))
				return path;

			free(path);
		}
	}

	path = malloc(sizeof(SCRIPTDIR) + strlen(name) + 2);
	if(path) {
		sprintf(path, "%s/%s", SCRIPTDIR, name);

		error = stat(path, &s);
		if((!error) &&
		   (((s.st_mode & S_IFMT) == S_IFREG) ||
		    ((s.st_mode & S_IFMT) == S_IFLNK)))
			return path;

		free(path);
	}

	return NULL;
}

static int run_script(const char *name)
{
	int error;
       	char *path;

	path = find_script(name);
	if(!path)
		return 0;

	error = system(path) << 8;
	free(path);
	debug("%s returns %d", path, error);

	return error;
}

static int x11_ioerror(Display *dpy)
{
	return keep_running = 0;
}

static Display* x11_init(void)
{
	Display *display;
	int major, minor;

	display = XOpenDisplay(NULL);
	if(!display) {
		fprintf(stderr, "x11 initalisation failed\n");
		return NULL;
	}

	XSetIOErrorHandler(x11_ioerror);

	if( !XRRQueryVersion(display, &major, &minor) ) {
		fprintf(stderr, "RandR extension missing\n");
		XCloseDisplay(display);
		return NULL;
	} else
		debug("RANDR: major=%d minor=%d", major, minor);

	return display;
}

static void print_dbus_error(char *message, DBusError *e)
{
	fprintf(stderr, "%s: %s - %s\n", message,
			e->name, e->message);
	dbus_error_free(e);
}

static LibHalContext* hal_init(void)
{
	LibHalContext *hal;
	DBusConnection *dbus;
	DBusError dbus_error;

	dbus_error_init(&dbus_error);

	dbus = dbus_bus_get(DBUS_BUS_SYSTEM, &dbus_error);
	if(!dbus || dbus_error_is_set(&dbus_error)) {
		print_dbus_error("dbus_bus_get failed", &dbus_error);
		return NULL;
	}

	hal = libhal_ctx_new();
	if(!hal) {
		fprintf(stderr, "libhal_ctx_new failed\n");
		return NULL;
	}

	libhal_ctx_init(hal, &dbus_error);
	if(dbus_error_is_set(&dbus_error)) {
		print_dbus_error("libhal_ctx_init failed", &dbus_error);
		libhal_ctx_free(hal);
		return NULL;
	}

	libhal_ctx_set_dbus_connection(hal, dbus);

	return hal;
}

static char* hal_find_switch(LibHalContext *hal)
{
	char *udi = NULL;
	char **devices;
	int n;
	DBusError dbus_error;

	dbus_error_init(&dbus_error);

	devices = libhal_find_device_by_capability(hal,
			"input.switch", &n, &dbus_error);
	if(dbus_error_is_set(&dbus_error))
		return NULL;

	if((!devices) || (n <= 0))
		return NULL;

	debug("%d input.switch device(s) found:", n);

	while(!udi && devices[--n]) {
		char *type;
		debug("  check switch %s", devices[n]);

		type = libhal_device_get_property_string(hal,
				devices[n], "button.type",
				&dbus_error);
		if(!dbus_error_is_set(&dbus_error)) {
			if(type && !strcmp("tablet_mode", type)) {
				debug("tablet switch found: %s", devices[n]);
				udi = strdup(devices[n]);
				break;
			}

			libhal_free_string(type);
		}
	}
	libhal_free_string_array(devices);

	return udi;
}

static int hal_add_switch_filter(LibHalContext *hal, char *udi)
{
	DBusError dbus_error;
	DBusConnection *dbus = libhal_ctx_get_dbus_connection(hal);
	char *buffer;

	dbus_error_init(&dbus_error);

	buffer = malloc(sizeof(HAL_SIGNAL_FILTER) + strlen(udi));
	if(!buffer)
		return -1;

	sprintf(buffer, HAL_SIGNAL_FILTER, udi);
	debug("dbus signal filter: %s.", buffer);

	dbus_bus_add_match(dbus, buffer, &dbus_error);
	if(dbus_error_is_set(&dbus_error))
		print_dbus_error("dbus_bus_add_match failed", &dbus_error);

	free(buffer);
	return 0;
}

static char* hal_find_panel(LibHalContext *hal)
{
	char *udi = NULL;
	char **devices;
	int n;
	DBusError dbus_error;

	dbus_error_init(&dbus_error);

	devices = libhal_find_device_by_capability(hal,
			"tablet_panel", &n, &dbus_error);
	if(dbus_error_is_set(&dbus_error))
		return NULL;

	if((devices) && (n > 0)) {
		debug("laptop panel found: %s", udi);
		udi = strdup(devices[0]);
	}

	libhal_free_string_array(devices);

	return udi;
}

//TODO: error handling?
static int get_tablet_sw(LibHalContext *hal, char *udi)
{
	dbus_bool_t tablet_mode;
	DBusError dbus_error;

	dbus_error_init(&dbus_error);

	tablet_mode = libhal_device_get_property_bool(hal, udi,
			"button.state.value", &dbus_error);
	if(dbus_error_is_set(&dbus_error)) {
		print_dbus_error("query button state", &dbus_error);
		return 0;
	}

	return (tablet_mode == TRUE);
}

static Rotation get_tablet_orientation(LibHalContext *hal, char *udi, int mode)
{
	char propname[40];
	char *orientation;
	int orientation_id;
	DBusError dbus_error;

	dbus_error_init(&dbus_error);

	snprintf(propname, 39, "tablet_panel.orientation.%s",
			(mode == 0 ? "normal" : "tablet_mode"));

	orientation = libhal_device_get_property_string(hal,
			udi, propname, &dbus_error);
	if(!orientation || dbus_error_is_set(&dbus_error)) {
		print_dbus_error("query orientation", &dbus_error);
		return -1;
	}

	debug("hal saith, orientation for %s mode should be %s",
		(mode ? "tablet" : "normal"), orientation);

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

	return orientation_id;	
}

static void handle_display_rotation(Display *display, Rotation rr)
{
	Window rw;
	XRRScreenConfiguration *sc;
	Rotation cr;
	SizeID sz;
	int error;

	rw = DefaultRootWindow(display);
	sc = XRRGetScreenInfo(display, rw);
	if(!sc) return;

	sz = XRRConfigCurrentConfiguration(sc, &cr);

	if(rr != (cr & 0xf)) {
		error = run_script((rr & RR_Rotate_0)
				? "pre-rotate-normal"
				: "pre-rotate-tablet");
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
	
		error = run_script((rr & RR_Rotate_0)
				? "rotate-normal"
				: "rotate-tablet");
		if(error)
			goto err;

		//TODO: screen_rotated();
	}

  err:
	XRRFreeScreenConfigInfo(sc);
}

static int rotation_locked(void)
{
	char *lockfile, *homedir;
	struct stat s;
	int error;

	homedir = getenv("HOME");
	if(!homedir)
		return -1;

	lockfile = malloc(strlen(homedir) + strlen(PACKAGE) + 17);
	if(!lockfile)
		return -1;

	sprintf(lockfile, "%s/." PACKAGE "/lock.rotation", homedir);
	error = stat(lockfile, &s);
	free(lockfile);

	return (error == 0);
}

static void handle_rotation(Display *display, LibHalContext *hal, char *udi, int mode)
{
	Rotation rr;

	if(rotation_locked()) {
		debug("LOCKED");
		return;
	}

	rr = (udi) ? get_tablet_orientation(hal, udi, mode) : 0;
	if(!rr)
		rr = (mode == 0) ? RR_Rotate_0 : RR_Rotate_270;

	handle_display_rotation(display, rr);
}

int main(int argc, char **argv)
{
	Display *display;
	LibHalContext *hal;
	char *udi_switch, *udi_panel;
	int error;

	hal = hal_init();
	if(!hal) {
		fprintf(stderr, "hal initalisation failed\n");
		XCloseDisplay(display);
		return 1;
	}

	udi_switch = hal_find_switch(hal);
	if(!udi_switch) {
		fprintf(stderr, "no tablet switch\n");
		//TODO: goto after_hal_error;
		exit(1);
	}

	error = hal_add_switch_filter(hal, udi_switch);
	if(error) {
		fprintf(stderr, "failed to add signal filter\n");
		//TODO: goto after_hal_error;
		exit(1);
	}

	udi_panel = hal_find_panel(hal);
	if(!udi_panel)
		fprintf(stderr, "no panel device\n");

	display = x11_init();
	if(!display) {
		fprintf(stderr, "x11 initalisation failed\n");
		//TODO: goto after_hal_error;
		exit(1);
	}

#ifdef ENABLE_WACOM
	error = wacom_init(display);
	if(error)
		fprintf(stderr, "wacom initalisation failed\n");
#endif

	handle_rotation(display, hal, udi_panel,
			get_tablet_sw(hal, udi_switch));

	while(keep_running) {
		DBusConnection *dbus = libhal_ctx_get_dbus_connection(hal);
		DBusMessage *msg;

		dbus_connection_read_write(dbus, -1);

		while((msg = dbus_connection_pop_message(dbus))) {
			if(dbus_message_is_signal(msg, "org.freedesktop.Hal.Device", "PropertyModified") &&
			   dbus_message_has_path(msg, udi_switch))
				handle_rotation(display, hal, udi_panel,
						get_tablet_sw(hal, udi_switch));

			dbus_message_unref(msg);
		}
	}

#ifdef ENABLE_WACOM
	wacom_exit();
#endif
	XCloseDisplay(display);
	libhal_ctx_free(hal);
	return 0;
}

