/******************************************************************************/
/* File: rpm_control_profile.c
   Author: N.Kim
   Abstract: BlueZ profile for controlling RPM of brushless motors

   Description:
     Implementation of a BlueZ profile to be registered on a Bluetooth
     device connected to the ESP to be controlled (which in turn goes
     to a brushless motor propelling a small plane) */

/******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>

#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>

#include <gio/gio.h>
#include <gio/gnetworking.h>
#include <gio/gunixfdmessage.h>
#include <glib.h>

#include "profile_record.h"

static GDBusNodeInfo *introspect_data = NULL;
static GMainLoop *loop = NULL;

static int loop_done = 0;
static int data_sent = 0;

/******************************************************************************/

void on_signal_received(int signo)
{
  if (signo == SIGINT) loop_done = 1;
}

/******************************************************************************/

static gboolean on_loop_idle(gpointer udata)
{
  /* triggered via SIGINT */
  if (loop_done || data_sent)
  {
    g_main_loop_quit(loop);
    return G_SOURCE_REMOVE;
  }

  return G_SOURCE_CONTINUE;
}

/******************************************************************************/

void on_register_complete(GObject *source_object,
                          GAsyncResult *res,
                          gpointer user_data)
{
  g_print("Profile (un)registered\n");
}

/******************************************************************************/

static void register_profile(GDBusConnection *conn,
                             gboolean enable)
{
  GVariant *args;
  const gchar *method_name;

  if (enable)
  {
    GVariant *opv;
    GVariant *uuid;
    GVariant *dict;
    GVariantBuilder *builder;

    method_name = "RegisterProfile";

    /* argument 1: path to my object/profile implementation */
    opv = g_variant_new("o", PROFILE_OBJECT_PATH);

    /* argument 2: 128-bit SIG uuid. Use 'InterCom for now */
    uuid = g_variant_new_string(
      "00001110-0000-1000-8000-00805f9b34fb");

    /* argument 3: dictionary, for example, use 'Name' and 
       'RequireAuthentication' */
    builder = g_variant_builder_new(G_VARIANT_TYPE("a{sv}"));

    g_variant_builder_add(builder, "{sv}",
      "Name", g_variant_new_string("BITZAP InterCom"));

    g_variant_builder_add(builder, "{sv}",
      "Role", g_variant_new_string("client"));

    /* this is essential for the internal profile probing, i.e.
       SDP record registration. Pick a channel (i.e. a RFCOMM
       socket) that you know is free  */
    g_variant_builder_add(builder, "{sv}",
      "Channel", g_variant_new_uint16(27));

    g_variant_builder_add(builder, "{sv}",
      "RequireAuthorization", g_variant_new_boolean(TRUE));

    g_variant_builder_add(builder, "{sv}",
      "AutoConnect", g_variant_new_boolean(TRUE));

    /* lazy stuff, for now we restrict records to 2047 bytes at
       most. NOTE, that this is actually not that much and can
       be easily exceeded */
    gchar rec[2048] = {0};

    sprintf(rec, MY_INTERCOM_SDP_RECORD,
      27, 0xdead, "BITZAP-Intercom-Profile", 0);

    /* provide a service record to be inserted into the SDP
       database, test with 'sdptool' on your remote device
       whether you can find the record or not */
    g_variant_builder_add(builder, "{sv}", "ServiceRecord",
      g_variant_new_string(rec));

    dict = g_variant_builder_end(builder);
    g_variant_builder_unref(builder);

    GVariant *params[3] = { opv, uuid, dict };
    args = g_variant_new_tuple(params, 3);
  }
  else
  {
    method_name = "UnregisterProfile";
    args = g_variant_new("(o)", PROFILE_OBJECT_PATH);
  }

  /* off we go */
  g_dbus_connection_call(
    conn,
    "org.bluez",
    "/org/bluez",
    "org.bluez.ProfileManager1",
    method_name,
    args,
    NULL,
    G_DBUS_CALL_FLAGS_NONE,
    G_MAXINT,
    NULL,
    on_register_complete,
    NULL);
}

/******************************************************************************/

static gboolean data_writer(GIOChannel *channel,
                            GIOCondition condition,
                            gpointer user_data)
{
  if (condition & G_IO_OUT && data_sent == 0) {

    GError *err = NULL;
    gsize num_written;

    const gchar *data =
      "Satan oscillate my metallic sonatas";

    GIOStatus s = g_io_channel_write_chars(
      channel,
      data,
      strlen(data) + 1,
      &num_written,
      &err);

    if (s != G_IO_STATUS_NORMAL || err != NULL)
    { 
      g_print("Failed writing sample data\n");
    }
    else
    {
      g_print("Written sample data:\n");
      g_print("  '%s'\n", data);

      data_sent = 1;

      s = g_io_channel_shutdown(channel, TRUE, &err);

      if (s != G_IO_STATUS_NORMAL || err != NULL)
      { 
        g_print("Failed shutting down channel\n");
      }

      return TRUE;
    }
  }

  return FALSE;
}

