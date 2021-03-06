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

#include "config.h"

using namespace QtContacts;

class QContactsTest : public BaseClientTest
{
    Q_OBJECT
private:
    QContactManager *m_manager;

    QContact testContact()
    {
        // create a contact
        QContact contact;

        QContactName name;
        name.setFirstName("Fulano");
        name.setMiddleName("de");
        name.setLastName("Tal");
        contact.saveDetail(&name);

        QContactEmailAddress email;
        email.setEmailAddress("fulano@email.com");
        contact.saveDetail(&email);

        return contact;
    }

private Q_SLOTS:
    void initTestCase()
    {
        BaseClientTest::initTestCase();
        QCoreApplication::setLibraryPaths(QStringList() << QT_PLUGINS_BINARY_DIR);
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

    void testAvailableManager()
    {
        QVERIFY(QContactManager::availableManagers().contains("galera"));
    }

    /*
     * Test create a new contact
     */
    void testCreateContact()
    {
        // filter all contacts
        QContactFilter filter;

        // check result, must be empty
        QList<QContact> contacts = m_manager->contacts(filter);
        QCOMPARE(contacts.size(), 0);

        // create a contact
        QDateTime createdAt = QDateTime::currentDateTime();
        QContact contact = testContact();
        QSignalSpy spyContactAdded(m_manager, SIGNAL(contactsAdded(QList<QContactId>)));
        bool result = m_manager->saveContact(&contact);
        QCOMPARE(result, true);
        QTRY_COMPARE(spyContactAdded.count(), 1);

        // query for new contacts
        contacts = m_manager->contacts(filter);
        QCOMPARE(contacts.size(), 1);

        QContact createdContact = contacts[0];
        // id
        QVERIFY(!createdContact.id().isNull());

        // createdAt
        QContactTimestamp timestamp = contact.detail<QContactTimestamp>();
        QVERIFY(createdAt.secsTo(timestamp.created()) < 60);
        QVERIFY(createdAt.secsTo(timestamp.lastModified()) < 60);

        // email
        QContactEmailAddress email = contact.detail<QContactEmailAddress>();
        QContactEmailAddress createdEmail = createdContact.detail<QContactEmailAddress>();
        QCOMPARE(createdEmail.emailAddress(), email.emailAddress());

        // name
        QContactName name = contact.detail<QContactName>();
        QContactName createdName = createdContact.detail<QContactName>();
        QCOMPARE(createdName.firstName(), name.firstName());
        QCOMPARE(createdName.middleName(), name.middleName());
        QCOMPARE(createdName.lastName(), name.lastName());

        QContactSyncTarget target = contact.detail<QContactSyncTarget>();
        QCOMPARE(target.syncTarget(), QStringLiteral("Dummy personas"));
        QCOMPARE(target.value(QContactSyncTarget::FieldSyncTarget + 1).toString(),
                 QStringLiteral("dummy-store"));
    }

    /*
     * Test update a contact
     */
    void testUpdateContact()
    {
        // filter all contacts
        QContactFilter filter;

        // create a contact
        QContact contact = testContact();
        QSignalSpy spyContactAdded(m_manager, SIGNAL(contactsAdded(QList<QContactId>)));
        bool result = m_manager->saveContact(&contact);
        QCOMPARE(result, true);
        QTRY_COMPARE(spyContactAdded.count(), 1);

        // modify contact
        QContactName name = contact.detail<QContactName>();
        name.setMiddleName("da");
        name.setLastName("Silva");
        contact.saveDetail(&name);

        QSignalSpy spyContactChanged(m_manager, SIGNAL(contactsChanged(QList<QContactId>)));
        result = m_manager->saveContact(&contact);
        QCOMPARE(result, true);
        QTRY_COMPARE(spyContactChanged.count(), 1);

        // query for the contacts
        QList<QContact> contacts = m_manager->contacts(filter);
        QCOMPARE(contacts.size(), 1);
        QContact updatedContact = contacts[0];

        // name
        QContactName updatedName = updatedContact.detail<QContactName>();
        QCOMPARE(updatedName.firstName(), name.firstName());
        QCOMPARE(updatedName.middleName(), name.middleName());
        QCOMPARE(updatedName.lastName(), name.lastName());
    }

    /*
     * Test query a contact source using the contact group
     */
    void testQueryGroups()
    {
        QContactManager manager("galera");

        // filter all contact groups/addressbook
        QContactDetailFilter filter;
        filter.setDetailType(QContactDetail::TypeType, QContactType::FieldType);
        filter.setValue(QContactType::TypeGroup);

        // check result
        QList<QContact> contacts = manager.contacts(filter);
        QCOMPARE(contacts.size(), 1);
        QCOMPARE(contacts[0].id().toString(), QStringLiteral("qtcontacts:galera::source@dummy-store"));
        QCOMPARE(contacts[0].type(), QContactType::TypeGroup);

        QContactDisplayLabel label = contacts[0].detail(QContactDisplayLabel::Type);
        QCOMPARE(label.label(), QStringLiteral("Dummy personas"));
    }

    /*
     * Test sort contacts by with symbols in the end
     */
    void testQuerySortedWithSymbolsInTheEnd()
    {
        QSignalSpy spyContactAdded(m_manager, SIGNAL(contactsAdded(QList<QContactId>)));

        // create a contact ""
        QContact contact;
        QContactName name = contact.detail<QContactName>();
        name.setFirstName("");
        name.setMiddleName("");
        name.setLastName("");
        contact.saveDetail(&name);
        bool result = m_manager->saveContact(&contact);
        QCOMPARE(result, true);

        // create contact "Aa"
        contact = QContact();
        name = contact.detail<QContactName>();
        name.setFirstName("Aa");
        contact.saveDetail(&name);
        result = m_manager->saveContact(&contact);
        QCOMPARE(result, true);

        // create contact "Ba"
        contact = QContact();
        name = contact.detail<QContactName>();
        name.setFirstName("Ba");
        contact.saveDetail(&name);
        result = m_manager->saveContact(&contact);
        QCOMPARE(result, true);

        // create contact "Bb"
        contact = QContact();
        name = contact.detail<QContactName>();
        name.setFirstName("Bb");
        contact.saveDetail(&name);
        result = m_manager->saveContact(&contact);
        QCOMPARE(result, true);

        // create contact "321"
        contact = QContact();
        name = contact.detail<QContactName>();
        name.setFirstName("321");
        contact.saveDetail(&name);
        result = m_manager->saveContact(&contact);
        QCOMPARE(result, true);

        // create contact "*"
        contact = QContact();
        name = contact.detail<QContactName>();
        name.setFirstName("*");
        contact.saveDetail(&name);
        result = m_manager->saveContact(&contact);
        QCOMPARE(result, true);

        // create contact "" with company "x"
        contact = QContact();
        name = contact.detail<QContactName>();
        name.setFirstName("");
        name.setMiddleName("");
        name.setLastName("");
        contact.saveDetail(&name);

        QContactOrganization org;
        org.setName("x");
        contact.saveDetail(&org);
        result = m_manager->saveContact(&contact);
        QCOMPARE(result, true);

        QTRY_COMPARE(spyContactAdded.count(), 1);

        // sort contact by tag and display name
        QContactFilter filter;
        QContactSortOrder sortTag;
        sortTag.setDetailType(QContactDetail::TypeTag, QContactTag::FieldTag);
        sortTag.setDirection(Qt::AscendingOrder);
        sortTag.setCaseSensitivity(Qt::CaseInsensitive);
        sortTag.setBlankPolicy(QContactSortOrder::BlanksLast);

        QContactSortOrder sortDisplayName;
        sortDisplayName.setDetailType(QContactDetail::TypeDisplayLabel, QContactDisplayLabel::FieldLabel);
        sortDisplayName.setDirection(Qt::AscendingOrder);
        sortDisplayName.setCaseSensitivity(Qt::CaseInsensitive);
        sortDisplayName.setBlankPolicy(QContactSortOrder::BlanksLast);

        QList<QContactSortOrder> sortOrders;
        sortOrders << sortTag << sortDisplayName;

        // check result
        QList<QContact> contacts = m_manager->contacts(filter, sortOrders);
        QCOMPARE(contacts.size(), 7);
        QCOMPARE(contacts[0].detail(QContactDetail::TypeDisplayLabel).value(QContactDisplayLabel::FieldLabel).toString(),
                QStringLiteral("Aa"));
        QCOMPARE(contacts[1].detail(QContactDetail::TypeDisplayLabel).value(QContactDisplayLabel::FieldLabel).toString(),
                QStringLiteral("Ba"));
        QCOMPARE(contacts[2].detail(QContactDetail::TypeDisplayLabel).value(QContactDisplayLabel::FieldLabel).toString(),
                QStringLiteral("Bb"));
        QCOMPARE(contacts[3].detail(QContactDetail::TypeDisplayLabel).value(QContactDisplayLabel::FieldLabel).toString(),
                QStringLiteral("x"));
        QCOMPARE(contacts[4].detail(QContactDetail::TypeDisplayLabel).value(QContactDisplayLabel::FieldLabel).toString(),
                QStringLiteral("*"));
        QCOMPARE(contacts[5].detail(QContactDetail::TypeDisplayLabel).value(QContactDisplayLabel::FieldLabel).toString(),
                QStringLiteral("321"));
        QCOMPARE(contacts[6].detail(QContactDetail::TypeDisplayLabel).value(QContactDisplayLabel::FieldLabel).toString(),
                QStringLiteral(""));
    }

    void testContactDisplayName()
    {
        // create a contact ""
        QContact contact;
        QContactName name = contact.detail<QContactName>();
        name.setFirstName("");
        name.setMiddleName("");
        name.setLastName("");
        contact.saveDetail(&name);
        bool result = m_manager->saveContact(&contact);
        QCOMPARE(result, true);

        // create contact "Aa"
        contact = QContact();
        name = contact.detail<QContactName>();
        name.setFirstName("Aa");
        contact.saveDetail(&name);
        result = m_manager->saveContact(&contact);
        QCOMPARE(result, true);

        // create contact "" with company "x"
        contact = QContact();
        name = contact.detail<QContactName>();
        name.setFirstName("");
        name.setMiddleName("");
        name.setLastName("");
        contact.saveDetail(&name);
        QContactOrganization org;
        org.setName("x");
        contact.saveDetail(&org);
        result = m_manager->saveContact(&contact);

        QCOMPARE(result, true);

        // check result
        QContactFilter filter;
        QList<QContact> contacts = m_manager->contacts(filter);

        QStringList expectedContacts;
        expectedContacts << ""
                         << "Aa"
                         << "x";
        QCOMPARE(contacts.size(), 3);
        Q_FOREACH(const QContact &c, contacts) {
            expectedContacts.removeAll(c.detail<QContactDisplayLabel>().label());
        }
        QCOMPARE(expectedContacts.size(), 0);
    }

    void testContactUpdateDisplayName()
    {
        // create a contact ""
        QContact contact;
        QContactName name = contact.detail<QContactName>();
        name.setFirstName("Fulano");
        name.setMiddleName("de");
        name.setLastName("Tal");
        contact.saveDetail(&name);
        bool result = m_manager->saveContact(&contact);
        QCOMPARE(result, true);

        // check result
        QList<QContactId> ids;
        ids << contact.id();
        QList<QContact> contacts = m_manager->contacts(ids);

        QCOMPARE(contacts[0].details<QContactTag>().size(), 1);
        QCOMPARE(contacts[0].detail<QContactDisplayLabel>().label(), QStringLiteral("Fulano de Tal"));
        QCOMPARE(contacts[0].detail<QContactTag>().tag(), QStringLiteral("FULANO DE TAL"));

        // Change contact name
        name = contact.detail<QContactName>();
        name.setFirstName("xFulano");
        contact.saveDetail(&name);
        m_manager->saveContact(&contact);

        contacts = m_manager->contacts(ids);
        QCOMPARE(contacts[0].details<QContactTag>().size(), 1);
        QCOMPARE(contacts[0].detail<QContactDisplayLabel>().label(), QStringLiteral("xFulano de Tal"));
        QCOMPARE(contacts[0].detail<QContactTag>().tag(), QStringLiteral("XFULANO DE TAL"));
    }

    void testContactChangeOrder()
    {
        // create a contact "A de Tal"
        QContact contactA;
        QContactName name = contactA.detail<QContactName>();
        name.setFirstName("A");
        name.setMiddleName("de");
        name.setLastName("Tal");
        contactA.saveDetail(&name);
        bool result = m_manager->saveContact(&contactA);
        QCOMPARE(result, true);

        // create a contact "B de Tal"
        QContact contactB;
        name = contactB.detail<QContactName>();
        name.setFirstName("B");
        name.setMiddleName("de");
        name.setLastName("Tal");
        contactB.saveDetail(&name);
        result = m_manager->saveContact(&contactB);
        QCOMPARE(result, true);

        // create a contact "C de Tal"
        QContact contactC;
        name = contactC.detail<QContactName>();
        name.setFirstName("C");
        name.setMiddleName("de");
        name.setLastName("Tal");
        contactC.saveDetail(&name);
        result = m_manager->saveContact(&contactC);
        QCOMPARE(result, true);

        // create a contact "D de Tal"
        QContact contactD;
        name = contactD.detail<QContactName>();
        name.setFirstName("D");
        name.setMiddleName("de");
        name.setLastName("Tal");
        contactD.saveDetail(&name);
        result = m_manager->saveContact(&contactD);
        QCOMPARE(result, true);

        // check result
        QContactFilter filter;
        QList<QContact> contacts = m_manager->contacts(filter);

        QCOMPARE(contacts[0].detail<QContactDisplayLabel>().label(), QStringLiteral("A de Tal"));
        QCOMPARE(contacts[1].detail<QContactDisplayLabel>().label(), QStringLiteral("B de Tal"));
        QCOMPARE(contacts[2].detail<QContactDisplayLabel>().label(), QStringLiteral("C de Tal"));
        QCOMPARE(contacts[3].detail<QContactDisplayLabel>().label(), QStringLiteral("D de Tal"));

        // Update contact B name
        contactB = contacts[1];
        name = contactB.detail<QContactName>();
        name.setFirstName("X");
        contactB.saveDetail(&name);
        m_manager->saveContact(&contactB);

        // Fetch contacts again
        contacts = m_manager->contacts(filter);

        // check new order
        QCOMPARE(contacts[0].detail<QContactDisplayLabel>().label(), QStringLiteral("A de Tal"));
        QCOMPARE(contacts[1].detail<QContactDisplayLabel>().label(), QStringLiteral("C de Tal"));
        QCOMPARE(contacts[2].detail<QContactDisplayLabel>().label(), QStringLiteral("D de Tal"));
        QCOMPARE(contacts[3].detail<QContactDisplayLabel>().label(), QStringLiteral("X de Tal"));
    }

    /*
     * Test create a contact with two address
     * BUG: #1334402
     */
    void testCreateContactWithTwoAddress()
    {
        QContact contact = galera::VCardParser::vcardToContact("BEGIN:VCARD\r\n"
                                                               "VERSION:3.0\r\n"
                                                               "N:Gump;Forrest;Mr.\r\n"
                                                               "FN:Forrest Gump\r\n"
                                                               "ORG:Bubba Gump Shrimp Co.\r\n"
                                                               "TITLE:Shrimp Man\r\n"
                                                               "PHOTO;VALUE=URL;TYPE=GIF:http://www.example.com/dir_photos/my_photo.gif\r\n"
                                                               "TEL;TYPE=WORK,VOICE:(111) 555-12121\r\n"
                                                               "TEL;TYPE=HOME,VOICE:(404) 555-1212\r\n"
                                                               "ADR;TYPE=WORK:;;100 Waters Edge;Baytown;LA;30314;United States of America\r\n"
                                                               "LABEL;TYPE=WORK:100 Waters Edge\nBaytown, LA 30314\nUnited States of America\r\n"
                                                               "ADR;TYPE=HOME:;;42 Plantation St.;Baytown;LA;30314;United States of America\r\n"
                                                               "LABEL;TYPE=HOME:42 Plantation St.\nBaytown, LA 30314\nUnited States of America\r\n"
                                                               "EMAIL;TYPE=PREF,INTERNET:forrestgump@example.com\r\n"
                                                               "REV:2008-04-24T19:52:43Z\r\n"
                                                               "END:VCARD\r\n");


        // create a contact
        QSignalSpy spyContactAdded(m_manager, SIGNAL(contactsAdded(QList<QContactId>)));
        bool result = m_manager->saveContact(&contact);
        QCOMPARE(result, true);
        QTRY_COMPARE(spyContactAdded.count(), 1);

        // query for new contacts
        QContactFilter filter;
        QList<QContact> contacts = m_manager->contacts(filter);
        QCOMPARE(contacts.size(), 1);

        QContact createdContact = contacts[0];
        // id
        QVERIFY(!createdContact.id().isNull());

        // address
        QList<QContactAddress> address = createdContact.details<QContactAddress>();
        QCOMPARE(address.size(), 2);

        QList<QContactAddress> originalAddress = contact.details<QContactAddress>();
        Q_FOREACH(const QContactAddress addr, originalAddress) {
            address.removeAll(addr);
        }
        QCOMPARE(address.size(), 0);
    }

    /*
     * Test saving a contact with a multi line field
     * BUG: #240587
     */
    void testFieldWithNewLineChar()
    {
        // create a contact
        QContact contact = testContact();
        QContactAddress addr;
        addr.setCountry("Line1\nLine2\r\nLine3");
        contact.saveDetail(&addr);
        QSignalSpy spyContactAdded(m_manager, SIGNAL(contactsAdded(QList<QContactId>)));
        bool result = m_manager->saveContact(&contact);
        QCOMPARE(result, true);
        QTRY_COMPARE(spyContactAdded.count(), 1);

        addr = contact.detail<QContactAddress>();
        QCOMPARE(addr.country(), QStringLiteral("Line1Line2Line3"));
    }

    void testUpdateAcontactWithoutChange()
    {
        // filter all contacts
        QContactFilter filter;

        // create a contact
        QContactPhoneNumber pn;
        QContact contact = testContact();

        pn.setNumber("1234567");
        pn.setContexts(QList<int>() << 0);
        contact.saveDetail(&pn);

        pn = QContactPhoneNumber();
        pn.setNumber("12345678");
        pn.setContexts(QList<int>() << 1);
        contact.saveDetail(&pn);

        QSignalSpy spyContactAdded(m_manager, SIGNAL(contactsAdded(QList<QContactId>)));
        bool result = m_manager->saveContact(&contact);
        QCOMPARE(result, true);
        QTRY_COMPARE(spyContactAdded.count(), 1);

        // save contact again without any change
        QContact contact2 = testContact();

        pn = QContactPhoneNumber();
        pn.setNumber("12345678");
        pn.setContexts(QList<int>() << 1);
        contact2.saveDetail(&pn);

        pn = QContactPhoneNumber();
        pn.setNumber("1234567");
        pn.setContexts(QList<int>() << 0);
        contact2.saveDetail(&pn);

        contact2.setId(contact.id());
        QSignalSpy spyContactChanged(m_manager, SIGNAL(contactsChanged(QList<QContactId>)));
        result = m_manager->saveContact(&contact2);
        QCOMPARE(result, true);
        QTRY_COMPARE(spyContactChanged.count(), 1);

        // query for the contacts
        QList<QContact> contacts = m_manager->contacts(filter);
        QCOMPARE(contacts.size(), 1);
        QContact updatedContact = contacts[0];

        // name
        QContactName name = contact.detail<QContactName>();
        QContactName updatedName = updatedContact.detail<QContactName>();
        QCOMPARE(updatedName.firstName(), name.firstName());
        QCOMPARE(updatedName.middleName(), name.middleName());
        QCOMPARE(updatedName.lastName(), name.lastName());
    }

    void testAddressWithMultipleSubTypes()
    {
        // create a contact
        QContact contact = testContact();
        QContactAddress addr;
        addr.setLocality("8777 West Six Mile Road");
        addr.setContexts(QList<int>() << QContactDetail::ContextWork);
        addr.setSubTypes(QList<int>() << QContactAddress::SubTypeParcel << QContactAddress::SubTypeDomestic);
        contact.saveDetail(&addr);

        QSignalSpy spyContactAdded(m_manager, SIGNAL(contactsAdded(QList<QContactId>)));
        bool result = m_manager->saveContact(&contact);
        QCOMPARE(result, true);
        QTRY_COMPARE(spyContactAdded.count(), 1);
    }
};

QTEST_MAIN(QContactsTest)

#include "qcontacts-test.moc"
