/* FSC Buttons Daemon
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 */

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <linux/input.h>
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <X11/XF86keysym.h>
#include <X11/extensions/XTest.h>
#include <X11/extensions/Xrandr.h>

#ifdef ENABLE_WACOM
#  include <wacomcfg/wacomcfg.h>
#  include <Xwacom.h>
#endif

#ifdef ENABLE_XOSD
#  include <xosd.h>
#endif

#ifdef DEBUG
#  define debug(msg, a...)	fprintf(stderr, msg "\n", ##a)
#else
#  define debug(msg, a...)	/**/
#endif

#define die(msg)		{ fprintf(stderr, msg); exit(1); }

#define SCROLL_STEPS		1


typedef enum {
	SM_ZAXIS,
	SM_KEY_PAGE
} ScrollMode;

typedef enum {
	UL_LOCKED,
	UL_UNLOCKED
} UserLock;

typedef struct {
	KeySym sym;
	char *text;
} keymap_entry;

static struct {
	ScrollMode scrollmode;
	UserLock lock_rotate;
	keymap_entry keymap[10];
} settings = {
	.scrollmode = SM_ZAXIS,
	.lock_rotate = UL_UNLOCKED,

	.keymap = {
		/* FN + ... */
		{ .sym=XF86XK_LaunchA,	.text="Launch A" },		// ScrollDown
		{ .sym=XF86XK_LaunchB,	.text="Launch B" },		// ScrollUp
		{ .sym=XF86XK_LaunchC,	.text="Launch C" },		// Rotate
		{ .sym=XF86XK_LaunchD,	.text="Launch D" },		// FN
		{ .sym=XF86XK_LaunchE,	.text="Launch E" },		// Alt
		/* ALT + ... */
		{ .sym=XF86XK_Launch1,	.text="Launch 1" },		// ScrollDown
		{ .sym=XF86XK_Launch2,	.text="Launch 2" },		// ScrollUp
		{ .sym=XF86XK_Launch3,	.text="Launch 3" },		// Rotate
		{ .sym=XF86XK_Launch4,	.text="Launch 4" },		// FN
		{ .sym=XF86XK_Launch5,	.text="Launch 5" },		// Alt
	},
};


int find_and_open_input_device(void)
{
	int f, x;
	char filename[64];
	struct input_id iid;
	int rep[2];

	for(x = 0; x < 32; x++) {
		snprintf(filename, sizeof(filename), "/dev/input/event%d", x);
		debug("check input device %s...", filename);

		f = open(filename, O_RDONLY);
		if(f < 0)
			continue;

		if(ioctl(f, EVIOCGID, &iid) == 0) {
			if(iid.vendor == 0x1734 && iid.product == 0x0001)
				return f;
		}

		close(f);
	}

	return -1;
}

//{{{ OSD stuff
#ifdef ENABLE_XOSD
static xosd *osd = NULL;

void osd_exit();

void osd_init(Display *dpy)
{
	if(osd) {
		osd_exit();
		debug("reinitalize osd");
	} else
		debug("initalize osd");

	osd = xosd_create(2);
	if(!osd)
		die(xosd_error);

	xosd_set_pos(osd, XOSD_bottom);
	xosd_set_vertical_offset(osd, 50);
	xosd_set_align(osd, XOSD_center);
	xosd_set_horizontal_offset(osd, 0);

	xosd_set_font(osd, "-*-helvetica-bold-r-normal-*-*-400-*-*-*-*-*-*");
	xosd_set_colour(osd, "green");
	xosd_set_outline_offset(osd, 1);
	xosd_set_outline_colour(osd, "darkgreen");
	xosd_set_shadow_offset(osd, 2);
}

void osd_exit()
{
	if(osd) {
		xosd_destroy(osd);
		osd = NULL;
	}
}

void osd_hide()
{
	if(xosd_is_onscreen(osd))
		xosd_hide(osd);
}

void osd_message(const char *message, const unsigned timeout)
{
	osd_hide();
	xosd_set_timeout(osd, timeout);
	xosd_display(osd, 0, XOSD_string, message);
	xosd_display(osd, 1, XOSD_string, "");
}

void osd_slider(const char *message, const int slider)
{
	osd_hide();
	xosd_set_timeout(osd, 1);
	xosd_display(osd, 0, XOSD_string, message);
	xosd_display(osd, 1, XOSD_slider, slider);
}

#else
#define osd_init(a...)		/**/
#define osd_exit(a...)		/**/
#define osd_message(a...)	/**/
#define osd_slider(a...)	/**/
#define osd_hide(a...)		/**/
#endif
// }}}

