/*
 * Copyright (C) 2012 Robert Gerlach <khnz@users.sourceforge.net>
 * 
 * fjbtndrv is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * fjbtndrv3 is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <glib.h>
#include <gio/gio.h>

#include <X11/keysym.h>
#include <X11/XF86keysym.h>
#include <X11/extensions/XInput.h>
#include <X11/extensions/XInput2.h>

#include "fjbtndrv.h"
#include "fjbtndrv-device.h"


#define FJBTNDRV_DEVICE_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), FJBTNDRV_TYPE_DEVICE, FjbtndrvDevicePrivate))

enum {
	EVTYPE_KEYPRESS,
	EVTYPE_KEYRELEASE,
	EVTYPE_LAST
};

struct _FjbtndrvDevicePrivate {
	Display *display;

	/* buttons */
	XDevice *device;
	XID evtype[EVTYPE_LAST];

	/* switches */
	GDBusProxy *proxy;

	struct {
		FjbtndrvDeviceEventCallback func;
		gpointer data;
	} callback;
};

G_DEFINE_TYPE (FjbtndrvDevice, fjbtndrv_device, G_TYPE_OBJECT);

typedef struct _FjbtndrvDevicePrivate FjbtndrvDevicePrivate;


/*
static void
on_quit(gpointer user_data)
{
	g_debug("on_quit: data=%p", user_data);
}
*/

static void
on_switch_event(GDBusProxy *proxy, char *sender, char *signal, GVariant *parameters, gpointer user_data)
{
	FjbtndrvDevice *this = (FjbtndrvDevice*) user_data;
	FjbtndrvDevicePrivate *priv = FJBTNDRV_DEVICE_GET_PRIVATE(this);
	FjbtndrvDeviceSwitchEvent event = { SWITCH };

	debug("on_signal_system: this=%p signal=%s sender=%s parameter=%s",
			this, signal, sender, g_variant_print(parameters, FALSE));

	if (g_strcmp0(signal, "TabletModeChanged") == 0) {
		gboolean data;
		g_variant_get(parameters, "(b)", &(data));

		debug("TabletModeChanged: state=%s",
				data ? "true" : "false");

		event.code = TABLET_MODE;
		event.value = data;
	}
	else if (g_strcmp0(signal, "DockStateChanged") == 0) {
		gboolean data;
		g_variant_get(parameters, "(b)", &(data));

		debug("DockStateChanged: state=%s",
				data ? "true" : "false");

		event.code = DOCK_STATE;
		event.value = data;
	}
	else {
		debug("unknown signal - %s", signal);
		return;
	}

	if (priv->callback.func)
		priv->callback.func((FjbtndrvDeviceEvent*)&event, priv->callback.data);
}

static gboolean
on_button_event(GIOChannel *source, GIOCondition condition, gpointer user_data)
{
	FjbtndrvDevice *this = (FjbtndrvDevice*) user_data;
	FjbtndrvDevicePrivate *priv = FJBTNDRV_DEVICE_GET_PRIVATE(this);
	FjbtndrvDeviceButtonEvent event = { BUTTON };
	XEvent xevent;

	g_assert (priv);

	XSync(priv->display, False);

	while (XPending(priv->display)) {
		XNextEvent(priv->display, &xevent);

		if (xevent.type == priv->evtype[EVTYPE_KEYPRESS]) { /* keypress */
			event.code = ((XDeviceKeyEvent*)&xevent)->keycode;
			event.value = 1;
		}
		else if (xevent.type == priv->evtype[EVTYPE_KEYRELEASE]) { /* keyrelease */
			event.code = ((XDeviceKeyEvent*)&xevent)->keycode;
			event.value = 0;
		}
		else {
			debug("unknown x11 event - %d", xevent.type);
			return TRUE;
		}

		if (priv->callback.func)
			priv->callback.func((FjbtndrvDeviceEvent*)&event, priv->callback.data);
	}

	return TRUE;
}

/*
static void
on_system_bus_connected(GObject *source, GAsyncResult *result, gpointer user_data)
{
	FjbtndrvDevice *this = (FjbtndrvDevice*) user_data;
	FjbtndrvDevicePrivate *priv = FJBTNDRV_DEVICE_GET_PRIVATE(this);
	GError *error = NULL;

	debug("fjbtndrv_daemon_system_bus_ready: system bus connected");

	priv->proxy = g_dbus_proxy_new_for_bus_finish(result, &error);
	if (error) {
		g_error("%s", error->message);
		g_error_free(error);
		priv->proxy = NULL;
		return;
	}

	g_signal_connect(priv->proxy, "g-signal",
			G_CALLBACK(on_signal_system),
			this);
}
*/

static XDevice *
open_button_device(Display *display)
{
	XDevice *device = NULL;
	XDeviceInfo *list;
	int num, i;

	debug("searching tablet buttons device ...");

	list = XListInputDevices(display, &num);

	for(i = 0; i < num; i++) {
		debug("  %s", list[i].name);
		if((g_strcmp0(list[i].name, "Fujitsu tablet buttons") == 0) ||
		   (g_strcmp0(list[i].name, "Fujitsu FUJ02BD") == 0) ||
		   (g_strcmp0(list[i].name, "Fujitsu FUJ02BF") == 0)) {
			debug("device found");
			device = XOpenDevice(display, list[i].id);
			break;
		}
	}

	XFreeDeviceList(list);

	return device;
}

