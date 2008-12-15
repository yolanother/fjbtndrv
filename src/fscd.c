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

#ifdef HAVE_CONFIG_H
#  include "../config.h"
#endif

#include "fscd.h"
#include "wacom.h"
#include "gui.h"

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

#ifndef BRIGHTNESS_CONTROL
#  ifdef BRIGHTNESS_KEYS
#    undef BRIGHTNESS_KEYS
#  endif
#endif

static struct {
	ScrollMode scrollmode;
} settings = {
	.scrollmode = SM_KEY_PAGE,
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
	},

	{ .code = 0 }
};

static unsigned keep_running = 1;
static clock_t  current_time;
static int mode_configure, mode_brightness;

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

static void init_private_dir(void)
{
	char *path, *homedir;
	struct stat s;
	int error;

	homedir = getenv("HOME");
	if(!homedir)
		return;

	path = malloc(strlen(homedir) + strlen(PACKAGE) + 17);
	if(!path)
		return;

	sprintf(path, "%s/." PACKAGE, homedir);
	error = stat(path, &s);
	if(error) {
		error = mkdir(path, 0700);
		if(error) {
			fprintf(stderr, "can't create directory '%s'\n", path);
			free(path);
			return;
		}
	} else {
		if(!S_ISDIR(s.st_mode)) {
			fprintf(stderr, "'%s' exists, but is not a directory\n", path);
			free(path);
			return;
		}
	}

	strcat(path, "/lock.rotate");
	error = stat(path, &s);
	if(!error) {
		error = unlink(path);
		if(error) {
			fprintf(stderr, "failed to remove rotation lock.\n");
			free(path);
			return;
		}
	}
	
	free(path);
}


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
	//TODO: randr version check, >= 1.2
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
	XGrabKey(display, keymap[KEYMAP_SCROLLDOWN].code, AnyModifier, root,
			False, GrabModeAsync, GrabModeAsync);
	XGrabKey(display, keymap[KEYMAP_SCROLLUP].code, AnyModifier, root,
			False, GrabModeAsync, GrabModeAsync);
	XSync(display, False);
}

static void x11_ungrab_scrollkeys(void)
{
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

	DPMSInfo(display, &state, &on);
	if(!on)
		enable_dpms();

	XSync(display, True);

	DPMSForceLevel(display, DPMSModeOff);
	XSync(display, False);
}

static void rotate_screen(void)
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

	// TODO: make rotation steps configurable
	rotation = (current_rotation & 0x7) << 1;
	if(!rotation)
		rotation = RR_Rotate_0;

	rotation |= current_rotation & ~0xf;

	if(rotation != current_rotation) {
		int error;

		error = XRRSetScreenConfig(display, sc, rwin, size,
				rotation, CurrentTime);
		if(error)
			goto err;

#ifdef ENABLE_WACOM
		wacom_rotate(rotation);
#endif

		// TODO: needed???	
		screen_rotated();
	}

  err:
	XRRFreeScreenConfigInfo(sc);
}

static void fake_key(KeySym sym)
{
	KeyCode keycode;

	keycode = XKeysymToKeycode(display, sym);

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
		XTestFakeButtonEvent(display, button, True,  CurrentTime);
		XSync(display, False);
		XTestFakeButtonEvent(display, button, False, CurrentTime);
		XSync(display, False);
	}
}
//}}}

//{{{ Brightness stuff
#include <X11/Xatom.h>
#ifdef BRIGHTNESS_CONTROL
static Atom backlight;
static int brightness_output = -1;
static long brightness_offset, brightness_max;

