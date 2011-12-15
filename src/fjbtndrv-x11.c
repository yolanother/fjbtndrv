/*******************************************************************************
 * Copyright (C) 2011 Robert Gerlach <khnz@users.sourceforge.net>
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

#ifdef HAVE_CONFIG_H
#  include "../config.h"
#endif

#include <glib.h>

#include <stdarg.h>

#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <X11/XF86keysym.h>

#include <X11/extensions/XInput.h>
#include <X11/extensions/Xrandr.h>
#include <X11/extensions/XTest.h>
#include <X11/extensions/dpms.h>

#include "fjbtndrv.h"
#include "fjbtndrv-x11.h"

#define FJBTNDRV_X11_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), FJBTNDRV_TYPE_X11, FjbtndrvX11Private))

struct _FjbtndrvX11Private {
	Display *display;
};

G_DEFINE_TYPE (FjbtndrvX11, fjbtndrv_x11, G_TYPE_OBJECT)

static int
x11_error(Display *display, XErrorEvent *event)
{
	g_error("X11 error");
	// exit ?
	return 0;
}

static int
x11_io_error(Display *display)
{
	g_error("X11 IO error");
	// exit ?
	return 0;
}

// TODO: optional version check (XTestQueryExtension)
static inline gboolean
_find_extension(char **list, int list_num, const char *name)
{
	int i;

	for (i = 0; i < list_num; i++) {
		if (g_strcmp0(name, list[i]) == 0) {
			return TRUE;
		}
	}

	g_error("%s extension not installed", name);
	return FALSE;
}

static gboolean
check_extensions(Display *display)
{
	char **list;
	int num;

	list = XListExtensions(display, &num);
	if (!list) {
		g_error("XListExtensions failed");
		return FALSE;
	}

	gboolean xtest  = _find_extension(list, num, "XTEST");
	gboolean xinput = _find_extension(list, num, "XInputExtension");
	gboolean randr  = _find_extension(list, num, "RANDR");
	gboolean dpms   = _find_extension(list, num, "DPMS");

	XFreeExtensionList(list);

	return (xtest && xinput && randr && dpms);
}


static inline void
send_event(FjbtndrvX11 *self, key_event *event)
{
	FjbtndrvX11Private *priv = FJBTNDRV_X11_GET_PRIVATE(self);

	g_return_if_fail(FJBTNDRV_IS_X11(self));

	XTestFakeKeyEvent(priv->display,
			event->keycode,
			(event->value ? True : False),
			CurrentTime);

	XSync(priv->display, False);
}

static inline void
send_key(FjbtndrvX11 *self, KeySym sym)
{
	FjbtndrvX11Private *priv = FJBTNDRV_X11_GET_PRIVATE(self);
	KeyCode keycode;

	g_return_if_fail(FJBTNDRV_IS_X11(self));

	keycode = XKeysymToKeycode(priv->display, sym);
	if (!keycode) {
		g_error("No keycode for %s", XKeysymToString(sym));
		return;
	}

	XTestFakeKeyEvent(priv->display, keycode, True,  CurrentTime);
	XTestFakeKeyEvent(priv->display, keycode, False, CurrentTime);

	XSync(priv->display, False);
}

static inline void
send_button(FjbtndrvX11 *self, unsigned int button)
{
	FjbtndrvX11Private *priv = FJBTNDRV_X11_GET_PRIVATE(self);
	int repeat;
       
	g_return_if_fail(FJBTNDRV_IS_X11(self));

	repeat = (button > 3) ? 3 : 1;

	do {
		XTestFakeButtonEvent(priv->display, button, True,  CurrentTime);
		XTestFakeButtonEvent(priv->display, button, False, CurrentTime);
	} while(--repeat);

	XSync(priv->display, False);
}

void
fjbtndrv_x11_send_event(FjbtndrvX11 *self, FjbtndrvX11_EventType type, ...)
{
	va_list a;

	g_return_if_fail(FJBTNDRV_IS_X11(self));

	va_start(a, type);

	switch (type) {
	case TYPE_EVENT:
		send_event(self, va_arg(a, key_event*));
		break;

	case TYPE_KEY:
		send_key(self, va_arg(a, KeySym));
		break;

	case TYPE_MOUSE_BUTTON:
		send_button(self, va_arg(a, int));
		break;
	}

	va_end(a);
}


static void
fjbtndrv_x11_finalize (GObject *self)
{
	FjbtndrvX11 *x11 = FJBTNDRV_X11(self);
	FjbtndrvX11Private *priv = x11->priv;

	if (priv->display) {
		XSync(priv->display, True);
		XCloseDisplay(priv->display);
	}

	G_OBJECT_CLASS (fjbtndrv_x11_parent_class)->finalize(self);
}

static void
fjbtndrv_x11_class_init(FjbtndrvX11Class *klass)
{
	GObjectClass *object_klass = G_OBJECT_CLASS(klass);
	object_klass->finalize = fjbtndrv_x11_finalize;

	klass->send_event = fjbtndrv_x11_send_event;

	g_type_class_add_private(klass, sizeof(FjbtndrvX11Private));
}

static void
fjbtndrv_x11_init(FjbtndrvX11 *self)
{
	FjbtndrvX11Private *priv = FJBTNDRV_X11_GET_PRIVATE(self);

	self->priv = priv;

	priv->display = XOpenDisplay(NULL);
	if(!priv->display) {
		g_error("XOpenDisplay failed");
		return;
	}

	XSetErrorHandler(x11_error);
	XSetIOErrorHandler(x11_io_error);

	if (!check_extensions(priv->display)) {
		priv->display = NULL;
		return;
	}

	//fjbtndrv_x11_fix_keymap(self);
}

FjbtndrvX11 *
fjbtndrv_x11_new (void)
{
	FjbtndrvX11 *x;

	x = g_object_new(FJBTNDRV_TYPE_X11, NULL);
	return FJBTNDRV_X11(x);
}

