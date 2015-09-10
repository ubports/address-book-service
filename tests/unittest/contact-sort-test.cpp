/*
 * Copyright 2013 Canonical Ltd.
 *
 * This file is part of contact-service-app.
 *
 * contact-service-app is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 3.
 *
 * contact-service-app is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "base-client-test.h"
#include "common/source.h"
#include "common/dbus-service-defs.h"
#include "common/vcard-parser.h"

#include <QObject>
#include <QtDBus>
#include <QtTest>
#include <QDebug>
#include <QtContacts>
#include <QUuid>

#include "config.h"

using namespace QtContacts;

class ContactSortTest : public BaseClientTest
{
    Q_OBJECT
private:
    QContactManager *m_manager;
    QString m_basicVcard;

    QString createContact(const QString &fullName, const QString &phoneNumber = QString())
    {
        QContact contact;

        QContactName name;
        QStringList names = fullName.split(" ");
        name.setFirstName(names.value(0));
        name.setLastName(names.value(1));
        contact.saveDetail(&name);

        QContactPhoneNumber phone;
        if (phoneNumber.isEmpty()) {
            phone.setNumber(QUuid::createUuid().toString());
        } else {
            phone.setNumber(phoneNumber);
        }
        contact.saveDetail(&phone);

        return galera::VCardParser::contactToVcardSync(QList<QContact>() << contact)[0];
    }

private Q_SLOTS:
    void initTestCase()
    {
        BaseClientTest::initTestCase();
        QCoreApplication::setLibraryPaths(QStringList() << QT_PLUGINS_BINARY_DIR);
        m_basicVcard = QStringLiteral("BEGIN:VCARD\n"
                                      "VERSION:3.0\n"
                                      "N:Tal;Fulano_;de;;\n"
                                      "EMAIL:fulano_@ubuntu.com\n"
                                      "TEL;PID=1.1;TYPE=ISDN:(999) 999-9999\n"
                                      "END:VCARD");
    }

    void init()
    {
        BaseClientTest::init();
        m_manager = new QContactManager("galera");
    }

    void cleanup()
    {
        delete m_manager;
        BaseClientTest::cleanup();
    }

    void testSortContacts()
    {
        QString copyVcard = createContact("Foo Bar");
        m_serverIface->call("createContact", copyVcard, "dummy-store");

        copyVcard = createContact("Baz Quux");
        m_serverIface->call("createContact", copyVcard, "dummy-store");

        copyVcard = createContact("Renato Araujo");
        m_serverIface->call("createContact", copyVcard, "dummy-store");

        copyVcard = createContact("Fone Broke");
        m_serverIface->call("createContact", copyVcard, "dummy-store");

        // check if the cotact is listed in the correct order
        QDBusMessage result = m_serverIface->call("query", "", "", 0, QStringList());
        QDBusObjectPath viewObjectPath = result.arguments()[0].value<QDBusObjectPath>();
        QDBusInterface *view = new QDBusInterface(m_serverIface->service(),
                                                  viewObjectPath.path(),
                                                  CPIM_ADDRESSBOOK_VIEW_IFACE_NAME);

        QDBusReply<QStringList> reply = view->call("contactsDetails",
                                                   QStringList(),
                                                   0,
                                                   100);
        QCOMPARE(reply.value().count(), 4);
        QList<QtContacts::QContact> contactsCreated = galera::VCardParser::vcardToContactSync(reply.value());
        QCOMPARE(contactsCreated.count(), 4);

        // compare result order (different from creation)
        // Baz Quux, Fone Broke, Foo Bar, Renato Araujo
        QCOMPARE(contactsCreated[0].detail<QtContacts::QContactDisplayLabel>().label(), QStringLiteral("Baz Quux"));
        QCOMPARE(contactsCreated[1].detail<QtContacts::QContactDisplayLabel>().label(), QStringLiteral("Fone Broke"));
        QCOMPARE(contactsCreated[2].detail<QtContacts::QContactDisplayLabel>().label(), QStringLiteral("Foo Bar"));
        QCOMPARE(contactsCreated[3].detail<QtContacts::QContactDisplayLabel>().label(), QStringLiteral("Renato Araujo"));
    }

    /*
     * Test sort contact only with only phone number
     */

    void testSortContactsWithOnlyPhoneNumber()
    {
        QString copyVcard = createContact("Fone Broke");
        m_serverIface->call("createContact", copyVcard, "dummy-store");

        copyVcard = createContact("", "555-5555");
        m_serverIface->call("createContact", copyVcard, "dummy-store");

        copyVcard = createContact("Baz Quux");
        m_serverIface->call("createContact", copyVcard, "dummy-store");

        copyVcard = createContact("Foo Bar");
        m_serverIface->call("createContact", copyVcard, "dummy-store");

        copyVcard = createContact("Spider Man");
        m_serverIface->call("createContact", copyVcard, "dummy-store");

        copyVcard = createContact("", "(999) 999-9999");
        m_serverIface->call("createContact", copyVcard, "dummy-store");

        // check if the cotact is listed in the correct order
        QDBusMessage result = m_serverIface->call("query", "", "", 0, QStringList());
        QDBusObjectPath viewObjectPath = result.arguments()[0].value<QDBusObjectPath>();
        QDBusInterface *view = new QDBusInterface(m_serverIface->service(),
                                                  viewObjectPath.path(),
                                                  CPIM_ADDRESSBOOK_VIEW_IFACE_NAME);

        QDBusReply<QStringList> reply = view->call("contactsDetails",
                                                   QStringList(),
                                                   0,
                                                   100);
        delete view;
        QCOMPARE(reply.value().count(), 6);
        QList<QtContacts::QContact> contactsCreated = galera::VCardParser::vcardToContactSync(reply.value());
        QCOMPARE(contactsCreated.count(), 6);

        // compare result order (different from creation)
        // Baz Quux, Fone Broke, Foo Bar, Spider Man, (999) 999-9999, 555-5555
        QCOMPARE(contactsCreated[0].detail<QtContacts::QContactDisplayLabel>().label(), QStringLiteral("Baz Quux"));
        QCOMPARE(contactsCreated[1].detail<QtContacts::QContactDisplayLabel>().label(), QStringLiteral("Fone Broke"));
        QCOMPARE(contactsCreated[2].detail<QtContacts::QContactDisplayLabel>().label(), QStringLiteral("Foo Bar"));
        QCOMPARE(contactsCreated[3].detail<QtContacts::QContactDisplayLabel>().label(), QStringLiteral("Spider Man"));
        QCOMPARE(contactsCreated[4].detail<QtContacts::QContactDisplayLabel>().label(), QStringLiteral("(999) 999-9999"));
        QCOMPARE(contactsCreated[5].detail<QtContacts::QContactDisplayLabel>().label(), QStringLiteral("555-5555"));

        QContact c = contactsCreated[3];
        QContactFavorite fav;
        fav.setFavorite(true);
        c.saveDetail(&fav);
        QStringList vcard = galera::VCardParser::contactToVcardSync(QList<QContact>() << c);
        m_serverIface->call("updateContact", vcard, "dummy-store");

        // check if the cotact still in the correct order
        result = m_serverIface->call("query", "", "", 0, QStringList());
        viewObjectPath = result.arguments()[0].value<QDBusObjectPath>();
        view = new QDBusInterface(m_serverIface->service(),
                                  viewObjectPath.path(),
                                  CPIM_ADDRESSBOOK_VIEW_IFACE_NAME);

        reply = view->call("contactsDetails",
                           QStringList(),
                           0,
                           100);
        delete view;
        QCOMPARE(reply.value().count(), 6);
        contactsCreated = galera::VCardParser::vcardToContactSync(reply.value());
        QCOMPARE(contactsCreated.count(), 6);

        // Baz Quux, Fone Broke, Foo Bar, Spider Man, (999) 999-9999, 555-5555
        QCOMPARE(contactsCreated[0].detail<QtContacts::QContactDisplayLabel>().label(), QStringLiteral("Baz Quux"));
        QCOMPARE(contactsCreated[1].detail<QtContacts::QContactDisplayLabel>().label(), QStringLiteral("Fone Broke"));
        QCOMPARE(contactsCreated[2].detail<QtContacts::QContactDisplayLabel>().label(), QStringLiteral("Foo Bar"));
        QCOMPARE(contactsCreated[3].detail<QtContacts::QContactDisplayLabel>().label(), QStringLiteral("Spider Man"));
        QCOMPARE(contactsCreated[4].detail<QtContacts::QContactDisplayLabel>().label(), QStringLiteral("(999) 999-9999"));
        QCOMPARE(contactsCreated[5].detail<QtContacts::QContactDisplayLabel>().label(), QStringLiteral("555-5555"));
    }
};

QTEST_MAIN(ContactSortTest)

#include "contact-sort-test.moc"
