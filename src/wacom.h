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

#ifndef _FSCD_WACOM_H
#define _FSCD_WACOM_H

#ifdef ENABLE_WACOM

#include <X11/Xlib.h>

int wacom_init(Display*);
void wacom_exit(void);
void wacom_rotate(int RR_Rotation);

#endif
#endif
