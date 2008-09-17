/* FSC Tablet Buttons Helper Daemon (base)
 * Copyright (C) 2007 Robert Gerlach
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

#include "fscd-base.h"
#include "fscd-gui.h"

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

#define ZAXIS_SCROLL_STEPS	3

#ifndef STICKY_TIMEOUT
#  define STICKY_TIMEOUT 1400
#endif

#ifdef ENABLE_NLS
#  include <libintl.h>
#  include <locale.h>
#  define _(x) gettext(x)
#else
#  define _(x) (x)
#endif

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

#ifndef BRIGHTNESS_CONTROL
#  ifdef BRIGHTNESS_KEYS
#    undef BRIGHTNESS_KEYS
#  endif
#endif

static struct {
	ScrollMode scrollmode;
	UserLock lock_rotate;
} settings = {
	.scrollmode = SM_KEY_PAGE,
	.lock_rotate = UL_UNLOCKED
};

static keymap_entry keymap[] = {
#define KEYMAP_SCROLLDOWN 0
	{ .code = 186, .name = "XF86ScrollDown" },
#define KEYMAP_SCROLLUP 1
	{ .code = 185, .name = "XF86ScrollUp" },
#define KEYMAP_ROTATEWINDOWS 2
	{ .code = 161, .name = "XF86RotateWindows",
	  .grab = 1 },
#define KEYMAP_BRIGHTNESSDOWN 3
	{ .code = 232, .name = "SunVideoLowerBrightness",
#ifdef BRIGHTNESS_KEYS
	  .grab = 1
#endif
       	},
#define KEYMAP_BRIGHTNESSUP 4
	{ .code = 233, .name = "SunVideoRaiseBrightness",
#ifdef BRIGHTNESS_KEYS
	  .grab = 1
#endif
	}
};

static unsigned keep_running = 1;
static clock_t  current_time;
static int mode_configure, mode_brightness;

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

//{{{ WACOM stuff
#ifdef ENABLE_WACOM
#include <wacomcfg/wacomcfg.h>
#include "Xwacom.h"
#include <X11/extensions/Xrandr.h>

#ifdef ENABLE_DYNAMIC
static struct {
	void *hdnl;
	WACOMCONFIG * (*WacomConfigInit)(Display* pDisplay, WACOMERRORFUNC pfnErrorHandler);
	WACOMDEVICE * (*WacomConfigOpenDevice)(WACOMCONFIG * hConfig, const char* pszDeviceName);
	int (*WacomConfigCloseDevice)(WACOMDEVICE * hDevice);
	int (*WacomConfigSetRawParam)(WACOMDEVICE * hDevice, int nParam, int nValue, unsigned * keys);
	void (*WacomConfigFree)(void* pvData);
} wclib;
#endif

static WACOMCONFIG * wacom_config;

static int wacom_init(Display *display)
{
#ifdef ENABLE_DYNAMIC
	if( !(DLOPEN(&wclib, "libwacomcfg.so.0") &&
			DLSYM(&wclib, WacomConfigInit) &&
			DLSYM(&wclib, WacomConfigFree) &&
			DLSYM(&wclib, WacomConfigSetRawParam) &&
			DLSYM(&wclib, WacomConfigOpenDevice) &&
			DLSYM(&wclib, WacomConfigCloseDevice)) ) {
		debug("WACOM", "%s", dlerror());
		wclib.hdnl = NULL;
		return -1;
	}

	debug("WACOM", "wacomcfg library ready");
#endif

	wacom_config = DLCALL(&wclib, WacomConfigInit, display, NULL);
	if(!wacom_config) {
		fprintf(stderr, "Can't open Wacom Device\n");
		return -1;
	}

	return 0;
}

static void wacom_exit(void)
{
	if(wacom_config)
		DLCALL(&wclib, WacomConfigFree, wacom_config);

#ifdef ENABLE_DYNAMIC
	DLCLOSE(&wclib);
#endif
}

static void wacom_rotate(int rr_rotation)
{
	WACOMDEVICE * d;
	int rotation;

	debug("TRACE", "wacom_rotate");

	if(!wacom_config)
		return;

	switch(rr_rotation) {
		case RR_Rotate_0:
			rotation = XWACOM_VALUE_ROTATE_NONE;
			break;
		case RR_Rotate_90:
			rotation = XWACOM_VALUE_ROTATE_CCW;
			break;
		case RR_Rotate_180:
			rotation = XWACOM_VALUE_ROTATE_HALF;
			break;
		case RR_Rotate_270:
			rotation = XWACOM_VALUE_ROTATE_CW;
			break;
		default:
			return;
	}

	debug("WACOM", "rotate to %d", rotation);

	d = DLCALL(&wclib, WacomConfigOpenDevice, wacom_config, "stylus");
	if(!d)
		return;

	DLCALL(&wclib, WacomConfigSetRawParam, d, XWACOM_PARAM_ROTATE,
			rotation, 0);

	DLCALL(&wclib, WacomConfigCloseDevice, d);
}
#endif
//}}}

//{{{ X11 stuff
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <X11/XF86keysym.h>
#include <X11/extensions/XTest.h>
#include <X11/extensions/Xrandr.h>
#include <X11/extensions/dpms.h>
static Display *display;
static XDevice *idevice;
static Window root;
static int x11_error(Display*, XErrorEvent*);
static int x11_ioerror(Display*);
static int xi_keypress;
static int xi_keyrelease;

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

int x11_open_input_device(void)
{
	XDeviceInfo *idev_list;
	XEventClass xeclass[2];
	int i, idev_num, error;

	debug("xinput", "searching fsc_btns device ...");
	idev_list = XListInputDevices(display, &idev_num);
	for(i=0; i < idev_num; i++) {
		debug("xinput", " ... device %s", idev_list[i].name);
		if(strncmp(idev_list[i].name, "fsc_btns", 3) == 0) {
			idevice = XOpenDevice(display, idev_list[i].id);
			break;
		}
	}
	XFreeDeviceList(idev_list);

	if(!idevice)
		return -1;

	DeviceKeyPress(idevice, xi_keypress, xeclass[0]);
	DeviceKeyRelease(idevice, xi_keyrelease, xeclass[1]);
	error = XSelectExtensionEvent(display, XDefaultRootWindow(display), xeclass, 2);
	if(error) {
		fprintf(stderr, "XSelectExtensionEvent failed.\n");
		XCloseDevice(display, idevice);
		return -1;
	}

	error = XGrabDevice(display, idevice, XDefaultRootWindow(display), False,
			2, xeclass, GrabModeAsync, GrabModeAsync, CurrentTime);
	if(error) {
		fprintf(stderr, "XGrabDevice failed.\n");
		XCloseDevice(display, idevice);
		return -1;
	}

	return 0;
}

Display* x11_init(void)
{
	Bool xinput, xtest, randr, dpms;
	keymap_entry *km;
	int error;

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

	xinput = x11_check_extension("XInputExtension");
	xtest  = x11_check_extension("XTEST");
	randr  = x11_check_extension("RANDR");
	dpms   = x11_check_extension("DPMS");

	if(!xinput || !xtest || !randr || !dpms) {
		XCloseDisplay(display);
		return NULL;
	}

	error = x11_open_input_device();
	if(error) {
		debug("X11", "xinput device not found");
		XCloseDisplay(display);
		return NULL;
	}

	for(km = keymap; km->code; km++) {
		km->sym = XStringToKeysym(km->name);

		if(km->sym && km->grab) {
			debug("X11", "grab key %s [%d]", km->name, km->code);
			XGrabKey(display, km->code, 0, root, False, GrabModeAsync, GrabModeAsync);
		}
	}

	XSync(display, False);
	return display;
}

static void x11_exit(void)
{
	keymap_entry *km;

	for(km = keymap; km->code; km++)
		if(km->sym && km->grab)
			XUngrabKey(display, km->code, 0, root);

	XSync(display, True);
	XUngrabDevice(display, idevice, XDefaultRootWindow(display));
	XCloseDevice(display, idevice);
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

static int x11_keyremap(int code, KeySym sym)
{
	int min, max, spc, me;
	KeySym *map;

	XDisplayKeycodes(display, &min, &max);
	if((code < min) || (code > max))
		return -1;

	map = XGetKeyboardMapping(display, code, 1, &spc);
	me  = (code - min) * spc;

	if(map[me] != sym) {
		debug("X11", "mapping keycode %d to symbol %s (0x%08x)",
				code, XKeysymToString(sym), (unsigned)sym);
		XChangeKeyboardMapping(display, code, 1, &sym, 1);
		return 1;
	} else
		return 0;
}

static int x11_fix_keymap(void)
{
	int min, max, spc, me;
	KeySym *map;
	keymap_entry *km;

	XDisplayKeycodes(display, &min, &max);
	map = XGetKeyboardMapping(display, min, (max - min + 1), &spc);
	debug("X11", "keymap with %d (%d-%d) entries and %d symbols per code",
			(max-min), min, max, spc);

	for(km = keymap; km->code; km++) {
		me = (km->code - min) * spc;

		if(map[me] != km->sym) {
			debug("X11", "mapping keycode %d to symbol %s (0x%08x)",
					km->code, km->name, (unsigned)km->sym);
			XChangeKeyboardMapping(display, km->code, 1, &(km->sym), 1);
		} else 
			debug("X11", "keycode %d is ok.",
					km->code);
	}

	x11_keyremap(keymap[KEYMAP_SCROLLDOWN].code, XK_Next);
	x11_keyremap(keymap[KEYMAP_SCROLLUP].code, XK_Prior);

	XSync(display, False);
	return 0;
}

static void x11_grab_scrollkeys(void)
{
	debug("X11", "grab scroll keys");
	XGrabKey(display, keymap[KEYMAP_SCROLLDOWN].code, AnyModifier, root,
			False, GrabModeAsync, GrabModeAsync);
	XGrabKey(display, keymap[KEYMAP_SCROLLUP].code, AnyModifier, root,
			False, GrabModeAsync, GrabModeAsync);
	XSync(display, False);
}

static void x11_ungrab_scrollkeys(void)
{
	debug("X11", "ungrab scroll keys");
	XUngrabKey(display, keymap[KEYMAP_SCROLLDOWN].code, AnyModifier, root);
	XUngrabKey(display, keymap[KEYMAP_SCROLLUP].code, AnyModifier, root);
	XSync(display, False);
}

static int dpms_enabled(void)
{
	CARD16 state;
	BOOL on;
	DPMSInfo(display, &state, &on);
	return (on == True);
}

static int enable_dpms(void)
{
	DPMSEnable(display);
	return dpms_enabled();
}

/* TODO: toggle_dpms
static int disable_dpms(void)
{
	DPMSDisable(display);
	return !dpms_enabled();
}
*/

