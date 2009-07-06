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

#include "fjbtndrv.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <limits.h>
#include <dirent.h>
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
#endif

#define HAL_SIGNAL_FILTER "type='signal', sender='org.freedesktop.Hal', interface='org.freedesktop.Hal.Device', member='PropertyModified', path='%s'"


static int keep_running = 1;

typedef struct _scriptlist {
	char name[PATH_MAX];
	struct _scriptlist * next;
} scriptlist;

static int is_regular_file(const char *filename)
{
	struct stat s;
	char buffer[PATH_MAX];
	int error, len;

	error = stat(filename, &s);
	if(error)
		return 0;

	if((s.st_mode & S_IFMT) == S_IFREG)
		return 1;

	else if((s.st_mode & S_IFMT) == S_IFLNK) {
		len = readlink(filename, buffer, PATH_MAX-1);
		if(len > 0) {
			buffer[len] = '\0';
			return is_regular_file(buffer);
		} else
			perror(filename);
	}

	return 0;
}

// TODO: better name
static int is_script(const char *filename)
{
	int len = strlen(filename);

	return ((filename[0] != '.') &&
		(filename[len-1] != '~') &&
		(strcasecmp(&(filename[len-4]), ".bak") != 0));
}

static scriptlist* find_scripts(const char *name)
{
	DIR *dh;
	struct dirent *de;
	int error, len;
	char *homedir, buffer[PATH_MAX];
	scriptlist *paths, **next;

	paths = NULL;
	next = &paths;

	homedir = getenv("HOME");
	if(homedir) {
		len = snprintf(buffer, PATH_MAX, "%s/." PACKAGE "/%s", homedir, name);
		if(len > 0 && is_regular_file(buffer)) {
			fprintf(stderr, "fscrotd: %s is obsolete\n",
					buffer, buffer);
			*next = malloc(sizeof(scriptlist));
			strcpy((*next)->name, buffer);
			next = &((*next)->next);
		}

		
		len = snprintf(buffer, PATH_MAX, "%s/." PACKAGE "/%s.d", homedir, name);
		if(len > 0) {
			dh = opendir(buffer);
			if(dh) {
				buffer[len++] = '/';

				while((de = readdir(dh))) {
					if((!de->d_name) || (de->d_name[0] == '.'))
						continue;

					strncpy(&(buffer[len]), de->d_name, PATH_MAX - len);
					if(is_regular_file(buffer) &&
					   is_script(buffer)) {
						*next = malloc(sizeof(scriptlist));
						strcpy((*next)->name, buffer);
						next = &((*next)->next);
					}
				}

				closedir(dh);
			}
		}
	}

	len = snprintf(buffer, PATH_MAX, "%s/%s", SCRIPTDIR, name);
	if(len > 0 && is_regular_file(buffer)) {
		fprintf(stderr, "fscrotd: %s is obsolete\n",
				buffer, buffer);
		*next = malloc(sizeof(scriptlist));
		strcpy((*next)->name, buffer);
		next = &((*next)->next);
	}

	len = snprintf(buffer, PATH_MAX, "%s/%s.d", SCRIPTDIR, name);
	if(len > 0) {
		dh = opendir(buffer);
		if(dh) {
			buffer[len++] = '/';

			while((de = readdir(dh))) {
				if((!de->d_name) || (de->d_name[0] == '.'))
					continue;

				strncpy(&(buffer[len]), de->d_name, PATH_MAX - len);
				if(is_regular_file(buffer) &&
				   is_script(buffer)) {
					*next = malloc(sizeof(scriptlist));
					strcpy((*next)->name, buffer);
					next = &((*next)->next);
				}
			}

			closedir(dh);
		}
	}

	(*next) = NULL;
	return paths;
}

static void free_scriptlist(scriptlist* list)
{
	if(!list)
		return;

	if(list->next)
		free_scriptlist(list->next);

	free(list);
}

static int run_scripts(const char *name)
{
	int error;
       	scriptlist *paths, *path;

	debug("HOOKS: %s", name);

	paths = find_scripts(name);
	if(!paths)
		return 0;

	path = paths;
	do {	
		error = system(path->name) << 8;
		debug("  %s: %d", path->name, error);
	} while((!error) && (path = path->next));

	free_scriptlist(paths);
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
		error = run_scripts((rr & RR_Rotate_0)
				? "pre-rotate-normal"
				: "pre-rotate-tablet");
		if(error)
			goto err;

		error = XRRSetScreenConfig(display, sc, rw, sz,
				rr | (cr & ~0xf),
				CurrentTime);
		if(error)
			goto err;

		error = run_scripts((rr & RR_Rotate_0)
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
		return 1;
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

	XCloseDisplay(display);
	libhal_ctx_free(hal);
	return 0;
}