static int brightness_init(void)
{
	int o;
	int err = -1;
	XRRPropertyInfo *info;

	backlight = XInternAtom(display, "BACKLIGHT", True);
	if(backlight == None)
		return -1;

	XRRScreenResources *sr = XRRGetScreenResources(display, XDefaultRootWindow(display));
	if(!sr)
		return -1;

	for(o = 0; (err != 0) && (o < sr->noutput); o++) {	
		info = XRRQueryOutputProperty(display, sr->outputs[o], backlight);
		if(info) {
			if(info->range && info->num_values == 2) {
				brightness_output = o;
				brightness_offset = info->values[0];
				brightness_max = info->values[1] - brightness_offset;
				debug("X11", "brightness: output=%d offset=%ld max=%ld",
					brightness_output, brightness_offset, brightness_max);
				err = 0;
			}
			XFree(info);
		}
	}

	XRRFreeScreenResources(sr);
	return err;
}

static void brightness_exit(void)
{
}

static long get_brightness(void)
{
	unsigned long   items, ba;
	unsigned char   *prop;
	Atom		type;
	int		format;
	long		value = -1;

	if(brightness_output < 0)
		return -1;

	XRRScreenResources *sr = XRRGetScreenResources(display, XDefaultRootWindow(display));
	if(!sr)
		return -1;
	
	int err = XRRGetOutputProperty(display, sr->outputs[brightness_output], backlight,
			0, 4, False, False, None, &type, &format,
			&items, &ba, &prop);

	XRRFreeScreenResources(sr);

	if(err == Success && prop) {
		if (type == XA_INTEGER || format == 32 || items == 1)
			value = *((long*)prop) - brightness_offset;

		XFree(prop);
	}

	debug("X11", "backlight value: %ld", value);
	return value;
}

static void set_brightness(long value)
{
	if(brightness_output < 0)
		return;

	XRRScreenResources *sr = XRRGetScreenResources(display, XDefaultRootWindow(display));
	if(!sr)
		return;

	value += brightness_offset;

	XRRChangeOutputProperty(display, sr->outputs[brightness_output], backlight,
			XA_INTEGER, 32, PropModeReplace,
			(unsigned char*) &value, 1);

	XRRFreeScreenResources(sr);
}


static void brightness_show()
{
	long current = get_brightness();
	gui_brightness_show(current < 0 ? 0 : current * 100 / brightness_max);
}

static void brightness_down(void)
{
	long current = get_brightness();
	if(current < 0)
		return;

	if(current > 0)
		current--;

	set_brightness(current);
	gui_brightness_show(current * 100 / brightness_max);
}

static void brightness_up(void)
{
	int current = get_brightness();
	if(current < 0)
		return;

	if(current < brightness_max)
		current++;

	set_brightness(current);
	gui_brightness_show(current * 100 / brightness_max);
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
	char *lockfile, *homedir;
	struct stat s;
	int error, h;

	homedir = getenv("HOME");
	if(!homedir)
		return;

	lockfile = malloc(strlen(homedir) + strlen(PACKAGE) + 17);
	if(!lockfile)
		return;

	sprintf(lockfile, "%s/." PACKAGE "/lock.rotation", homedir);

	error = stat(lockfile, &s);	
	if(error < 0) {
		h = open(lockfile, O_CREAT, 0600);
		if(h > 0) {
			close(h);
			gui_info(_("Rotation locked"));
		}
	} else {
		h = unlink(lockfile);
		if(h == 0)
			gui_info(_("Rotation unlocked"));
	}

	free(lockfile);
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

static void handle_x11_event(unsigned int keycode, unsigned int state, int pressed)
{
	debug("TRACE", "handle_x11_event: time=%lu keycode=%d, state=%d, action=%s [cfg=%d, bri=%d]",
			current_time, keycode, state, (pressed ? "pressed" : "released"),
			mode_configure, mode_brightness);

	if(keycode == keymap[KEYMAP_SCROLLDOWN].code) {
		if(pressed)
			;

		else if(mode_configure) {
			mode_configure = current_time + STICKY_TIMEOUT;
			scrollmode_prev();
		}

#ifdef BRIGHTNESS_CONTROL
		else if(mode_brightness) {
			mode_brightness = current_time + STICKY_TIMEOUT;
			brightness_down();
		}
#endif

		else if(settings.scrollmode == SM_ZAXIS)
			fake_button(5);

	} else if(keycode == keymap[KEYMAP_SCROLLUP].code) {
		if(pressed)
			;

		else if(mode_configure) {
			mode_configure = current_time + STICKY_TIMEOUT;
			scrollmode_next();
		}

#ifdef BRIGHTNESS_CONTROL
		else if(mode_brightness) {
			mode_brightness = current_time + STICKY_TIMEOUT;
			brightness_up();
		}
#endif

		else if(settings.scrollmode == SM_ZAXIS)
			fake_button(4);

	} else if(keycode == keymap[KEYMAP_ROTATEWINDOWS].code) {
		if(pressed)
			;

		else if(mode_configure) {
			mode_configure = current_time + STICKY_TIMEOUT;
			toggle_lock_rotate();
		}

#ifdef BRIGHTNESS_CONTROL
		else if(mode_brightness) {
			mode_brightness = 0;
			gui_hide();
			dpms_force_off();
		}
#endif

		else
			rotate_screen();

#ifdef BRIGHTNESS_CONTROL
	} else if(keycode == keymap[KEYMAP_BRIGHTNESSDOWN].code) {
		if(!pressed)
			brightness_down();

	} else if(keycode == keymap[KEYMAP_BRIGHTNESSUP].code) {
		if(!pressed)
			brightness_up();
#endif

	}
}

static void handle_xinput_event(unsigned int keycode, unsigned int state, int pressed)
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
				mode_brightness = 0;
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
				mode_configure = 0;
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

	}
}

