/* FSC Tablet Buttons Helper Daemon (no GUI / template)
 * Copyright (C) 2007 Robert Gerlach
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
/******************************************************************************/

#include "fscd-base.h"
#include "fscd-gui.h"

#ifdef DEBUG
#  include <stdio.h>
#  define debug(p, m, a...) fprintf(stderr, "%s: " m "\n", p, ##a)
#else
#  define debug(p, m, a...) /**/
#endif

int gui_init(Display *display)
{ return 0; }

void gui_exit(void)
{}

void gui_info(char *format, ...)
{}

void screen_rotated(void)
{
	debug("TRACE", "screen rotated");
}

void brightness_show(int percent)
{
	debug("TRACE", "brightness_show");
}

