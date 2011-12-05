/*******************************************************************************
 * fjbtndrv dbus proxy daemon
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

#include <gio/gio.h>

#define FJBTNDRV_DAEMON_SERVICE_PATH      "/de/khnz/FjBtnDrv"
#define FJBTNDRV_DAEMON_SERVICE_NAME      "de.khnz.FjBtnDrv"
#define FJBTNDRV_DAEMON_SERVICE_INTERFACE FJBTNDRV_DAEMON_SERVICE_NAME

G_BEGIN_DECLS

#define FJBTNDRV_TYPE_DAEMON (fjbtndrv_daemon_get_type())

#define FJBTNDRV_DAEMON(object) \
	(G_TYPE_CHECK_INSTANCE_CAST((object), FJBTNDRV_TYPE_DAEMON, FjbtndrvDaemon))
#define FJBTNDRV_DAEMON_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_CAST((klass), FJBTNDRV_TYPE_DAEMON, FjbtndrvDaemonClass))
#define FJBTNDRV_IS_DAEMON(object) \
	(G_TYPE_CHECK_INSTANCE_TYPE((object), FJBTNDRV_TYPE_DAEMON))
#define FJBTNDRV_IS_DAEMON_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_TYPE((klass), FJBTNDRV_TYPE_DAEMON))
#define FJBTNDRV_DAEMON_GET_CLASS(object) \
	(G_TYPE_INSTANCE_GET_CLASS((object), FJBTNDRV_TYPE_DAEMON, FjbtndrvDaemonClass))

typedef struct {
	GObject parent;
	GDBusConnection *dbus;
	GIOChannel *input;
} FjbtndrvDaemon;

typedef struct {
	GObjectClass parent;
} FjbtndrvDaemonClass;

G_END_DECLS

#endif
