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

#ifndef _FJBTNDRV_PROXY_H_
#define _FJBTNDRV_PROXY_H_

#include <glib-object.h>
#include <gio/gio.h>

G_BEGIN_DECLS

#define FJBTNDRV_TYPE_PROXY             (fjbtndrv_proxy_get_type ())
#define FJBTNDRV_PROXY(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), FJBTNDRV_TYPE_PROXY, FjbtndrvProxy))
#define FJBTNDRV_PROXY_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), FJBTNDRV_TYPE_PROXY, FjbtndrvProxyClass))
#define FJBTNDRV_IS_PROXY(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), FJBTNDRV_TYPE_PROXY))
#define FJBTNDRV_IS_PROXY_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), FJBTNDRV_TYPE_PROXY))
#define FJBTNDRV_PROXY_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), FJBTNDRV_TYPE_PROXY, FjbtndrvProxyClass))

typedef struct _FjbtndrvProxyClass FjbtndrvProxyClass;
typedef struct _FjbtndrvProxyPrivate FjbtndrvProxyPrivate;
typedef struct _FjbtndrvProxy FjbtndrvProxy;

struct _FjbtndrvProxyClass {
	GObjectClass parent_class;
};

struct _FjbtndrvProxy {
	GObject parent_instance;
	GDBusConnection *dbus;
};

GType fjbtndrv_proxy_get_type (void) G_GNUC_CONST;

FjbtndrvProxy* fjbtndrv_proxy_new(void);

void fjbtndrv_proxy_set_tablet_mode(FjbtndrvProxy*, gboolean);
void fjbtndrv_proxy_set_dock_state(FjbtndrvProxy*, gboolean);

G_END_DECLS

#endif /* _FJBTNDRV_PROXY_H_ */
