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

#ifndef _FJBTNDRV_DEVICE_H_
#define _FJBTNDRV_DEVICE_H_

#include <glib-object.h>
#include <X11/Xlib.h>

G_BEGIN_DECLS

#define FJBTNDRV_TYPE_DEVICE             (fjbtndrv_device_get_type ())
#define FJBTNDRV_DEVICE(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), FJBTNDRV_TYPE_DEVICE, FjbtndrvDevice))
#define FJBTNDRV_DEVICE_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), FJBTNDRV_TYPE_DEVICE, FjbtndrvDeviceClass))
#define FJBTNDRV_IS_DEVICE(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), FJBTNDRV_TYPE_DEVICE))
#define FJBTNDRV_IS_DEVICE_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), FJBTNDRV_TYPE_DEVICE))
#define FJBTNDRV_DEVICE_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), FJBTNDRV_TYPE_DEVICE, FjbtndrvDeviceClass))

typedef struct _FjbtndrvDeviceClass FjbtndrvDeviceClass;
typedef struct _FjbtndrvDevicePrivate FjbtndrvDevicePrivate;
typedef struct _FjbtndrvDevice FjbtndrvDevice;

typedef enum _FjbtndrvDeviceEventType FjbtndrvDeviceEventType;
typedef union _FjbtndrvDeviceEvent FjbtndrvDeviceEvent;
typedef struct _FjbtndrvDeviceButtonEvent FjbtndrvDeviceButtonEvent;
typedef struct _FjbtndrvDeviceSwitchEvent FjbtndrvDeviceSwitchEvent;

struct _FjbtndrvDeviceClass
{
	GObjectClass parent_class;
};

struct _FjbtndrvDevice
{
	GObject parent_instance;
};

enum _FjbtndrvDeviceEventType
{
	BUTTON,
	SWITCH,
};

struct _FjbtndrvDeviceButtonEvent
{
	FjbtndrvDeviceEventType type;
	guint code;
	guint value;
};

struct _FjbtndrvDeviceSwitchEvent
{
	FjbtndrvDeviceEventType type;
	enum
{
		NONE,
		TABLET_MODE,
		DOCK_STATE,
	} code;
	gboolean value;
};

union _FjbtndrvDeviceEvent
{
	FjbtndrvDeviceEventType type;
	FjbtndrvDeviceButtonEvent _button;
	FjbtndrvDeviceSwitchEvent _switch;
};

GType fjbtndrv_device_get_type (void) G_GNUC_CONST;

FjbtndrvDevice* fjbtndrv_device_new (Display *display);

typedef void (*FjbtndrvDeviceEventCallback) (FjbtndrvDeviceEvent*, gpointer);
void fjbtndrv_device_set_callback (FjbtndrvDevice*, FjbtndrvDeviceEventCallback, gpointer);

G_END_DECLS

#endif /* _FJBTNDRV_DEVICE_H_ */
