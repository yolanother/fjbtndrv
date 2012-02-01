/*
 * Copyright (C) 2012 Robert Gerlach <khnz@users.sourceforge.net>
 * 
 * fjbtndrv is free software: you can redistribute it and/or modify it
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

#include <stdlib.h>  // exit() only

#include <glib.h>
#include <glib/gutils.h>

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/extensions/XTest.h>
#include <X11/extensions/Xrandr.h>
#include <X11/extensions/dpms.h>

#include "fjbtndrv.h"
#include "fjbtndrv-display.h"
#include "fjbtndrv-backlight.h"
#include "fjbtndrv-osd.h"


#define FJBTNDRV_DISPLAY_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), FJBTNDRV_TYPE_DISPLAY, FjbtndrvDisplayPrivate))

G_DEFINE_TYPE (FjbtndrvDisplay, fjbtndrv_display, G_TYPE_OBJECT);

struct _FjbtndrvDisplayPrivate {
	Display *display;

	FjbtndrvDevice *device;
	FjbtndrvBacklight *backlight;
	FjbtndrvOSD *osd;
};

/*
static int
on_display_error(Display *display, XErrorEvent *event)
{
	debug("x11_error");
	exit(0);
}

static int
on_display_io_error(Display *display)
{
	debug("x11_io_error");
	exit(0);
}
*/

FjbtndrvDevice*
fjbtndrv_display_get_device(FjbtndrvDisplay *this)
{
	FjbtndrvDisplayPrivate *priv = FJBTNDRV_DISPLAY_GET_PRIVATE(this);

	return priv->device;
}

FjbtndrvBacklight*
fjbtndrv_display_get_backlight(FjbtndrvDisplay *this)
{
	FjbtndrvDisplayPrivate *priv = FJBTNDRV_DISPLAY_GET_PRIVATE(this);

	return priv->backlight;
}

FjbtndrvOSD*
fjbtndrv_display_get_osd(FjbtndrvDisplay *this)
{
	FjbtndrvDisplayPrivate *priv = FJBTNDRV_DISPLAY_GET_PRIVATE(this);

	return priv->osd;
}

guint
fjbtndrv_display_backlight_get(FjbtndrvDisplay *this)
{
	FjbtndrvDisplayPrivate *priv = FJBTNDRV_DISPLAY_GET_PRIVATE(this);

	g_return_val_if_fail(priv->backlight, 0);

	return fjbtndrv_backlight_get(priv->backlight);
}

guint
fjbtndrv_display_backlight_up(FjbtndrvDisplay *this)
{
	FjbtndrvDisplayPrivate *priv = FJBTNDRV_DISPLAY_GET_PRIVATE(this);

	g_return_val_if_fail(priv->backlight, 0);

	return fjbtndrv_backlight_up(priv->backlight);
}

guint
fjbtndrv_display_backlight_down(FjbtndrvDisplay *this)
{
	FjbtndrvDisplayPrivate *priv = FJBTNDRV_DISPLAY_GET_PRIVATE(this);

	g_return_val_if_fail(priv->backlight, 0);

	return fjbtndrv_backlight_down(priv->backlight);
}

void
fjbtndrv_display_show_info(FjbtndrvDisplay *this, gchar *format, ...)
{
	FjbtndrvDisplayPrivate *priv = FJBTNDRV_DISPLAY_GET_PRIVATE(this);
	char buffer[256];
	va_list a;

	va_start(a, format);
	g_vsnprintf(buffer, 255, format, a);
	va_end(a);

	fjbtndrv_osd_info(priv->osd, buffer);
}

void
fjbtndrv_display_show_percentage(FjbtndrvDisplay *this, guint percent, gchar *title, guint timeout)
{
	FjbtndrvDisplayPrivate *priv = FJBTNDRV_DISPLAY_GET_PRIVATE(this);

	fjbtndrv_osd_percentage(priv->osd, percent, title, timeout);
}

void
fjbtndrv_display_show_slider(FjbtndrvDisplay *this, guint percent, gchar *title, guint timeout)
{
	FjbtndrvDisplayPrivate *priv = FJBTNDRV_DISPLAY_GET_PRIVATE(this);

	fjbtndrv_osd_slider(priv->osd, percent, title, timeout);
}

