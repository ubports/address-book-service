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

class QContactsCreateSourceTest : public BaseClientTest
{
    Q_OBJECT
private Q_SLOTS:
    void initTestCase()
    {
        BaseClientTest::initTestCase();
        QCoreApplication::setLibraryPaths(QStringList() << QT_PLUGINS_BINARY_DIR);
    }

    /*
     * Test create a contact source using the contact group
     */
    void testCreateGroup()
    {
        QContactManager manager("galera");

        // create a contact
        QContact contact;
        contact.setType(QContactType::TypeGroup);

        QContactDisplayLabel label;
        label.setLabel("test group");
        contact.saveDetail(&label);

        bool result = manager.saveContact(&contact);
        QCOMPARE(result, true);

        // query for new contacts
        QContactDetailFilter filter;
        filter.setDetailType(QContactDetail::TypeType, QContactType::FieldType);
        filter.setValue(QContactType::TypeGroup);
        QList<QContact> contacts = manager.contacts(filter);

        // will be two sources since we have the main source already created
        QCOMPARE(contacts.size(), 2);

        QContact createdContact = contacts[0];
        // id
        QVERIFY(!createdContact.id().isNull());

        // label
        QContactDisplayLabel createdlabel = createdContact.detail<QContactDisplayLabel>();
        QCOMPARE(createdlabel.label(), label.label());
    }
};

QTEST_MAIN(QContactsCreateSourceTest)

#include "qcontacts-create-source-test.moc"
