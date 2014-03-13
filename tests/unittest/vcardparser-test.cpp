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

#include "common/vcard-parser.h"

using namespace QtContacts;
using namespace galera;

typedef QList<QContact> QContactList;

class VCardParseTest : public QObject
{
    Q_OBJECT

private:
    QStringList vcards()
    {
        QStringList cards;
        cards << QStringLiteral("BEGIN:VCARD\r\n"
                                "VERSION:3.0\r\n"
                                "N:Tal;Fulano_;de;;\r\n"
                                "EMAIL:fulano_@ubuntu.com\r\n"
                                "TEL;PID=1.1;TYPE=ISDN:33331410\r\n"
                                "TEL;PID=1.2;TYPE=CELL:8888888\r\n"
                                "END:VCARD\r\n");
        cards << QStringLiteral("BEGIN:VCARD\r\n"
                                "VERSION:3.0\r\n"
                                "N:Araujo;Renato;Oliveira;;\r\n"
                                "EMAIL:renato@email.com\r\n"
                                "TEL;PID=1.1;TYPE=ISDN:1111111\r\n"
                                "TEL;PID=1.2;TYPE=CELL:2222222\r\n"
                                "END:VCARD\r\n");

        return cards;
    }

private Q_SLOTS:

    void testVCardToContact()
    {
        VCardParser parser;
        qRegisterMetaType< QList<QtContacts::QContact> >();
        QSignalSpy vcardToContactSignal(&parser, SIGNAL(contactsParsed(QList<QtContacts::QContact>)));
        parser.vcardToContact(vcards());

        QTRY_COMPARE(vcardToContactSignal.count(), 1);
        QList<QVariant> arguments = vcardToContactSignal.takeFirst();
        QCOMPARE(arguments.size(), 1);
        QList<QtContacts::QContact> contacts =  qvariant_cast<QList<QtContacts::QContact> >(arguments.at(0));
        QCOMPARE(contacts.size(), 2);
    }

    void testContactToVCard()
    {
        QList<QContact> contacts;
        Q_FOREACH(QString vcard, vcards()) {
            contacts << VCardParser::vcardToContact(vcard);
        }

        QCOMPARE(contacts.size(), 2);

        VCardParser parser;
        QSignalSpy contactToVCardSignal(&parser, SIGNAL(vcardParsed(QStringList)));
        parser.contactToVcard(contacts);

        QTRY_COMPARE(contactToVCardSignal.count(), 1);
        QList<QVariant> arguments = contactToVCardSignal.takeFirst();
        QCOMPARE(arguments.size(), 1);
        QStringList vcardsResults = qvariant_cast<QStringList>(arguments.at(0));
        QCOMPARE(vcardsResults.size(), 2);
        /*
        QStringList expectedVcards = vcards();
        QCOMPARE(vcardsResults.size(), expectedVcards.size());
        Q_FOREACH(QString vcard, vcardsResults) {
            expectedVcards.removeOne(vcard);
        }
        QCOMPARE(expectedVcards.size(), 0);
        */
    }
};

QTEST_MAIN(VCardParseTest)

#include "vcardparser-test.moc"
