/******************************************************************************/
/* File: connector.c
   Author: N.Kim
   Abstract: Simple C DBus/BlueZ connection/pairing example */
/******************************************************************************/

#include <stdio.h>
#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>

#include <gio/gio.h>
#include <gio/gnetworking.h>
#include <glib.h>

#define SCAN_TIMEOUT_SECONDS 10
#define MAIN_TIMEOUT_SECONDS 60

struct
{
  gint hci_id;
  guint scan_subscription_id;

  char *bus_name;
  char *interface_name;
  char *object_path;
} adapter_info = {
  -1,
  0,
  NULL,
  NULL,
  NULL
};

GList *devices = NULL;
GMainLoop *scan_loop = NULL;
GMainLoop *main_loop = NULL;
GDBusConnection *conn = NULL;
gboolean loop_exit = FALSE;

/******************************************************************************/
/* On SIGINT, set the global variable that controls the state of the main
   processing loop (the one after scanning/connecting) */

static void on_sigint_received(int signo)
{
  if (signo == SIGINT) loop_exit = TRUE;
}

/******************************************************************************/
/* GIO suggests to drop the dynamically allocated content of a list manually */

void free_device_string(gpointer data)
{
  free((gchar *)data);
}

/******************************************************************************/
/* Watch the state of the adapter and insert detected remote devices as
   they come in */

static void on_adapter_changed(GDBusConnection *conn,
                               const gchar *sender_name,
                               const gchar *object_path,
                               const gchar *interface_name,
                               const gchar *signal_name,
                               GVariant *parameters,
                               gpointer user_data)
{
  const char *adapter_path = adapter_info.object_path;

  if (g_strcmp0(object_path, adapter_path) == 0)
    return;

  if (g_list_length(devices) > 10)
    return;

  if (object_path && strstr(object_path, adapter_path) != NULL)
  {
    GList *l;
    gboolean found = FALSE;

    for (l = devices; l != NULL; l = l->next)
    {
      if (g_strcmp0(l->data, object_path) == 0) {
        found = TRUE;
	break;
      }
    }

    if (found == FALSE)
    {
      devices = g_list_prepend(devices, g_strdup(object_path));
    }
  }
}

/******************************************************************************/
/* Limit the scan period to a specified number of seconds */

static gboolean on_scan_timeout(gpointer user_data)
{
  g_assert(scan_loop != NULL);
  g_main_loop_quit(scan_loop);
  return G_SOURCE_REMOVE;
}

/******************************************************************************/
/* When there is nothing to do, do nothing */

static gboolean on_loop_idle(gpointer user_data)
{
  if (loop_exit == TRUE)
  {
    g_main_loop_quit(main_loop);
    return G_SOURCE_REMOVE;
  }

  return G_SOURCE_CONTINUE;
}

/******************************************************************************/
/* Not strictly necessary for this example, as most paths/names can be
   hardcoded for such a simple example, but i provide it anyway */

static int acquire_adapter_info(void)
{
  int dev_id = hci_get_route(NULL);
  struct hci_dev_info hci_info;

  if (dev_id < 0 || hci_devinfo(dev_id, &hci_info) < 0)
    return -1;

  adapter_info.hci_id = dev_id;
  adapter_info.bus_name = "org.bluez";
  adapter_info.interface_name = "org.bluez.Adapter1";

  char *root_path = "/org/bluez/";
  const int path_length =
    strlen(root_path) + strlen(hci_info.name) + 1;

  char *ad_path = (char *)malloc(path_length);

  if (ad_path != NULL)
  {
    strcpy(ad_path, root_path);
    strcat(ad_path, hci_info.name);
    adapter_info.object_path = ad_path;

    return 0;
  }

  return -1;
}

/******************************************************************************/
/* Kick off the discovery process and subsequently listen to the signal
   'PropertiesChanged' on the BlueZ bus. React to it in 'on_adapter_changed'
   (i.e. inspect the state and add new devices) */

