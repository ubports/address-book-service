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
#include "base-eds-test.h"
#include "common/vcard-parser.h"

using namespace QtContacts;

class ContactTimeStampTest : public QObject, public BaseEDSTest
{
    Q_OBJECT

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
        BaseEDSTest::cleanupImpl();
    }

    void testFilterContactByChangeLog()
    {
        QDateTime currentDate = QDateTime::currentDateTime();
        // wait one sec to cause a create date later
        QTest::qWait(1000);
        QContact contact = galera::VCardParser::vcardToContact(QStringLiteral("BEGIN:VCARD\r\n"
                                                                              "VERSION:3.0\r\n"
                                                                              "X-REMOTE-ID:1dd7d51a1518626a\r\n"
                                                                              "N:;Fulano;;;\r\n"
                                                                              "EMAIL:fulano@gmail.com\r\n"
                                                                              "TEL:123456\r\n"
                                                                              "CLIENTPIDMAP:56183a5b-5da7-49fe-8cf6-9bfd3633bf6d\r\n"
                                                                              "END:VCARD\r\n"));

        // create a contact
        QSignalSpy spyContactAdded(m_manager, SIGNAL(contactsAdded(QList<QContactId>)));
        bool result = m_manager->saveContact(&contact);
        QCOMPARE(result, true);
        QTRY_COMPARE(spyContactAdded.count(), 1);

        // qery contacts;
        QContactFilter filter;
        QList<QContact> contacts = m_manager->contacts(filter);
        QCOMPARE(contacts.size(), 1);

        QContactTimestamp timestamp = contacts[0].detail<QContactTimestamp>();
        qDebug() << "CURRENT DATE:" << currentDate.toUTC() << "\n"
                 << "CREATED DATE:" << timestamp.created() << "\n"
                 << "MODIFIED DATE:" << timestamp.lastModified();

        QVERIFY(timestamp.created().isValid());
        QVERIFY(timestamp.lastModified().isValid());
        QVERIFY(timestamp.created() >= currentDate);
        QVERIFY(timestamp.lastModified() >= currentDate);

        QContactChangeLogFilter changeLogFilter;
        changeLogFilter.setSince(currentDate);

        QList<QContactId> ids = m_manager->contactIds(changeLogFilter);
        QCOMPARE(ids.size(), 1);
        QCOMPARE(ids[0], contacts[0].id());
    }

    void testDeletedDateFilter()
    {
        QDateTime currentDate = QDateTime::currentDateTime();
        // wait one sec to cause a create date later
        QTest::qWait(1000);
        QContact contact = galera::VCardParser::vcardToContact(QStringLiteral("BEGIN:VCARD\r\n"
                                                                              "VERSION:3.0\r\n"
                                                                              "X-REMOTE-ID:1dd7d51a1518626a\r\n"
                                                                              "N:;Fulano;;;\r\n"
                                                                              "EMAIL:fulano@gmail.com\r\n"
                                                                              "TEL:123456\r\n"
                                                                              "CLIENTPIDMAP:56183a5b-5da7-49fe-8cf6-9bfd3633bf6d\r\n"
                                                                              "END:VCARD\r\n"));

        // create a contact
        bool result = m_manager->saveContact(&contact);
        QCOMPARE(result, true);

        QContactChangeLogFilter changeLogFilter;
        changeLogFilter.setEventType(QContactChangeLogFilter::EventRemoved);
        changeLogFilter.setSince(currentDate);

        QContactDetailFilter detFilter;
        detFilter.setDetailType(QContactDetail::TypeEmailAddress,
                                QContactEmailAddress::FieldEmailAddress);
        detFilter.setValue(QStringLiteral("fulano@gmail.com"));
        detFilter.setMatchFlags(QContactDetailFilter::MatchExactly);

        // no contact removed
        QList<QContactId> ids = m_manager->contactIds(changeLogFilter);
        QCOMPARE(ids.size(), 0);

        // test intersection filter
        ids = m_manager->contactIds(changeLogFilter & detFilter);
        QCOMPARE(ids.size(), 0);

        // wait one more sec to remove the contact
        QTest::qWait(1000);

        result = m_manager->removeContact(contact.id());
        QVERIFY(result);

        ids = m_manager->contactIds(changeLogFilter);
        QCOMPARE(ids.size(), 1);
        QCOMPARE(ids[0], contact.id());

        // test intersection filter
        ids = m_manager->contactIds(changeLogFilter & detFilter);
        QCOMPARE(ids.size(), 1);
        QCOMPARE(ids[0], contact.id());
    }
};

QTEST_MAIN(ContactTimeStampTest)

#include "contact-timestamp-test.moc"
