#!/usr/bin/python3

'''buteo syncfw mock template

This creates the expected methods and properties of the main
com.meego.msyncd object. You can specify D-BUS property values
'''

# This program is free software; you can redistribute it and/or modify it under
# the terms of the GNU Lesser General Public License as published by the Free
# Software Foundation; either version 3 of the License, or (at your option) any
# later version.  See http://www.gnu.org/copyleft/lgpl.html for the full text
# of the license.

__author__ = 'Renato Araujo Oliveira Filho'
__email__ = 'renato.filho@canonical.com'
__copyright__ = '(c) 2015 Canonical Ltd.'
__license__ = 'LGPL 3+'

import dbus
from gi.repository import GObject

import dbus
import dbus.service
import dbus.mainloop.glib

BUS_NAME = 'com.canonical.pim'
MAIN_OBJ = '/com/canonical/pim/AddressBook'
MAIN_IFACE = 'com.canonical.pim.AddressBook'

VIEW_OBJ = '/com/canonical/pim/AddressBookView'
VIEW_IFACE = 'com.canonical.pim.AddressBookView'
SYSTEM_BUS = False

class AddressBookView(dbus.service.Object):
    DBUS_NAME = None

    def __init__(self, object_path):
        dbus.service.Object.__init__(self, dbus.SessionBus(), object_path)
        self._mainloop = GObject.MainLoop()
        self._sources = {}

    @dbus.service.method(dbus_interface=VIEW_IFACE,
                         in_signature='asii', out_signature='as')
    def contactsDetails(self, fiels, startIndex, pageSize):
        return []

