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
        QCOMPARE(target.syncTarget(), QString("Dummy personas"));
    }

    /*
     * Test create a new contact
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
        QCOMPARE(contacts[0].id().toString(), QStringLiteral("qtcontacts:galera::dummy-store"));
        QCOMPARE(contacts[0].type(), QContactType::TypeGroup);

        QContactDisplayLabel label = contacts[0].detail(QContactDisplayLabel::Type);
        QCOMPARE(label.label(), QStringLiteral("Dummy personas"));
    }

    /*
     * Test sort contacts by tag
     */
    void testQuerySortedByTag()
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

        // sort contact by tag
        QContactFilter filter;
        QContactSortOrder sortTag;
        sortTag.setDetailType(QContactDetail::TypeTag, QContactTag::FieldTag);
        sortTag.setDirection(Qt::AscendingOrder);
        sortTag.setCaseSensitivity(Qt::CaseInsensitive);
        sortTag.setBlankPolicy(QContactSortOrder::BlanksLast);
        QList<QContactSortOrder> sortOrders;
        sortOrders << sortTag;

        // check result
        QList<QContact> contacts = m_manager->contacts(filter, sortOrders);
        QCOMPARE(contacts.size(), 7);
        QCOMPARE(contacts[0].tags().at(0), QStringLiteral("AA"));
        QCOMPARE(contacts[1].tags().at(0), QStringLiteral("BA"));
        QCOMPARE(contacts[2].tags().at(0), QStringLiteral("BB"));
        QCOMPARE(contacts[3].tags().at(0), QStringLiteral("X"));
        QCOMPARE(contacts[4].tags().at(0), QStringLiteral("#"));
        QCOMPARE(contacts[5].tags().at(0), QStringLiteral("#*"));
        QCOMPARE(contacts[6].tags().at(0), QStringLiteral("#321"));
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
};

QTEST_MAIN(QContactsTest)

#include "qcontacts-test.moc"
