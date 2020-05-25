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

/* bluez and GDBus paths and interfaces */
#define BLUEZ_OBJECT_ROOT "/org/bluez/"
#define BLUEZ_BUS_NAME "org.bluez"
#define BLUEZ_ADVERT_IFACE "org.bluez.LEAdvertisement1"
#define BLUEZ_ADVERT_MAN_IFACE "org.bluez.LEAdvertisingManager1"
#define DBUS_PROPERTIES_IFACE "org.freedesktop.DBus.Properties"

/* own object to be registered */
#define ADVERT_OBJECT_PATH "/org/bitzap/dev/advertising"

/* adapter path hardcoded. See 'Connection tutorial' how to
   extract it */
#define ADAPTER_PATH "/org/bluez/hci0"

/******************************************************************************/

struct advertisement_data {
  /* Bluetooth SIG: 16-bit service uuids. BlueZ reserves 2
     additional bytes for 16,32 and 128 bit uuids. Thus, our
     uuid list occupies 8 bytes in total */
  char *uuids[3];

  /* pay attention to the overal packet length (31 byte in total,
     including uuids, service data, manufacturer data, local name,
     etc.). Make sure it fits */
  char *service_data;

  /* just for internal navigation, not part of the ad,
     allow cycling through uuids, each time changing the
     data to be advertised */
  int curr_uuid;

} adv_data = {

  /* first name, second name, email */
  { "0x2a8a", "0x2a90", "0x2a87" },

  /* dynamic data advertising content */
  NULL,

  /* index of the 'current' service uuid */
  0,
};

/******************************************************************************/
/* Interfaces we implement

   For simplicity, we maintain a minimal number of properties.
   Also, we only support the 'GetAll' method and do not allow
   properties to be 'Set' remotely */

static const gchar introspect_xml[] =
  "<node>"
  "  <interface name='org.bluez.LEAdvertisement1'>"
  "    <property name='ServiceUUIDs' type='as' access='read'/>"
  "    <property name='ServiceData' type='a{sv}' access='read'/>"
  "  </interface>"
  "  <interface name='org.freedesktop.DBus.Properties'>"
  "    <method name='GetAll'>"
  "      <arg name='interface_name' type='s' direction='in'/>"
  "      <arg name='props' type='a{sv}' direction='out'/>"
  "    </method>"
  "    <signal name='PropertiesChanged'>"
  "      <arg type='s' name='interface_name'/>"
  "      <arg type='a{sv}' name='changed_properties'/>"
  "      <arg type='as' name='invalidated_properties'/>"
  "    </signal>"
  "  </interface>"
"</node>";

static GDBusNodeInfo *introspect_data = NULL;
static GMainLoop *loop = NULL;
static GDBusConnection *conn = NULL;

/* global event processing flags */
static int loop_done = 0;
static int adv_changed = 0;

/******************************************************************************/

void on_signal_received(int signo)
{
  if (signo == SIGQUIT)
  {
    /* terminate */
    loop_done = 1;
  }
  else if (signo == SIGINT)
  {
    /* change advertisement data */
    adv_changed = 1;
  }
}

/******************************************************************************/
/* Pack maintained uuids into a GVariant */

static GVariant *get_service_uuids(void)
{
  GVariantBuilder *builder = g_variant_builder_new(G_VARIANT_TYPE("as"));

  g_variant_builder_add(builder, "s", adv_data.uuids[0]);
  g_variant_builder_add(builder, "s", adv_data.uuids[1]);
  g_variant_builder_add(builder, "s", adv_data.uuids[2]);

  GVariant *result = g_variant_builder_end(builder);
  return result;
}

/******************************************************************************/
/* Pack maintained service data into a GVariant */

