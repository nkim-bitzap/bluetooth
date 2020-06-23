/******************************************************************************/
/* File: advertizer.c
   Author: N.Kim
   Abstract: Simple C DBus/BlueZ advertising example */
/******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>

#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>

#include <gio/gio.h>
#include <gio/gnetworking.h>
#include <glib.h>

/******************************************************************************/
/* introspection for the advertisement we provide */

static const gchar advert_le_xml[] =
  "<node>"
  "  <interface name='org.bluez.LEAdvertisement1'>"
  "    <property name='ServiceUUIDs' type='as' access='read'/>"
  "    <property name='ServiceData' type='a{sv}' access='read'/>"
  "  </interface>"
  ""
  "  <interface name='org.freedesktop.DBus.Properties'>"
  "    <method name='GetAll'>"
  "      <arg name='interface_name' type='s' direction='in'/>"
  "      <arg name='props' type='a{sv}' direction='out'/>"
  "    </method>"
  ""
  "    <signal name='PropertiesChanged'>"
  "      <arg type='s' name='interface_name'/>"
  "      <arg type='a{sv}' name='changed_properties'/>"
  "      <arg type='as' name='invalidated_properties'/>"
  "    </signal>"
  "  </interface>"
"</node>";

struct {
  const gchar *service_uuid;
  const gchar *service_data;
}
advert_data = {
  /* NOTE, we need to use the mesh provisioning service in order
     to be considered for provisioning. We use a 16-bit variant
     here for a more compact representation */
  "1827",

  /* NOTE, we need to submit the device 'address' for provisioning
     and it has to contain at least 18 bytes with the last two
     bytes (i.e. index 16 and 17) being the (shuffled) OOB */
  "cafebabedeadfaceBT"
};

/*******************************************************************************/

#define ADAPTER_PATH "/org/bluez/hci0"
#define DBUS_PROPERTIES_IFACE "org.freedesktop.DBus.Properties"

#define BLUEZ_BUS_NAME "org.bluez"
#define BLUEZ_ADVERT_IFACE "org.bluez.LEAdvertisement1"
#define BLUEZ_ADVERT_MAN_IFACE "org.bluez.LEAdvertisingManager1"
#define BITZAP_ADVERT_OBJECT_PATH "/org/bitzap/advertisement"

static GMainLoop *loop = NULL;
static gboolean loop_done = FALSE;

/******************************************************************************/
/* Signal handler, used to terminate the main event loop */

void on_signal_received(int signo)
{
  if (signo == SIGINT) loop_done = TRUE;
}

/******************************************************************************/
/* Pack advertisement service uuid into a GVariant */

static GVariant *get_advert_uuid()
{
  GVariantBuilder *builder =
    g_variant_builder_new(G_VARIANT_TYPE("as"));

  g_variant_builder_add(builder, "s", advert_data.service_uuid);

  GVariant *result = g_variant_builder_end(builder);
  g_variant_builder_unref(builder);

  return result;
}

/******************************************************************************/
/* Pack advertisement service data into a GVariant */

static GVariant *get_advert_data()
{
  int i;

  GVariantBuilder *data_builder =
    g_variant_builder_new(G_VARIANT_TYPE("ay"));

  /* the data is expected to be an array of bytes and not
     just a plain string, thus, pack each character of the
     service data string separately */
  for (i = 0; i < strlen(advert_data.service_data); ++i)
  {
    g_variant_builder_add(
      data_builder, "y", advert_data.service_data[i]);
  }

  GVariant *data = g_variant_builder_end(data_builder);

  GVariantBuilder *res_builder =
    g_variant_builder_new(G_VARIANT_TYPE("a{sv}"));

  g_variant_builder_add(
    res_builder, "{sv}", advert_data.service_uuid, data);

  GVariant *result = g_variant_builder_end(res_builder);

  g_variant_builder_unref(data_builder);
  g_variant_builder_unref(res_builder);

  return result;
}

/******************************************************************************/
/* React to calls via advertisement interface */

static void on_advert_method_call(GDBusConnection *con,
                                  const gchar *sender,
                                  const gchar *obj_path,
                                  const gchar *iface_name,
                                  const gchar *method_name,
                                  GVariant *params,
                                  GDBusMethodInvocation *invoc,
                                  gpointer udata)
{
  if (strcmp(iface_name, DBUS_PROPERTIES_IFACE) == 0)
  {
    if (strcmp(method_name, "GetAll") == 0)
    {
      GVariantBuilder *builder =
        g_variant_builder_new(G_VARIANT_TYPE("a{sv}"));

      g_variant_builder_add(
        builder,
        "{sv}",
        "ServiceUUIDs",
        get_advert_uuid());

      g_variant_builder_add(
        builder,
        "{sv}",
        "ServiceData",
        get_advert_data());

      GVariant *result = g_variant_builder_end(builder);
      GVariant *tuples[1] = { result };
      g_variant_builder_unref(builder);

      g_dbus_method_invocation_return_value(invoc,
        g_variant_new_tuple(tuples, 1));
    }
  }
}

