/* TabletPC Helper Daemon
 * Copyright (C) 2007 Robert Gerlach
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
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*******************************************************************************/

#define PROGRAM "fscd"
#define VERSION "0.30a"

/******************************************************************************/


#define ZAXIS_SCROLL_STEPS		1

#define XOSD_COLOR "green"
#define XOSD_OUTLINE_COLOR "darkgreen"
#undef  XOSD_FADEIN
#define XOSD_FLASH


/******************************************************************************/

#include <X11/XF86keysym.h>

typedef struct {
	unsigned sym;
	char *text;
} keymap_entry;

static keymap_entry keymap_t4010[] = {
		/* FN + ... */
		{ .sym=XF86XK_LaunchA,	.text="Launch A" },		// ScrollDown
		{ .sym=XF86XK_LaunchB,	.text="Launch B" },		// ScrollUp
		{ .sym=XF86XK_LaunchC,	.text="Launch C" },		// Rotate
		{ .sym=XF86XK_LaunchD,	.text="Launch D" },		// FN
		{ .sym=XF86XK_LaunchE,	.text="Launch E" },		// Alt	// config
		/* ALT + ... */
		{ .sym=XF86XK_Launch1,	.text="Launch 1" },		// ScrollDown
		{ .sym=XF86XK_Launch2,	.text="Launch 2" },		// ScrollUp
		{ .sym=XF86XK_Launch3,	.text="Launch 3" },		// Rotate
		{ .sym=XF86XK_Launch4,	.text="Launch 4" },		// FN	// alt-fn
		{ .sym=XF86XK_Launch5,	.text="Launch 5" },		// Alt
		/* FN-ALT + ... */
		{ .sym=XF86XK_Launch6,	.text="Launch 6" },		// ScrollDown
		{ .sym=XF86XK_Launch7,	.text="Launch 7" },		// ScrollUp
		{ .sym=XF86XK_Launch8,	.text="Launch 8" },		// Rotate
		{ .sym=XF86XK_Launch9,	.text="Launch 9" },		// FN
		{ .sym=XF86XK_Launch0,	.text="Launch 0" },		// Alt
		/* ALT-FN + ... = runtime configuration */
	};

