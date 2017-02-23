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
    QStringList m_vcards;
    QList<QContact> m_contacts;

    void compareContact(const QtContacts::QContact &contact, const QtContacts::QContact &other)
    {
        // name
        QCOMPARE(contact.detail(QtContacts::QContactDetail::TypeName),
                 other.detail(QtContacts::QContactDetail::TypeName));

        // phone - this is necessary because:
        //    1 ) the QContactDetail::FieldDetailUri can change based on the detail order
        //    2 ) the phone number can be returned in different order
        QList<QtContacts::QContactDetail> phones = contact.details(QtContacts::QContactDetail::TypePhoneNumber);
        QList<QtContacts::QContactDetail> otherPhones = other.details(QtContacts::QContactDetail::TypePhoneNumber);
        QCOMPARE(phones.size(), otherPhones.size());
        for(int i=0; i < phones.size(); i++) {
            QtContacts::QContactDetail phone = phones[i];
            bool found = false;
            for(int x=0; x < otherPhones.size(); x++) {
                QtContacts::QContactDetail otherPhone = otherPhones[x];
                if (phone.value(QtContacts::QContactPhoneNumber::FieldNumber) ==
                    otherPhone.value(QtContacts::QContactPhoneNumber::FieldNumber)) {
                    found = true;
                    QList<int> phoneTypes = phone.value(QtContacts::QContactPhoneNumber::FieldSubTypes).value< QList<int> >();
                    QList<int> otherPhoneTypes = otherPhone.value(QtContacts::QContactPhoneNumber::FieldSubTypes).value< QList<int> >();
                    QCOMPARE(phoneTypes, otherPhoneTypes);
                    QCOMPARE(phone.value(QtContacts::QContactPhoneNumber::FieldContext),
                         otherPhone.value(QtContacts::QContactPhoneNumber::FieldContext));
                    break;
                }
            }
            QVERIFY2(found, "Phone number is not equal");
        }

        // email same as phone number
        QList<QtContacts::QContactDetail> emails = contact.details(QtContacts::QContactDetail::TypeEmailAddress);
        QList<QtContacts::QContactDetail> otherEmails = other.details(QtContacts::QContactDetail::TypeEmailAddress);
        QCOMPARE(emails.size(), otherEmails.size());
        for(int i=0; i < emails.size(); i++) {
            QtContacts::QContactDetail email = emails[i];
            bool found = false;
            for(int x=0; x < otherEmails.size(); x++) {
                QtContacts::QContactDetail otherEmail = otherEmails[x];
                if (email.value(QtContacts::QContactEmailAddress::FieldEmailAddress) ==
                    otherEmail.value(QtContacts::QContactEmailAddress::FieldEmailAddress)) {
                    found = true;
                    QCOMPARE(email.value(QtContacts::QContactEmailAddress::FieldContext),
                             otherEmail.value(QtContacts::QContactEmailAddress::FieldContext));
                    break;
                }
            }
            QVERIFY2(found, "Email is not equal");
        }
    }

    /*
     * Use this function to compare vcards because the order of the attributes in the returned vcard
     * can be different for each vcard. Example:
     *      "TEL;PID=1.1;TYPE=ISDN:33331410\r\n" or "TEL;TYPE=ISDN;PID=1.1:33331410\r\n"
     */
    void compareVCards(const QString &vcard, const QString &other)
    {
        QStringList vcardLines = vcard.split("\n", QString::SkipEmptyParts);
        QStringList otherLines = other.split("\n", QString::SkipEmptyParts);

        QCOMPARE(vcardLines.size(), otherLines.size());

        for(int i=0; i < vcardLines.size(); i++) {
            QString value = vcardLines[i].split(":").last();
            QString otherValue = otherLines.first().split(":").last();

            // compare values. After ":"
            QCOMPARE(value, otherValue);

            QString attribute = vcardLines[i].split(":").first();
            QString attributeOther = otherLines.first().split(":").first();

            // compare attributes. Before ":"
            QStringList attributeFields = attribute.split(";");
            QStringList attributeOtherFields = attributeOther.split(";");
            Q_FOREACH(const QString &attr, attributeFields) {
                attributeOtherFields.removeOne(attr);
            }

            QVERIFY2(attributeOtherFields.size() == 0,
                     QString("Vcard attribute is not equal (%1) != (%2)").arg(vcardLines[i]).arg(otherLines.first()).toUtf8());

            otherLines.removeFirst();
        }
    }