static GVariant *get_service_data(void)
{
  int i;

  /* remember the size limit of 31 bytes in total (i.e.
     including uuids) */
  if (adv_data.curr_uuid == 0) adv_data.service_data = "Stan";
  else if (adv_data.curr_uuid == 1) adv_data.service_data = "Satan";
  else if (adv_data.curr_uuid == 2) adv_data.service_data = "stan@sat.an";

  GVariantBuilder *data_builder =
    g_variant_builder_new(G_VARIANT_TYPE("ay"));

  /* pack each character of the service data string */
  for (i = 0; i < strlen(adv_data.service_data); ++i)
  {
    g_variant_builder_add(data_builder, "y", adv_data.service_data[i]);
  }

  GVariant *data = g_variant_builder_end(data_builder);

  /* array of dictionaries */
  GVariantBuilder *builder =
    g_variant_builder_new(G_VARIANT_TYPE("a{sv}"));

  /* (string, variant) pair, connect a particular uuid with
     the corresponding service data string */
  g_variant_builder_add(builder, "{sv}",
    adv_data.uuids[adv_data.curr_uuid], data);

  GVariant *result = g_variant_builder_end(builder);

  g_variant_builder_unref(data_builder);
  g_variant_builder_unref(builder);

  return result;
}

/******************************************************************************/
/* 'Method call' callback, read and return all properties at once */

static void on_method_call(GDBusConnection *con,
                           const gchar *sender,
                           const gchar *obj_path,
                           const gchar *iface_name,
                           const gchar *method_name,
                           GVariant *params,
                           GDBusMethodInvocation *invoc,
                           gpointer udata)
{
  /* only support 'GetAll' via 'org.freedesktop.DBus.Properties'
     interface */
  if (strcmp(iface_name, DBUS_PROPERTIES_IFACE) == 0)
  {
    if (strcmp(method_name, "GetAll") == 0)
    {
      g_print("Reading properties...");

      GVariantBuilder *builder =
        g_variant_builder_new(G_VARIANT_TYPE("a{sv}"));

      g_variant_builder_add(
        builder, "{sv}", "ServiceUUIDs", get_service_uuids());

      g_variant_builder_add(
        builder, "{sv}", "ServiceData", get_service_data());

      GVariant *result = g_variant_builder_end(builder);
      g_variant_builder_unref(builder);

      /* required to be a tuple, even if containing only one
         'out' argument */
      GVariant *tuples[1] = { result };

      g_dbus_method_invocation_return_value(invoc,
        g_variant_new_tuple(tuples, 1));

      g_print("ok\n");
    }
  }
}

/******************************************************************************/
/* Provide a simple user interaction */

static gboolean on_loop_idle(gpointer udata)
{
  /* triggered via SIGQUIT */
  if (loop_done)
  {
    g_main_loop_quit(loop);
    return G_SOURCE_REMOVE;
  }

  if (adv_changed)
  {
    /* cycle through uuids */
    adv_data.curr_uuid = (adv_data.curr_uuid + 1) % 3;

    /* builder for the second argument to 'PropertiesChanged' */
    GVariantBuilder *prop_builder =
      g_variant_builder_new(G_VARIANT_TYPE("a{sv}"));

    /* builder for the third argument to 'PropertiesChanged' */
    GVariantBuilder *inv_prop_builder =
      g_variant_builder_new(G_VARIANT_TYPE("as"));

    g_variant_builder_add(
      prop_builder, "{sv}", "ServiceData", get_service_data());

    g_print("\nadvertising service data:\n");
    g_print("  uuid: %s\n", adv_data.uuids[adv_data.curr_uuid]);
    g_print("  data: %s\n\n", adv_data.service_data);

    GVariant *args[3] = {
      g_variant_new_string(BLUEZ_ADVERT_IFACE),
      g_variant_builder_end(prop_builder),
      g_variant_builder_end(inv_prop_builder)
    };

    gboolean sig = g_dbus_connection_emit_signal(
      conn,
      BLUEZ_BUS_NAME,               /* org.bluez */
      ADVERT_OBJECT_PATH,           /* /org/bitzap/dev/advertising */
      DBUS_PROPERTIES_IFACE,        /* org.freedesktop.DBus.Properties */
      "PropertiesChanged",          /* signal 'PropertiesChanged'... */
      g_variant_new_tuple(args, 3), /* ...with 3 arguments */
      NULL);

    g_variant_builder_unref(prop_builder);
    g_variant_builder_unref(inv_prop_builder);
    adv_changed = 0;
  }

  return G_SOURCE_CONTINUE;
}

