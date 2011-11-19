/*
 * Copyright (C) 2007-2011 Robert Gerlach
 *
 * You can redistribute and/or modify this program under the terms of the
 * GNU General Public License version 3 as published by the Free Software
 * Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _FSCD_BASE_H
#define _FSCD_BASE_H

#include <X11/Xlib.h>
#include <X11/extensions/randr.h>

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
void debug(const char *format, ...);
#else
#define debug(...) do {} while(0)
#endif

#endif
