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

        // wait one sec to remove the contact
        QTest::qWait(1000);
        result = m_manager->removeContact(contact.id());
        QVERIFY(result);

        // if the contact was deleted it should not appear on change log filter
        // with event type = EventAdded
        ids = m_manager->contactIds(changeLogFilter);
        QCOMPARE(ids.size(), 0);
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

        // test deleted filter
        ids = m_manager->contactIds(changeLogFilter);
        QCOMPARE(ids.size(), 1);
        QCOMPARE(ids[0], contact.id());

        // test intersection filter
        ids = m_manager->contactIds(changeLogFilter & detFilter);
        QCOMPARE(ids.size(), 1);
        QCOMPARE(ids[0], contact.id());

        // test deleted filter with a future date
        changeLogFilter.setSince(currentDate.addDays(1));
        ids = m_manager->contactIds(changeLogFilter);
        QCOMPARE(ids.size(), 0);

        // test
    }

    void testPhoneNumberFilterWithDeleted()
    {
        QDateTime currentDate = QDateTime::currentDateTime();
        // wait one sec to cause a create date later
        QTest::qWait(1000);
        QContact contact = galera::VCardParser::vcardToContact(QStringLiteral("BEGIN:VCARD\r\n"
                                                                              "VERSION:3.0\r\n"
                                                                              "X-REMOTE-ID:1dd7d51a1518626a\r\n"
                                                                              "N:;Fulano;;;\r\n"
                                                                              "EMAIL:fulano@gmail.com\r\n"
                                                                              "TEL:78910\r\n"
                                                                              "CLIENTPIDMAP:56183a5b-5da7-49fe-8cf6-9bfd3633bf6d\r\n"
                                                                              "END:VCARD\r\n"));

        // create a contact
        bool result = m_manager->saveContact(&contact);
        QCOMPARE(result, true);

        // no contact removed
        QContactChangeLogFilter changeLogFilter;
        changeLogFilter.setEventType(QContactChangeLogFilter::EventRemoved);
        changeLogFilter.setSince(currentDate);

        QList<QContactId> ids = m_manager->contactIds(changeLogFilter);
        QCOMPARE(ids.size(), 0);

        // Phone number filter
        QContactDetailFilter detFilter = QContactPhoneNumber::match("78910");
        ids = m_manager->contactIds(detFilter);
        QCOMPARE(ids.size(), 1);

        // Intersection filter (phone + deleted)
        ids = m_manager->contactIds(detFilter & changeLogFilter);
        QCOMPARE(ids.size(), 0);

        // wait one more sec to remove the contact
        QTest::qWait(1000);

        result = m_manager->removeContact(contact.id());
        QVERIFY(result);

        ids = m_manager->contactIds(changeLogFilter);
        QCOMPARE(ids.size(), 1);
        QCOMPARE(ids[0], contact.id());

        // Phone number filter (contact was removed)
        ids = m_manager->contactIds(detFilter);
        QCOMPARE(ids.size(), 0);

        // test intersection filter (phone number + removed)
        ids = m_manager->contactIds(changeLogFilter & detFilter);
        QCOMPARE(ids.size(), 1);
        QCOMPARE(ids[0], contact.id());
    }

    void testIdFilterWithDeleted()
    {
        QDateTime currentDate = QDateTime::currentDateTime();
        // wait one sec to cause a create date later
        QTest::qWait(1000);
        QContact contact = galera::VCardParser::vcardToContact(QStringLiteral("BEGIN:VCARD\r\n"
                                                                              "VERSION:3.0\r\n"
                                                                              "X-REMOTE-ID:1dd7d51a1518626a\r\n"
                                                                              "N:;Fulano;;;\r\n"
                                                                              "EMAIL:fulano@gmail.com\r\n"
                                                                              "TEL:78910\r\n"
                                                                              "CLIENTPIDMAP:56183a5b-5da7-49fe-8cf6-9bfd3633bf6d\r\n"
                                                                              "END:VCARD\r\n"));

        // create a contact
        bool result = m_manager->saveContact(&contact);
        QCOMPARE(result, true);

        // no contact removed
        QContactChangeLogFilter changeLogFilter;
        changeLogFilter.setEventType(QContactChangeLogFilter::EventRemoved);
        changeLogFilter.setSince(currentDate);

        QList<QContactId> ids = m_manager->contactIds(changeLogFilter);
        QCOMPARE(ids.size(), 0);

        // query by id
        ids.clear();
        ids.append(contact.id());
        QList<QContact> contacts = m_manager->contacts(ids);
        QCOMPARE(contacts.size(), 1);

        // wait one more sec to remove the contact
        QTest::qWait(1000);

        result = m_manager->removeContact(contact.id());
        QVERIFY(result);

        // query by id (query by id return the contact deleted or not)
        ids.clear();
        ids.append(contact.id());
        contacts = m_manager->contacts(ids);
        QCOMPARE(contacts.size(), 1);
    }

    void testSyncTargetDeletedFilter()
    {
        QDateTime currentDate = QDateTime::currentDateTime();
        // wait one sec to cause a create date later
        QTest::qWait(1000);
        QContact contact = galera::VCardParser::vcardToContact(QStringLiteral("BEGIN:VCARD\r\n"
                                                                              "VERSION:3.0\r\n"
                                                                              "X-REMOTE-ID:1dd7d51a1518626a\r\n"
                                                                              "N:;Fulano;;;\r\n"
                                                                              "EMAIL:fulano@gmail.com\r\n"
                                                                              "TEL:78910\r\n"
                                                                              "CLIENTPIDMAP:56183a5b-5da7-49fe-8cf6-9bfd3633bf6d\r\n"
                                                                              "END:VCARD\r\n"));

        // create a contact
        bool result = m_manager->saveContact(&contact);
        QCOMPARE(result, true);
        QString syncTargetId = contact.detail<QContactSyncTarget>().value(QContactSyncTarget::FieldSyncTarget + 1).toString();
        QVERIFY(!syncTargetId.isEmpty());

        // wait one more sec to remove the contact
        QTest::qWait(1000);

        // remove contact
        result = m_manager->removeContact(contact.id());
        QVERIFY(result);

        // sync target filter
        QContactDetailFilter fSyncTarget;
        fSyncTarget.setDetailType(QContactSyncTarget::Type,
                                  QContactSyncTarget::FieldSyncTarget + 1);
        fSyncTarget.setValue(syncTargetId);

        QContactChangeLogFilter fDeleted(QContactChangeLogFilter::EventRemoved);
        fDeleted.setSince(currentDate);

        QList<QContactId> ids = m_manager->contactIds(fDeleted & fSyncTarget);
        QCOMPARE(ids.size(), 1);
        QCOMPARE(ids.at(0), contact.id());
    }

};

QTEST_MAIN(ContactTimeStampTest)

#include "contact-timestamp-test.moc"