/******************************************************************************/

static void register_service(gboolean enable)
{
  GVariant *args;
  const gchar *method_name;

  if (enable)
  {
    GVariant *opv;
    GVariant *dict;
    GVariantBuilder *builder;

    method_name = "RegisterAdvertisement";
    opv = g_variant_new("o", ADVERT_OBJECT_PATH);

    builder = g_variant_builder_new(G_VARIANT_TYPE("a{sv}"));

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
    args = g_variant_new("(o)", ADVERT_OBJECT_PATH);
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
  g_print("  SIGQUIT (e.g. Ctrl-\\) to quit\n");
  g_print("  SIGINT (e.g. Ctrl-C ) to cycle advertisement data\n\n");

  conn = g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL, &err);
  g_print("Connecting to the system D-Bus...");

  if (err != NULL || conn == NULL)
  {
    g_print("failed\n");
    goto done;
  } else g_print("ok\n");

  introspect_data =
    g_dbus_node_info_new_for_xml(introspect_xml, NULL);

  g_assert(introspect_data != NULL);

  GDBusInterfaceInfo *iface_info[2];

  /* interface 'org.bluez.LEAdvertisement1' */
  iface_info[0] = g_dbus_node_info_lookup_interface(
    introspect_data, BLUEZ_ADVERT_IFACE);

  /* interface 'org.freedesktop.DBus.Properties' */
  iface_info[1] = g_dbus_node_info_lookup_interface(
    introspect_data, DBUS_PROPERTIES_IFACE);

  g_assert(iface_info[0] != NULL && iface_info[1] != NULL);
  g_print("Exporting advertising object...");

  GDBusInterfaceVTable interface_vtable;
  interface_vtable.method_call = on_method_call;
  interface_vtable.get_property = NULL;
  interface_vtable.set_property = NULL;

  guint reg_id1 =
    g_dbus_connection_register_object(conn,
                                      ADVERT_OBJECT_PATH,
                                      iface_info[0],
                                      &interface_vtable,
                                      NULL,
                                      NULL,
                                      &err);

  guint reg_id2 =
    g_dbus_connection_register_object(conn,
                                      ADVERT_OBJECT_PATH,
                                      iface_info[1],
                                      &interface_vtable,
                                      NULL,
                                      NULL,
                                      &err);

  if (err != NULL) {
    g_print("failed\n");
    goto done;
  } else g_print("ok\n");

  signal(SIGINT, on_signal_received);
  signal(SIGQUIT, on_signal_received);

  /* now actually try to register our exported object as a service.
     This will trigger the extraction of properties via 'GetAll' 
     and start broadcasting the data */
  g_print("Registering service...");

  register_service(TRUE);

  g_print("ok\n");

  /* kick off the main loop now. Be careful not to have anything
     blocking in 'on_loop_idle' */
  loop = g_main_loop_new(NULL, FALSE);
  g_idle_add(on_loop_idle, NULL);
  g_main_loop_run(loop);

  /* we are done, tear everything down now */
  g_print("\nUnregistering advertising object...");

  if (g_dbus_connection_unregister_object(conn, reg_id1) == FALSE
  || g_dbus_connection_unregister_object(conn, reg_id2) == FALSE)
  {
    g_print("failed\n");
  }
  else
  {
    g_print("ok\n");
    g_print("Unregistering service...");

    register_service(FALSE);

    g_print("ok\n");
  }

/* housekeeping target */
done:
  g_main_loop_unref(loop);
  g_dbus_node_info_unref(introspect_data);
  g_object_unref(conn);

  return 0;
}