void
fjbtndrv_display_hide_osd(FjbtndrvDisplay *this)
{
	FjbtndrvDisplayPrivate *priv = FJBTNDRV_DISPLAY_GET_PRIVATE(this);

	fjbtndrv_osd_hide(priv->osd);
}

void
fjbtndrv_display_fake_key(FjbtndrvDisplay *this, KeySym sym)
{
	FjbtndrvDisplayPrivate *priv = FJBTNDRV_DISPLAY_GET_PRIVATE(this);
	Display *display = priv->display;

	KeyCode keycode = XKeysymToKeycode(display, sym);
	if (!keycode) {
		g_warning("No keycode for %s", XKeysymToString(sym));
		return;
	}

	debug("fjbtndrv_display_fake_key: sym=0x%08lx code=%d key=%s",
			sym, keycode, XKeysymToString(keycode));

	XTestFakeKeyEvent(display, keycode, True,  CurrentTime);
	XSync(display, False);

	XTestFakeKeyEvent(display, keycode, False, CurrentTime);
	XSync(display, False);

}

void
fjbtndrv_display_fake_button(FjbtndrvDisplay *this, guint button)
{
	FjbtndrvDisplayPrivate *priv = FJBTNDRV_DISPLAY_GET_PRIVATE(this);
	Display *display = priv->display;

	debug("fjbtndrv_display_fake_button: button=%d",
			button);

	gint steps = (button > 3) ? 3 : 1;
	while(steps--) {
		XTestFakeButtonEvent(display, button, True,  CurrentTime);
		XSync(display, False);

		XTestFakeButtonEvent(display, button, False, CurrentTime);
		XSync(display, False);
	}
}

void
fjbtndrv_display_fake_event(FjbtndrvDisplay *this, FjbtndrvDeviceEvent *event)
{
	FjbtndrvDisplayPrivate *priv = FJBTNDRV_DISPLAY_GET_PRIVATE(this);
	Display *display = priv->display;

	debug("fjbtndrv_display_fake_event: key=%d value=%d",
			event->code, event->value);

	XTestFakeKeyEvent(display,
			event->code, (event->value ? True : False),
			CurrentTime);

	XSync(display, False);
}

void
fjbtndrv_display_off(FjbtndrvDisplay *this)
{
	FjbtndrvDisplayPrivate *priv = FJBTNDRV_DISPLAY_GET_PRIVATE(this);
	Display *display = priv->display;
	CARD16 state;
	BOOL on;

	DPMSInfo(display, &state, &on);
	if(!on)
		DPMSEnable(display);

	XSync(display, True);

	DPMSForceLevel(display, DPMSModeOff);
	XSync(display, False);
}

static void
fjbtndrv_display_init (FjbtndrvDisplay *this)
{
}

static void
fjbtndrv_display_finalize (GObject *object)
{
	FjbtndrvDisplay *this = (FjbtndrvDisplay*) object;
	FjbtndrvDisplayPrivate *priv = FJBTNDRV_DISPLAY_GET_PRIVATE(this);

	g_object_unref(priv->device);
	g_object_unref(priv->backlight);
	XCloseDisplay(priv->display);

	G_OBJECT_CLASS (fjbtndrv_display_parent_class)->finalize (object);
}

static void
fjbtndrv_display_class_init (FjbtndrvDisplayClass *klass)
{
	GObjectClass* object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = fjbtndrv_display_finalize;

	g_type_class_add_private(klass, sizeof(FjbtndrvDisplayPrivate));
}

FjbtndrvDisplay*
fjbtndrv_display_new (gchar *display_name)
{
	FjbtndrvDisplay *this;
	FjbtndrvDisplayPrivate *priv;
	Display *display;

	this = g_object_new(FJBTNDRV_TYPE_DISPLAY, NULL);
	priv = FJBTNDRV_DISPLAY_GET_PRIVATE(this);

	display = XOpenDisplay(display_name);
	if (!display)
		return NULL;

	//XSetErrorHandler(on_display_error);
	//XSetIOErrorHandler(on_display_io_error);

	priv->display = display;
	priv->device = fjbtndrv_device_new(display);
	priv->backlight = fjbtndrv_backlight_new(display);
	priv->osd = fjbtndrv_osd_new(display);

	return this;
}