int main(int argc, char **argv)
{
	Display *display;
	static struct timeval tv;
	fd_set in;
	int error, xfh;

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

	init_private_dir();

	display = x11_init();
	if(!display) {
		fprintf(stderr, "x11 initalisation failed\n");
		exit(1);
	}

	xfh = XConnectionNumber(display);

#ifdef BRIGHTNESS_CONTROL
	error = brightness_init();
	if(error) {
		fprintf(stderr, "brightness initalisation failed\n");
	}
#endif

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

	while(keep_running) {
		if(mode_brightness || mode_configure) {
			clock_t t = (mode_brightness + mode_configure) - current_time + 100;
			tv.tv_sec  = (t / 1000);
			tv.tv_usec = (t % 1000) * 1000;
			debug("MAIN", "block for %d.%d seconds...", tv.tv_sec, tv.tv_usec);
		} else
			tv.tv_sec = tv.tv_usec = 1000000;
			
		FD_ZERO(&in);
		FD_SET(xfh, &in);
		select(xfh+1, &in, NULL, NULL, &tv);

		gettimeofday(&tv, NULL);
		current_time = (tv.tv_sec * 1000) + (tv.tv_usec / 1000);

		while(keep_running && XPending(display)) {
			static XEvent xevent;

			XNextEvent(display, &xevent);

			if(xevent.type == KeyPress) {
				XKeyEvent *e = (XKeyEvent*) &xevent;
				handle_x11_event(e->keycode, e->state, 1);
			} else if(xevent.type == KeyRelease) {
				XKeyEvent *e = (XKeyEvent*) &xevent;
				handle_x11_event(e->keycode, e->state, 0);
			} else if(xevent.type == xi_keypress) {
				XDeviceKeyPressedEvent *e = (XDeviceKeyPressedEvent*) &xevent;
				handle_xinput_event(e->keycode, e->state, 1);
			} else if(xevent.type == xi_keyrelease) {
				XDeviceKeyReleasedEvent *e = (XDeviceKeyReleasedEvent*) &xevent;
				handle_xinput_event(e->keycode, e->state, 0);
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
	}

	gui_exit();
#ifdef ENABLE_WACOM
	wacom_exit();
#endif
#ifdef BRIGHTNESS_CONTROL
	brightness_exit();
#endif
	x11_exit();
	return 0;
}

