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

BUS_NAME = 'com.canonical.SyncMonitor'
MAIN_OBJ = '/com/canonical/SyncMonitor'
MAIN_IFACE = 'com.canonical.SyncMonitor'
SYSTEM_BUS = False

class SyncMonitor(dbus.service.Object):
    DBUS_NAME = None

    def __init__(self, object_path):
        dbus.service.Object.__init__(self, dbus.SessionBus(), object_path)
        self._mainloop = GObject.MainLoop()
        self._accountsById = {140: 'renato.teste2@gmail.com',
                              141: 'renato.teste3@gmail.com',
                              142: 'renato.teste4@gmail.com'}

    def _mock_sync_done(self, accountId):
        print("Emit sync done: %s" % (self._accountsById[accountId]))
        self.syncFinished(self._accountsById[accountId], 'contacts')
        return False

    @dbus.service.method(dbus_interface=MAIN_IFACE,
                         in_signature='us')
    def syncAccount(self, accountId, serviceName):
        print("Will sync:", accountId)
        GObject.timeout_add(1000, self._mock_sync_done, accountId)

    @dbus.service.signal(dbus_interface=MAIN_IFACE,
                         signature='ss')
    def syncFinished(self, accountName, serviceName):
        print("syncFinished called")

    @dbus.service.signal(dbus_interface=MAIN_IFACE,
                         signature='sss')
    def syncError(self, accountName, serviceName, errorMessage):
        print("syncError called")

    def _run(self):
        self._mainloop.run()

if __name__ == '__main__':
    dbus.mainloop.glib.DBusGMainLoop(set_as_default=True)

    SyncMonitor.DBUS_NAME = dbus.service.BusName(BUS_NAME)
    sync = SyncMonitor(MAIN_OBJ)
    sync._run()
