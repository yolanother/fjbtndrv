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

#ifdef HAVE_CONFIG_H
#  include "../config.h"
#endif

#ifdef ENABLE_WACOM

#include "fjbtndrv.h"
#include "wacom.h"

#include <string.h>
#ifdef HAVE_WACOMCFG_H
#include <wacomcfg.h>
#endif
#ifdef HAVE_WACOMCFG_WACOMCFG_H
#include <wacomcfg/wacomcfg.h>
#endif
#include <Xwacom.h>
#include <X11/extensions/Xrandr.h>
#include <X11/extensions/XInput.h>

#ifdef ENABLE_DYNAMIC

#include <dlfcn.h>

static struct wclib_t {
	void *hdnl;
	WACOMCONFIG * (*WacomConfigInit)(Display* pDisplay, WACOMERRORFUNC pfnErrorHandler);
	WACOMDEVICE * (*WacomConfigOpenDevice)(WACOMCONFIG * hConfig, const char* pszDeviceName);
	int (*WacomConfigCloseDevice)(WACOMDEVICE * hDevice);
	int (*WacomConfigSetRawParam)(WACOMDEVICE * hDevice, int nParam, int nValue, unsigned * keys);
	void (*WacomConfigFree)(void* pvData);
} wclib;

#define CALL(func, args...) \
	((&wclib)->hdnl ? (&wclib)->func(args) : 0)

#else /* ENABLE_DYNAMIC */

#define CALL(func, args...) \
	func(args)

#endif

static WACOMCONFIG *wacom_config;
static char *devname;

int wacom_init(Display *display)
{
	Atom a;
	XDeviceInfo *devices;
	int nr;

	a = XInternAtom(display, "Wacom Stylus", True);
	if(!a) {
		debug("no 'Wacom Stylus' atom");
		return -1;
	}

	devices = XListInputDevices(display, &nr);
	if(devices) {
		int i;
		for(i = 0; i < nr; i++)
			if(devices[i].type == a) {
				devname = strdup(devices[i].name);
				debug("wacom device found - %s", devname);
				break;
			}

		XFreeDeviceList(devices);
	}

	if(!devname) {
		debug("no wacom device found.");
		return -1;
	}
	
#ifdef ENABLE_DYNAMIC
	struct wclib_t w;

	w.hdnl = dlopen("libwacomcfg.so.0", RTLD_NOW);
	if(!w.hdnl)
		return -1;

	w.WacomConfigInit = dlsym(w.hdnl, "WacomConfigInit");
	if(!w.WacomConfigInit)
		return -1;
	
	w.WacomConfigFree = dlsym(w.hdnl, "WacomConfigFree");
	if(!w.WacomConfigFree)
		return -1;
	
	w.WacomConfigSetRawParam = dlsym(w.hdnl, "WacomConfigSetRawParam");
	if(!w.WacomConfigSetRawParam)
		return -1;
	
	w.WacomConfigOpenDevice = dlsym(w.hdnl, "WacomConfigOpenDevice");
	if(!w.WacomConfigOpenDevice)
		return -1;
	
	w.WacomConfigCloseDevice = dlsym(w.hdnl, "WacomConfigCloseDevice");
	if(!w.WacomConfigCloseDevice)
		return -1;

	memcpy(&wclib, &w, sizeof(struct wclib_t));

	debug("wacomcfg library loaded");
#endif

	wacom_config = CALL(WacomConfigInit, display, NULL);
	if(!wacom_config) {
		debug("failed to init wacomcfg");
		return -1;
	}

	return 0;
}

void wacom_exit(void)
{
	if(devname)
		free(devname);

	if(wacom_config)
		CALL(WacomConfigFree, wacom_config);

#ifdef ENABLE_DYNAMIC
	dlclose(wclib.hdnl);
	wclib.hdnl = NULL;
#endif
}

void wacom_rotate(int rr_rotation)
{
	WACOMDEVICE * d;
	int rotation;

	debug("wacom_rotate");

	if(!wacom_config || !devname)
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

	debug("rotate to %d", rotation);

	d = CALL(WacomConfigOpenDevice, wacom_config, devname);
	if(!d)
		return;

	CALL(WacomConfigSetRawParam, d, XWACOM_PARAM_ROTATE,
			rotation, 0);

	CALL(WacomConfigCloseDevice, d);
}

#endif
