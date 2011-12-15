/*
 * fjbtndrv dbus proxy this
 *
 * Copyright (C) Robert Gerlach 2011 <khnz@users.sourceforge.net>
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

#include <stdlib.h> // for exit() only
#include <glib.h>
#include <gio/gio.h>

#include "fjbtndrv.h"
#include "fjbtndrv-proxy.h"

static const gchar introspection_xml[] =
	"<node>"
	"  <interface name='" FJBTNDRV_DBUS_SERVICE_INTERFACE "'>"
	"    <property type='b' name='TabletMode' access='read' />"
	"    <signal name='TabletModeChanged'>"
	"      <arg direction='out' name='value' type='b' />"
	"    </signal>"
	"    <property type='b' name='DockState' access='read' />"
	"    <signal name='DockStateChanged'>"
	"      <arg direction='out' name='value' type='b' />"
	"    </signal>"
	"  </interface>"
	"</node>";

static GDBusNodeInfo *introspection_data = NULL;

#define FJBTNDRV_PROXY_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), FJBTNDRV_TYPE_PROXY, FjbtndrvProxyPrivate))

struct _FjbtndrvProxyPrivate {
	gboolean tablet_mode;
	gboolean dock_state;
};

G_DEFINE_TYPE (FjbtndrvProxy, fjbtndrv_proxy, G_TYPE_OBJECT);


static GVariant *
fjbtndrv_proxy_dbus_get_property(GDBusConnection *connection, const gchar *sender, const gchar *object_path, const gchar *interface_name, const gchar *property_name, GError **error, gpointer user_data)
{
	GVariant *value = NULL;
	FjbtndrvProxy *this = user_data;
	FjbtndrvProxyPrivate *priv = FJBTNDRV_PROXY_GET_PRIVATE(this);

	debug("handle_get_property: sender=%s path=%s interface=%s name=%s",
			sender, object_path, interface_name, property_name);

	if (g_strcmp0 (property_name, "TabletMode") == 0) {
		value = g_variant_new_boolean(priv->tablet_mode);
	}
	else if (g_strcmp0 (property_name, "DockState") == 0) {
		value = g_variant_new_boolean(priv->dock_state);
	}

	return value;
}

static const GDBusInterfaceVTable fjbtndrv_proxy_vtable = {
	NULL,
	fjbtndrv_proxy_dbus_get_property,
	NULL,
};

static void
on_bus_acquired (GDBusConnection *connection, const gchar *name, gpointer user_data)
{
	FjbtndrvProxy *this = user_data;
	GError *error = NULL;

	debug("on_bus_acquired: name=%s", name);

	guint id = g_dbus_connection_register_object(
			connection, 
			FJBTNDRV_DBUS_SERVICE_PATH,
			introspection_data->interfaces[0],
			&fjbtndrv_proxy_vtable,
			this, NULL, &error);
	if (error)
		g_error("%s", error->message);

	g_assert (id > 0);
}

static void
on_name_acquired (GDBusConnection *connection, const gchar *name, gpointer user_data)
{
	FjbtndrvProxy *this = user_data;

	debug("on_name_acquired: name=%s", name);

	this->dbus = connection;
}

static void
on_name_lost (GDBusConnection *connection, const gchar *name, gpointer user_data)
{
	FjbtndrvProxy *this = user_data;

	debug("on_name_lost: name=%s", name);

	this->dbus = NULL;

	// TODO: reconnect
	exit(0);
}


static void
fjbtndrv_proxy_emit_signal(FjbtndrvProxy *this, const char *name, GVariant *parameters)
{
	GError *error = NULL;

	g_assert(this);

	if (!this->dbus)
		return;

	debug("fjbtndrv_proxy_emit_signal: signal=%s parameters=%s",
			name, g_variant_print(parameters, TRUE));

	g_dbus_connection_emit_signal(
			this->dbus,
			NULL,
			FJBTNDRV_DBUS_SERVICE_PATH,
			FJBTNDRV_DBUS_SERVICE_INTERFACE,
			name,
			parameters,
			&error);
	if (error) {
		g_warning("%s", error->message);
		g_error_free(error);
	}
}

void
fjbtndrv_proxy_set_tablet_mode(FjbtndrvProxy *this, gboolean value)
{
	FjbtndrvProxyPrivate *priv = FJBTNDRV_PROXY_GET_PRIVATE(this);

	debug("fjbtndrv_proxy_set_tablet_mode: value=%d", value);

	GVariant *v_value = g_variant_new("(b)", value);

	priv->tablet_mode = value;
	fjbtndrv_proxy_emit_signal(this, "TabletModeChanged", v_value);

	g_variant_unref(v_value);
}

void
fjbtndrv_proxy_set_dock_state(FjbtndrvProxy *this, gboolean value)
{
	FjbtndrvProxyPrivate *priv = FJBTNDRV_PROXY_GET_PRIVATE(this);

	debug("fjbtndrv_proxy_set_dock_state: value=%d", value);

	GVariant *v_value = g_variant_new("(b)", value);

	priv->dock_state = value;
	fjbtndrv_proxy_emit_signal(this, "DockStateChanged", v_value);

	g_variant_unref(v_value);
}

/*
static void
fjbtndrv_proxy_get_property(GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
	//FjbtndrvProxy *this = FJBTNDRV_PROXY(object);
	//FjbtndrvProxyPrivate *priv = this->priv;

	//g_assert(this);

	switch (prop_id) {
	case PROP_TABLET_MODE:
		g_value_set_boolean(value, FjbtndrvState.tablet_mode);
		break;
	case PROP_DOCK_STATE:
		g_value_set_boolean(value, FjbtndrvState.dock_state);
		break;
	}
}

static void
fjbtndrv_proxy_set_property(GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
	FjbtndrvProxy *this = FJBTNDRV_PROXY(object);
	//FjbtndrvProxyPrivate *priv = this->priv;
	char *signal_name = NULL;
	GVariant *signal_data = NULL;
	
	g_assert(this);

	switch (prop_id) {
	case PROP_TABLET_MODE:
		FjbtndrvState.tablet_mode = g_value_get_boolean(value);
		fjbtndrv_proxy_emit_signal(this, "TabletModeChanged",
				g_variant_new("(b)", FjbtndrvState.tablet_mode));
		break;

	case PROP_DOCK_STATE:
		FjbtndrvState.dock_state = g_value_get_boolean(value);
		fjbtndrv_proxy_emit_signal(this, "DockStateChanged",
				g_variant_new("(b)", FjbtndrvState.tablet_mode));
		break;
	}

	// emit dbus signal
	if (signal_name) {

		fjbtndrv_proxy_emit_signal(this, signal_name, signal_data);
		g_variant_unref(signal_data);
	}


}
*/