static int enable_device_discovery(GDBusConnection *conn, gboolean enable)
{
  g_assert(conn != NULL && "Expected a valid D-Bus connection!");

  if (enable == TRUE)
  {
    adapter_info.scan_subscription_id =
      g_dbus_connection_signal_subscribe(conn,
                                         "org.bluez",
                                         "org.freedesktop.DBus.Properties",
                                         "PropertiesChanged",
                                         NULL,
                                         NULL,
                                         G_DBUS_SIGNAL_FLAGS_NONE,
                                         on_adapter_changed,
                                         NULL,
                                         NULL);
  }
  else
  {
    g_dbus_connection_signal_unsubscribe(conn,
                                         adapter_info.scan_subscription_id);
  }

  char *method_call = enable ? "StartDiscovery" : "StopDiscovery";

  GError *error = NULL;
  GVariant *res =
    g_dbus_connection_call_sync(conn,
                                adapter_info.bus_name,
                                adapter_info.object_path,
                                adapter_info.interface_name,
                                method_call,
                                NULL,
                                NULL,
                                G_DBUS_CALL_FLAGS_NONE,
                                -1,
                                NULL,
                                &error);

  g_variant_unref(res);

  if (error != NULL) return -1;
  else return 0;
}

/******************************************************************************/
/* Once the discovery has finished, we are offered a list of all detected
   devices. Pick one of them */

static gchar *select_device(void)
{
  guint num_devices = 0;
  GList *l = NULL;

  g_print("Detected devices:\n");

  for (l = devices; l != NULL; l = l->next)
  {
    g_print("  %d: %s\n", num_devices++, (gchar*) l->data);
  }

  if (num_devices == 0)
  {
    g_print("  none\n");
    return NULL;
  }

  g_print("Select device number: ");

  const char id = getchar();
  const guint key = id - 0x30;

  if (key < 0 || key > 9 || key >= g_list_length(devices))
  {
    g_print("invalid selection\n");
    return NULL;
  }

  gchar *device = g_list_nth_data(devices, key);
  return device;
}

/******************************************************************************/
/* Finally, establish a connection to the device selected in 'select_device' */

static int connect_device(GDBusConnection *conn,
                          gchar *device_path,
                          gboolean connect)
{
  g_assert(conn != NULL && "Expected a valid D-Bus connection!");
  g_assert(device_path != NULL && "Expected a valid device path!");

  GError *error = NULL;

  if (connect)
  {
    GVariant *params =
      g_variant_new("(ss)", "org.bluez.Device1", "Paired");

    GVariant *q_res =
      g_dbus_connection_call_sync(conn,
                                  "org.bluez",
                                  device_path,
                                  "org.freedesktop.DBus.Properties",
                                  "Get",
                                  params,
                                  NULL,
                                  G_DBUS_CALL_FLAGS_NONE,
                                  -1,
                                  NULL,
                                  &error);

    if (error != NULL)
    {
      g_variant_unref(q_res);
      g_print(
        "Error reading remote properties: %s\n", error->message);

      return -1;
    }

    GVariant *value = NULL;
    g_variant_get(q_res, "(v)", &value);
    gboolean paired = g_variant_get_boolean(value);

    g_variant_unref(q_res);
    g_variant_unref(value);

    if (paired == TRUE)
      g_print("  device already paired\n");
    else
    {
      g_print("  device not yet paired, pairing...");

      GVariant *p_res =
        g_dbus_connection_call_sync(conn,
                                    "org.bluez",
                                    device_path,
                                    "org.bluez.Device1",
                                    "Pair",
                                    NULL,
                                    NULL,
                                    G_DBUS_CALL_FLAGS_NONE,
                                    -1,
                                    NULL,
                                    &error);

      if (p_res != NULL) g_variant_unref(p_res);

      if (error != NULL)
      {
        g_print("failed\n");
        return -1;
      } else g_print("ok\n");
    }
  }

  char *method_call = connect ? "Connect" : "Disconnect";

  GVariant *res =
    g_dbus_connection_call_sync(conn,
                                "org.bluez",
                                device_path,
                                "org.bluez.Device1",
                                method_call,
                                NULL,
                                NULL,
                                G_DBUS_CALL_FLAGS_NONE,
                                -1,
                                NULL,
                                &error);

  if (res != NULL)
    g_variant_unref(res);

  if (error != NULL || res == NULL)
  {
    if (connect)
      g_print("  connection failed\n");
    else g_print("  disconnection failed\n");

    return -1;
  }
  else
  {
    if (connect)
      g_print("  connection established\n");
    else g_print("  connection terminated\n");

    return 0;
  }
}

