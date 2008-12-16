/* fscd-gui
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
