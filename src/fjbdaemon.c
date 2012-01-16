/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * main.c
 * Copyright (C) Robert Gerlach 2011 <r.gerlach@users.sourceforge.net>
 * 
fjbtndrv is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * fjbtndrv is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <time.h>
#include <sys/time.h>

#include <X11/keysym.h>
#include <X11/XF86keysym.h>

#include "fjbtndrv.h"
#include "fjbtndrv-device.h"
#include "fjbtndrv-display.h"

#ifdef ENABLE_NLS
#  include <libintl.h>
#  include <locale.h>
#  define _(x) gettext(x)
#else
#  define _(x) (x)
#endif

typedef enum _Mode {
	NORMAL = 0,
	STICKY_FN,
	STICKY_ALT,
	CONFIGURE,
	BRIGHTNESS,
} Mode;

typedef enum _ScrollMode {
	SM_KEY_PAGE,
	SM_KEY_SPACE,
	SM_ZAXIS,
	SM_KEY_MAX
} ScrollMode;

static struct FjbtndrvConfig {
	ScrollMode scroll_mode;
	gboolean rotation_locked;
} config;

static struct FjbtndrvState {
	guint key_code;
	guint key_time;

	Mode  mode;
	guint mode_timeout;
} state;

static guint current_time;


static void
scroll_up(FjbtndrvDisplay *display)
{
	debug ("SCROLL_UP");

	switch (config.scroll_mode) {
	case SM_KEY_PAGE:
		fjbtndrv_display_fake_key(display, XK_Prior);
		break;
	case SM_KEY_SPACE:
		fjbtndrv_display_fake_key(display, XK_space);
		break;
	default:
	case SM_ZAXIS:
		fjbtndrv_display_fake_button(display, 4);
		break;
	}
}

static void
scroll_down(FjbtndrvDisplay *display)
{
	debug("SCROLL_DOWN");

	switch (config.scroll_mode) {
	case SM_KEY_PAGE:
		fjbtndrv_display_fake_key(display, XK_Next);
		break;
	case SM_KEY_SPACE:
		fjbtndrv_display_fake_key(display, XK_BackSpace);
		break;
	default:
	case SM_ZAXIS:
		fjbtndrv_display_fake_button(display, 5);
		break;
	}
}

static void
set_scrollmode(ScrollMode mode, FjbtndrvDisplay *display)
{
	gchar *n;

	switch(mode) {
		default:
		case SM_ZAXIS:
			n = _("Z-Axis");
			break;

		case SM_KEY_PAGE:
			n = _("Page Up/Down");
			break;

		case SM_KEY_SPACE:
			n = _("Space/Backspace");
			break;
	}

	fjbtndrv_display_show_info(display, "%s: %s", _("Scrolling"), n);
	config.scroll_mode = mode;

}

static void
scrollmode_next(FjbtndrvDisplay *display)
{
	debug("SCROLLMODE_NEXT");

	set_scrollmode( (config.scroll_mode + 1) % SM_KEY_MAX, display);
}

static void
scrollmode_prev(FjbtndrvDisplay *display)
{
	debug("SCROLLMODE_PREV");

	set_scrollmode( (config.scroll_mode ? config.scroll_mode : SM_KEY_MAX) - 1, display);
}

static void
brightness_up(FjbtndrvDisplay *display)
{
	guint c;

	debug("BRIGHTNESS_UP");

	c = fjbtndrv_display_backlight_up(display);
	fjbtndrv_display_show_slider(display, c, _("Brightness"), 2);
}

static void
brightness_down(FjbtndrvDisplay *display)
{
	guint c;

	debug("BRIGHTNESS_DOWN");

	c = fjbtndrv_display_backlight_down(display);
	fjbtndrv_display_show_slider(display, c, _("Brightness"), 2);
}

static void
dpms_force_off(FjbtndrvDisplay *display)
{
	debug("DPMS_FORCE_OFF");

	fjbtndrv_display_off(display);
}