/******************************************************************************/

int main(int argc, char **argv)
{
  /* first of all, acquire the info about the adapter we are going to use and
     initialize the destination strings for the incoming RPC */
  g_print("Acquiring adapter info...");

  if (acquire_adapter_info() < 0)
  {
    g_print("failed\n");
    return -1;
  }
  else
  {
    g_print("ok\n");
    g_print("  id: %d\n", adapter_info.hci_id);
    g_print("  bus name: %s\n", adapter_info.bus_name);
    g_print("  interface name: %s\n", adapter_info.interface_name);
    g_print("  object path: %s\n\n", adapter_info.object_path);
  }

  GError *err = NULL;

  /* estsablish a connection to D-Bus which in this case must be a system ty-
     pe. Accession the session bus results in bluez path's being unknown.
     Thus, you might want to adjust your DBus permission policy */
  conn = g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL, &err);

  g_print("Connecting to the system D-Bus...");

  if (err != NULL || conn == NULL)
  {
    g_print("failed\n");
    return -1;
  } else g_print("ok\n");

  /* given a valid connection and the basic info about the adapter in use, we
     now can initiate the scanning process to look for available remote
     devices */
  g_print(
    "Starting discovery service for %d seconds...", SCAN_TIMEOUT_SECONDS);

  if (enable_device_discovery(conn, TRUE) < 0)
  {
    g_print("failed\n");
    goto done;
  } else g_print("ok\n");

  scan_loop = g_main_loop_new (NULL, FALSE);

  g_timeout_add_seconds(SCAN_TIMEOUT_SECONDS, on_scan_timeout, NULL);
  g_main_loop_run(scan_loop);
  g_main_loop_unref(scan_loop);

  /* after having done what is needed, terminate the scanning process expli-
     citly given a valid connection and the basic info about the adapter in use,
     we now can initiate the scanning process to look for available remote
     devices */
  g_print("Terminating discovery service...");

  if (enable_device_discovery(conn, FALSE) < 0)
  {
    g_print("failed\n");
    goto done;
  } else g_print("ok\n");

  /* inspect the list of collected devices and let the user pick one of them.
     Upon a valid selection a device name as contained in the device list is
     returned */
  gchar *device = select_device();

  if (device == NULL) goto done;

  /* now try to establish a connection to the device selected above. This also
     should initiate/invoke a pin pairing */
  g_print("Connecting to %s\n", device);

  if (connect_device(conn, device, TRUE) != 0) goto done;

  g_print("\nConnected, use 'SIGINT' (e.g. Ctrl-C) to disconnect...");

  signal(SIGINT, on_sigint_received);

  main_loop = g_main_loop_new (NULL, FALSE);
  g_idle_add(on_loop_idle, NULL);
  g_main_loop_run(main_loop);
  g_main_loop_unref(main_loop);

  /* once the terminator has been detected, close the connection explicitly,
     and perform final memory housekeeping */
  g_print("\nDisconnecting from %s\n", device);

  connect_device(conn, device, FALSE);

done:
  /* concerning adapter info, we only need to free the object path, the rest
     points to const literals */
  free(adapter_info.object_path);

  g_object_unref(conn);

  /* GIO manual says, if a list contains dynamically allocated things, they
     have to be dropped manually. This is not really a must at program's
     exit, but let's do it anyway to make valgrind happy */
  g_list_free_full(devices, free_device_string);

  return 0;
}
