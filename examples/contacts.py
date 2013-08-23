#!/usr/bin/env python3
# -*- encoding: utf-8 -*-
#
#  Copyright 2013 Canonical Ltd.
#
#  This program is free software; you can redistribute it and/or modify
#  it under the terms of the GNU Lesser General Public License as published by
#  the Free Software Foundation; version 3.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU Lesser General Public License for more details.
#
#  You should have received a copy of the GNU Lesser General Public License
#  along with this program.  If not, see <http://www.gnu.org/licenses/>.
#

import dbus
import argparse

VCARD_JOE = """
BEGIN:VCARD
VERSION:3.0
N:Gump;Forrest
FN:Forrest Gump
TEL;TYPE=WORK,VOICE:(111) 555-1212
TEL;TYPE=HOME,VOICE:(404) 555-1212
EMAIL;TYPE=PREF,INTERNET:forrestgump@example.com
END:VCARD
"""

class Contacts(object):
    def __init__(self):
        self.bus = None
        self.addr = None
        self.addr_iface = None

    def connect(self):
        self.bus = dbus.SessionBus()
        self.addr = self.bus.get_object('com.canonical.pim',
                                        '/com/canonical/pim/AddressBook')
        self.addr_iface = dbus.Interface(self.addr,
                                         dbus_interface='com.canonical.pim.AddressBook')

    def query(self, fields = '', query = '', sources = []):
        view_path = self.addr_iface.query(fields, query, [])
        view = self.bus.get_object('com.canonical.pim',
                                   view_path)
        view_iface = dbus.Interface(view,
                                    dbus_interface='com.canonical.pim.AddressBookView')
        contacts = view_iface.contactsDetails([], 0, -1)
        view.close()
        return contacts

    def update(self, vcard):
        return self.addr_iface.updateContacts([vcard])

    def create(self, vcard):
        return self.addr_iface.createContact(vcard, "")

    def delete(self, ids):
        return self.addr_iface.removeContacts(ids)

service = Contacts()
service.connect()

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument('command', choices=['query','create','update',
        'delete','load'])
    parser.add_argument('filename', action='store', nargs='?')
    args = parser.parse_args()

    if args.command == 'query':
        contacts = service.query()
        if contacts:
            for contact in contacts:
                print (contact)
        else:
            print ("No contacts found")

    if args.command == 'update':
        vcard = VCARD_JOE
        contactId = service.create(vcard)
        vcard = vcard.replace("VERSION:3.0", "VERSION:3.0\nUID:%s" % (contactId))
        vcard = vcard.replace("N:Gump;Forrest", "N:Hanks;Tom")
        vcard = vcard.replace("FN:Forrest Gump", "N:Tom Hanks")
        print (service.update(vcard))

    if args.command == 'create':
        print ("New UID:", service.create(VCARD_JOE))

    if args.command == 'delete':
        vcard = VCARD_JOE
        contactId = service.create(vcard)
        print ("Deleted contact: %d" % service.delete([contactId]))

    if args.command == 'load':
        if args.filename:
            f = open(args.filename, 'r')
            vcard = f.read()
            print ("New UID:", service.create(vcard))
        else:
            print ("You must supply a path to a VCARD")