static void
fjbtndrv_proxy_init (FjbtndrvProxy *this)
{
	guint dbus_owner_id;

	//this->priv = FJBTNDRV_PROXY_GET_PRIVATE(this);

	dbus_owner_id = g_bus_own_name (
			G_BUS_TYPE_SYSTEM,
			FJBTNDRV_DBUS_SERVICE_NAME,
			G_BUS_NAME_OWNER_FLAGS_NONE,
			on_bus_acquired,
			on_name_acquired,
			on_name_lost,
			this,
			NULL);

	g_assert(dbus_owner_id > 0);
}

static void
fjbtndrv_proxy_finalize (GObject *object)
{
	G_OBJECT_CLASS (fjbtndrv_proxy_parent_class)->finalize (object);
}

static void
fjbtndrv_proxy_class_init (FjbtndrvProxyClass *klass)
{
	GObjectClass* object_class = G_OBJECT_CLASS (klass);
	//GParamSpec *pspec;

	object_class->finalize = fjbtndrv_proxy_finalize;

	introspection_data = g_dbus_node_info_new_for_xml (introspection_xml, NULL);
	g_assert (introspection_data);

	g_type_class_add_private(klass, sizeof(FjbtndrvProxyPrivate));
}


FjbtndrvProxy *
fjbtndrv_proxy_new (void)
{
	FjbtndrvProxy *this;

	this = g_object_new(FJBTNDRV_TYPE_PROXY, NULL);
	return FJBTNDRV_PROXY(this);
}

