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
        print "CREATE:", self.addr_iface.createContact(vcard, "")


service = Contacts()
service.connect()

if "query" in sys.argv:
    for contact in service.query():
        print contact

if "update" in sys.argv:
    contacts = service.query()
    service.update(contacts[0])

if "create" in sys.argv:
    contacts = service.create(VCARD_JOE)
