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

#include "fjbtndrv-daemon.h"



G_DEFINE_TYPE (FjbtndrvDaemon, fjbtndrv_daemon, G_TYPE_OBJECT);

static void
fjbtndrv_daemon_init (FjbtndrvDaemon *object)
{
	/* TODO: Add initialization code here */
}

static void
fjbtndrv_daemon_finalize (GObject *object)
{
	/* TODO: Add deinitalization code here */

	G_OBJECT_CLASS (fjbtndrv_daemon_parent_class)->finalize (object);
}

static void
fjbtndrv_daemon_class_init (FjbtndrvDaemonClass *klass)
{
	GObjectClass* object_class = G_OBJECT_CLASS (klass);
	GObjectClass* parent_class = G_OBJECT_CLASS (klass);

	object_class->finalize = fjbtndrv_daemon_finalize;
}