/******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>

#ifdef DEBUG
#  define debug(msg, a...) \
	fprintf(stderr, PROGRAM ": " msg "\n", ##a)
#else
#  define debug(msg, a...) \
	NOTHING
#endif

#define info(msg, a...) \
	fprintf(stderr, PROGRAM ": "           msg "\n", ##a)
#define error(msg, a...) \
	fprintf(stderr, PROGRAM ": " "ERROR: " msg "\n", ##a)

#define NOTHING			do {} while (0)


typedef enum {
	SM_ZAXIS,
	SM_KEY_PAGE
} ScrollMode;

typedef enum {
	UL_LOCKED,
	UL_UNLOCKED
} UserLock;

static struct {
	ScrollMode scrollmode;
	UserLock lock_rotate;
	keymap_entry *keymap;
} settings = {
	.scrollmode = SM_ZAXIS,
	.lock_rotate = UL_UNLOCKED,
	.keymap = keymap_t4010
};


//{{{ OSD stuff
#ifdef ENABLE_XOSD
#include <X11/Xlib.h>
#include <xosd.h>

#if ( defined XOSD_FADEIN || defined XOSD_FLASH )
#include <pthread.h>
pthread_t effect_thread;
#endif

static xosd *_S_osd = NULL;
static int _S_was_visible = 0;

int osd_init(Display *dpy)
{
	if(_S_osd) {
		pthread_join(effect_thread, NULL);
		xosd_destroy(_S_osd);
		_S_osd = NULL;
	}
	return 0;
}

void osd_exit(void)
{
	osd_init(NULL);
}

xosd *osd_new(int lines)
{
	debug("osd_new: _S_osd(%p)", _S_osd);
	if(_S_osd) {
		_S_was_visible = xosd_is_onscreen(_S_osd);

		debug("_S_osd EXISTS WITH %d LINES", xosd_get_number_lines(_S_osd));
		if(xosd_get_number_lines(_S_osd) == lines)
			return _S_osd;

		pthread_join(effect_thread, NULL);
		xosd_destroy(_S_osd);
		_S_was_visible = 0;
	}

	if(lines <= 0)
		return _S_osd = NULL;

	_S_osd = xosd_create(lines);

	xosd_set_pos(_S_osd, XOSD_bottom);
	xosd_set_vertical_offset(_S_osd, 16);
	xosd_set_align(_S_osd, XOSD_center);
	xosd_set_horizontal_offset(_S_osd, 0);

	xosd_set_font(_S_osd, "-*-helvetica-bold-r-normal-*-*-400-*-*-*-*-*-*");
	xosd_set_outline_offset(_S_osd, 1);
	xosd_set_outline_colour(_S_osd, XOSD_OUTLINE_COLOR);
	xosd_set_shadow_offset(_S_osd, 3);

#ifdef XOSD_FADEIN
	if(!_S_was_visible) {
		xosd_set_colour(_S_osd, "#000000");
	} else
#endif
		xosd_set_colour(_S_osd, XOSD_COLOR);

	return _S_osd;
}
#  define osd_clear()	osd_new(0)

#ifdef XOSD_FLASH
void* osd_flash(void *a) {
	const duration = 40000;	/* µs */

	usleep(duration/8);
	xosd_set_outline_colour(_S_osd, "#ffffff");
	usleep(duration/8);
	xosd_set_colour(_S_osd, "#ffffff");
	usleep(duration/2);
	xosd_set_colour(_S_osd, XOSD_COLOR);
	usleep(duration/4);
	xosd_set_outline_colour(_S_osd, XOSD_OUTLINE_COLOR);
	return NULL;
}
#endif

#ifdef XOSD_FADEIN
void* osd_fadein(void *a)
{
	int i;
	char color[8];
	const duration	= 250000; /* µs */
	const steps	= 4;

	xosd_set_colour(_S_osd, "#000000");

	for(i = (256/steps)-1; i < 256; i += (256/steps)) {
		sprintf(color, "#%02x%02x%02x", (i/2), i, (i/3));
		xosd_set_colour(_S_osd, color);
		usleep(duration/(steps*2));
	}

#ifdef XOSD_FLASH
	osd_flash(NULL);
#else
	xosd_set_colour(_S_osd, XOSD_COLOR);
#endif

	return NULL;
}
#endif

inline void osd_effect(void) {
	if(!_S_was_visible)
#if defined XOSD_FADEIN
		pthread_create(&effect_thread, NULL, osd_fadein, NULL);
#elif defined XOSD_FLASH
		pthread_create(&effect_thread, NULL, osd_flash, NULL);
#else
		NOTHING;
#endif
}

void osd_message(const char *message, const unsigned timeout)
{
	xosd *osd = osd_new(1);

	xosd_display(osd, 0, XOSD_string, message);
	xosd_set_timeout(osd, timeout);

	osd_effect();
}

void osd_slider(const char *message, const int slider)
{
	xosd *osd = osd_new(2);

	xosd_display(osd, 0, XOSD_string, message);
	xosd_display(osd, 1, XOSD_slider, slider);
	xosd_set_timeout(osd, 1);

	osd_effect();
}

#else
#  define osd_message(a...)	NOTHING
#  define osd_slider(a...)	NOTHING
#  define osd_clear()		NOTHING
#endif
// }}}

//{{{ WACOM stuff
#ifdef ENABLE_WACOM
#include <wacomcfg/wacomcfg.h>
#include <Xwacom.h>

static WACOMCONFIG * wacom_config = NULL;


int wacom_init(Display *dpy)
{
	wacom_config = WacomConfigInit(dpy, NULL);
	if(!wacom_config) {
		error("Can't open Wacom Device\n");
		//return -1;	// don't fail if wacom failed
	}

	return 0;
}

