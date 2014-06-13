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

class QContactsAsyncRequestTest : public BaseClientTest
{
    Q_OBJECT
private Q_SLOTS:
    /*
     * Test cancel a running request
     */
    void testCancelRequest()
    {
        QContactManager manager("galera");

        // create a contact
        QContact contact;
        contact.setType(QContactType::TypeGroup);

        QContactDisplayLabel label;
        label.setLabel("test group");
        contact.saveDetail(&label);

        QContactSaveRequest req;
        req.setManager(&manager);
        req.setContact(contact);

        QSignalSpy spyContactAdded(&manager, SIGNAL(contactsAdded(QList<QContactId>)));
        // start request

        req.start();
        QCOMPARE(req.state(), QContactAbstractRequest::ActiveState);

        // cancel running request
        req.cancel();
        QCOMPARE(req.state(), QContactAbstractRequest::CanceledState);

        // check if the signal did not fire
        QCOMPARE(spyContactAdded.count(), 0);
    }

    /*
     * Test delete a running request
     */
    void testDestroyRequest()
    {
        QContactManager manager("galera");

        // create a contact
        QContact contact;
        contact.setType(QContactType::TypeGroup);

        QContactDisplayLabel label;
        label.setLabel("test group");
        contact.saveDetail(&label);

        QContactSaveRequest *req = new QContactSaveRequest();
        req->setManager(&manager);
        req->setContact(contact);

        QSignalSpy spyContactAdded(&manager, SIGNAL(contactsAdded(QList<QContactId>)));
        // start request

        req->start();
        QCOMPARE(req->state(), QContactAbstractRequest::ActiveState);

        // delete req
        delete req;
        QTest::qWait(100);

        // check if the signal did not fire
        QCOMPARE(spyContactAdded.count(), 0);
    }
};

QTEST_MAIN(QContactsAsyncRequestTest)

#include "qcontacts-async-request-test.moc"