/******************************************************************************/
/* Poll the loop exit condition */

static gboolean on_loop_timeout(gpointer udata)
{
  if (loop_done)
  {
    g_main_loop_quit(loop);
    return G_SOURCE_REMOVE;
  }

  return G_SOURCE_CONTINUE;
}

/******************************************************************************/
/* Register and start advertising */

static void register_advertisement(GDBusConnection *conn, gboolean enable)
{
  GVariant *args;
  const gchar *method_name;

  if (enable)
  {
    GVariant *opv;
    GVariant *dict;
    GVariantBuilder *builder;

    method_name = "RegisterAdvertisement";
    opv = g_variant_new("o", BITZAP_ADVERT_OBJECT_PATH);

    builder = g_variant_builder_new(G_VARIANT_TYPE("a{sv}"));

    /* dummy dictionary entry */
    g_variant_builder_add(
      builder, "{sv}", "param", g_variant_new_string("value"));

    dict = g_variant_builder_end(builder);
    g_variant_builder_unref(builder);

    GVariant *params[2] = { opv, dict };
    args = g_variant_new_tuple(params, 2);
  }
  else
  {
    method_name = "UnregisterAdvertisement";
    args = g_variant_new("(o)", BITZAP_ADVERT_OBJECT_PATH);
  }

  /* off we go */
  g_dbus_connection_call(
    conn,
    BLUEZ_BUS_NAME,          /* org.bluez */
    ADAPTER_PATH,            /* /org/bluez/hci0 */
    BLUEZ_ADVERT_MAN_IFACE,  /* org.bluez.LEAdvertisingManager1 */
    method_name,             /* Register/UnregisterAdvertisement */
    args,                    /* /org/bitzap/dev/advertisement */
    NULL,
    G_DBUS_CALL_FLAGS_NONE,
    G_MAXINT,
    NULL,
    NULL,
    NULL);
}

/******************************************************************************/

int main(int argc, char **argv)
{
  GError *err = NULL;

  g_print("\nUse the following commands:\n");
  g_print("  SIGINT (e.g. Ctrl-C ) to quit\n\n");

  GDBusConnection *conn =
    g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL, &err);

  if (err != NULL || conn == NULL) return 0;

  g_print("Connected to the system D-Bus\n");

  GDBusNodeInfo *advert_info =
    g_dbus_node_info_new_for_xml(advert_le_xml, NULL);

  g_assert(advert_info != NULL);

  GDBusInterfaceInfo *advert_iface[2];

  /* interface 'org.bluez.LEAdvertisement1' */
  advert_iface[0] = g_dbus_node_info_lookup_interface(
    advert_info, BLUEZ_ADVERT_IFACE);

  /* interface 'org.freedesktop.DBus.Properties' */
  advert_iface[1] = g_dbus_node_info_lookup_interface(
    advert_info, DBUS_PROPERTIES_IFACE);

  g_assert(
    advert_iface[0] != NULL && advert_iface[1] != NULL);

  GDBusInterfaceVTable advert_vtable;
  advert_vtable.method_call = on_advert_method_call;
  advert_vtable.get_property = NULL;
  advert_vtable.set_property = NULL;

  guint advert_reg_id1 =
    g_dbus_connection_register_object(conn,
                                      BITZAP_ADVERT_OBJECT_PATH,
                                      advert_iface[0],
                                      &advert_vtable,
                                      NULL,
                                      NULL,
                                      &err);

  guint advert_reg_id2 =
    g_dbus_connection_register_object(conn,
                                      BITZAP_ADVERT_OBJECT_PATH,
                                      advert_iface[1],
                                      &advert_vtable,
                                      NULL,
                                      NULL,
                                      &err);

  if (err != NULL) goto done;

  g_print("Registered LE advertisement\n");

  /* now actually start advertising */
  register_advertisement(conn, TRUE);
  g_print("Started LE advertisement\n");

  signal(SIGINT, on_signal_received);

  loop = g_main_loop_new(NULL, FALSE);

  /* prefer 'g_timeout_add' over 'g_idle_add' for a much
     friendlier resource utilization */
  g_timeout_add(250, on_loop_timeout, NULL);
  g_main_loop_run(loop);
  g_main_loop_unref(loop);

  /* we are done, tear everyting down */
  register_advertisement(conn, FALSE);
  g_print("\nUnregistered LE advertisement\n");

  g_dbus_connection_unregister_object(conn, advert_reg_id1);
  g_dbus_connection_unregister_object(conn, advert_reg_id2);
  g_print("Unregistered objects\n");

done:
  g_dbus_node_info_unref(advert_info);
  g_object_unref(conn);

  return 0;
}

