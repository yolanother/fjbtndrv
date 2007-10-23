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

typedef struct {
	KeySym sym;			// Xorg
	char *text;			// osd
} keymap_entry;

typedef struct {
	unsigned long keycode;		// linux keycode
	keymap_entry map[3];		// normal, FN, ALT
} keymap_t;

static struct {
	ScrollMode scrollmode;
	UserLock lock_rotate;
	keymap_t keymap[];
} settings = {
	.scrollmode = SM_ZAXIS,
	.lock_rotate = UL_UNLOCKED,

	.keymap = {
		{ KEY_SCROLLDOWN,
		  {
		    {},	// fixed: scroll down
		    { .sym=XF86XK_LaunchA,	.text="Launch A" },
		    { .sym=XF86XK_Launch1,	.text="Launch 1" },
		  }
		},
		{ KEY_SCROLLUP,
		  {
		    {},	// fixed: scroll up
		    { .sym=XF86XK_LaunchB,	.text="Launch B" },
		    { .sym=XF86XK_Launch2,	.text="Launch 2" },
		  }
		},
		{ KEY_PAGEDOWN,
		  {
		    {},	// do nothing
		    { .sym=XK_Undo,		.text="Undo" },
		    { .sym=XK_Begin,		.text=NULL },
		  }
		},
		{ KEY_PAGEUP,
		  {
		    {},	// do nothing
		    { .sym=XK_Redo,		.text="Redo" },
		    { .sym=XK_End,		.text=NULL },
		  }
		},
		{ KEY_DIRECTION,
		  {
		    {},	// fixed: rotate screen
		    { .sym=XF86XK_LaunchC,	.text="Launch C" },
		    { .sym=XF86XK_Launch3,	.text="Launch 3" },
		  }
		},
		{ KEY_FN,
		  {
		    {},	// fixed: modification
		    { .sym=XF86XK_LaunchD,	.text="Launch D" },
		    { .sym=XF86XK_Launch4,	.text="Launch 4" },
		  }
		},
		{ KEY_LEFTALT,
		  {
		    {},	// fixed: modification
		    { .sym=XF86XK_LaunchE,	.text="Launch E" },
		    { .sym=XF86XK_Launch5,	.text="Launch 5" },
		  }
		},
		{ KEY_MAIL,
		  {
		    { .sym=XF86XK_Mail,		.text="Mail"},
		    { .sym=XF86XK_WWW,		.text="WWW" },
		    { .sym=XF86XK_Launch1,	.text="Launch 1" },
		  }
		},
		{ KEY_ESC,
		  {
		    { .sym=XK_Escape,		.text=NULL},
		    { .sym=XF86XK_LaunchA,	.text="Launch A" },
		    { .sym=XF86XK_Launch3,	.text="Launch 3" },
		  }
		},
		{ KEY_DELETE,
		  {
		    { .sym=XK_Delete,		.text=NULL},
		    { .sym=XF86XK_LaunchB,	.text="Launch B" },
		    { .sym=XF86XK_Launch4,	.text="Launch 4" },
		  }
		},
		{ 0 }
	}
};
		    

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
		debug("check input device %s...", filename);

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
	debug("osd_new: osd(%p)", osd);
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

#define osd_info(format, a...) {			\
	xosd *osd = osd_new(1);				\
	xosd_display(osd, 0, XOSD_printf, format, ##a); \
	xosd_set_timeout(osd, 1);			\
}

#define osd_message(format, a...) {			\
	xosd *osd = osd_new(1);				\
	xosd_display(osd, 0, XOSD_printf, format, ##a); \
	xosd_set_timeout(osd, -1);			\
}

#define osd_slider(percent, format, a...) {	\
	xosd *osd = osd_new(2);				\
	xosd_display(osd, 0, XOSD_printf, format, ##a);	\
	xosd_display(osd, 1, XOSD_slider, percent);	\
	xosd_set_timeout(osd, -1);			\
}

#else
#  define osd_hide()		do {} while(0)
#  define osd_info(a...)	do {} while(0)
#  define osd_message(a...)	do {} while(0)
#  define osd_slider(a...)	do {} while(0)
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
		debug("%s", dlerror());
		wclib.hdnl = NULL;
		return -1;
	}

	debug("wacomcfg library ready");
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

