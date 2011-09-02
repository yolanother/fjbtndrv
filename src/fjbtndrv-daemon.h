/*******************************************************************************
 * fjbtndrv uinput daemon
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA
 */

#ifndef __FJBTNDRV_DAEMON_H
#define __FJBTNDRV_DAEMON_H

#include <glib-object.h>
#include <dbus/dbus-glib.h>

#define FJBTNDRV_DAEMON_SERVICE_PATH      "/de/khnz/fjbtndrv"
#define FJBTNDRV_DAEMON_SERVICE_NAME      "de.khnz.fjbtndrv"
#define FJBTNDRV_DAEMON_SERVICE_INTERFACE "de.khnz.fjbtndrv"

G_BEGIN_DECLS

#define FJBTNDRV_TYPE_DAEMON (fjbtndrv_daemon_get_type())

#define FJBTNDRV_DAEMON(object) \
	(G_TYPE_CHECK_INSTANCE_CAST((object), FJBTNDRV_TYPE_DAEMON, FjbtndrvDaemon))
#define FJBTNDRV_DAEMON_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_CAST((klass), FJBTNDRV_TYPE_DAEMON, FjbtndrvDaemonClass))
#define FJBTNDRV_IS_DAEMON(object) \
	(G_TYPE_CHECK_INSTANCE_TYPE((onject), FJBTNDRV_TYPE_DAEMON))
#define FJBTNDRV_IS_DAEMON_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_TYPE((klass), FJBTNDRV_TYPE_DAEMON))
#define FJBTNDRV_DAEMON_GET_CLASS(object) \
	(G_TYPE_INSTANCE_GET_CLASS((object), FJBTNDRV_TYPE_DAEMON, FjbtndrvDaemonClass))

typedef struct {
	__u16 normal;
	__u16 fn;
	__u16 alt;
	__u16 lp;
} KeymapEntry;

typedef struct {
	GObject parent;

	gchar *product;

	GIOChannel *input;
	GIOChannel *uinput;

	DBusGProxy *proxy;

	KeymapEntry *keymap;
	guint sticky_timeout;

	/* private */
	guint skey;
	guint stime;
	GSource *sinterval;
} FjbtndrvDaemon;

typedef struct {
	GObjectClass parent;

//	DBusGConnection *connection;
} FjbtndrvDaemonClass;

#define FJBTNDRV_DAEMON_ERROR 1

/* G_DEFINE_TYPE(FjbtndrvDaemon, fjbtndrv_daemon, G_TYPE_OBJECT); */

GType fjbtndrv_daemon_get_type(void);
FjbtndrvDaemon* fjbtndrv_daemon_new(void);

gboolean
fjbtndrv_daemon_bind_dbus(
		FjbtndrvDaemon*,
		DBusGConnection *dbus,
		GError **error);

gboolean
fjbtndrv_daemon_register_callbacks(
		FjbtndrvDaemon*,
		GMainLoop *mainloop);

gboolean
fjbtndrv_daemon_input_event_dispatcher(
		GIOChannel *source,
		GIOCondition condition,
		gpointer); /* FjbtndrvDaemon* */

gboolean
fjbtndrv_daemon_get_sticky_timeout(
		FjbtndrvDaemon*,
		gint *msec,
		DBusGMethodInvocation *error);

gboolean
fjbtndrv_daemon_set_sticky_timeout(
		FjbtndrvDaemon*,
		gint msec,
	       	DBusGMethodInvocation *error);

G_END_DECLS

#endif