private Q_SLOTS:
    void init()
    {
        m_vcards << QStringLiteral("BEGIN:VCARD\r\n"
                                "VERSION:3.0\r\n"
                                "N:Sauro;Dino;da Silva;;\r\n"
                                "EMAIL:dino@familiadinosauro.com.br\r\n"
                                "TEL;PID=1.1;TYPE=ISDN:33331410\r\n"
                                "TEL;PID=1.2;TYPE=CELL:8888888\r\n"
                                "END:VCARD\r\n");
        m_vcards << QStringLiteral("BEGIN:VCARD\r\n"
                                "VERSION:3.0\r\n"
                                "N:Sauro;Baby;da Silva;;\r\n"
                                "EMAIL:baby@familiadinosauro.com.br\r\n"
                                "TEL;PID=1.1;TYPE=ISDN:1111111\r\n"
                                "TEL;PID=1.2;TYPE=CELL:2222222\r\n"
                                "END:VCARD\r\n");

        QContact contactDino;
        QContactName name;
        name.setFirstName("Dino");
        name.setMiddleName("da Silva");
        name.setLastName("Sauro");
        contactDino.saveDetail(&name);

        QContactEmailAddress email;
        email.setEmailAddress("dino@familiadinosauro.com.br");
        contactDino.saveDetail(&email);

        QContactPhoneNumber phoneLandLine;
        phoneLandLine.setSubTypes(QList<int>() << QContactPhoneNumber::SubTypeLandline);
        phoneLandLine.setNumber("33331410");
        phoneLandLine.setDetailUri("1.1");
        contactDino.saveDetail(&phoneLandLine);

        QContactPhoneNumber phoneMobile;
        phoneMobile.setSubTypes(QList<int>() << QContactPhoneNumber::SubTypeMobile);
        phoneMobile.setNumber("8888888");
        phoneMobile.setDetailUri("1.2");
        contactDino.saveDetail(&phoneMobile);

        QContact contactBaby;
        name.setFirstName("Baby");
        name.setMiddleName("da Silva");
        name.setLastName("Sauro");
        contactBaby.saveDetail(&name);

        email.setEmailAddress("baby@familiadinosauro.com.br");
        contactBaby.saveDetail(&email);

        phoneLandLine.setSubTypes(QList<int>() << QContactPhoneNumber::SubTypeLandline);
        phoneLandLine.setNumber("1111111");
        phoneLandLine.setDetailUri("1.1");
        contactBaby.saveDetail(&phoneLandLine);

        phoneMobile.setSubTypes(QList<int>() << QContactPhoneNumber::SubTypeMobile);
        phoneMobile.setNumber("2222222");
        phoneMobile.setDetailUri("1.2");
        contactBaby.saveDetail(&phoneMobile);

        m_contacts << contactDino << contactBaby;
    }

    void cleanup()
    {
        m_vcards.clear();
        m_contacts.clear();
    }

    /*
     * Test parse from vcard to contact using the async function
     */
    void testVCardToContactAsync()
    {
        VCardParser parser;
        qRegisterMetaType< QList<QtContacts::QContact> >();
        QSignalSpy vcardToContactSignal(&parser, SIGNAL(contactsParsed(QList<QtContacts::QContact>)));
        parser.vcardToContact(m_vcards);

        QTRY_COMPARE(vcardToContactSignal.count(), 1);
        QList<QVariant> arguments = vcardToContactSignal.takeFirst();
        QCOMPARE(arguments.size(), 1);
        QList<QtContacts::QContact> contacts =  qvariant_cast<QList<QtContacts::QContact> >(arguments.at(0));
        QCOMPARE(contacts.size(), 2);
        compareContact(contacts[0], m_contacts[0]);
        compareContact(contacts[1], m_contacts[1]);
    }

    /*
     * Test parse from vcard to contact using the sync function
     */
    void testVCardToContactSync()
    {
        QList<QtContacts::QContact> contacts = VCardParser::vcardToContactSync(m_vcards);
        QCOMPARE(contacts.size(), 2);
        compareContact(contacts[0], m_contacts[0]);
        compareContact(contacts[1], m_contacts[1]);
    }

    /*
     * Test parse a single vcard to contact using the sync function
     */
    void testSingleVCardToContactSync()
    {
        QContact contact = VCardParser::vcardToContact(m_vcards[0]);
        compareContact(contact, m_contacts[0]);
    }

    /*
     * Test parse a invalid vcard
     */
    void testInvalidVCard()
    {
        QString vcard("BEGIN:VCARD\r\nEND::VCARD\r\n");
        QContact contact = VCardParser::vcardToContact(vcard);
        QVERIFY(contact.isEmpty());
    }

    /*
     * Test parse contacts to vcard using the async function
     */
    void testContactToVCardAsync()
    {
        VCardParser parser;
        QSignalSpy contactToVCardSignal(&parser, SIGNAL(vcardParsed(QStringList)));
        parser.contactToVcard(m_contacts);

        // Check if the vcardParsed signal was fired
        QTRY_COMPARE(contactToVCardSignal.count(), 1);
        // Check if the signal was fired with two vcards
        QList<QVariant> arguments = contactToVCardSignal.takeFirst();
        QCOMPARE(arguments.size(), 1);
        QStringList vcardsResults = qvariant_cast<QStringList>(arguments.at(0));
        QCOMPARE(vcardsResults.size(), 2);

        // Check if the vcard in the signals was correct parsed
        compareVCards(vcardsResults[0], m_vcards[0]);
        compareVCards(vcardsResults[1], m_vcards[1]);
    }

    /*
     * Test parse contacts to vcard using the sync function
     */
    void testContactToVCardSync()
    {
        QStringList vcards = VCardParser::contactToVcardSync(m_contacts);
        QCOMPARE(vcards.size(), 2);

        // Check if the returned vcards are correct
        compareVCards(vcards[0], m_vcards[0]);
        compareVCards(vcards[1], m_vcards[1]);
    }

    /*
     * Test parse a single contact to vcard using the sync function
     */
    void testSingContactToVCardSync()
    {
        QString vcard = VCardParser::contactToVcard(m_contacts[0]);

        // Check if the returned vcard is correct
        compareVCards(vcard, m_vcards[0]);
    }


    /*
     * Test parse a vcard with sync target into a Contact
     */
    void testVCardWithSyncTargetToContact()
    {
        QString vcard = QStringLiteral("BEGIN:VCARD\r\n"
                                       "VERSION:3.0\r\n"
                                       "CLIENTPIDMAP;PID=1.ADDRESSBOOKID0:ADDRESSBOOKNAME0\r\n"
                                       "CLIENTPIDMAP;PID=2.ADDRESSBOOKID1:ADDRESSBOOKNAME1\r\n"
                                       "N:Sauro;Dino;da Silva;;\r\n"
                                       "EMAILPID=1.1;:dino@familiadinosauro.com.br\r\n"
                                       "TEL;PID=1.1;TYPE=ISDN:33331410\r\n"
                                       "TEL;PID=1.2;TYPE=CELL:8888888\r\n"
                                       "END:VCARD\r\n");
        QContact contact = VCardParser::vcardToContact(vcard);
        QList<QContactSyncTarget> targets = contact.details<QContactSyncTarget>();
        QCOMPARE(targets.size(), 2);

        QContactSyncTarget target0;
        QContactSyncTarget target1;

        // put the target in order
        if (targets[0].detailUri().startsWith("1.")) {
            target0 = targets[0];
            target1 = targets[1];
        } else {
            target0 = targets[1];
            target1 = targets[2];
        }

        QCOMPARE(target0.detailUri(), QString("1.ADDRESSBOOKID0"));
        QCOMPARE(target0.syncTarget(), QString("ADDRESSBOOKNAME0"));
        QCOMPARE(target1.detailUri(), QString("2.ADDRESSBOOKID1"));
        QCOMPARE(target1.syncTarget(), QString("ADDRESSBOOKNAME1"));
    }

    /*
     * Test parse a Contact with sync target into a vcard
     */
    void testContactWithSyncTargetToVCard()
    {
        QContact c = m_contacts[0];

        QContactSyncTarget target;
        target.setDetailUri("1.ADDRESSBOOKID0");
        target.setSyncTarget("ADDRESSBOOKNAME0");
        c.saveDetail(&target);

        QContactSyncTarget target1;
        target1.setDetailUri("2.ADDRESSBOOKID1");
        target1.setSyncTarget("ADDRESSBOOKNAME1");
        c.saveDetail(&target1);

        QString vcard = VCardParser::contactToVcard(c);
        QVERIFY(vcard.contains("CLIENTPIDMAP;PID=1.ADDRESSBOOKID0:ADDRESSBOOKNAME0"));
        QVERIFY(vcard.contains("CLIENTPIDMAP;PID=2.ADDRESSBOOKID1:ADDRESSBOOKNAME1"));
    }

    /*
     * Test parse a vcard with timestamp
     */
    void testVCardWithTimestamp()
    {
        QString vcard = QStringLiteral("BEGIN:VCARD\r\n"
                                       "VERSION:3.0\r\n"
                                       "N:Sauro;Dino;da Silva;;\r\n"
                                       "EMAILPID=1.1;:dino@familiadinosauro.com.br\r\n"
                                       "TEL;PID=1.1;TYPE=ISDN:33331410\r\n"
                                       "TEL;PID=1.2;TYPE=CELL:8888888\r\n"
                                       "X-CREATED-AT:2015-04-14T16:16:44Z\r\n"
                                       "REV:2015-04-14T21:56:56Z(2300)\r\n"
                                       "END:VCARD\r\n");
        QContact contact = VCardParser::vcardToContact(vcard);
        QList<QContactTimestamp> timestamps = contact.details<QContactTimestamp>();
        QCOMPARE(timestamps.size(), 1);

        QContactTimestamp timestamp = timestamps.at(0);
        QCOMPARE(timestamp.created(), QDateTime::fromString("2015-04-14T16:16:44Z", Qt::ISODate));
        QCOMPARE(timestamp.lastModified(), QDateTime::fromString("2015-04-14T21:56:56Z(2300)", Qt::ISODate));
    }

    /*
     * Test parse a vcard with extend details
     */
    void testVCardWithExtendDetails()
    {
        QString vcard = QStringLiteral("BEGIN:VCARD\r\n"
                                       "VERSION:3.0\r\n"
                                       "N:Sauro;Dino;da Silva;;\r\n"
                                       "EMAILPID=1.1;:dino@familiadinosauro.com.br\r\n"
                                       "TEL;PID=1.1;TYPE=ISDN:33331410\r\n"
                                       "TEL;PID=1.2;TYPE=CELL:8888888\r\n"
                                       "X-AIM:foo@aim.com\r\n"
                                       "X-REMOTE-ID:MY_REMOTE_ID\r\n"
                                       "END:VCARD\r\n");
        QContact contact = VCardParser::vcardToContact(vcard);
        QList<QContactExtendedDetail> xDetails = contact.details<QContactExtendedDetail>();
        QCOMPARE(xDetails.size(), 1);

        QContactExtendedDetail xDetail = xDetails.at(0);
        QCOMPARE(xDetail.name(), QStringLiteral("X-REMOTE-ID"));
        QCOMPARE(xDetail.data().toString(), QStringLiteral("MY_REMOTE_ID"));
    }

    void testVCardFromGoogle()
    {
        QString vcard = QStringLiteral("BEGIN:VCARD\r\n"
                                       "VERSION:3.0\r\n"
                                       "REV:2015-04-16T15:26:50Z\r\n"
                                       "X-GOOGLE-ETAG:\"RXc_eTVSLit7I2A9XRdUF04NRwc.\"\r\n"
                                       "X-REMOTE-ID:1dd7d51a1518626a\r\n"
                                       "PHOTO;VALUE=URL,URL:https://www.google.com/m8/feeds/photos/media/renato.test\r\n"
                                       " e2%40gmail.com/1dd7d51a1518626a\r\n"
                                       "N:;REnato;;;\r\n"
                                       "EMAIL:renatox@gmail.com\r\n"
                                       "TEL:87042144\r\n"
                                       "CLIENTPIDMAP:56183a5b-5da7-49fe-8cf6-9bfd3633bf6d\r\n"
                                       "END:VCARD\r\n");
        QContact contact = VCardParser::vcardToContact(vcard);
        QList<QContactExtendedDetail> xDetails = contact.details<QContactExtendedDetail>();
        QCOMPARE(xDetails.size(), 2);
    }

     void testContactIdToUid()
     {
         // Create manager to allow us to creact contact id
         QContactManager manager("memory");

         QContact c = m_contacts[0];
         c.setId(QContactId::fromString(QStringLiteral("qtcontacts:memory::11")));
         QString vcard = VCardParser::contactToVcard(c);
         QVERIFY(vcard.contains("UID:11"));
     }

     /*
      * Test parse a vcard with irc
      */
     void testVCardWithIrc()
     {
         QString vcard = QStringLiteral("BEGIN:VCARD\r\n"
                                        "VERSION:3.0\r\n"
                                        "N:Sauro;Dino;da Silva;;\r\n"
                                        "X-IRC;PROVIDER=irc.freenode.net:myIRC\r\n"
                                        "REV:2015-04-14T21:56:56Z(2300)\r\n"
                                        "END:VCARD\r\n");
         QContact contact = VCardParser::vcardToContact(vcard);
         QList<QContactOnlineAccount> accs = contact.details<QContactOnlineAccount>();
         QCOMPARE(accs.size(), 1);

         QContactOnlineAccount acc = accs.at(0);
         QCOMPARE(acc.protocol(), QContactOnlineAccount::ProtocolIrc);
         QCOMPARE(acc.accountUri(), QStringLiteral("myIRC"));
         QCOMPARE(acc.serviceProvider(), QStringLiteral("irc.freenode.net"));

         QList<QContactExtendedDetail> exs = contact.details<QContactExtendedDetail>();
         QCOMPARE(exs.size(), 1);

         QContactExtendedDetail ex = exs.at(0);
         QCOMPARE(ex.name(), QStringLiteral("X-IRC"));
         QCOMPARE(ex.data().toString(), QStringLiteral("myIRC"));
         QCOMPARE(ex.value(QContactExtendedDetail::FieldData+1).toString(), QStringLiteral("irc.freenode.net"));
     }

     void testContactWithIrc()
     {
         QContact c = m_contacts[0];

         QContactOnlineAccount acc;
         acc.setProtocol(QContactOnlineAccount::ProtocolIrc);
         acc.setAccountUri(QStringLiteral("myIRC"));
         acc.setServiceProvider(QStringLiteral("irc.freenode.net"));
         c.saveDetail(&acc);

         QString vcard = VCardParser::contactToVcard(c);
         QVERIFY(vcard.contains("X-IRC;PROVIDER=irc.freenode.net:myIRC"));
     }
};

QTEST_MAIN(VCardParseTest)

#include "vcardparser-test.moc"
