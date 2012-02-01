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

#ifndef _FJBTNDRV_DAEMON_H_
#define _FJBTNDRV_DAEMON_H_

#include <glib-object.h>

typedef enum {
	SM_ZAXIS,
	SM_KEY_PAGE,
	SM_KEY_SPACE,
	SM_KEY_MAX
} ScrollMode;

typedef enum {
	UL_LOCKED,
	UL_UNLOCKED
} UserLock;

typedef struct keymap_entry {
	int code;
	char *name;
	KeySym sym;
	Bool grab;
} keymap_entry;

typedef struct {
	unsigned int keycode;
	unsigned int value;
} key_event;

G_BEGIN_DECLS

#define FJBTNDRV_TYPE_DAEMON             (fjbtndrv_daemon_get_type ())
#define FJBTNDRV_DAEMON(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), FJBTNDRV_TYPE_DAEMON, FjbtndrvDaemon))
#define FJBTNDRV_DAEMON_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), FJBTNDRV_TYPE_DAEMON, FjbtndrvDaemonClass))
#define FJBTNDRV_IS_DAEMON(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), FJBTNDRV_TYPE_DAEMON))
#define FJBTNDRV_IS_DAEMON_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), FJBTNDRV_TYPE_DAEMON))
#define FJBTNDRV_DAEMON_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), FJBTNDRV_TYPE_DAEMON, FjbtndrvDaemonClass))

typedef struct _FjbtndrvDaemonClass FjbtndrvDaemonClass;
typedef struct _FjbtndrvDaemon FjbtndrvDaemon;

struct _FjbtndrvDaemonClass
{
	GObjectClass parent_class;
};

struct _FjbtndrvDaemon
{
	GObject parent_instance;
};

GType fjbtndrv_daemon_get_type (void) G_GNUC_CONST;

G_END_DECLS

#endif /* _FJBTNDRV_DAEMON_H_ */
