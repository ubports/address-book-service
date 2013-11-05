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
#include "lib/source.h"
#include "common/dbus-service-defs.h"
#include "common/vcard-parser.h"

#include <QObject>
#include <QtDBus>
#include <QtTest>
#include <QDebug>
#include <QtVersit>

class AddressBookTest : public BaseClientTest
{
    Q_OBJECT
private:
    QEventLoop *m_eventLoop;
    QString m_basicVcard;
    QString m_resultBasicVcard;
    QString m_basicContactId;
    QList<QtContacts::QContact> m_contactsResult;

private Q_SLOTS:
    void initTestCase()
    {
        BaseClientTest::initTestCase();
        m_basicContactId = QStringLiteral("81ee878e531264bb4123a12ff7397dc44cb6ffd5");
        m_basicVcard = QStringLiteral("BEGIN:VCARD\n"
                                      "VERSION:3.0\n"
                                      "N:Tal;Fulano_;de;;\n"
                                      "EMAIL:fulano_@ubuntu.com\n"
                                      "TEL;TYPE=ISDN:33331410\n"
                                      "TEL;TYPE=CELL:8888888\n"
                                      "END:VCARD");

        m_resultBasicVcard = QStringLiteral("BEGIN:VCARD\r\n"
                                       "VERSION:3.0\r\n"
                                       "UID:81ee878e531264bb4123a12ff7397dc44cb6ffd5\r\n"
                                       "CLIENTPIDMAP:1;dummy:dummy-store:0\r\n"
                                       "N;PID=1.1:Tal;Fulano_;de;;\r\n"
                                       "FN;PID=1.1:Fulano_ Tal\r\n"
                                       "X-QTPROJECT-FAVORITE;PID=1.1:false;0\r\n"
                                       "EMAIL;PID=1.1:fulano_@ubuntu.com\r\n"
                                       "TEL;PID=1.1;TYPE=ISDN:33331410\r\n"
                                       "TEL;PID=1.2;TYPE=CELL:8888888\r\n"
                                       "END:VCARD\r\n");

        m_contactsResult = galera::VCardParser::vcardToContact(QStringList() << m_resultBasicVcard);
    }

    void testSortFields()
    {
        QStringList defaultSortFields;
        defaultSortFields << "ADDR_COUNTRY"
                          << "ADDR_LOCALITY"
                          << "ADDR_POSTCODE"
                          << "ADDR_POST_OFFICE_BOX"
                          << "ADDR_REGION"
                          << "ADDR_STREET"
                          << "BIRTHDAY"
                          << "EMAIL"
                          << "FIRST_NAME"
                          << "FULL_NAME"
                          << "IM_PROTOCOL"
                          << "IM_URI"
                          << "LAST_NAME"
                          << "MIDLE_NAME"
                          << "NAME_PREFIX"
                          << "NAME_SUFFIX"
                          << "NICKNAME"
                          << "ORG_DEPARTMENT"
                          << "ORG_LOCATION"
                          << "ORG_NAME"
                          << "ORG_ROLE"
                          << "ORG_TITLE"
                          << "PHONE"
                          << "PHOTO"
                          << "URL";
        QDBusReply<QStringList> reply = m_serverIface->call("sortFields");
        QCOMPARE(reply.value(), defaultSortFields);
    }

    void testSource()
    {
        QDBusReply<galera::Source> reply = m_serverIface->call("source");
        QVERIFY(reply.isValid());
        QCOMPARE(reply.value().id(), QStringLiteral("dummy-store"));
    }

    void testAvailableSources()
    {
        QDBusReply<QList<galera::Source> > reply = m_serverIface->call("availableSources");
        galera::SourceList list = reply.value();
        QCOMPARE(list.count(), 1);
        galera::Source src = list[0];
        QCOMPARE(src.id(), QStringLiteral("dummy-store"));
    }

    void testCreateContact()
    {
        // spy 'contactsAdded' signal
        QSignalSpy addedContactSpy(m_serverIface, SIGNAL(contactsAdded(const QStringList &)));

        // call create contact
        QDBusReply<QString> reply = m_serverIface->call("createContact", m_basicVcard, "dummy-store");

        // check if the returned id is valid
        QCOMPARE(reply.value(), m_basicContactId);

        // check if the cotact was created with the correct fields
        QDBusReply<QStringList> reply2 = m_dummyIface->call("listContacts");
        QCOMPARE(reply2.value().count(), 1);
        QList<QtContacts::QContact> contactsCreated = galera::VCardParser::vcardToContact(reply2.value());
        QCOMPARE(contactsCreated.count(), 1);
        QCOMPARE(contactsCreated, m_contactsResult);

        // wait for folks to emit the signal
        QTest::qWait(500);

        // check if the signal "contactAdded" was fired
        QCOMPARE(addedContactSpy.count(), 1);
        QList<QVariant> args = addedContactSpy.takeFirst();
        QCOMPARE(args.count(), 1);
        QStringList ids = args[0].toStringList();
        QCOMPARE(ids[0], m_basicContactId);
    }
};

QTEST_MAIN(AddressBookTest)

#include "addressbook-test.moc"
