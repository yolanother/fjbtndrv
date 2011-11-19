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

#include "fjbtndrv.h"
#include "gui.h"

int gui_init(Display *display)
{ return 0; }

void gui_exit(void)
{}

void gui_info(char *format, ...)
{}

void gui_hide(void)
{}

void screen_rotated(void)
{
	debug("screen rotated");
}

void gui_brightness_show(int percent, int timeout)
{
	debug("brightness_show");
}