class AddressBook(dbus.service.Object):
    DBUS_NAME = None

    def __init__(self, object_path):
        dbus.service.Object.__init__(self, dbus.SessionBus(), object_path)
        self._mainloop = GObject.MainLoop()
        self._view = AddressBookView(VIEW_OBJ)
        self._isReady = True
        self._sources = {}

    @dbus.service.method(dbus_interface=MAIN_IFACE,
                         in_signature='', out_signature='a(ssssubb)')
    def availableSources(self):
        return self._sources.values()

    @dbus.service.method(dbus_interface=MAIN_IFACE,
                         in_signature='s', out_signature='b')
    def removeSource(self, sourceId):
        if sourceId in self._sources:
            del self._sources[sourceId]
            return True
        else:
            return False

    @dbus.service.method(dbus_interface=MAIN_IFACE,
                         in_signature='as', out_signature='i')
    def removeContacts(self, contactIds):
        count = 0
        for id in contactIds:
            if sourceId in self._sources:
                del self._sources[sourceId]
                count += 1
        return count

    @dbus.service.method(dbus_interface=MAIN_IFACE,
                         in_signature='ssibas', out_signature='o')
    def query(self, clause, sort, max_count, show_invisible, sources):
        return VIEW_OBJ

    @dbus.service.method(dbus.PROPERTIES_IFACE,
                         in_signature='ss', out_signature='v')
    def Get(self, interface_name, prop_name):
        if interface_name == MAIN_IFACE:
            if property_name == 'isReady':
                return True
        return None

    @dbus.service.signal(dbus_interface=MAIN_IFACE,
                         signature='')
    def readyChanged(self):
        print("readyChanged called")

    @dbus.service.signal(dbus_interface=MAIN_IFACE,
                         signature='as')
    def contactsAdded(self, contacts):
        print("contactsAdded called")

    @dbus.service.signal(dbus_interface=MAIN_IFACE,
                         signature='as')
    def contactsRemoved(self, contacts):
        print("contactsRemoved called")

    @dbus.service.signal(dbus_interface=MAIN_IFACE,
                         signature='as')
    def contactsUpdated(self, contacts):
        print("contactsUpdated called")

    @dbus.service.signal(dbus_interface=MAIN_IFACE)
    def safeModeChanged(self):
        print("safeModeChanged called")

    @dbus.service.signal(dbus_interface=MAIN_IFACE)
    def readyChanged(self):
        print("readyChanged called")


    #properties
    @dbus.service.method(dbus_interface='org.freedesktop.DBus.Introspectable',
                         out_signature='s')
    def Introspect(self):
        return """<!DOCTYPE node PUBLIC "-//freedesktop//DTD D-BUS Object Introspection 1.0//EN"
    "http://www.freedesktop.org/standards/dbus/1.0/introspect.dtd">
    <node name="/com/canonical/pim/AddressBook">
      <interface name="com.canonical.pim.AddressBook">
        <property name="isReady" type="b" access="read"/>
        <signal name="readyChanged">
        </signal>
        <method name="reset">
          <arg direction="out" type="b" />
        </method>
        <method name="availableSources">
          <arg direction="out" type="a(ssssubb)" />
        </method>
        <signal name="safeModeChanged">
        </signal>
        <method name="query">
          <arg direction="in"  type="s" name="clause" />
          <arg direction="in"  type="s" name="sort" />
          <arg direction="in"  type="i" name="max_count" />
          <arg direction="in"  type="b" name="show_invisible" />
          <arg direction="in"  type="as" name="sources" />
          <arg direction="out" type="o" />
        </method>
        <method name="createSource">
          <arg direction="in"  type="s" name="sourceId" />
          <arg direction="in"  type="s" name="sourceName" />
          <arg direction="in"  type="s" name="provider" />
          <arg direction="in"  type="s" name="applicationId" />
          <arg direction="in"  type="i" name="accountId" />
          <arg direction="in"  type="b" name="readOnly" />
          <arg direction="in"  type="b" name="primary" />
          <arg direction="out" type="b" />
        </method>
        <signal name="contactsRemoved">
          <arg type="as" name="contacts" />
        </signal>
        <method name="removeSource">
          <arg direction="in"  type="s" name="sourceId" />
          <arg direction="out" type="b" />
        </method>
        <signal name="contactsAdded">
          <arg type="as" name="contacts" />
        </signal>
        <method name="removeContacts">
          <arg direction="in"  type="as" name="contactIds" />
          <arg direction="out" type="i" />
        </method>
        <signal name="contactsUpdated">
          <arg type="as" name="contacts" />
        </signal>
      </interface>
      <interface name="org.freedesktop.DBus.Introspectable">
        <method name="Introspect">
          <arg direction="out" type="s" />
        </method>
      </interface>
      <interface name="org.freedesktop.DBus.Properties">
        <method name="Set">
          <arg direction="in"  type="s" name="interface_name" />
          <arg direction="in"  type="s" name="property_name" />
          <arg direction="in"  type="v" name="new_value" />
        </method>
        <signal name="PropertiesChanged">
          <arg type="s" name="interface_name" />
          <arg type="a{sv}" name="changed_properties" />
          <arg type="as" name="invalidated_properties" />
        </signal>
        <method name="Get">
          <arg direction="in"  type="s" name="interface_name" />
          <arg direction="in"  type="s" name="property_name" />
          <arg direction="out" type="v" />
        </method>
        <method name="GetAll">
          <arg direction="in"  type="s" name="interface_name" />
          <arg direction="out" type="a{sv}" />
        </method>
      </interface>
    </node>
    """

    @dbus.service.method(dbus_interface=dbus.PROPERTIES_IFACE,
                         in_signature='ss', out_signature='v')
    def Get(self, interface_name, property_name):
        return self.GetAll(interface_name)[property_name]

    @dbus.service.method(dbus_interface=dbus.PROPERTIES_IFACE,
                         in_signature='s', out_signature='a{sv}')
    def GetAll(self, interface_name):
        print("Get Property")
        return {'isReady': True}

    @dbus.service.method(dbus_interface=dbus.PROPERTIES_IFACE,
                         in_signature='ssv')
    def Set(self, interface_name, property_name, new_value):
        # validate the property name and value, update internal state…
        if property_name == 'isReady':
            self._isReady = new_value
            self.readyChanged()

        self.PropertiesChanged(interface_name,
            { property_name: new_value }, [])

    @dbus.service.signal(dbus_interface=dbus.PROPERTIES_IFACE,
                         signature='sa{sv}as')
    def PropertiesChanged(self, interface_name, changed_properties,
                          invalidated_properties):
        pass

    #helper functions
    @dbus.service.method(dbus_interface=MAIN_IFACE,
                         in_signature='ssssibb', out_signature='b')
    def createSource(self, sourceId, sourceName, provider, applicationId, accountId, readOnly, primary):
        self._sources[sourceId] = (sourceId, sourceName, provider, applicationId, accountId, readOnly, primary)
        return True

    @dbus.service.method(dbus_interface=MAIN_IFACE,
                         in_signature='', out_signature='b')
    def reset(self):
        self._sources = {}
        return True

    def _run(self):
        self._mainloop.run()

if __name__ == '__main__':
    dbus.mainloop.glib.DBusGMainLoop(set_as_default=True)

    AddressBook.DBUS_NAME = dbus.service.BusName(BUS_NAME)
    galera = AddressBook(MAIN_OBJ)

    galera._run()
