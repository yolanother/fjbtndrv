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

#ifndef __FJBTNDRV_X11_H
#define __FJBTNDRV_X11_H

#include <glib-object.h>

G_BEGIN_DECLS

#define FJBTNDRV_TYPE_X11 (fjbtndrv_x11_get_type())

#define FJBTNDRV_X11(object) \
	(G_TYPE_CHECK_INSTANCE_CAST((object), FJBTNDRV_TYPE_X11, FjbtndrvX11))
#define FJBTNDRV_X11_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_CAST((klass), FJBTNDRV_TYPE_X11, FjbtndrvX11Class))
#define FJBTNDRV_IS_X11(object) \
	(G_TYPE_CHECK_INSTANCE_TYPE((object), FJBTNDRV_TYPE_X11))
#define FJBTNDRV_IS_X11_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_TYPE((klass), FJBTNDRV_TYPE_X11))
#define FJBTNDRV_X11_GET_CLASS(object) \
	(G_TYPE_INSTANCE_GET_CLASS((object), FJBTNDRV_TYPE_X11, FjbtndrvX11Class))


typedef struct _FjbtndrvX11 FjbtndrvX11;
typedef struct _FjbtndrvX11Class FjbtndrvX11Class;
typedef struct _FjbtndrvX11Private FjbtndrvX11Private;


struct _FjbtndrvX11 {
	GObject parent;
	FjbtndrvX11Private *priv;
};

typedef enum {
	TYPE_EVENT,
	TYPE_KEY,
	TYPE_MOUSE_BUTTON,
} FjbtndrvX11_EventType;

struct _FjbtndrvX11Class {
	GObjectClass parent_class;

	void (*send_event) (FjbtndrvX11 *self, FjbtndrvX11_EventType type, ...);
	//void (*send_event)  (FjbtndrvX11 *self, key_event *event);
	//void (*send_key)    (FjbtndrvX11 *self, KeySym *key);
	//void (*send_button) (FjbtndrvX11 *self, guint button);
};


GType fjbtndrv_x11_get_type(void);

FjbtndrvX11* fjbtndrv_x11_new(void);

//fjbtndrv_x11_send_event(FjbtndrvX11 *self, FjbtndrvX11_EventType type, ...);

G_END_DECLS

#endif
