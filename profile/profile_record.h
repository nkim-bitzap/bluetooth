/******************************************************************************/
/* File: profile_record.h
   Author: N.Kim
   Abstract: Simple C DBus/BlueZ profile registration example */
/******************************************************************************/

#ifndef PROFILE_RECORD_H
#define PROFILE_RECORD_H

/* bluez and GDBus paths and interfaces */
#define BLUEZ_OBJECT_ROOT "/org/bluez/"
#define BLUEZ_BUS_NAME "org.bluez"

#define PROFILE_OBJECT_PATH "/org/bitzap/profile"
#define ADAPTER_PATH "/org/bluez/hci0"

static const char INTROSPECT_XML[] =
  "<node name='/org/bitzap/profile'>"
  "  <interface name='org.bluez.Profile1'>"
  "    <method name='NewConnection'>"
  "      <arg name='device' type='o' direction='in'/>"
  "      <arg name='fd' type='h' direction='in'/>"
  "      <arg name='fd_properties' type='a{sv}' direction='in'/>"
  "    </method>"
  "    <method name='RequestDisconnection'>"
  "      <arg name='device' type='o' direction='in'/>"
  "    </method>"
  "    <method name='Release'/>"
  "  </interface>"
  "</node>";

/* NOTE, we register the same profile for both sides (client AND server).

   When adding a profile with 'Role=server', the profile gets added to
   the list of the local adapter and is thus (eventually) visible to
   clients.

   When adding a profile with 'Role=client', the profile gets added to
   each remove devices known (i.e. discovered and paired) to the local
   adapter. Obviously, a profile UUID can only be attached to a device,
   IF the device supports that kind of UUID. If not, it needs to be
   added explicitly (thats what we do, e.g. 0x1110 InterCom is usually
   not supported on many machines (Ubuntu/Raspberry). Thus, we need
   to add the profile to remotes prior to pairing.

   When using multiple connection protocols, specify each as a sequence,
   and in case of custom ports, MAKE SURE they are available, otherwise
   'probe-device' fails. */

static const gchar *MY_INTERCOM_SDP_RECORD =
  "<?xml version='1.0' encoding='UTF-8' ?>"
  "  <record>"
  "    <attribute id='0x0001' name='ServiceClassID'>"
  "      <sequence>"
  "        <uuid value='0x1110' desc='Intercom Profile'/>"
  "      </sequence>"
  "    </attribute>"
  "    <attribute id='0x0005' desc='BrowseGroupList'>"
  "      <sequence>"
  "        <uuid value='0x1002' name='PublicBrowseGroup'/>"
  "      </sequence>"
  "    </attribute>"
  "    <attribute id='0x0004' desc='ProtocolDescList'>"
  "      <sequence>"
  "        <sequence>"
  "          <uuid value='0x0001' desc='SDP'/>"
  "        </sequence>"
  "        <sequence>"
  "          <uuid value='0x0100' desc='L2CAP'/>"
  "        </sequence>"
  "        <sequence>"
  "          <uuid value='0x0003' desc='RFComm'/>"
  "          <uint8 value='0x%02x' desc='Channel'/> "
  "        </sequence>"
  "      </sequence>"
  "    </attribute>"
  "    <attribute id='0x0009' desc='ProfileDescList'>"
  "      <sequence>"
  "        <sequence>"
  "          <uuid value='0x1110' desc='Intercom'/>"
  "          <uint16 value='0x%04x' desc='Version'/>"
  "        </sequence>"
  "      </sequence>"
  "    </attribute>"
  "    <attribute id='0x0100' desc='ServiceName'>"
  "      <text value='%s'/>"
  "    </attribute>"
  "    <attribute id='0x0311' desc='Features'>"
  "      <uint16 value='0x%04x'/>"
  "    </attribute>"
  "  </record>";

#endif
