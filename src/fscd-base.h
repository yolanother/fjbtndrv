/* fscd
 * Copyright (C) 2008 Robert Gerlach <khnz@users.sourceforge.net>
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

#ifdef HAVE_CONFIG_H
#  include "../config.h"
#endif

#include <X11/Xlib.h>

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

#ifdef DEBUG
void debug(const char *tag, const char *format, ...);
#else
#define debug(...) do {} while(0)
#endif

int handle_display_rotation(int mode);

