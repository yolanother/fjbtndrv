/*
 * Copyright (C) 2007-2008 Robert Gerlach
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
#include <stdio.h>
#include <stdarg.h>
#include <xosd.h>

#ifdef ENABLE_NLS
#  include <libintl.h>
#  define _(x) gettext(x)
#else
#  define _(x) (x)
#endif

#define XOSD_COLOR		"green"
#define XOSD_OUTLINE_COLOR	"darkgreen"
#define XOSD_FONT               "-*-*-*-r-normal-sans-*-240-*-*-*-*-*-*"

static xosd *osd = NULL;

xosd *osd_new(int lines)
{
	if(osd) {
		if(xosd_get_number_lines(osd) == lines)
			return osd;

		xosd_destroy(osd);
	}

	if(lines <= 0)
		return osd = NULL;

	osd = xosd_create(lines);

	xosd_set_pos(osd, XOSD_bottom);
	xosd_set_vertical_offset(osd, 16);
	xosd_set_align(osd, XOSD_center);
	xosd_set_horizontal_offset(osd, 0);

	xosd_set_font(osd, XOSD_FONT);
	xosd_set_outline_offset(osd, 1);
	xosd_set_outline_colour(osd, XOSD_OUTLINE_COLOR);
	xosd_set_shadow_offset(osd, 3);
	xosd_set_colour(osd, XOSD_COLOR);

	return osd;
}

int gui_init(Display *display)
{
	return 0;
}

void gui_exit(void)
{
	gui_hide();
}

void gui_info(char *format, ...)
{
	va_list a;
	char buffer[256];

	va_start(a, format);
	vsnprintf(buffer, 255, format, a);
	va_end(a);

	debug("gui info: %s", buffer);

	osd = osd_new(1);
	xosd_display(osd, 0, XOSD_string, buffer);
	xosd_set_timeout(osd, 2);
}

void gui_hide(void)
{
	if(osd) {
		xosd_destroy(osd);
		osd = NULL;
	}
}

void screen_rotated(void)
{
	debug("screen rotated");

	xosd_destroy(osd);
	osd = NULL;
}

void gui_brightness_show(int percent, int timeout)
{
	debug("brightness_show");

	osd = osd_new(2);
	xosd_display(osd, 0, XOSD_printf, "%s", _("Brightness"));
	xosd_display(osd, 1, XOSD_slider, percent);
	xosd_set_timeout(osd, timeout);
}