static XDevice *
grab_button_device(Display *display, XDevice *device, XID *evtype)
{
	XEventClass evclass[2];
	XIDetachSlaveInfo devinfo;
	Window rootwin = XDefaultRootWindow(display);
	int error;

	devinfo.type = XIDetachSlave;
	devinfo.deviceid = device->device_id;
	error = XIChangeHierarchy(display, (XIAnyHierarchyChangeInfo*)&devinfo, 1);
	if (error != Success)
		return NULL;

	DeviceKeyPress(device, evtype[EVTYPE_KEYPRESS], evclass[0]);
	DeviceKeyRelease(device, evtype[EVTYPE_KEYRELEASE], evclass[1]);

	error = XSelectExtensionEvent(display, rootwin, evclass, 2);
	if (error != Success)
		return NULL;

	error = XGrabDevice(display, device, rootwin, False,
			2, evclass,
			GrabModeAsync, GrabModeAsync,
			CurrentTime);
	if (error != Success)
		return NULL;

	XSync(display, False);

	debug("device %lu grabbed", device->device_id);

	return device;
}

static void
close_button_device(Display *display, XDevice *device)
{
	Window rootwin = XDefaultRootWindow(display);
	XUngrabDevice(display, device, rootwin);
	XCloseDevice(display, device);
}

static GDBusProxy *
open_switch_device(void)
{
	GDBusProxy *proxy;
	GError *error = NULL;

	//g_dbus_proxy_new_for_bus(
	proxy = g_dbus_proxy_new_for_bus_sync (
			G_BUS_TYPE_SYSTEM,
			G_DBUS_PROXY_FLAGS_NONE,
			NULL,
			FJBTNDRV_DBUS_SERVICE_NAME,
			FJBTNDRV_DBUS_SERVICE_PATH,
			FJBTNDRV_DBUS_SERVICE_INTERFACE,
			NULL,
			//on_system_bus_connected,
			//this);
			&error);
	if (error) {
		g_error("%s", error->message);
		g_error_free(error);
		return NULL;
	}

	return proxy;
}

static void
close_switch_device(GDBusProxy *proxy)
{
}

void
fjbtndrv_device_set_callback(FjbtndrvDevice *this,
		FjbtndrvDeviceEventCallback func, gpointer user_data)
{
	FjbtndrvDevicePrivate *priv = FJBTNDRV_DEVICE_GET_PRIVATE(this);

	g_assert (priv);

	priv->callback.func = func;
	priv->callback.data = user_data;

}

static void
fjbtndrv_device_init (FjbtndrvDevice *this)
{
	FjbtndrvDevicePrivate *priv = FJBTNDRV_DEVICE_GET_PRIVATE(this);

	priv->callback.func = NULL;
	priv->callback.data = NULL;
}

static void
fjbtndrv_device_finalize (GObject *object)
{
	FjbtndrvDevice *this = (FjbtndrvDevice*) object;
	FjbtndrvDevicePrivate *priv = FJBTNDRV_DEVICE_GET_PRIVATE(this);

	if (priv->device) {
		close_button_device(priv->display, priv->device);
	}

	if (priv->proxy) {
		close_switch_device(priv->proxy);
		g_object_unref(priv->proxy);
	}

	G_OBJECT_CLASS (fjbtndrv_device_parent_class)->finalize (object);
}

static void
fjbtndrv_device_class_init (FjbtndrvDeviceClass *klass)
{
	GObjectClass* object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = fjbtndrv_device_finalize;

	g_type_class_add_private(klass, sizeof(FjbtndrvDevicePrivate));
}

FjbtndrvDevice *
fjbtndrv_device_new (Display *display)
{
	FjbtndrvDevice *this;
	FjbtndrvDevicePrivate *priv;
	GDBusProxy *proxy;
	XDevice *device;
	gint fd;
      
	g_assert(display);

	device = open_button_device(display);
	if (!device) {
		// TODO: g_error
		return NULL;
	}

	proxy = open_switch_device();
	if (!proxy) {
		// TODO: g_error
		return NULL;
	}

	this = g_object_new(FJBTNDRV_TYPE_DEVICE, NULL);
	g_assert(this);

	priv = FJBTNDRV_DEVICE_GET_PRIVATE(this);
	g_assert(priv);

	priv->display = display;
	priv->device = device;
	priv->proxy = proxy;

	if (!grab_button_device(display, device, priv->evtype)) {
		// TODO: g_error
		close_button_device(display, device);
		return NULL;
	}

	fd = XConnectionNumber(priv->display);
	g_io_add_watch (g_io_channel_unix_new(fd),
			G_IO_IN | G_IO_ERR | G_IO_HUP,
			on_button_event, this);

	g_signal_connect(proxy, "g-signal",
			G_CALLBACK(on_switch_event), this);

	return this;
}