//{{{ WACOM stuff
#ifdef ENABLE_WACOM
static WACOMCONFIG * wacom_config = NULL;

void wacom_init(Display *dpy)
{
	wacom_config = WacomConfigInit(dpy, NULL);
	if(!wacom_config)
		fprintf(stderr, "Can't open Wacom Device");
}

void wacom_exit()
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
#define wacom_init(a...)   /**/
#define wacom_rotate(a...) /**/
#define wacom_exit(a...)   /**/
#endif
//}}} WACOM stuff

int rotate_screen(Display *dpy, int mode)
{
	Window rwin;
	XRRScreenConfiguration *sc;
	Rotation rotation, current_rotation;
	SizeID size;
	int error = -1;

	if(settings.lock_rotate == UL_LOCKED)
		return 0;

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
#if XOSD_VERBOSE >= 3
	osd_init(dpy);
	osd_message((mode ? "Portrait" : "Landscape"), 1);
#endif
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
		return 1;

	keycode = XKeysymToKeycode(dpy, sym);
	debug("fake keycode %d (keysym 0x%08x)", keycode, sym);

	if(keycode)
		return XTestFakeKeyEvent(dpy, keycode, True,  CurrentTime) &&
		       XTestFakeKeyEvent(dpy, keycode, False, CurrentTime);

	fprintf(stderr, "There are no keycode for %s\n", XKeysymToString(sym));
	return 0;
}

int fake_key(Display *dpy, keymap_entry *key)
{

	Status status = _fake_key(dpy, key->sym);

#ifdef ENABLE_XOSD
#if XOSD_VERBOSE >= 2
	if(status && key->text)
		osd_message(key->text, 1);
#endif
#endif

	return (status ? 0 : -1);
}

int fake_button(Display *dpy, unsigned int button)
{
	Status status;
	int steps = SCROLL_STEPS;

	while(steps--) {
		debug("fake button %d event", button);
		status =
			XTestFakeButtonEvent(dpy, button, True,  CurrentTime) &&
			XTestFakeButtonEvent(dpy, button, False, CurrentTime);

		if(!status)
			break;
	}

	return (status ? 0 : -1);
}

int get_brightness(int fh)
{
	char buffer[1024], *a;
	int err;

	if(fh < 0)
		return -1;

	err = lseek(fh, 0, SEEK_SET);
	if(err < 0)
		return err;

	err = read(fh, buffer, sizeof(buffer));
	if(err < 0)
		return err;

	a = strstr(buffer, "current: ");
	if(!a)
		return -1;

	return atoi(a + 9);
}

int set_brightness(int fh, int level)
{
	char buffer[4] = "";
	int len;

	if(fh < 0)
		return -1;

	len = snprintf(buffer, sizeof(buffer), "%d", level);
	debug("set lcd brightness to %d (%s)", level, buffer);
	osd_slider("Brightness", (level * 100)/8);
	return write(fh, buffer, len);
}


void toggle_scrollmode()
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

void toggle_lock_rotate()
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