Display* x11_init(void)
{
	display = XOpenDisplay(NULL);
	if(display) {
		Bool xtest, randr, dpms;
		int opcode, event, error;
#ifdef DEBUG
		int major, minor;
#endif

		xtest = XQueryExtension(display, "XTEST",
				&opcode, &event, &error);
		if(xtest) {
#ifdef DEBUG
			XTestQueryExtension(display,
					&event, &error,
					&major, &minor);
			debug("Found XTest %d.%d extension (%d, %d, %d)",
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
			debug("Found RandR %d.%d extension (%d, %d, %d)",
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
			debug("Found DPMS %d.%d extension (%d, %d, %d)",
					major, minor,
					opcode, event, error);
#endif
		} else
			fprintf(stderr, "No DPMS extension\n");

		if(xtest && randr && dpms) {
			return display;
		} else {
			XCloseDisplay(display);
			return NULL;
		}
	}

	fprintf(stderr, "Can't open display\n");
	return NULL;
}

void x11_exit(void)
{
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
	debug("fake keycode %d (keysym 0x%04x)", keycode, (unsigned)sym);

	if(keycode) {
		debug("fake key %d event", keycode);
		XTestFakeKeyEvent(display, keycode, True,  CurrentTime);
		XSync(display, False);
		XTestFakeKeyEvent(display, keycode, False, CurrentTime);
		XSync(display, False);
		return 0;
	}

	fprintf(stderr, "There is no keycode for %s, use xmodmap to define one\n", XKeysymToString(sym));
	return -1;
}

int fake_key_map(unsigned long keycode, int fn, int alt)
{
	int error;
	keymap_t *keymap = &(settings.keymap[0]);
	keymap_entry *key;

	while(keymap->keycode && keymap->keycode != keycode)
		keymap++;

	if(!keymap->keycode) {
		debug("no keymap for %lu\n", keycode);
		return -1;
	}

	debug("fake key: %lu (fn:%d alt:%d)", keycode, fn, alt);
	key = &(keymap->map[fn+(alt*2)]);

	error = fake_key(key->sym);
#ifdef ENABLE_XOSD
	if(!error && key->text)
		osd_message(1, key->text);
#endif

	return error;
}

int fake_button(unsigned int button)
{
	int steps = ZAXIS_SCROLL_STEPS;

	while(steps--) {
		debug("fake button %d event", button);
		XTestFakeButtonEvent(display, button, True,  CurrentTime);
		XSync(display, False);
		XTestFakeButtonEvent(display, button, False, CurrentTime);
		XSync(display, False);
	}

	return 0;
}
//}}}

//{{{ HAL stuff
//#include <dbus/dbus.h>
#include <hal/libhal.h>
static DBusConnection *dbus;
static DBusError dbus_error;
static LibHalContext *hal;
static char *FSCTabletDevice;

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

	debug("hal: %d input.switch device(s) found:", count);
	while(count-- && devices[count]) {
		char *type;
		debug("hal:   check device %s", devices[count]);

		type = libhal_device_get_property_string(hal,
				devices[count], "button.type",
				&dbus_error);
		if(dbus_error_is_set(&dbus_error)) {
			fprintf(stderr, "prop get failed - %s\n",
					dbus_error.message);
			goto err_input_next_device;
		}

		if(type && !strcmp("tablet_mode", type)) {
			debug("hal:   found: %s @ %s",
					type, devices[count]);

			FSCTabletDevice = strdup(devices[count]);
			break;
		}

		debug("hal:   check done, next?");
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

	tablet_mode = libhal_device_get_property_bool(hal, FSCTabletDevice,
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

int brightness_init()
{
	char **devices;
	int count;

	debug("brightness_init");

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

void brightness_exit()
{
}

int get_brightness(void)
{
	int level;
	DBusMessage *message, *reply;

	debug("get_brightness");

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

	debug("get done (%d)", level);
	return level;

 err_free_msg:
	debug("get brightness: error");
	dbus_message_unref(message);
	return -1;
}

void set_brightness(int level)
{
	DBusMessage *message;

	debug("set_brightness to %d", level);

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
		debug("append to message failed");
		goto err_free_msg;
	}

	dbus_connection_send(dbus, message, NULL);

 err_free_msg:
	dbus_message_unref(message);
	return;
}

void brightness_show()
{
#ifdef ENABLE_XOSD
	int current = get_brightness();

	debug("brightness: %d/%d (%d%%)",
			current, brightness_max,
			((current-1) * 100)/(brightness_max-1));

	osd_slider(((current-1) * 100)/(brightness_max-1),
			"%s", _("Brightness"));
#endif
}

void brightness_down ()
{
	int current = get_brightness();

	// XXX: workaround
	if(current == 0)
		current = 8;

	set_brightness(current-1);
	brightness_show();
}

void brightness_up ()
{
	int current = get_brightness();

	if(current < brightness_max)
		current++;

	set_brightness(current);
	brightness_show();
}
//}}}

//{{{ RC stuff
void scrollmode_info(void)
{
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
	debug("scrollmode: %d", settings.scrollmode);
}

void scrollmode_next(void)
{
	settings.scrollmode = (++settings.scrollmode % 3);
	scrollmode_info();
}

void scrollmode_prev(void)
{
	debug("SM: %d", sizeof(ScrollMode));
	settings.scrollmode = (settings.scrollmode? --settings.scrollmode : 2);
	scrollmode_info();
}


void toggle_lock_rotate(void)
{
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
}

void toggle_dpms(void)
{
	if(dpms_enabled()) {
		disable_dpms();
		osd_info(_("DPMS disabled"));
	} else {
		enable_dpms();
		osd_info(_("DPMS enabled"));
	}
}
//}}}


int main_loop()
{
	unsigned key_fn=0, key_alt=0, key_rep=0;
	time_t key_cfg=0, key_scr=0;
	struct input_event input_event;

	while(1) {
		if(read(input, &input_event, sizeof(input_event)) < 0)
			break;

		switch(input_event.type) {
		case EV_SYN:
			debug("---");
			break;

		case EV_MSC:
			debug("input: misc event %d with %d",
					input_event.code, input_event.value);
			break;

		case EV_KEY:
			debug("input: key %d %s", input_event.code,
					((input_event.value == 1)? "pressed" :
						((input_event.value == 2)? "autorepeat" :
							"released")));

			if(key_cfg+3 < input_event.time.tv_sec)
				key_cfg = 0;

			if(key_scr+3 < input_event.time.tv_sec)
				key_scr = 0;

			switch(input_event.code) {
			case KEY_SCROLLDOWN:
				if(!input_event.value)
					break;

				if(key_scr) {
					key_scr = input_event.time.tv_sec - 1;
					debug("brightness down");
					brightness_down();
					brightness_show(1);
					break;
				}

				if(key_cfg) {
					key_cfg = input_event.time.tv_sec - 1;
					scrollmode_next();
					break;
				}

				if(key_alt)
					break;

				if(key_fn)
					break;

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

			case KEY_SCROLLUP:
				if(!input_event.value)
					break;

				if(key_scr) {
					key_scr = input_event.time.tv_sec - 1;
					debug("brightness up");
					brightness_up();
					brightness_show(1);
					break;
				}

				if(key_cfg) {
					key_cfg = input_event.time.tv_sec - 1;
					scrollmode_prev();
					break;
				}

				if(key_alt)
					break;

				if(key_fn)
					break;

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

			case KEY_DIRECTION:
				if(key_scr) {
					if(input_event.value == 1)
						osd_info(_("off"));

					if(input_event.value == 0) {
						sleep(1);
						osd_hide();
						dpms_force_off();
					}
					break;
				}

				if(!input_event.value)
					break;

				if(key_cfg) {
					toggle_lock_rotate();
					break;
				}

				if(key_alt)
					break;

				if(key_fn)
					break;

				rotate_screen(-1);
				break;

			case KEY_FN:
				key_fn = input_event.value;

				if(input_event.value == 0) {
					if(!key_cfg && !key_scr)
						osd_hide();
					break;
				}

				if(input_event.value != 1)
					break;

				if(key_scr) {
					key_scr = 0;
					osd_hide();
					break;
				}

				if(key_cfg) {
					key_cfg = 0;
					osd_hide();
					break;
				}

				if(key_alt) {
					key_cfg = input_event.time.tv_sec;
					osd_message(_("configuration..."));
					break;
				}

				osd_message("Fn...");
				break;

			case KEY_LEFTALT:
				key_alt = input_event.value;

				if(input_event.value == 0) {
					if(!key_cfg && !key_scr)
						osd_hide();
					break;
				}

				if(input_event.value != 1)
					break;

				if(key_scr) {
					key_scr = 0;
					osd_hide();
					break;
				}

				if(key_cfg)
					break;

				if(key_fn) {
					key_scr = input_event.time.tv_sec;
					brightness_show(3);
					break;
				}

				osd_message("Alt...");
				break;

			case KEY_BRIGHTNESSUP:
				if(input_event.value)
					brightness_up();
				break;

			case KEY_BRIGHTNESSDOWN:
				if(input_event.value)
					brightness_down();
				break;

			case KEY_BRIGHTNESS_ZERO:
				switch(input_event.value) {
				case 0:
					key_rep = 0;
					break;
				case 1:
					dpms_force_off();
					break;
				case 2:
					if(!key_rep) {
						key_rep = 1;
						toggle_dpms();
					}
					break;
				}

			default:
				debug("unknown key, skipping");
			}

			debug("%lu.%lu:  fn:%u alt:%u cfg:%lu scr:%lu rep:%u",
					input_event.time.tv_sec, input_event.time.tv_usec,
					key_fn, key_alt, key_cfg, key_scr, key_rep);

			break;

		case EV_SW:
			switch(input_event.code) {
			case SW_TABLET_MODE:
				if(settings.lock_rotate == UL_UNLOCKED)
					rotate_screen(input_event.value);
				break;
			default:
				debug("unknown switch, skipping");
			}
			break;

		default:
			fprintf(stderr, "unsupported event type %d",
					input_event.type);
		}
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
#endif

	osd_info("%s %s %s", PACKAGE, VERSION, _("started"));
	debug("\n *** Please report bugs to " PACKAGE_BUGREPORT " ***\n");

	main_loop();

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
