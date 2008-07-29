/* FSC Tablet Buttons Helper Daemon (simple OSD)
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

#define XOSD_COLOR		"green"
#define XOSD_OUTLINE_COLOR	"darkgreen"

/******************************************************************************/

#include "fscd-base.h"
#include "fscd-gui.h"
#include <stdarg.h>
#include <xosd.h>

#ifdef ENABLE_NLS
#  include <libintl.h>
#  define _(x) gettext(x)
#else
#  define _(x) (x)
#endif

#ifdef DEBUG
#  include <stdio.h>
#  define debug(p, m, a...) fprintf(stderr, "%s: " m "\n", p, ##a)
#else
#  define debug(p, m, a...) /**/
#endif

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

	xosd_set_font(osd, "-*-helvetica-bold-r-normal-*-*-400-*-*-*-*-*-*");
	xosd_set_outline_offset(osd, 1);
	xosd_set_outline_colour(osd, XOSD_OUTLINE_COLOR);
	xosd_set_shadow_offset(osd, 3);
	xosd_set_colour(osd, XOSD_COLOR);

	return osd;
}

/*
#define osd_hide() osd_exit()
#define osd_timeout(s) xosd_set_timeout(osd, s)

#define osd_info(format, a...) do {			\
	xosd *osd = osd_new(1);				\
	xosd_display(osd, 0, XOSD_printf, format, ##a); \
} while(0)

#define osd_slider(percent, format, a...) do {	\
	xosd *osd = osd_new(2);				\
	xosd_display(osd, 0, XOSD_printf, format, ##a);	\
	xosd_display(osd, 1, XOSD_slider, percent);	\
} while(0)
*/

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
	debug("TRACE", "screen rotated");

	xosd_destroy(osd);
	osd = NULL;
}

void gui_brightness_show(int percent)
{
	debug("TRACE", "brightness_show");

	osd = osd_new(2);
	xosd_display(osd, 0, XOSD_printf, "%s", _("Brightness"));
	xosd_display(osd, 1, XOSD_slider, percent);
	xosd_set_timeout(osd, 2);
}

