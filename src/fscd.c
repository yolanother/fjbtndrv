/* FSC Tablet Buttons Helper Daemon
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

#define ZAXIS_SCROLL_STEPS	1
#define XOSD_COLOR		"green"
#define XOSD_OUTLINE_COLOR	"darkgreen"

/******************************************************************************/

#ifdef HAVE_CONFIG_H
#  include "../config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <signal.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <linux/input.h>
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <X11/XF86keysym.h>

#ifdef ENABLE_NLS
#  include <libintl.h>
#  define _(x) gettext(x)
#else
#  define _(x) (x)
#endif

#ifdef DEBUG
#  define debug(msg, a...)	fprintf(stderr, msg "\n", ##a)
#else
#  define debug(msg, a...)	/**/
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


typedef enum {
	SM_ZAXIS,
	SM_KEY_PAGE,
	SM_KEY_SPACE
} ScrollMode;

typedef enum {
	UL_LOCKED,
	UL_UNLOCKED
} UserLock;

static struct {
	ScrollMode scrollmode;
	UserLock lock_rotate;
} settings = {
	.scrollmode = SM_ZAXIS,
	.lock_rotate = UL_UNLOCKED,
};


static unsigned keep_running = 1;
#ifdef ENABLE_XOSD
static clock_t  current_time;
static int mode_configure, mode_brightness;
#endif

//{{{ Input stuff
#include <linux/input.h>
static int input;

#ifndef KEY_BRIGHTNESS_ZERO
#define KEY_BRIGHTNESS_ZERO 244
#endif

int input_init(void)
{
	char filename[64];
	char name[64];
	int x;

	for(x = 0; x < 255; x++) {
		snprintf(filename, sizeof(filename), "/dev/input/event%d", x);
		debug("INPUT: check input device %s...", filename);

		input = open(filename, O_RDONLY);
		if(input < 0)
			continue;

		if(ioctl(input, EVIOCGNAME(sizeof(name)), name) > 0)
			if(strcmp(name, "fsc tablet buttons") == 0)
				return 0;

		close(input);
	}

	return -1;
}

void input_exit(void)
{
	close(input);
}
//}}}

//{{{ OSD stuff
#ifdef ENABLE_XOSD
#include <xosd.h>

static xosd *osd = NULL;

int osd_init(Display *display)
{
	if(osd) {
		xosd_destroy(osd);
		osd = NULL;
	}
	return 0;
}

void osd_exit(void)
{
	if(osd)
		xosd_destroy(osd);
	osd = NULL;
}

xosd *osd_new(int lines)
{
	if(osd) {
		if(xosd_get_number_lines(osd) == lines)
			return osd;

		xosd_destroy(osd);
	}

	if(lines <= 0)
		return osd = NULL;

	osd = xosd_create(lines);

	xosd_set_pos(osd, XOSD_bottom);
	xosd_set_vertical_offset(osd, 16);
	xosd_set_align(osd, XOSD_center);
	xosd_set_horizontal_offset(osd, 0);

	xosd_set_font(osd, "-*-helvetica-bold-r-normal-*-*-400-*-*-*-*-*-*");
	xosd_set_outline_offset(osd, 1);
	xosd_set_outline_colour(osd, XOSD_OUTLINE_COLOR);
	xosd_set_shadow_offset(osd, 3);
	xosd_set_colour(osd, XOSD_COLOR);

	return osd;
}

#define osd_hide() osd_exit()
#define osd_timeout(s) xosd_set_timeout(osd, s)

#define osd_info(format, a...) do {			\
	xosd *osd = osd_new(1);				\
	xosd_display(osd, 0, XOSD_printf, format, ##a); \
} while(0)

#define osd_slider(percent, format, a...) do {	\
	xosd *osd = osd_new(2);				\
	xosd_display(osd, 0, XOSD_printf, format, ##a);	\
	xosd_display(osd, 1, XOSD_slider, percent);	\
} while(0)

#endif
//}}}

//{{{ WACOM stuff
#ifdef ENABLE_WACOM
#include <wacomcfg/wacomcfg.h>
#include <Xwacom.h>

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

