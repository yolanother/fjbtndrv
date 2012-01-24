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

#ifndef _FJBTNDRV_DISPLAY_H_
#define _FJBTNDRV_DISPLAY_H_

#include <glib-object.h>
#include <X11/X.h>

#include "fjbtndrv-device.h"

G_BEGIN_DECLS

#define FJBTNDRV_TYPE_DISPLAY             (fjbtndrv_display_get_type ())
#define FJBTNDRV_DISPLAY(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), FJBTNDRV_TYPE_DISPLAY, FjbtndrvDisplay))
#define FJBTNDRV_DISPLAY_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), FJBTNDRV_TYPE_DISPLAY, FjbtndrvDisplayClass))
#define FJBTNDRV_IS_DISPLAY(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), FJBTNDRV_TYPE_DISPLAY))
#define FJBTNDRV_IS_DISPLAY_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), FJBTNDRV_TYPE_DISPLAY))
#define FJBTNDRV_DISPLAY_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), FJBTNDRV_TYPE_DISPLAY, FjbtndrvDisplayClass))

typedef struct _FjbtndrvDisplayClass FjbtndrvDisplayClass;
typedef struct _FjbtndrvDisplayPrivate FjbtndrvDisplayPrivate;
typedef struct _FjbtndrvDisplay FjbtndrvDisplay;

struct _FjbtndrvDisplayClass
{
	GObjectClass parent_class;
};

struct _FjbtndrvDisplay
{
	GObject parent_instance;
};

GType fjbtndrv_display_get_type (void) G_GNUC_CONST;

FjbtndrvDisplay* fjbtndrv_display_new (gchar *display_name);

FjbtndrvDevice* fjbtndrv_display_get_device(FjbtndrvDisplay*);

//FjbtndrvBacklight* fjbtndrv_display_get_backlight(FjbtndrvDisplay*);
guint fjbtndrv_display_backlight_get(FjbtndrvDisplay*);
guint fjbtndrv_display_backlight_up(FjbtndrvDisplay*);
guint fjbtndrv_display_backlight_down(FjbtndrvDisplay*);

void fjbtndrv_display_show_info(FjbtndrvDisplay*, gchar *format, ...);
void fjbtndrv_display_show_percentage(FjbtndrvDisplay*, guint percent, gchar *title, guint timeout);
void fjbtndrv_display_show_slider(FjbtndrvDisplay*, guint percent, gchar *title, guint timeout);
void fjbtndrv_display_hide_osd(FjbtndrvDisplay*);

void fjbtndrv_display_fake_key(FjbtndrvDisplay*, KeySym);
void fjbtndrv_display_fake_button(FjbtndrvDisplay*, guint button);
void fjbtndrv_display_fake_event(FjbtndrvDisplay*, FjbtndrvDeviceEvent*);

void fjbtndrv_display_off(FjbtndrvDisplay*);

G_END_DECLS

#endif /* _FJBTNDRV_DISPLAY_H_ */
