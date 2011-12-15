/*
 * Copyright (C) Robert Gerlach 2011 <khnz@users.sourceforge.net>
 * 
 * fjbtndrv is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * fjbtndrv3 is distributed in the hope that it will be useful, but
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
#include <xosd.h>

#include "fjbtndrv.h"
#include "fjbtndrv-osd.h"

#define XOSD_COLOR		"green"
#define XOSD_OUTLINE_COLOR	"DarkGreen"
#define XOSD_FONT               "-*-*-*-r-normal-*-*-200-*-*-*-*-*-*"

#define FJBTNDRV_OSD_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), FJBTNDRV_TYPE_OSD, FjbtndrvOSDPrivate))

G_DEFINE_TYPE (FjbtndrvOSD, fjbtndrv_osd, G_TYPE_OBJECT);

typedef struct _FjbtndrvOSDPrivate FjbtndrvOSDPrivate;

struct _FjbtndrvOSDPrivate {
	xosd *osd;
};

static gboolean
new_osd(xosd **_osd, guint lines)
{
	xosd *osd = *_osd;

	if(osd) {
		if(xosd_get_number_lines(osd) == lines)
			return TRUE;

		xosd_destroy(osd);
	}

	if(lines <= 0) {
		osd = NULL;
		return FALSE;
	}

	osd = xosd_create(lines);

	xosd_set_pos(osd, XOSD_bottom);
	xosd_set_vertical_offset(osd, 16);
	xosd_set_align(osd, XOSD_center);
	xosd_set_horizontal_offset(osd, 0);

	xosd_set_font(osd, XOSD_FONT);
	xosd_set_outline_offset(osd, 1);
	xosd_set_outline_colour(osd, XOSD_OUTLINE_COLOR);
	xosd_set_shadow_offset(osd, 2);
	xosd_set_colour(osd, XOSD_COLOR);

	*_osd = osd;

	return TRUE;
}

void
fjbtndrv_osd_info(FjbtndrvOSD *this, gchar *text)
{
	FjbtndrvOSDPrivate *priv = FJBTNDRV_OSD_GET_PRIVATE(this);

	debug("fjbtndrv_osd_info: text=%s", text);

	new_osd(&priv->osd, 1);
	g_assert(priv->osd);

	xosd_display(priv->osd, 0, XOSD_string, text);
	xosd_set_timeout(priv->osd, 2);
}

void
fjbtndrv_osd_vinfo(FjbtndrvOSD *this, gchar *format, ...)
{
	va_list a;
	char buffer[256];

	va_start(a, format);
	vsnprintf(buffer, 255, format, a);
	va_end(a);

	//debug("fjbtndrv_osd_info: test=%s", buffer);

	fjbtndrv_osd_info(this, buffer);
}

void
fjbtndrv_osd_percentage(FjbtndrvOSD *this, guint percent, gchar *title, guint timeout)
{
	FjbtndrvOSDPrivate *priv = FJBTNDRV_OSD_GET_PRIVATE(this);

	debug("fjbtndrv_osd_percentage: title=%s percent=%d%%", title, percent);

	new_osd(&priv->osd, 2);
	g_assert(priv->osd);

	xosd_display(priv->osd, 0, XOSD_printf, "%s", title);
	xosd_display(priv->osd, 1, XOSD_percentage, percent);
	xosd_set_timeout(priv->osd, timeout);
}

void
fjbtndrv_osd_slider(FjbtndrvOSD *this, guint percent, gchar *title, guint timeout)
{
	FjbtndrvOSDPrivate *priv = FJBTNDRV_OSD_GET_PRIVATE(this);

	debug("fjbtndrv_osd_slider: title=%s percent=%d%%", title, percent);

	new_osd(&priv->osd, 2);
	g_assert(priv->osd);

	xosd_display(priv->osd, 0, XOSD_printf, "%s", title);
	xosd_display(priv->osd, 1, XOSD_slider, percent);
	xosd_set_timeout(priv->osd, timeout);
}

void
fjbtndrv_osd_hide(FjbtndrvOSD *this)
{
	FjbtndrvOSDPrivate *priv = FJBTNDRV_OSD_GET_PRIVATE(this);

	if(priv->osd) {
		xosd_destroy(priv->osd);
		priv->osd = NULL;
	}
}

// XXX: screen rotated signal -> osd_hide ?

static void
fjbtndrv_osd_init (FjbtndrvOSD *object)
{
	/* TODO: Add initialization code here */
}

static void
fjbtndrv_osd_finalize (GObject *object)
{
	FjbtndrvOSD *this = (FjbtndrvOSD*) object;

	fjbtndrv_osd_hide(this);

	G_OBJECT_CLASS (fjbtndrv_osd_parent_class)->finalize (object);
}

static void
fjbtndrv_osd_class_init (FjbtndrvOSDClass *klass)
{
	GObjectClass* object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = fjbtndrv_osd_finalize;

	g_type_class_add_private(klass, sizeof(FjbtndrvOSDPrivate));
}

FjbtndrvOSD*
fjbtndrv_osd_new(Display *display)
{
	FjbtndrvOSD *this;
	FjbtndrvOSDPrivate *priv;

	this = g_object_new(FJBTNDRV_TYPE_OSD, NULL);
	priv = FJBTNDRV_OSD_GET_PRIVATE(this);

	debug("osd=%p", priv->osd);
	priv->osd = NULL;

	return this;
}
