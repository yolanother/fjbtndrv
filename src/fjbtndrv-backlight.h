/*
 * Copyright (C) 2012 Robert Gerlach <khnz@users.sourceforge.net>
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

#ifndef _FJBTNDRV_BACKLIGHT_H_
#define _FJBTNDRV_BACKLIGHT_H_

#include <glib-object.h>
#include <X11/X.h>

G_BEGIN_DECLS

#define FJBTNDRV_TYPE_BACKLIGHT             (fjbtndrv_backlight_get_type ())
#define FJBTNDRV_BACKLIGHT(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), FJBTNDRV_TYPE_BACKLIGHT, FjbtndrvBacklight))
#define FJBTNDRV_BACKLIGHT_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), FJBTNDRV_TYPE_BACKLIGHT, FjbtndrvBacklightClass))
#define FJBTNDRV_IS_BACKLIGHT(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), FJBTNDRV_TYPE_BACKLIGHT))
#define FJBTNDRV_IS_BACKLIGHT_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), FJBTNDRV_TYPE_BACKLIGHT))
#define FJBTNDRV_BACKLIGHT_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), FJBTNDRV_TYPE_BACKLIGHT, FjbtndrvBacklightClass))

typedef struct _FjbtndrvBacklightClass FjbtndrvBacklightClass;
typedef struct _FjbtndrvBacklightPrivate FjbtndrvBacklightPrivate;
typedef struct _FjbtndrvBacklight FjbtndrvBacklight;

struct _FjbtndrvBacklightClass
{
	GObjectClass parent_class;
};

struct _FjbtndrvBacklight
{
	GObject parent_instance;
};

GType fjbtndrv_backlight_get_type (void) G_GNUC_CONST;

FjbtndrvBacklight* fjbtndrv_backlight_new (Display*);

guint fjbtndrv_backlight_get (FjbtndrvBacklight*);
guint fjbtndrv_backlight_set (FjbtndrvBacklight*, guint value);
guint fjbtndrv_backlight_up (FjbtndrvBacklight*);
guint fjbtndrv_backlight_down (FjbtndrvBacklight*);

G_END_DECLS

#endif /* _FJBTNDRV_BACKLIGHT_H_ */
