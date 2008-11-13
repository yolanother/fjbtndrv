/* FSC Tablet Helper (wacom support)
 * Copyright (C) 2007-2008 Robert Gerlach
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

#ifdef HAVE_CONFIG_H
#  include "../config.h"
#endif

#ifdef ENABLE_WACOM

#include "fscd-wacom.h"

#ifdef HAVE_WACOMCFG_H
#include <wacomcfg.h>
#endif
#ifdef HAVE_WACOMCFG_WACOMCFG_H
#include <wacomcfg/wacomcfg.h>
#endif

#include <Xwacom.h>
#include <X11/extensions/Xrandr.h>

#ifdef DEBUG
void debug(const char *tag, const char *format, ...);
#endif

#ifdef ENABLE_DYNAMIC 
#  include <dlfcn.h>
#  define DLOPEN(info, name) \
	((info)->hdnl = dlopen(name, RTLD_NOW))
#  define DLSYM(info, func) \
	((info)->func = dlsym((info)->hdnl, #func))
#  define DLCLOSE(info) \
	(((info)->hdnl) \
		? (dlclose((info)->hdnl) ? (int)((info)->hdnl=NULL) : -1) \
		: 0)
#  define DLCALL(info, func, args...) \
	(((info)->hdnl && (info)->func) ? (info)->func(args) : 0)
#else
#  define DLCALL(info, func, args...) \
	func(args)
#endif

#ifdef ENABLE_DYNAMIC
static struct {
	void *hdnl;
	WACOMCONFIG * (*WacomConfigInit)(Display* pDisplay, WACOMERRORFUNC pfnErrorHandler);
	WACOMDEVICE * (*WacomConfigOpenDevice)(WACOMCONFIG * hConfig, const char* pszDeviceName);
	int (*WacomConfigCloseDevice)(WACOMDEVICE * hDevice);
	int (*WacomConfigSetRawParam)(WACOMDEVICE * hDevice, int nParam, int nValue, unsigned * keys);
	void (*WacomConfigFree)(void* pvData);
} wclib;
#endif

static WACOMCONFIG * wacom_config;

int wacom_init(Display *display)
{
#ifdef ENABLE_DYNAMIC
	if( !(DLOPEN(&wclib, "libwacomcfg.so.0") &&
			DLSYM(&wclib, WacomConfigInit) &&
			DLSYM(&wclib, WacomConfigFree) &&
			DLSYM(&wclib, WacomConfigSetRawParam) &&
			DLSYM(&wclib, WacomConfigOpenDevice) &&
			DLSYM(&wclib, WacomConfigCloseDevice)) ) {
		debug("WACOM", "%s", dlerror());
		wclib.hdnl = NULL;
		return -1;
	}

	debug("WACOM", "wacomcfg library ready");
#endif

	wacom_config = DLCALL(&wclib, WacomConfigInit, display, NULL);
	if(!wacom_config)
		return -1;

	return 0;
}

void wacom_exit(void)
{
	if(wacom_config)
		DLCALL(&wclib, WacomConfigFree, wacom_config);

#ifdef ENABLE_DYNAMIC
	DLCLOSE(&wclib);
#endif
}

void wacom_rotate(int rr_rotation)
{
	WACOMDEVICE * d;
	int rotation;

	debug("TRACE", "wacom_rotate");

	if(!wacom_config)
		return;

	switch(rr_rotation) {
		case RR_Rotate_0:
			rotation = 0; /* XWACOM_VALUE_ROTATE_NONE */
			break;
		case RR_Rotate_90:
			rotation = 2; /* XWACOM_VALUE_ROTATE_CCW */
			break;
		case RR_Rotate_180:
			rotation = 3; /* XWACOM_VALUE_ROTATE_HALF */
			break;
		case RR_Rotate_270:
			rotation = 1; /* XWACOM_VALUE_ROTATE_CW */
			break;
		default:
			return;
	}

	debug("WACOM", "rotate to %d", rotation);

	d = DLCALL(&wclib, WacomConfigOpenDevice, wacom_config, "stylus");
	if(!d)
		return;

	DLCALL(&wclib, WacomConfigSetRawParam, d, XWACOM_PARAM_ROTATE,
			rotation, 0);

	DLCALL(&wclib, WacomConfigCloseDevice, d);
}

#endif