static void dpms_force_off(void)
{
	CARD16 state;
	BOOL on;

	debug("TRACE", "dpms_force_off");

	DPMSInfo(display, &state, &on);
	if(!on)
		enable_dpms();

	XSync(display, True);

	DPMSForceLevel(display, DPMSModeOff);
	XSync(display, False);
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

		screen_rotated();

	}

  err:
	XRRFreeScreenConfigInfo(sc);
}

static void fake_key(KeySym sym)
{
	KeyCode keycode;

	keycode = XKeysymToKeycode(display, sym);
	debug("X11", "fake keycode %d (keysym 0x%04x)", keycode, (unsigned)sym);

	if(!keycode) {
		fprintf(stderr, "No keycode for %s, use xmodmap to define one.\n",
				XKeysymToString(sym));
		return;
	}

	XTestFakeKeyEvent(display, keycode, True,  CurrentTime);
	XSync(display, False);
	XTestFakeKeyEvent(display, keycode, False, CurrentTime);
	XSync(display, False);
}

static void fake_button(unsigned int button)
{
	int steps = ZAXIS_SCROLL_STEPS;

	while(steps--) {
		debug("X11", "fake button %d event", button);
		XTestFakeButtonEvent(display, button, True,  CurrentTime);
		XSync(display, False);
		XTestFakeButtonEvent(display, button, False, CurrentTime);
		XSync(display, False);
	}
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

//{{{ Brightness stuff
#ifdef BRIGHTNESS_CONTROL
static int brightness_max;

static int brightness_init(void)
{
	if(!laptop_panel)
		return -1;

	brightness_max = libhal_device_get_property_int(hal, laptop_panel,
			"laptop_panel.num_levels", &dbus_error) - 1;
	if(dbus_error_is_set(&dbus_error)) {
		fprintf(stderr, "query max brightness levels failed - %s\n",
				dbus_error.message);
		return -1;
	}

	return 0;
}

static void brightness_exit(void)
{
}

static int get_brightness(void)
{
	int level;
	DBusMessage *message, *reply;

	if(!laptop_panel)
		return -1;

	message = dbus_message_new_method_call(
			"org.freedesktop.Hal",
			laptop_panel,
			"org.freedesktop.Hal.Device.LaptopPanel",
			"GetBrightness");

	reply = dbus_connection_send_with_reply_and_block(dbus,
			message, -1, &dbus_error);
	if(dbus_error_is_set(&dbus_error)) {
		fprintf(stderr, "send get brightness message - %s\n",
				dbus_error.message);
		goto err_free_msg;
	}
	dbus_message_unref(message);

	if( !dbus_message_get_args(reply, NULL,
			DBUS_TYPE_INT32, &level,
			DBUS_TYPE_INVALID)) {
		fprintf(stderr, "dbus_message_get_args failed - %s\n",
				dbus_error.message);
		goto err_free_msg;
	}
	dbus_message_unref(reply);

	return level;

 err_free_msg:
	dbus_message_unref(message);
	return -1;
}

static void set_brightness(int level)
{
	DBusMessage *message;

	debug("HAL", "set_brightness: level = %d", level);

	if(!laptop_panel)
		return;

	message = dbus_message_new_method_call(
			"org.freedesktop.Hal",
			laptop_panel,
			"org.freedesktop.Hal.Device.LaptopPanel",
			"SetBrightness");

	if( !dbus_message_append_args(message,
			DBUS_TYPE_INT32, &level,
			DBUS_TYPE_INVALID)) {
		debug("HAL", "append to message failed");
		goto err_free_msg;
	}

	dbus_connection_send(dbus, message, NULL);

 err_free_msg:
	debug("HAL", "set_brightness: error");
	dbus_message_unref(message);
	return;
}

static void brightness_show(void)
{
	gui_brightness_show((get_brightness()*100) / brightness_max);
}

static void brightness_down(void)
{
	int current = get_brightness();

	if(current > 0)
		set_brightness(--current);

	gui_brightness_show((current*100) / brightness_max);
}

static void brightness_up(void)
{
	int current = get_brightness();

	if(current < brightness_max)
		set_brightness(++current);

	gui_brightness_show((current*100) / brightness_max);
}
#endif
//}}}

static void scrollmode_set(ScrollMode mode)
{
	settings.scrollmode = mode;

	switch(mode) {
		case SM_ZAXIS:
			gui_info("%s: %s", _("Scrolling"), _("Z-Axis"));
			x11_keyremap(keymap[KEYMAP_SCROLLDOWN].code, XF86XK_ScrollDown);
			x11_keyremap(keymap[KEYMAP_SCROLLUP].code, XF86XK_ScrollUp);
			break;

		case SM_KEY_PAGE:
			gui_info("%s: %s", _("Scrolling"), _("Page Up/Down"));
			x11_keyremap(keymap[KEYMAP_SCROLLDOWN].code, XK_Next);
			x11_keyremap(keymap[KEYMAP_SCROLLUP].code, XK_Prior);
			break;

		case SM_KEY_SPACE:
			gui_info("%s: %s", _("Scrolling"), _("Space/Backspace"));
			x11_keyremap(keymap[KEYMAP_SCROLLDOWN].code, XK_space);
			x11_keyremap(keymap[KEYMAP_SCROLLUP].code, XK_BackSpace);
			break;

		case SM_KEY_MAX:
			break;
	}
}

static void scrollmode_next(void)
{
	scrollmode_set((settings.scrollmode+1) % SM_KEY_MAX);
}

static void scrollmode_prev(void)
{
	scrollmode_set(settings.scrollmode? settings.scrollmode-1 : SM_KEY_MAX-1);
}


static void toggle_lock_rotate(void)
{
	switch(settings.lock_rotate) {
		case UL_UNLOCKED:
			settings.lock_rotate = UL_LOCKED;
			gui_info(_("Rotation locked"));
			break;
		case UL_LOCKED:
			settings.lock_rotate = UL_UNLOCKED;
			gui_info(_("Rotation unlocked"));
			break;
	}
}

/* TODO:
static void toggle_dpms(void)
{
	if(dpms_enabled()) {
		disable_dpms();
		gui_info(_("DPMS disabled"));
	} else {
		enable_dpms();
		gui_info(_("DPMS enabled"));
	}
}
*/

int handle_display_rotation(int mode)
{
	int error;
		
	if(mode)
		error = run_script("fscd-pre-mode-tablet");
	else
		error = run_script("fscd-pre-mode-normal");
	if(error)
		return -1;

	if(settings.lock_rotate == UL_UNLOCKED)
		rotate_screen(mode);

	if(mode)
		error = run_script("fscd-mode-tablet");
	else
		error = run_script("fscd-mode-normal");

	return error;
}

static int handle_x11_event(unsigned int keycode, unsigned int state, int pressed)
{
	debug("TRACE", "handle_x11_event: time=%lu keycode=%d, state=%d, action=%s [cfg=%d, bri=%d]",
			current_time, keycode, state, (pressed ? "pressed" : "released"),
			mode_configure, mode_brightness);

	do { // FIXME: for the breaks

	if(keycode == keymap[KEYMAP_SCROLLDOWN].code) {
		if(pressed)
			return 0;

		if(mode_configure) {
			mode_configure = current_time + STICKY_TIMEOUT;
			scrollmode_prev();
			break;
		}

#ifdef BRIGHTNESS_CONTROL
		if(mode_brightness) {
			mode_brightness = current_time + STICKY_TIMEOUT;
			brightness_down();
			break;
		}
#endif

		if(settings.scrollmode == SM_ZAXIS)
			fake_button(5);

		break;

	} else if(keycode == keymap[KEYMAP_SCROLLUP].code) {
		if(pressed)
			return 0;

		if(mode_configure) {
			mode_configure = current_time + STICKY_TIMEOUT;
			scrollmode_next();
			break;
		}

#ifdef BRIGHTNESS_CONTROL
		if(mode_brightness) {
			mode_brightness = current_time + STICKY_TIMEOUT;
			brightness_up();
			break;
		}
#endif

		if(settings.scrollmode == SM_ZAXIS)
			fake_button(4);

		break;

	} else if(keycode == keymap[KEYMAP_ROTATEWINDOWS].code) {
		if(pressed)
			return 0;

		if(mode_configure) {
			mode_configure = current_time + STICKY_TIMEOUT;
			toggle_lock_rotate();
			break;
		}

#ifdef BRIGHTNESS_CONTROL
		if(mode_brightness) {
			mode_brightness = 0;
			gui_hide();
			dpms_force_off();
			break;
		}
#endif

		rotate_screen(-1);
		break;

#ifdef BRIGHTNESS_CONTROL
	} else if(keycode == keymap[KEYMAP_BRIGHTNESSDOWN].code) {
		if(pressed)
			return 0;

		brightness_down();
		break;

	} else if(keycode == keymap[KEYMAP_BRIGHTNESSUP].code) {
		if(pressed)
			return 0;

		brightness_up();
		break;
#endif

	} else {
		debug("X11", "WOW, what a key!?");
	}

	} while(0);

	return 0;
}

static int handle_xinput_event(unsigned int keycode, unsigned int state, int pressed)
{
	static int key_fn, key_alt;

	debug("TRACE", "handle_xinput_event: time=%lu keycode=%d, state=%d, action=%s [fn=%d, alt=%d, cfg=%d, bri=%d]",
			current_time, keycode, state, (pressed ? "pressed" : "released"),
			key_fn, key_alt, mode_configure, mode_brightness);

	if(keycode == 37) { // FN (Control)
		debug("XI", "Control %s", (pressed ? "pressed" : "released"));

		if(pressed) {
			if(key_alt) {
				gui_info("configuration...");
				mode_configure = current_time + (2 * STICKY_TIMEOUT);
				x11_grab_scrollkeys();
			} else
				gui_info("FN...");

			key_fn = current_time;
		} else {
			if(!mode_configure && !mode_brightness)
				gui_hide();
			key_fn = 0;
		}

	} else if(keycode == 64) { // Alt
		debug("XI", "Alt %s", (pressed ? "pressed" : "released"));

		if(pressed) {
#ifdef BRIGHTNESS_CONTROL
			if(key_fn) {
				brightness_show();
				mode_brightness = current_time + (2 * STICKY_TIMEOUT);
				x11_grab_scrollkeys();
			} else
#endif
				gui_info("Alt...");

			key_alt = current_time;
		} else {
			if(!mode_configure && !mode_brightness)
				gui_hide();
			if(key_alt + 3000 < current_time)
				fake_key(XF86XK_Standby);
			key_alt = 0;
		}

	} else {
		debug("X11", "WOW, what a key!?");
	}

	return 0;
}

int main(int argc, char **argv)
{
	int error;

	if((geteuid() == 0) && (getuid() > 0)) {
		fprintf(stderr, " *** suid is no longer needed ***\n");
		sleep(5);
		seteuid(getuid());
	}

#ifdef ENABLE_NLS
#ifndef DEBUG
	setlocale (LC_ALL, "");
	bindtextdomain (PACKAGE, LOCALEDIR);
	textdomain (PACKAGE);
#endif
#endif

	error = hal_init();
	if(error) {
		fprintf(stderr, "hal initalisation failed\n");
		goto hal_failed;
	}

#ifdef BRIGHTNESS_CONTROL
	error = brightness_init();
	if(error) {
		fprintf(stderr, "brightness initalisation failed\n");
	}
#endif

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

	error = gui_init(display);
	if(error) {
		fprintf(stderr, "gui initalisation failed\n");
	}

	gui_info("%s %s %s", PACKAGE, VERSION, _("started"));

	debug("INFO", "\n *** Please report bugs to " PACKAGE_BUGREPORT " ***\n");

	x11_fix_keymap();
	handle_display_rotation(get_tablet_sw());

	while(keep_running) {
		static struct timeval tv;

		gettimeofday(&tv, NULL);
		current_time = (tv.tv_sec * 1000) + (tv.tv_usec / 1000); //msec

		while(keep_running && XPending(display)) {
			static XEvent xevent;

			XNextEvent(display, &xevent);
			if(xevent.type == KeyPress) {
				XKeyEvent *e = (XKeyEvent*) &xevent;
				keep_running = (handle_x11_event(e->keycode, e->state, 1) >= 0);
			} else if(xevent.type == KeyRelease) {
				XKeyEvent *e = (XKeyEvent*) &xevent;
				keep_running = (handle_x11_event(e->keycode, e->state, 0) >= 0);
			} else if(xevent.type == xi_keypress) {
				XDeviceKeyPressedEvent *e = (XDeviceKeyPressedEvent*) &xevent;
				keep_running = (handle_xinput_event(e->keycode, e->state, 1) >= 0);
			} else if(xevent.type == xi_keyrelease) {
				XDeviceKeyReleasedEvent *e = (XDeviceKeyReleasedEvent*) &xevent;
				keep_running = (handle_xinput_event(e->keycode, e->state, 0) >= 0);
			}

			XSync(display, False);
		}

		if(mode_configure) {
			if(mode_configure < current_time) {
				mode_configure = 0;
				gui_hide();
				if(settings.scrollmode != SM_ZAXIS)
					x11_ungrab_scrollkeys();
			}
		}
#ifdef BRIGHTNESS_CONTROL
		else if(mode_brightness) {
			if(mode_brightness < current_time) {
				mode_brightness = 0;
				gui_hide();
				x11_ungrab_scrollkeys();
			}
		}
#endif

		debug("MAIN", "time = %lu, timeout = %d", current_time, 100);
		dbus_connection_read_write_dispatch(dbus, 100);	// timeout in msec
	}

	gui_exit();
#ifdef ENABLE_WACOM
	wacom_exit();
#endif
	x11_exit();
 x_failed:
#ifdef BRIGHTNESS_CONTROL
	brightness_exit();
#endif
	hal_exit();
 hal_failed:
	return 0;
}