int wacom_init(Display *display)
{
#ifdef ENABLE_DYNAMIC
	if( !(DLOPEN(&wclib, "libwacomcfg.so") &&
			DLSYM(&wclib, WacomConfigInit) &&
			DLSYM(&wclib, WacomConfigFree) &&
			DLSYM(&wclib, WacomConfigSetRawParam) &&
			DLSYM(&wclib, WacomConfigOpenDevice) &&
			DLSYM(&wclib, WacomConfigCloseDevice)) ) {
		debug("WACOM: %s", dlerror());
		wclib.hdnl = NULL;
		return -1;
	}

	debug("WACOM: wacomcfg library ready");
#endif

	wacom_config = DLCALL(&wclib, WacomConfigInit, display, NULL);
	if(!wacom_config) {
		fprintf(stderr, "Can't open Wacom Device\n");
		return -1;
	}

	return 0;
}

void wacom_exit(void)
{
	if(wacom_config)
		DLCALL(&wclib, WacomConfigFree, wacom_config);

#ifdef ENABLE_DYNAMIC
	DLCLOSE(&wclib);
#endif
}

void wacom_rotate(int mode)
{
	WACOMDEVICE * d;

	if(!wacom_config)
		return;

	d = DLCALL(&wclib, WacomConfigOpenDevice, wacom_config, "stylus");
	if(!d)
		return;

	DLCALL(&wclib, WacomConfigSetRawParam, d, XWACOM_PARAM_ROTATE,
			(mode ? XWACOM_VALUE_ROTATE_CW : XWACOM_VALUE_ROTATE_NONE),
			0);

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
static Window root;

Display* x11_init(void)
{
	Bool xtest, randr, dpms;
	int opcode, event, error;
#ifdef DEBUG
	int major, minor;
#endif

	display = XOpenDisplay(NULL);
	if(!display)
		return NULL;

	root = XDefaultRootWindow(display);
	if(!root) {
		XCloseDisplay(display);
		return NULL;
	}

	xtest = XQueryExtension(display, "XTEST",
			&opcode, &event, &error);
	if(xtest) {
#ifdef DEBUG
		XTestQueryExtension(display,
				&event, &error,
				&major, &minor);
		debug(" X11 :  Found XTest %d.%d extension (%d, %d, %d)",
			major, minor,
			opcode, event, error);
#endif
	} else
		fprintf(stderr, "No XTest extension\n");

	randr = XQueryExtension(display, "RANDR",
			&opcode, &event, &error);
	if(randr) {
#ifdef DEBUG
		XRRQueryVersion(display, &major,&minor);
		debug(" X11 : Found RandR %d.%d extension (%d, %d, %d)",
				major, minor,
				opcode, event, error);
#endif
	} else
		fprintf(stderr, "No RandR extension\n");

	dpms = XQueryExtension(display, "DPMS",
			&opcode, &event, &error);
	if(dpms) {
#ifdef DEBUG
		DPMSGetVersion(display, &major,&minor);
		debug(" X11 : Found DPMS %d.%d extension (%d, %d, %d)",
				major, minor,
				opcode, event, error);
#endif
	} else
		fprintf(stderr, "No DPMS extension\n");

	if(!xtest || !randr || !dpms) {
		fprintf(stderr, "Can't open display\n");
		XCloseDisplay(display);
		return NULL;
	}

	XGrabKey(display, 101, 0, root, True, GrabModeAsync, GrabModeAsync);
	XGrabKey(display, 143, 0, root, True, GrabModeAsync, GrabModeAsync);
	XGrabKey(display, 203, 0, root, True, GrabModeAsync, GrabModeAsync);
	XGrabKey(display, 212, 0, root, True, GrabModeAsync, GrabModeAsync);
	XGrabKey(display, 220, 0, root, True, GrabModeAsync, GrabModeAsync);
	XSync(display, False);

	return display;
}

void x11_exit(void)
{
	XUngrabKey(display, 101, 0, root);
	XUngrabKey(display, 143, 0, root);
	XUngrabKey(display, 203, 0, root);
	XUngrabKey(display, 212, 0, root);
	XUngrabKey(display, 220, 0, root);
	XSync(display, True);
	XCloseDisplay(display);
}

int dpms_enabled(void)
{
	CARD16 state;
	BOOL on;
	DPMSInfo(display, &state, &on);
	return (on == True);
}

int enable_dpms(void)
{
	DPMSEnable(display);
	return dpms_enabled();
}

int disable_dpms(void)
{
	DPMSDisable(display);
	return !dpms_enabled();
}

void dpms_force_off(void)
{
	CARD16 state;
	BOOL on;

	DPMSInfo(display, &state, &on);
	if(!on)
		enable_dpms();

	XSync(display, True);

	DPMSForceLevel(display, DPMSModeOff);
	XSync(display, False);
}

int rotate_screen(int mode)
{
	Window rwin;
	XRRScreenConfiguration *sc;
	Rotation rotation, current_rotation;
	SizeID size;
	int error = -1;
	static int vkbd_pid;

	rwin = DefaultRootWindow(display);
	sc = XRRGetScreenInfo(display, rwin);
	if(!sc)
		goto err;

	rotation = XRRRotations(display, 0, &current_rotation);
	if(!(rotation & RR_Rotate_0) || !(rotation & RR_Rotate_270))
		goto err_sc;

	size = XRRConfigCurrentConfiguration(sc, &current_rotation);

	if(mode == -1)
		mode = current_rotation & RR_Rotate_0;

	rotation  = current_rotation & ~0xf;
	rotation |= (mode ? RR_Rotate_270 : RR_Rotate_0);

	if(rotation != current_rotation) {
		error = XRRSetScreenConfig(display, sc, rwin, size, rotation, CurrentTime);

#ifdef ENABLE_WACOM
		if(!error)
			wacom_rotate(mode);
#endif
	} else
		error = 0;

#ifdef ENABLE_XOSD
	osd_init(display);
#endif

	if(mode) {	// tablet mode
		vkbd_pid = fork();
		if(vkbd_pid == 0)
			execlp("xvkbd", "xvkbd", NULL);
	} else {
		if(vkbd_pid > 0) {
			kill(vkbd_pid, SIGTERM);
			waitpid(vkbd_pid, NULL, 0);
		}
	}

 err_sc:
	XRRFreeScreenConfigInfo(sc);
 err:
	return error;
}

int fake_key(KeySym sym)
{
	KeyCode keycode;

	if(sym == 0)
		return -1;

	keycode = XKeysymToKeycode(display, sym);
	debug(" X11 : fake keycode %d (keysym 0x%04x)", keycode, (unsigned)sym);

	if(keycode) {
		debug(" X11 : fake key %d event", keycode);
		XTestFakeKeyEvent(display, keycode, True,  CurrentTime);
		XSync(display, False);
		XTestFakeKeyEvent(display, keycode, False, CurrentTime);
		XSync(display, False);
		return 0;
	}

	fprintf(stderr, "There is no keycode for %s, use xmodmap to define one\n", XKeysymToString(sym));
	return -1;
}

int fake_button(unsigned int button)
{
	int steps = ZAXIS_SCROLL_STEPS;

	while(steps--) {
		debug(" X11 : fake button %d event", button);
		XTestFakeButtonEvent(display, button, True,  CurrentTime);
		XSync(display, False);
		XTestFakeButtonEvent(display, button, False, CurrentTime);
		XSync(display, False);
	}

	return 0;
}
//}}}

//{{{ HAL stuff
#include <dbus/dbus.h>
#include <hal/libhal.h>
static DBusConnection *dbus;
static DBusError dbus_error;
static LibHalContext *hal;
static char *fsc_tablet_device;

int hal_init(void)
{
	char **devices;
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

	/* search fsc_btns driver */
	devices = libhal_find_device_by_capability(hal,
			"input.switch", &count, &dbus_error);
	if(dbus_error_is_set(&dbus_error)) {
		fprintf(stderr, "find_device_by_capability - %si\n",
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

	debug(" HAL : %d input.switch device(s) found:", count);
	while(count-- && devices[count]) {
		char *type;
		debug(" HAL : check device %s", devices[count]);

		type = libhal_device_get_property_string(hal,
				devices[count], "button.type",
				&dbus_error);
		if(dbus_error_is_set(&dbus_error)) {
			fprintf(stderr, "prop get failed - %s\n",
					dbus_error.message);
			goto err_input_next_device;
		}

		if(type && !strcmp("tablet_mode", type)) {
			debug(" HAL : tablet mode device found: %s",
					devices[count]);

			fsc_tablet_device = strdup(devices[count]);
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

void hal_exit(void)
{
	if(hal) {
		libhal_ctx_free(hal);
		dbus_error_free(&dbus_error);
	}
}

int get_tablet_sw(void)
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
//}}} 

//{{{ Brightness stuff
static char *laptop_panel;
static int brightness_max;

int brightness_init(void)
{
	char **devices;
	int count;

	devices = libhal_find_device_by_capability(hal,
			"laptop_panel", &count, &dbus_error);
	if(dbus_error_is_set(&dbus_error))
		return -1;

	if((devices == NULL) || (count <= 0)) {
		libhal_free_string_array(devices);
		return -1;
	}

	laptop_panel = strdup(devices[0]);
	libhal_free_string_array(devices);

	brightness_max = libhal_device_get_property_int(hal, laptop_panel,
			"laptop_panel.num_levels", &dbus_error);
	if(dbus_error_is_set(&dbus_error)) {
		fprintf(stderr, "query max brightness levels failed - %s\n",
				dbus_error.message);
		return -1;
	}

	return 0;
}

void brightness_exit(void)
{
}

int get_brightness(void)
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

	debug(" HAL : get_brightness: level = %d", level);
	return level;

 err_free_msg:
	debug(" HAL : get_brightness: error");
	dbus_message_unref(message);
	return -1;
}

void set_brightness(int level)
{
	DBusMessage *message;

	debug(" HAL : set_brightness: level = %d", level);

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
		debug(" HAL : append to message failed");
		goto err_free_msg;
	}

	dbus_connection_send(dbus, message, NULL);

 err_free_msg:
	debug(" HAL : set_brightness: error");
	dbus_message_unref(message);
	return;
}

void brightness_show(void)
{
#ifdef ENABLE_XOSD
	osd_slider( ((get_brightness()-1) * 100) / (brightness_max-1),
			"%s", _("Brightness") );

	if(!mode_brightness)
		osd_timeout(1);
#endif
}

void brightness_down(void)
{
	int current;

	current = get_brightness();

	if(current == 0)
		current = 8;

	set_brightness(current-1);
	brightness_show();
}

void brightness_up(void)
{
	int current;

	current = get_brightness();

	if(current < brightness_max)
		current++;

	set_brightness(current);
	brightness_show();
}
//}}}

//{{{ RC stuff
void scrollmode_info(void)
{
#ifdef ENABLE_XOSD
	switch(settings.scrollmode) {
		case SM_ZAXIS:
			osd_info("%s: %s", _("Scrolling"), _("Page Up/Down"));
			break;
		case SM_KEY_PAGE:
			osd_info("%s: %s", _("Scrolling"), _("Space/Backspace"));
			break;
		case SM_KEY_SPACE:
			osd_info("%s: %s", _("Scrolling"), _("Z-Axis"));
			break;
	}
#endif
}

void scrollmode_next(void)
{
	settings.scrollmode = (++settings.scrollmode % 3);
	scrollmode_info();
}

void scrollmode_prev(void)
{
	settings.scrollmode = (settings.scrollmode? --settings.scrollmode : 2);
	scrollmode_info();
}


void toggle_lock_rotate(void)
{
#ifdef ENABLE_XOSD
	switch(settings.lock_rotate) {
		case UL_UNLOCKED:
			settings.lock_rotate = UL_LOCKED;
			osd_info(_("Rotation locked"));
			break;
		case UL_LOCKED:
			settings.lock_rotate = UL_UNLOCKED;
			osd_info(_("Rotation unlocked"));
			break;
	}
#endif
}

void toggle_dpms(void)
{
#ifdef ENABLE_XOSD
	if(dpms_enabled()) {
		disable_dpms();
		osd_info(_("DPMS disabled"));
	} else {
		enable_dpms();
		osd_info(_("DPMS enabled"));
	}
#endif
}
//}}}

int handle_x11_event(XKeyEvent *event)
{
	switch(event->keycode) {
	case 143: /* XF86XK_ScrollDown */
#ifdef ENABLE_XOSD
		if(mode_configure) {
			mode_configure = current_time + STICKY_TIMEOUT;
			scrollmode_prev();
			break;
		}

		if(mode_brightness) {
			mode_brightness = current_time + STICKY_TIMEOUT;
			brightness_down();
			break;
		}
#endif

		switch(settings.scrollmode) {
			case SM_ZAXIS:
				fake_button(5);
				break;
			case SM_KEY_PAGE:
				fake_key(XK_Next);
				break;
			case SM_KEY_SPACE:
				fake_key(XK_space);
				break;
		}
		break;

	case 220: /* XF86XK_ScrollUp */
#ifdef ENABLE_XOSD
		if(mode_configure) {
			mode_configure = current_time + STICKY_TIMEOUT;
			scrollmode_next();
			break;
		}

		if(mode_brightness) {
			mode_brightness = current_time + STICKY_TIMEOUT;
			brightness_up();
			break;
		}
#endif

		switch(settings.scrollmode) {
			case SM_ZAXIS:
				fake_button(4);
				break;
			case SM_KEY_PAGE:
				fake_key(XK_Prior);
				break;
			case SM_KEY_SPACE:
				fake_key(XK_BackSpace);
				break;
		}
		break;

	case 203: /* XF86XK_RotateWindows */
#ifdef ENABLE_XOSD
		if(mode_configure) {
			mode_configure = current_time + STICKY_TIMEOUT;
			toggle_lock_rotate();
			break;
		}

		if(mode_brightness) {
			mode_brightness = current_time + STICKY_TIMEOUT;
			dpms_force_off();
			break;
		}
#endif

		rotate_screen(-1);
		break;

	case 101: /* XF86XK_MonBrightnessDown */
		brightness_down();
		break;

	case 212: /* XF86XK_MonBrightnessUp */
		brightness_up();
		break;

	default:
		debug(" X11 : WOW, what a key! I've grab it?");
	}

	return 0;
}

int handle_input_event(struct input_event *event)
{
	static unsigned key_fn=0, key_alt=0, key_rep=0;

	switch(event->type) {
	case EV_SYN:
		debug("INPUT: syn'd");
		break;

	case EV_MSC:
		debug("INPUT: misc event %d with %d",
				event->code, event->value);
		break;

	case EV_KEY:
		debug("INPUT: key %d %s", event->code,
				((event->value == 1)? "pressed" :
					((event->value == 2)? "autorepeat" :
						"released")));

		switch(event->code) {
		case KEY_FN:
			key_fn = event->value;

			if(event->value == 2)
				break;

			XTestFakeKeyEvent(display,
					XKeysymToKeycode(display, XK_Control_L),
					event->value, CurrentTime);
			XSync(display, False);

#ifdef ENABLE_XOSD
			if(event->value == 0) {
				if(!mode_brightness && !mode_configure)
					osd_hide();
				break;
			}

			if(key_alt) {
				osd_info(_("configuration..."));
				mode_configure = current_time + (3 * STICKY_TIMEOUT);
				break;
			}

			osd_info("[ Fn ]");
#endif
			break;

		case KEY_LEFTALT:
			key_alt = event->value;

#ifdef ENABLE_XOSD
			if(event->value == 2)
				break;

			if(event->value == 0) {
				if(!mode_brightness && !mode_configure)
					osd_hide();

				break;
			}

			if(key_fn) {
				brightness_show();
				mode_brightness = current_time + (3 * STICKY_TIMEOUT);
				break;
			}

			osd_info("[ Alt ]");
#endif
			break;

		case KEY_BRIGHTNESS_ZERO:
			switch(event->value) {
			case 0:
				if(!key_rep)
					dpms_force_off();
				else
					key_rep = 0;
				break;

			case 1:
				break;

			case 2:
				if(!key_rep) {
					key_rep = 1;
					toggle_dpms();
				}
				break;
			}
			break;

		default:
			debug("INPUT: unknown key, skipping");
		}
		break;

	case EV_SW:
		switch(event->code) {
		case SW_TABLET_MODE:
			debug("INPUT: tablet mode = %d", event->value);
			if(settings.lock_rotate == UL_UNLOCKED)
				rotate_screen(event->value);
			break;
		default:
			debug("INPUT: unknown switch, skipping");
		}
		break;

	default:
		fprintf(stderr, "unsupported event type %d",
				event->type);
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

	error = input_init();
	if(error) {
		fprintf(stderr, "can't open input device\n");
		goto input_failed;
	}

	error = hal_init();
	if(error) {
		fprintf(stderr, "hal initalisation failed\n");
		goto hal_failed;
	}

	error = brightness_init();
	if(error) {
		fprintf(stderr, "brightness initalisation failed\n");
	}

	Display *display = x11_init();
	if(!display) {
		fprintf(stderr, "x initalisation failed\n");
		goto x_failed;
	}

#ifdef ENABLE_WACOM
	error = wacom_init(display);
	if(error) {
		fprintf(stderr, "wacom initalisation failed\n");
	}
#endif

#ifdef ENABLE_XOSD
	error = osd_init(display);
	if(error) {
		fprintf(stderr, "osd initalisation failed\n");
	}

	osd_info("%s %s %s", PACKAGE, VERSION, _("started"));
	osd_timeout(3);
#endif
	debug("\n *** Please report bugs to " PACKAGE_BUGREPORT " ***\n");

	while(keep_running) {
		fd_set rfd;
		int result;
		struct timeval tv;
		int timeout = STICKY_TIMEOUT;

#ifdef ENABLE_XOSD
		gettimeofday(&tv, NULL);
		current_time = (tv.tv_sec * 1000) + (tv.tv_usec / 1000);
		debug("LOOPY: current_time = %lu", current_time);

		if(mode_configure) {
			timeout = mode_configure - current_time;
			debug("LOOPY: mode_configure = %u -> timeout = %d",
					mode_configure, timeout);
			if(timeout <= 0) {
				timeout = STICKY_TIMEOUT;
				mode_configure = 0;
				osd_hide();
			} else
				if(timeout > STICKY_TIMEOUT)
					timeout = STICKY_TIMEOUT;
		} else if(mode_brightness) {
			timeout = mode_brightness - current_time;
			debug("LOOPY: mode_brightness = %u -> timeout = %d",
					mode_brightness, timeout);
			if(timeout <= 0) {
				timeout = STICKY_TIMEOUT;
				mode_brightness = 0;
				osd_hide();
			} else
				if(timeout > STICKY_TIMEOUT)
					timeout = STICKY_TIMEOUT;
		} else timeout = STICKY_TIMEOUT;
		debug("LOOPY: timeout = %d");
#endif

		FD_ZERO(&rfd);
		FD_SET(input, &rfd);
		tv.tv_sec  = timeout / 1000;
		tv.tv_usec = (timeout - tv.tv_sec*1000) * 1000;
		result = select(input+1, &rfd, NULL, NULL, &tv);

		if(result > 0) {
			struct input_event ie;

			result = read(input, &ie, sizeof(struct input_event));
			if(result == sizeof(struct input_event))
				result = handle_input_event(&ie);
		}

		if(result < 0)
			keep_running = 0;

		XSync(display, False);
		while(keep_running && XPending(display)) {
			XEvent xe;

			XNextEvent(display, &xe);
			if(xe.type == KeyPress)
				keep_running = (handle_x11_event((XKeyEvent*)&xe) >= 0);

			XSync(display, False);
		}

		debug("LOOPY: fin'd");
	}

#ifdef ENABLE_XOSD
	osd_exit();
#endif
#ifdef ENABLE_WACOM
	wacom_exit();
#endif
	x11_exit();
 x_failed:
	brightness_exit();
 hal_failed:
	input_exit();
 input_failed:
	return 0;
}

// vim: foldenable foldmethod=marker foldmarker={{{,}}} foldlevel=0