static void
rotate_display(FjbtndrvDisplay *display)
{
	debug("ROTATE_DISPLAY");
}

static void
toggle_lock_rotate(FjbtndrvDisplay *display)
{
	debug("TOGGLE_LOCK_ROTATE");

	if (config.rotation_locked) {
		fjbtndrv_display_show_info(display, _("Rotation locked"));
		config.rotation_locked = FALSE;
	}
	else {
		fjbtndrv_display_show_info(display, _("Rotation unlocked"));
		config.rotation_locked = TRUE;
	}
}

static inline void
button_pressed(FjbtndrvDeviceButtonEvent *event)
{
	state.key_code = event->code;
	state.key_time = current_time;
}

// TODO: cleanup
static void
on_button_event(FjbtndrvDeviceButtonEvent *event, FjbtndrvDisplay *display)
{
	debug("on_button_event: code=%d value=%d mode=%d",
			event->code, event->value, state.mode);

	if ((event->value) && (state.mode_timeout < current_time)) {
		state.mode = NORMAL;
	}

	switch (event->code) {
	case 37:	/* FN */
		if (event->value) {
			button_pressed(event);
			return;
		}

		switch (state.mode) {
		case NORMAL:
			state.mode = STICKY_FN;
			state.mode_timeout = current_time + 1400;
			fjbtndrv_display_show_info(display, "FN...");
			break;
		case STICKY_FN:
			state.mode = NORMAL;
			fjbtndrv_display_hide_osd(display);
			break;
		case STICKY_ALT:
			if ((state.key_code == 37) && (state.key_time + 1000 < current_time)) {
				state.mode = CONFIGURE;
				state.mode_timeout = current_time + 3000;
				fjbtndrv_display_show_info(display, _("configuration..."));
			}
			else {
				state.mode = NORMAL;
				fjbtndrv_display_fake_key(display, XF86XK_Launch4);
				fjbtndrv_display_hide_osd(display);
			}
			break;
		case CONFIGURE:
			state.mode = NORMAL;
			fjbtndrv_display_hide_osd(display);
			break;
		case BRIGHTNESS:
			state.mode = NORMAL;
			fjbtndrv_display_hide_osd(display);
			break;
		}

		break;

	case 64:	/* ALT */
		if (event->value) {
			button_pressed(event);
			return;
		}

		switch (state.mode) {
		case NORMAL:
			state.mode = STICKY_ALT;
			state.mode_timeout = current_time + 1400;
			fjbtndrv_display_show_info(display, "ALT...");
			break;
		case STICKY_FN:
			state.mode = BRIGHTNESS;
			state.mode_timeout = current_time + 3000;
			fjbtndrv_display_show_slider(display, 
					fjbtndrv_display_backlight_get(display),
					_("Brightness"), 2);
			break;
		case STICKY_ALT:
			state.mode = NORMAL;
			fjbtndrv_display_hide_osd(display);
			break;
		case CONFIGURE:
			state.mode = NORMAL;
			fjbtndrv_display_hide_osd(display);
			break;
		case BRIGHTNESS:
			state.mode = NORMAL;
			fjbtndrv_display_hide_osd(display);
			break;
		}

		break;

	case 185:	/* scroll up */
		if (event->value) {
			button_pressed(event);
			return;
		}

		switch (state.mode) {
		case NORMAL:
			scroll_up(display);
			break;
		case STICKY_FN:
			state.mode = NORMAL;
			fjbtndrv_display_fake_key(display, XF86XK_LaunchB);
			fjbtndrv_display_hide_osd(display);
			break;
		case STICKY_ALT:
			state.mode = NORMAL;
			fjbtndrv_display_fake_key(display, XF86XK_Launch2);
			fjbtndrv_display_hide_osd(display);
			break;
		case CONFIGURE:
			scrollmode_prev(display);
			state.mode_timeout = current_time + 1000;
			break;
		case BRIGHTNESS:
			brightness_up(display);
			state.mode_timeout = current_time + 1000;
			break;
		}

		break;

	case 186:	/* scroll down */
		if (event->value) {
			button_pressed(event);
			return;
		}

		switch (state.mode) {
		case NORMAL:
			scroll_down(display);
			break;
		case STICKY_FN:
			state.mode = NORMAL;
			fjbtndrv_display_fake_key(display, XF86XK_LaunchA);
			fjbtndrv_display_hide_osd(display);
			break;
		case STICKY_ALT:
			state.mode = NORMAL;
			fjbtndrv_display_fake_key(display, XF86XK_Launch1);
			fjbtndrv_display_hide_osd(display);
			break;
		case CONFIGURE:
			scrollmode_next(display);
			state.mode_timeout = current_time + 1000;
			break;
		case BRIGHTNESS:
			brightness_down(display);
			state.mode_timeout = current_time + 1000;
			break;
		}

		break;

	case 161:	/* rotate */
		if (event->value) {
			button_pressed(event);
			return;
		}

		switch (state.mode) {
		case NORMAL:
			rotate_display(display);
			break;
		case STICKY_FN:
			state.mode = NORMAL;
			fjbtndrv_display_fake_key(display, XF86XK_LaunchC);
			fjbtndrv_display_hide_osd(display);
			break;
		case STICKY_ALT:
			state.mode = NORMAL;
			fjbtndrv_display_fake_key(display, XF86XK_Launch3);
			fjbtndrv_display_hide_osd(display);
			break;
		case CONFIGURE:
			toggle_lock_rotate(display);
			break;
		case BRIGHTNESS:
			dpms_force_off(display);
			break;
		}

		break;

	case 232:
		if (event->value)
			break;

		brightness_down(display);
		break;
	
	case 233:
		if (event->value)
			break;

		brightness_up(display);
		break;
	
	default:
		state.mode = NORMAL;
		fjbtndrv_display_hide_osd(display);
		fjbtndrv_display_fake_event(display,
				(FjbtndrvDeviceEvent*)event);
		break;
	}

	state.key_code = 0;
	state.key_time = 0;
}