void wacom_exit(void)
{
	if(wacom_config)
		WacomConfigFree(wacom_config);
}

void wacom_rotate(int mode)
{
	WACOMDEVICE * d;

	if(!wacom_config)
		return;

	d = WacomConfigOpenDevice(wacom_config, "stylus");
	if(!d)
		return;

	WacomConfigSetRawParam(d, XWACOM_PARAM_ROTATE,
			(mode ? XWACOM_VALUE_ROTATE_CW : XWACOM_VALUE_ROTATE_NONE),
			0);

	WacomConfigCloseDevice(d);
}

#else
#  define wacom_rotate(a...) NOTHING
#endif
//}}} WACOM stuff

//{{{ X11 stuff
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <X11/XF86keysym.h>
#include <X11/extensions/XTest.h>
#include <X11/extensions/Xrandr.h>
static Display *display;

Display* x11_init(void)
{
	display = XOpenDisplay(NULL);
	if(display) {
		Bool xtest, randr;
		int opcode, event, error;

		xtest = XQueryExtension(display, "XTEST",
				&opcode, &event, &error);
		if(xtest)
			debug("Found XTest extension (%d, %d, %d)",
				&opcode, &event, &error);
		else
			error("No XTest extension\n");


		randr = XQueryExtension(display, "RANDR",
				&opcode, &event, &error);
		if(randr)
			debug("Found RandR extension (%d, %d, %d)",
					opcode, event, error);
		else
			error("No RandR extension\n");


		if(xtest && randr) {
			return display;
		} else {
			XCloseDisplay(display);
			return NULL;
		}
	}

	error("Can't open display");
	return NULL;
}

void x11_exit(void)
{
	XCloseDisplay(display);
}

int rotate_screen(Display *dpy, int mode)
{
	Window rwin;
	XRRScreenConfiguration *sc;
	Rotation rotation, current_rotation;
	SizeID size;
	int error = -1;

	rwin = DefaultRootWindow(dpy);
	sc = XRRGetScreenInfo(dpy, rwin);
	if(!sc)
		goto err;

	rotation = XRRRotations(dpy, 0, &current_rotation);
	if(!(rotation & RR_Rotate_0) || !(rotation & RR_Rotate_270))
		goto err_sc;

	size = XRRConfigCurrentConfiguration(sc, &current_rotation);

	if(mode == -1)
		mode = current_rotation & RR_Rotate_0;

	rotation  = current_rotation & ~0xf;
	rotation |= (mode ? RR_Rotate_270 : RR_Rotate_0);

	if(rotation != current_rotation) {
		error = XRRSetScreenConfig(dpy, sc, rwin, size, rotation, CurrentTime);

		if(!error)
			wacom_rotate(mode);
	} else
		error = 0;

#ifdef ENABLE_XOSD
	osd_init(dpy);
#endif

err_sc:
	XRRFreeScreenConfigInfo(sc);
err:
	return error;
}

int _fake_key(Display *dpy, KeySym sym)
{
	KeyCode keycode;

	if(sym == 0)
		return -1;

	keycode = XKeysymToKeycode(dpy, sym);
	debug("fake keycode %d (keysym 0x%08x)", keycode, sym);

	if(keycode) {
/*
		debug("fake key %d event", keycode);
		XTestFakeKeyEvent(dpy, keycode, True,  CurrentTime);
		XSync(dpy, True);
		XTestFakeKeyEvent(dpy, keycode, False, CurrentTime);
		XSync(dpy, True);
*/
		return 0;
	}

	error("There are no keycode for %s\n", XKeysymToString(sym));
	return -1;
}

int fake_key(Display *dpy, keymap_entry *key)
{

	int error = _fake_key(dpy, key->sym);

#ifdef ENABLE_XOSD
	if(!error && key->text)
		osd_message(key->text, 1);
#endif

	return error;
}

