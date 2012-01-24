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

#include <unistd.h> // sleep()

#include <glib.h>

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/extensions/Xrandr.h>

#include "fjbtndrv.h"
#include "fjbtndrv-backlight.h"


#define FJBTNDRV_BACKLIGHT_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), FJBTNDRV_TYPE_BACKLIGHT, FjbtndrvBacklightPrivate))

G_DEFINE_TYPE (FjbtndrvBacklight, fjbtndrv_backlight, G_TYPE_OBJECT);

typedef struct _Output Output;
typedef struct _Backlight Backlight;

struct _Output {
	RROutput xid;
	XRROutputInfo *info;
};

struct _BacklightInfo {
	Atom xid;
	guint min, max, cur;
};


struct _FjbtndrvBacklightPrivate {
	Display *display;

//	XRRScreenResources *rsr;

//	struct _Output output;
	struct _BacklightInfo backlight;
};

static gboolean
randr_ok(Display *display)
{
	Status status;
	int major, minor;

	status = XRRQueryVersion(display, &major, &minor);

	// FIXME:
	if (!status) {
		// TODO: g_set_error?
		g_warning("RandR extension missing\n");
		return FALSE;
	}

	if ((major < 1) || ((major == 1) && (minor < 2))) {
		// TODO: g_set_error?
		g_warning("RandR version 1.2 required\n");
		return FALSE;
	}

	debug("RandR version %d.%d installed.", major, minor);

	return TRUE;
}

static RROutput
_get_lvds(Display *display, XRROutputInfo **output_info)
{
	XRRScreenResources *rsr;
	RROutput output;
	Window rootwin;
	XID o;

	output = None;
	*output_info = NULL;

	rootwin = RootWindow(display, 0);

	rsr = XRRGetScreenResources(display, rootwin);
	if(!rsr) return output;

	for(o = 0; o < rsr->noutput; o++) {
		XRROutputInfo *info = XRRGetOutputInfo(display, rsr, rsr->outputs[o]);
		if (info) {
			if (g_strcmp0("LVDS1", info->name) == 0) {
				output = rsr->outputs[o];
				*output_info = info;
				break;
			}
			XRRFreeOutputInfo(info);
		}
	}

	XRRFreeScreenResources(rsr);

	return output;
}

static RROutput
get_lvds(Display *display, XRROutputInfo **output_info)
{
	static RROutput output;
	static XRROutputInfo *info;

	if (!output) {
		output = _get_lvds(display, &info);
	}

	*output_info = info;
	return output;
}

static void
put_lvds(XRROutputInfo *output_info)
{
	//XRRFreeOutputInfo(output_info);
}

static Atom
init_backlight(Display *display, RROutput output, gchar *name, struct _BacklightInfo *backlight)
{
	Atom xid;
	XRRPropertyInfo *pinfo;

	xid = XInternAtom(display, name, False);
	if (xid == None)
		return None;

	pinfo = XRRQueryOutputProperty(display, output, xid);
	if (!pinfo)
		return None;

	if ((!pinfo->range) || (pinfo->num_values != 2)) {
		XFree(pinfo);
		return None;
	}

	backlight->min = pinfo->values[0];
	backlight->max = pinfo->values[1];

	XFree(pinfo);

	return xid;
}

static guint
get_backlight_level(Display *display, RROutput output, struct _BacklightInfo *backlight)
{
	guint value = 0;
	Atom type;
	int format;
	unsigned long nitems, after;
	unsigned char *prop_data;
	int status;

	status = XRRGetOutputProperty(display, output, backlight->xid,
			0, 4, False, False, None,
			&type, &format, &nitems, &after, &prop_data);

	if (status == Success) {
		if (type == XA_INTEGER || format == 32 || nitems == 1)
			value = *((unsigned int*) prop_data);
	}

	return value;
}

static void
set_backlight_level(Display *display, RROutput output, struct _BacklightInfo *backlight, guint value)
{
	XRRChangeOutputProperty(display, output, backlight->xid,
			XA_INTEGER, 32, PropModeReplace,
			(unsigned char*) &value, 1);
}

static guint
value_percent(guint value, struct _BacklightInfo *backlight)
{
	return 100 * (value - backlight->min) / (backlight->max - backlight->min);
}

static guint
percent_value(guint percent, struct _BacklightInfo *backlight)
{
	return backlight->min + (percent * (backlight->max - backlight->min) + 99) / 100;
}

guint
fjbtndrv_backlight_get (FjbtndrvBacklight *this)
{
	FjbtndrvBacklightPrivate *priv = FJBTNDRV_BACKLIGHT_GET_PRIVATE(this);
	Display *display = priv->display;
	RROutput output;
	XRROutputInfo *output_info;
	guint value = 0;

	g_assert(FJBTNDRV_IS_BACKLIGHT(this));

	g_return_val_if_fail (priv->backlight.xid, 0);

	output = get_lvds(display, &output_info);
	g_return_val_if_fail (output, 0);

	value = get_backlight_level(display, output, &priv->backlight);

	put_lvds(output_info);

	return value_percent(value, &priv->backlight);
}