int main(int argc, char **argv)
{
	char *program;
	ssize_t sz;

	Display *display;

	int input_fd;
	struct input_event input_event;

	int brightness_fd;
	int brightness_max, brightness_min, brightness_current;

	time_t key_fn=0, key_alt=0, key_cfg=0;


	program = argv[0];

	input_fd = find_and_open_input_device();
	if(input_fd < 0) {
		fprintf(stderr, "Can't open input device");
		return -1;
	}
	debug("... found and open");

	brightness_fd = open("/proc/acpi/video/VGA/LCD/brightness", O_RDWR);
	if(brightness_fd < 0)
		fprintf(stderr, "Can't open LCD brightness file");

	seteuid(getuid());

	display = XOpenDisplay(NULL);
	if(display) {
		Bool ext;
		int opcode, event, error;

		ext = XQueryExtension(display, "XTEST",
				&opcode, &event, &error);
		if(ext)
			debug("Found XTest extension (%d, %d, %d)",
					opcode, event, error);
		else {
			fprintf(stderr, "No XTest extension\n");
			return -1;
		}

		ext = XQueryExtension(display, "RANDR",
				&opcode, &event, &error);
		if(ext)
			debug("Found RandR extension (%d, %d, %d)",
					opcode, event, error);
		else
			fprintf(stderr, "No RandR extension\n");
	} else {
		fprintf(stderr, "Can't open display");
		return -1;
	}

	XSynchronize(display, True);

	osd_init(display);
	wacom_init(display);

	osd_message("fscd started", 1);
	while(1) {
		sz = read(input_fd, &input_event, sizeof(input_event));
		if(sz < 0)
			break;

		debug("input event: type=%d code=%d value=%d",
				input_event.type, input_event.code, input_event.value);

		switch(input_event.type) {
		case EV_SYN:
			break;

		case EV_KEY:
			if( !input_event.value )
				break;

			switch(input_event.code) {
			case KEY_SCROLLDOWN:
				if(key_fn + 3 > input_event.time.tv_sec) {
					if(input_event.value == 1)
						fake_key(display, &settings.keymap[0]);
				} else if(key_alt + 3 > input_event.time.tv_sec) {
					if(input_event.value == 1)
						fake_key(display, &settings.keymap[5]);
				} else if(key_cfg + 3 > input_event.time.tv_sec) {
					if(input_event.value == 1)
						toggle_scrollmode();
				} else {
					switch(settings.scrollmode) {
						case SM_ZAXIS:
							fake_button(display, 5);
							break;
						case SM_KEY_PAGE:
							_fake_key(display, XK_Next);
							break;
					}
				}
				break;

			case KEY_SCROLLUP:
				if(key_fn + 3 > input_event.time.tv_sec) {
					if(input_event.value == 1)
						fake_key(display, &settings.keymap[1]);
				} else if(key_alt + 3 > input_event.time.tv_sec) {
					if(input_event.value == 1)
						fake_key(display, &settings.keymap[6]);
				} else if(key_cfg + 3 > input_event.time.tv_sec) {
					if(input_event.value == 1)
						toggle_scrollmode();
				} else {
					switch(settings.scrollmode) {
						case SM_ZAXIS:
							fake_button(display, 4);
							break;
						case SM_KEY_PAGE:
							_fake_key(display, XK_Prior);
							break;
					}
				}
				break;

			case KEY_DIRECTION:
				if(input_event.value == 1) {
					if(key_fn + 3 > input_event.time.tv_sec)
						fake_key(display, &settings.keymap[2]);
					else if(key_alt + 3 > input_event.time.tv_sec)
						fake_key(display, &settings.keymap[7]);
					else if(key_cfg + 3 > input_event.time.tv_sec)
						toggle_lock_rotate();
					else
						rotate_screen(display, -1);
				}
				break;

			case KEY_FN:
				if(input_event.value == 1) {
					if(key_fn + 3 > input_event.time.tv_sec)
						fake_key(display, &settings.keymap[3]);
					else if(key_alt + 3 > input_event.time.tv_sec)
						fake_key(display, &settings.keymap[8]);
					else {
						osd_message("fn...", 3);
						key_fn = input_event.time.tv_sec;
						continue;
					}
				}
				break;

			case KEY_MENU:
				if(input_event.value == 1) {
					if(key_fn + 3 > input_event.time.tv_sec) {
						osd_message("config...", 3);
						key_fn  = 0;
						key_alt = 0;
						key_cfg = input_event.time.tv_sec;
						continue;
						//fake_key(display, &settings.keymap[4]);
					} else if(key_alt + 3 > input_event.time.tv_sec)
						fake_key(display, &settings.keymap[9]);
					else {
						osd_message("alt...", 3);
						key_alt = input_event.time.tv_sec;
						continue;
					}
				}
				break;

			case KEY_BRIGHTNESSUP:
				brightness_current = get_brightness(brightness_fd);
				set_brightness(brightness_fd, ++brightness_current);
				break;

			case KEY_BRIGHTNESSDOWN:
				brightness_current = get_brightness(brightness_fd);

				// TODO: workaround
				if(brightness_current == 0)
					brightness_current = 8;

				set_brightness(brightness_fd, --brightness_current);
				break;

			default:
				fprintf(stderr, "unsupported key event %d", input_event.code);
			}

			key_fn = key_alt = key_cfg = 0;
			break;

		case EV_SW:
			switch(input_event.code) {
			case SW_TABLET_MODE:
				rotate_screen(display, input_event.value);
				break;
			default:
				fprintf(stderr, "unsupported switch event %d", input_event.code);
			}
			break;

		default:
			fprintf(stderr, "unsupported event type %d", input_event.type);
		}
	}

	osd_exit();
	wacom_exit();

	if(display)
		XCloseDisplay(display);

	close(brightness_fd);
	close(input_fd);

	return 0;
}