int fake_button(Display *dpy, unsigned int button)
{
	int steps = ZAXIS_SCROLL_STEPS;

	while(steps--) {
		debug("fake button %d event", button);
		XTestFakeButtonEvent(dpy, button, True,  CurrentTime);
		XSync(dpy, True);
		XTestFakeButtonEvent(dpy, button, False, CurrentTime);
		XSync(dpy, True);
	}

	return 0;
}
//}}}

//{{{ DBUS stuff
#include <dbus/dbus.h>
static DBusConnection *dbus;
static DBusError dbus_error;

int event(const char *name);

#define IS_DBUS_ERROR \
	dbus_error_is_set(&dbus_error)
#define DBUS_ERROR(msg, a...) \
	error(msg " - %s\n", ##a, dbus_error.message)

int dbus_init(void)
{
	dbus_error_init(&dbus_error);

	dbus = dbus_bus_get(DBUS_BUS_SYSTEM, &dbus_error);
	if(!dbus) {
		error("Failed to connect to the D-BUS daemon: %s",
				dbus_error.message);
		dbus_error_free(&dbus_error);
		return -1;
	}

	return 0;
}

void dbus_exit(void)
{
}

int dbus_event(DBusMessageIter *iter)
{
	char *value;

	if(dbus_message_iter_get_arg_type(iter) == DBUS_TYPE_STRING) {
		dbus_message_iter_get_basic(iter, &value);
		if((strcmp("ButtonPressed", value) == 0) ||
		   (strcmp("ButtonRepeat",  value) == 0)) {
			debug("dbus: button pressed message");
			dbus_message_iter_next(iter);
			dbus_message_iter_get_basic(iter, &value);
			event(value);
		}
	}
}

int dbus_loop(void)
{
	DBusMessage *msg;
	DBusMessageIter msg_iter;

	dbus_bus_add_match(dbus, "type='signal',interface='org.freedesktop.Hal.Device'", &dbus_error);
	if (dbus_error_is_set(&dbus_error)) {
		error("Failed to connect to the D-BUS daemon: %s",
				dbus_error.message);
		dbus_error_free(&dbus_error);
		return -1;
	}

	while(1) {
		dbus_connection_read_write(dbus, -1);
		while(msg = dbus_connection_pop_message(dbus)) {
			debug("dbus: message received");
			if(dbus_message_is_signal(msg,
						"org.freedesktop.Hal.Device",
						"Condition"))
				if(dbus_message_iter_init(msg, &msg_iter))
					dbus_event(&msg_iter);

			dbus_message_unref(msg);
		}
	}
}

//}}}

//{{{ HAL stuff
#include <hal/libhal.h>
static LibHalContext *hal;
static char *LaptopPanelDevice;

#define HAL_ERROR(msg, a...) DBUS_ERROR(msg, ##a)
#define IF_HAL_ERROR(msg, code) \
	if(dbus_error_is_set(&dbus_error)) { HAL_ERROR(msg); code; }

int hal_init(void)
{
	int error;
	char **devices;
	int count;
	char *str;

	hal = libhal_ctx_new();
	if(!hal) {
		error("new hal ctx failed\n");
		return -1;
	}


	libhal_ctx_init(hal, &dbus_error);
	IF_HAL_ERROR("init hal ctx failed",
			goto err_ctx_free);

	libhal_ctx_set_dbus_connection(hal, dbus);

	devices = libhal_find_device_by_capability(hal,
			"laptop_panel", &count, &dbus_error);
	IF_HAL_ERROR("find_device_by_capability",
			goto err_ctx_free);

	if((devices == NULL) || (count < 0)) {
		HAL_ERROR("find failed");
		goto err_ctx_free;
	}

	if(count == 0) {
		HAL_ERROR("no devices found");
		goto err_ctx_free;
	}

	debug("hal: %d laptop_panel device(s) found, using %s",
			count, devices[0]);

	LaptopPanelDevice = devices[0];

	return 0;

err_ctx_free:
	libhal_ctx_free(hal);
	return -1;
}

int hal_exit(void)
{
		libhal_ctx_free(hal);
}

int get_brightness(void)
{
	int ok, level;
	DBusMessage *message, *reply;

	debug("get_brightness");

	message = dbus_message_new_method_call(
			"org.freedesktop.Hal",
			LaptopPanelDevice,
			"org.freedesktop.Hal.Device.LaptopPanel",
			"GetBrightness");

	reply = dbus_connection_send_with_reply_and_block(dbus,
			message, -1, &dbus_error);
	IF_HAL_ERROR("send get brightness message",
		goto err_free_msg);
	dbus_message_unref(message);

	if( !dbus_message_get_args(reply, NULL,
			DBUS_TYPE_UINT32, &level,
			DBUS_TYPE_INVALID))
		goto err_free_msg;

	dbus_message_unref(reply);
	debug("get done");
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

	message = dbus_message_new_method_call(
			"org.freedesktop.Hal",
			LaptopPanelDevice,
			"org.freedesktop.Hal.Device.LaptopPanel",
			"SetBrightness");
	if(!message) {
		debug("new method call failed");
		return;
	}

	if( !dbus_message_append_args(message,
			DBUS_TYPE_INT32, &level,
			DBUS_TYPE_INVALID)) {
		debug("append to message failed");
		goto err_free_msg;
	}

	dbus_connection_send(dbus, message, NULL);

	debug("set done");

	osd_slider("Brightness", (100/8)*level);

err_free_msg:
	dbus_message_unref(message);
	return;
}

//}}}


void toggle_scrollmode(void)
{
	switch(settings.scrollmode) {
		case SM_ZAXIS:
			settings.scrollmode = SM_KEY_PAGE;
			osd_message("Scroll: Page Up/Down (FakeKeys)", 1);
			break;
		case SM_KEY_PAGE:
			settings.scrollmode = SM_ZAXIS;
			osd_message("Scroll: Z-Axis", 1);
			break;
	}
}

void toggle_lock_rotate(void)
{
	switch(settings.lock_rotate) {
		case UL_UNLOCKED:
			settings.lock_rotate = UL_LOCKED;
			osd_message("Rotation locked", 1);
			break;
		case UL_LOCKED:
			settings.lock_rotate = UL_UNLOCKED;
			osd_message("Rotation unlocked", 1);
			break;
	}
}

int event(const char *name)
{
	static time_t key_fn=0, key_alt=0, key_both=0, key_cfg=0;
	time_t current_time = time(0);

	/* Macros:
	 *   FOR(event) DO(normal)
	 *   FOR(event) DO_MOD(normal, mod-fn, mod-alt, mod-both, mod-cfg)
	 *
	 *   event:	dbus event name (see addon-{keyboard,input}.c)
	 *   normal:	code without any modifierer
	 *   mod-fn:	code with fn modifierer
	 *   mod-alt:	code with alt modifierer
	 *   mod-fnalt:	code with fn+alt modifierer (alt->fn->key)
	 *   mod-altfn:	code for runtime configuration (fn->alt->key)
{{{	 */
	#define FOR(evname)						\
		if(strcmp(evname, name) == 0)

	#define DO(__code)						\
		{							\
	       		__code;						\
			return key_fn=key_alt=key_both=key_cfg=0;	\
		}

	#define DO_MOD(__normal, __fn, __alt, __both, __cfg)		\
		{							\
			if(key_fn + 3 > current_time) {			\
				__fn;	 				\
				return key_fn = 0;			\
			} else if(key_alt + 3 > current_time) {		\
				__alt;					\
				return key_alt = 0;			\
			} else if(key_both + 3 > current_time) {	\
				__both;					\
				return key_both = 0;			\
			} else if(key_cfg + 3 > current_time) {		\
				osd_clear();				\
				__cfg;					\
				return key_cfg = 0;			\
			} else {					\
				__normal;				\
				return 0;				\
			}						\
		}
//}}}

#ifdef DEBUG
	#define DEBUG_STATE { \
		debug("EVENT: key_fn: %d", key_fn); \
		debug("EVENT: key_alt: %d", key_alt); \
		debug("EVENT: key_both: %d", key_both); \
		debug("EVENT: key_cfg: %d", key_cfg); \
		debug("-------------------"); \
	}
	DEBUG_STATE;
#else
	#define DEBUG_STATE NOTHING
#endif

	FOR("brightness-down") DO( set_brightness(get_brightness()-1) );
	FOR("brightness-up")   DO( set_brightness(get_brightness()+1) );

	FOR("scroll-down") DO_MOD(
		switch(settings.scrollmode) {
			case SM_ZAXIS:
				fake_button(display, 5);
				break;
			case SM_KEY_PAGE:
				_fake_key(display, XK_Next);
				break;
		},
		fake_key(display, &settings.keymap[0]),
		fake_key(display, &settings.keymap[5]),
		fake_key(display, &settings.keymap[10]),
		toggle_scrollmode());

	FOR("scroll-up") DO_MOD(
		switch(settings.scrollmode) {
			case SM_ZAXIS:
				fake_button(display, 4);
				break;
			case SM_KEY_PAGE:
				_fake_key(display, XK_Prior);
				break;
		},
		fake_key(display, &settings.keymap[1]),
		fake_key(display, &settings.keymap[6]),
		fake_key(display, &settings.keymap[11]),
		toggle_scrollmode());

	FOR("direction") DO_MOD(
		rotate_screen(display, -1),
		fake_key(display, &settings.keymap[2]),
		fake_key(display, &settings.keymap[7]),
		fake_key(display, &settings.keymap[12]),
		toggle_lock_rotate());

	FOR("fn") DO_MOD(
		{ osd_message("fn", 3);
		  key_fn = current_time;
		  DEBUG_STATE;
		  return 0; },
		fake_key(display, &settings.keymap[3]),
		{ osd_message("alt+fn...", 3);		 // alt+fn -> both
		  key_alt = 0;
		  key_both = current_time;
		  DEBUG_STATE;
		  return 0; },
		fake_key(display, &settings.keymap[13]),
		osd_clear());

	FOR("menu") DO_MOD(
		{ osd_message("alt...", 3);
		  key_alt = current_time;
		  DEBUG_STATE;
		  return 0; },
		{ osd_message("config...", 3);		 // fn+alt -> config
		  key_fn = key_alt = key_both = 0;
		  key_cfg = current_time;
		  DEBUG_STATE;
		  return 0; },
		fake_key(display, &settings.keymap[9]),
		fake_key(display, &settings.keymap[14]),
		osd_clear());

	FOR("tablet_mode") DO (
		if(settings.lock_rotate == UL_UNLOCKED) {
			int state = 1;	// TODO: ask hal for state
			rotate_screen(display, state);
		}
	);

	error("unsupported event - %s\n", name);
	return -1;
}


int main (int argc, char **argv)
{
	int error;

	Display *display = x11_init();
	if(!display) {
		error("x initalisation failed");
		goto x_failed;
	}

	error = dbus_init();
	if(error) {
		error("dbus initalisation failed");
		goto dbus_failed;
	}

	error = hal_init();
	if(error) {
		error("hal initalisation failed");
		goto hal_failed;
	}

#ifdef ENABLE_WACOM
	error = wacom_init(display);
	if(error) {
		error("wacom initalisation failed");
		goto wacom_failed;
	}
#endif

#ifdef ENABLE_XOSD
	error = osd_init(display);
	if(error) {
		error("osd initalisation failed");
	}
#endif

	osd_message(PROGRAM " " VERSION " started", 1);

	dbus_loop();

#ifdef ENABLE_XOSD
	osd_exit();
#endif
#ifdef ENABLE_WACOM
	wacom_exit();
#endif
	x11_exit();
 wacom_failed:
	hal_exit();
 hal_failed:
	dbus_exit();
 dbus_failed:
 x_failed:
	return 0;
}


// vim: foldenable foldmethod=marker foldmarker={{{,}}} foldlevel=0