guint
fjbtndrv_backlight_set (FjbtndrvBacklight *this, guint value)
{
	FjbtndrvBacklightPrivate *priv = FJBTNDRV_BACKLIGHT_GET_PRIVATE(this);
	Display *display = priv->display;
	RROutput output;
	XRROutputInfo *output_info;

	g_assert(FJBTNDRV_IS_BACKLIGHT(this));

	g_return_val_if_fail (value <= 100, 0);
	g_return_val_if_fail (priv->backlight.xid, 0);

	output = get_lvds(display, &output_info);
	g_return_val_if_fail (output, 0);

	value = percent_value(value, &priv->backlight);

	set_backlight_level(display, output, &priv->backlight, value);

	value = get_backlight_level(display, output, &priv->backlight);

	put_lvds(output_info);

	return value_percent(value, &priv->backlight);
}

guint
fjbtndrv_backlight_up (FjbtndrvBacklight *this)
{
	FjbtndrvBacklightPrivate *priv = FJBTNDRV_BACKLIGHT_GET_PRIVATE(this);
	Display *display = priv->display;
	RROutput output;
	XRROutputInfo *output_info;
	guint value;

	g_assert(FJBTNDRV_IS_BACKLIGHT(this));

	g_return_val_if_fail (priv->backlight.xid, 0);

	output = get_lvds(display, &output_info);
	g_return_val_if_fail (output, 0);

	value = get_backlight_level(display, output, &priv->backlight);

	if (value < priv->backlight.max) {
		guint step = (priv->backlight.max - priv->backlight.min) / 100;
		step++;

		value += step;
	
		set_backlight_level(display, output, &priv->backlight, value);
		value = get_backlight_level(display, output, &priv->backlight);
	}

	put_lvds(output_info);

	return value_percent(value, &priv->backlight);
}

guint
fjbtndrv_backlight_down (FjbtndrvBacklight *this)
{
	FjbtndrvBacklightPrivate *priv = FJBTNDRV_BACKLIGHT_GET_PRIVATE(this);
	Display *display = priv->display;
	RROutput output;
	XRROutputInfo *output_info;
	guint value;

	g_assert(FJBTNDRV_IS_BACKLIGHT(this));

	g_return_val_if_fail (priv->backlight.xid, 0);

	output = get_lvds(display, &output_info);
	g_return_val_if_fail (output, 0);

	value = get_backlight_level(display, output, &priv->backlight);

	if (value > priv->backlight.min) {
		guint step = (priv->backlight.max - priv->backlight.min) / 100;
		step++;

		value -= step;

		set_backlight_level(display, output, &priv->backlight, value);
		value = get_backlight_level(display, output, &priv->backlight);
	}

	put_lvds(output_info);

	return value_percent(value, &priv->backlight);
}

static void
fjbtndrv_backlight_init (FjbtndrvBacklight *this)
{
}

static void
fjbtndrv_backlight_finalize (GObject *object)
{
	//FjbtndrvBacklight *this = FJBTNDRV_BACKLIGHT(object);
	//FjbtndrvBacklightPrivate *priv = FJBTNDRV_BACKLIGHT_GET_PRIVATE(this);

	G_OBJECT_CLASS (fjbtndrv_backlight_parent_class)->finalize (object);
}

static void
fjbtndrv_backlight_class_init (FjbtndrvBacklightClass *klass)
{
	GObjectClass* object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = fjbtndrv_backlight_finalize;

	g_type_class_add_private(klass, sizeof(FjbtndrvBacklightPrivate));
}

FjbtndrvBacklight *
fjbtndrv_backlight_new (Display *display)
{
	FjbtndrvBacklight *this;
	FjbtndrvBacklightPrivate *priv;
	RROutput output;
	XRROutputInfo *output_info;

	g_assert(display);

	g_return_val_if_fail (randr_ok(display), NULL);

	output = get_lvds(display, &output_info);
	g_return_val_if_fail (output, NULL);

	this = g_object_new(FJBTNDRV_TYPE_BACKLIGHT, NULL);
	g_assert(this);

	priv = FJBTNDRV_BACKLIGHT_GET_PRIVATE(this);
	g_assert(priv);

	priv->display = display;

	priv->backlight.xid = init_backlight(display, output,
			"Backlight", &(priv->backlight));
	if (priv->backlight.xid == None) {
		priv->backlight.xid = init_backlight(display, output,
				"BACKLIGHT", &(priv->backlight));
	}

	put_lvds(output_info);

	debug("Backlight: id=%lu min=%d max=%d cur=%d%%",
			priv->backlight.xid, priv->backlight.min, priv->backlight.max,
			fjbtndrv_backlight_get(this));

	return this;
}