/******************************************************************************/

static void on_method_call(GDBusConnection *con,
                           const gchar *sender,
                           const gchar *obj_path,
                           const gchar *iface_name,
                           const gchar *method_name,
                           GVariant *params,
                           GDBusMethodInvocation *invoc,
                           gpointer udata)
{
  g_print("Calling method '%s':\n", method_name);

  if (strcmp(method_name, "NewConnection") == 0) {
    g_print("  handling a new connection on the client side\n");

    GError *err = NULL;
    GVariant *dict = NULL;

    gint fd_index;
    gchar *path;

    /* now extract arguments submitted to 'NewConnection', for
       the time being don't require the feature-dictionary */
    g_variant_get(params, "(oh@a{sv})", &path, &fd_index, &dict);

    GUnixFDList *fd_list = g_dbus_message_get_unix_fd_list(
      g_dbus_method_invocation_get_message (invoc));

    g_assert(
      fd_list && "Failed obtaining a file descriptor list");

    g_assert(fd_index < g_unix_fd_list_get_length(fd_list) &&
             "File descriptor index out of range");

    gint fd = g_unix_fd_list_get(fd_list, fd_index, &err);

    g_assert(err == NULL && fd >= 0 &&
             "Failed extracting file descriptor");

    g_print("  obtained sender path: %s\n", path);
    g_print("  obtained file descriptor index: %d\n", fd_index);
    g_print("  obtained file descriptor: %d\n", fd);

    GIOChannel *channel = g_io_channel_unix_new(fd);

    g_assert(channel && "Failed creating a channel");

    g_io_channel_set_close_on_unref(channel, TRUE);
    g_io_channel_set_encoding(channel, NULL, NULL);
    g_io_channel_set_buffered(channel, FALSE);

    (void) g_io_add_watch(
      channel,      /* channel to watch */
      G_IO_OUT |    /* watch if we can write data */
      G_IO_HUP |    /* watch broken sockets */
      G_IO_NVAL |   /* watch invalid sockets */
      G_IO_ERR,     /* watch other errors */
      data_writer,  /* processing callback */
      NULL);        /* user data */

    g_io_channel_unref(channel);
    g_free(path);

    /* finally, process the dictionary and make valgrind happy */
    g_print("  processing dictionary argument:\n");

    gchar *dict_key;
    GVariant *dict_val;
    GVariantIter iter;
    
    g_variant_iter_init(&iter, dict);

    /* process the dictionary and make valgrind happy */
    while (g_variant_iter_loop(&iter, "{sv}", &dict_key, &dict_val))
    {
      g_print("    entry key: %s\n", dict_key);
      g_variant_unref(dict_val);
    }

    /* the method call is incomplete without a proper receipt */
    g_dbus_method_invocation_return_value(invoc, NULL);
  }
}

/******************************************************************************/

int main(int argc, char **argv)
{
  GError *err = NULL;

  GDBusConnection *conn =
    g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL, &err);

  if (!conn || err != NULL) return 0;

  introspect_data =
    g_dbus_node_info_new_for_xml(INTROSPECT_XML, NULL);

  g_assert(introspect_data != NULL);

  GDBusInterfaceInfo *iface_info =
    g_dbus_node_info_lookup_interface(
      introspect_data, "org.bluez.Profile1");

  g_assert(iface_info && "Invalid interface info object");

  GDBusInterfaceVTable interface_vtable;
  interface_vtable.method_call = on_method_call;
  interface_vtable.get_property = NULL;
  interface_vtable.set_property = NULL;

  g_print("Exporting profile object...");

  guint reg_id =
    g_dbus_connection_register_object(conn,
                                      PROFILE_OBJECT_PATH,
                                      iface_info,
                                      &interface_vtable,
                                      NULL,
                                      NULL,
                                      &err);
  if (err != NULL) {
    g_print("failed\n");
    goto done;
  } else g_print("ok\n");

  g_print("Registering profile\n");
  register_profile(conn, TRUE);

  signal(SIGINT, on_signal_received);

  /* kick off the main loop now. Be careful not to have anything
     blocking in 'on_loop_idle' */
  loop = g_main_loop_new(NULL, FALSE);
  g_idle_add(on_loop_idle, NULL);
  g_main_loop_run(loop);

  /* we are done, tear everything down now */
  g_print("Unregistering profile\n");
  register_profile(conn, FALSE);

  g_dbus_connection_unregister_object(conn, reg_id);

done:
  g_main_loop_unref(loop);
  g_object_unref(conn);
  return 0;
}