static void
on_switch_event(FjbtndrvDeviceSwitchEvent *event, FjbtndrvDisplay *display)
{
	debug("on_switch_event: code=%d value=%d",
			event->code, event->value);
}

static void
on_event(FjbtndrvDeviceEvent *event, gpointer data)
{
	struct timeval tv;

	gettimeofday (&tv, NULL);
	current_time = (tv.tv_sec * 1000) + (tv.tv_usec / 1000);

	switch (event->type) {
	case BUTTON:
		on_button_event((FjbtndrvDeviceButtonEvent*)event, data);
		break;

	case SWITCH:
		on_switch_event((FjbtndrvDeviceSwitchEvent*)event, data);
		break;
	}
}

// TODO
static void
load_config(void)
{
	config.scroll_mode = SM_ZAXIS;
	config.rotation_locked = False;
}

int main()
{
	FjbtndrvDisplay *display;
	FjbtndrvDevice *device;
	GMainLoop *mainloop;
	//GError *error = NULL;

	g_type_init();


	debug(" * initialization");

	load_config();

	mainloop = g_main_loop_new(NULL, FALSE);


	display = fjbtndrv_display_new(NULL);
	if (!display) {
		g_error("Can't open display");
		goto out;
	}

	device = fjbtndrv_display_get_device(display);
	if (!device) {
		g_error("Can't open tablet device");
		goto out;
	}

	fjbtndrv_device_set_callback(device, on_event, display);


	debug(" * start");
	fjbtndrv_display_show_info(display, "%s %s %s", PACKAGE, VERSION, _("started"));

	g_main_loop_run(mainloop);


out:
	debug(" * shutdown");

	if (display)
		g_object_unref(display);

	return (0);
}

