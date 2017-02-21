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

#include <QObject>
#include <QtTest>
#include <QDebug>
#include <QtContacts>

#include "config.h"
#include "common/vcard-parser.h"
#include "base-eds-test.h"

using namespace QtContacts;

class ContactAccountTest : public QObject, public BaseEDSTest
{
    Q_OBJECT
private:


private Q_SLOTS:
    void initTestCase()
    {
        BaseEDSTest::initTestCaseImpl();
    }

    void cleanupTestCase()
    {
        BaseEDSTest::cleanupTestCaseImpl();
    }

    void init()
    {
        BaseEDSTest::initImpl();
    }

    void cleanup()
    {
        QList<QContactId> contacts = m_manager->contactIds();
        m_manager->removeContacts(contacts);
        BaseEDSTest::cleanupImpl();
    }

    void testCreateIrcAccount()
    {
        QContact contact = galera::VCardParser::vcardToContact(QString("BEGIN:VCARD\r\n"
                                                                       "VERSION:3.0\r\n"
                                                                       "X-REMOTE-ID:1dd7d51a1518626a\r\n"
                                                                       "N:;Fulano;;;\r\n"
                                                                       "EMAIL:fulano@gmail.com\r\n"
                                                                       "TEL:123456\r\n"
                                                                       "X-IRC:myIRC\r\n"
                                                                       "CLIENTPIDMAP:56183a5b-5da7-49fe-8cf6-9bfd3633bf6d\r\n"
                                                                       "END:VCARD\r\n").arg(TEST_DATA_DIR));

        QCOMPARE(contact.details<QContactOnlineAccount>().size(), 1);

        // create a contact
        QSignalSpy spyContactAdded(m_manager, SIGNAL(contactsAdded(QList<QContactId>)));
        bool result = m_manager->saveContact(&contact);
        QCOMPARE(contact.details<QContactOnlineAccount>().size(), 1);
        QCOMPARE(result, true);
        QTRY_COMPARE(spyContactAdded.count(), 1);

        // query contacts;
        QContactFilter filter;
        QList<QContact> contacts = m_manager->contacts(filter);
        QCOMPARE(contacts.size(), 1);

        QList<QContactOnlineAccount> accs = contacts[0].details<QContactOnlineAccount>();
        QCOMPARE(accs.size(), 1);

        QContactOnlineAccount acc = accs[0];
        QCOMPARE(acc.protocol(), QContactOnlineAccount::ProtocolIrc);
        QCOMPARE(acc.accountUri(), QStringLiteral("myIRC"));
    }

    void testModifyIrcAccount()
    {
        QContact contact = galera::VCardParser::vcardToContact(QString("BEGIN:VCARD\r\n"
                                                                       "VERSION:3.0\r\n"
                                                                       "X-REMOTE-ID:1dd7d51a1518626a\r\n"
                                                                       "N:;Fulano;;;\r\n"
                                                                       "EMAIL:fulano@gmail.com\r\n"
                                                                       "TEL:123456\r\n"
                                                                       "X-IRC:myIRC\r\n"
                                                                       "CLIENTPIDMAP:56183a5b-5da7-49fe-8cf6-9bfd3633bf6d\r\n"
                                                                       "END:VCARD\r\n").arg(TEST_DATA_DIR));

        QCOMPARE(contact.details<QContactOnlineAccount>().size(), 1);

        // create a contact
        QSignalSpy spyContactAdded(m_manager, SIGNAL(contactsAdded(QList<QContactId>)));
        bool result = m_manager->saveContact(&contact);
        QCOMPARE(result, true);
        QTRY_COMPARE(spyContactAdded.count(), 1);

        // query contacts;
        QContactFilter filter;
        QList<QContact> contacts = m_manager->contacts(filter);
        QCOMPARE(contacts.size(), 1);

        contact = contacts[0];
        QList<QContactOnlineAccount> accs = contact.details<QContactOnlineAccount>();
        QCOMPARE(accs.size(), 1);

        QContactOnlineAccount acc = accs[0];
        acc.setAccountUri(QStringLiteral("otherIRC"));
        contact.saveDetail(&acc);

        QSignalSpy spyContactChanged(m_manager, SIGNAL(contactsChanged(QList<QContactId>)));
        result = m_manager->saveContact(&contact);
        QCOMPARE(result, true);
        QTRY_COMPARE(spyContactChanged.count(), 1);

        accs = contact.details<QContactOnlineAccount>();
        QCOMPARE(accs.size(), 1);

        acc = accs[0];
        QCOMPARE(acc.protocol(), QContactOnlineAccount::ProtocolIrc);
        QCOMPARE(acc.accountUri(), QStringLiteral("otherIRC"));
    }
};

QTEST_MAIN(ContactAccountTest)

#include "contact-accounts-test.moc"
