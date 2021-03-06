/*
 * Copyright (C) 2007-2012 Robert Gerlach
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

#ifndef _FSCD_GUI_H
#define _FSCD_GUI_H

#include <X11/Xlib.h>

int  gui_init(Display*);
void gui_exit(void);
void gui_info(char *format, ...);
void gui_hide(void);

void screen_rotated(void);

void gui_brightness_show(int percent, int timeout);

#endif
