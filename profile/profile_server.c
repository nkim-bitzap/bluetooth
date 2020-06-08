/******************************************************************************/
/* File: profile_server.c
   Author: N.Kim
   Abstract: Simple C DBus/BlueZ profile registration example

   Description:
     This handles the profile registration on the server side. By
     registering the profile with "Role=server" the profile gets
     attached to the local adapter ('probe') which becomes visible
     to remotes upon connecting/pairing. */

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

#include "profile_record.h"

static GDBusNodeInfo *introspect_data = NULL;
static GMainLoop *loop = NULL;
static GDBusConnection *conn = NULL;

/* global event processing flags */
static int loop_done = 0;

/******************************************************************************/

void on_signal_received(int signo)
{
  if (signo == SIGINT) loop_done = 1;
}

/******************************************************************************/

static gboolean on_loop_idle(gpointer udata)
{
  /* triggered via SIGTERM */
  if (loop_done)
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

static void register_profile(gboolean enable)
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
      "Role", g_variant_new_string("server"));

    /* this is essential for the internal profile probing, i.e.
       SDP record registration. Pick a channel (i.e. a RFCOMM
       socket) that you know is free  */
    g_variant_builder_add(builder, "{sv}",
      "Channel", g_variant_new_uint16(27));

    g_variant_builder_add(builder, "{sv}",
      "RequireAuthorization", g_variant_new_boolean(TRUE));

    g_variant_builder_add(builder, "{sv}",
      "AutoConnect", g_variant_new_boolean(TRUE));

    /* NOTE, lazy stuff, assume the entire service record to
       require 2K bytes at most */
    char sdp_record[2048] = {0};

    /* provide a service record to be inserted into the SDP
       database, test with 'sdptool' on your remote device
       whether you can find the record or not */
    sprintf(sdp_record, MY_INTERCOM_SDP_RECORD,
      27, 0xdead, "BITZAP-Intercom-Profile", 0);

    g_variant_builder_add(builder, "{sv}", "ServiceRecord",
      g_variant_new_string(sdp_record));

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

static void on_method_call(GDBusConnection *con,
                           const gchar *sender,
                           const gchar *obj_path,
                           const gchar *iface_name,
                           const gchar *method_name,
                           GVariant *params,
                           GDBusMethodInvocation *invoc,
                           gpointer udata)
{
  g_print("Calling method '%s'\n", method_name);

  if (strcmp(method_name, "NewConnection") == 0) {
    g_print("  handling a new connection on server's side\n");
  }
}

/******************************************************************************/

int main(int argc, char **argv)
{
  GError *err = NULL;

  conn = g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL, &err);

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
  register_profile(TRUE);

  signal(SIGINT, on_signal_received);

  /* kick off the main loop now. Be careful not to have anything
     blocking in 'on_loop_idle' */
  loop = g_main_loop_new(NULL, FALSE);
  g_idle_add(on_loop_idle, NULL);
  g_main_loop_run(loop);

  /* we are done, tear everything down now. Don't wait to be
     called back, bounce in the most ungraceful way for now */
  g_print("\nUnregistering profile\n");
  register_profile(FALSE);

done:
  g_main_loop_unref(loop);
  g_object_unref(conn);
  return 0;
}

