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
import sys

VCARD_JOE = """
BEGIN:VCARD
VERSION:3.0
N:Gump;Forrest
FN:Forrest Gump
ORG:Bubba Gump Shrimp Co.
TITLE:Shrimp Man
PHOTO;VALUE=URL;TYPE=GIF:http://www.example.com/dir_photos/my_photo.gif
TEL;TYPE=WORK,VOICE:(111) 555-1212
TEL;TYPE=HOME,VOICE:(404) 555-1212
ADR;TYPE=WORK:;;100 Waters Edge;Baytown;LA;30314;United States of America
LABEL;TYPE=WORK:100 Waters Edge\nBaytown, LA 30314\nUnited States of America
ADR;TYPE=HOME:;;42 Plantation St.;Baytown;LA;30314;United States of America
LABEL;TYPE=HOME:42 Plantation St.\nBaytown, LA 30314\nUnited States of America
EMAIL;TYPE=PREF,INTERNET:forrestgump@example.com
REV:2008-04-24T19:52:43Z
END:VCARD
"""

class Contacts(object):
    def __init__(self):
        self.bus = None
        self.addr = None
        self.addr_iface = None

    def connect(self):
        self.bus = dbus.SessionBus()
        self.addr = self.bus.get_object('com.canonical.galera',
                                        '/com/canonical/galera/AddressBook')
        self.addr_iface = dbus.Interface(self.addr,
                                         dbus_interface='com.canonical.galera.AddressBook')

    def query(self, fields = '', query = '', sources = []):
        view_path = self.addr_iface.query(fields, query, [])
        view = self.bus.get_object('com.canonical.galera',
                                   view_path)
        view_iface = dbus.Interface(view,
                                    dbus_interface='com.canonical.galera.View')
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

if "query" in sys.argv:
    for contact in service.query():
        print contact

if "update" in sys.argv:
    vcard = VCARD_JOE
    contactId = service.create(vcard)
    vcard = vcard.replace("VERSION:3.0", "VERSION:3.0\nUID:%s" % (contactId))
    vcard = vcard.replace("N:Gump;Forrest", "N:Hanks;Tom")
    vcard = vcard.replace("FN:Forrest Gumpt", "N:Tom Hanks")
    print service.update(vcard)

if "create" in sys.argv:
    print "New UID:", service.create(VCARD_JOE)

if "delete" in sys.argv:
    vcard = VCARD_JOE
    contactId = service.create(vcard)
    print service.delete([contactId])
