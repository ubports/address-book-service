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

#include "common/fetch-hint.h"

using namespace QtContacts;
using namespace galera;

class FetchHintTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:

    void testSimpleFields()
    {
        const QString strHint = QString("FIELDS:N,TEL");
        FetchHint hint(strHint);

        QVERIFY(!hint.isEmpty());
        QCOMPARE(hint.fields(), QStringList() << "N" << "TEL");

        QContactFetchHint cHint;
        cHint.setDetailTypesHint(QList<QContactDetail::DetailType>() << QContactDetail::TypeName
                                                                     << QContactDetail::TypePhoneNumber);
        QVERIFY(cHint.detailTypesHint() == hint.toContactFetchHint().detailTypesHint());

        FetchHint hint2(cHint);
        QCOMPARE(hint2.toString(), strHint);
    }

    void testInvalidHint()
    {
        const QString strHint = QString("FIELDSS:N,TEL");
        FetchHint hint(strHint);

        QVERIFY(hint.isEmpty());
    }

    void testInvalidFieldName()
    {
        const QString strHint = QString("FIELDS:N,NONE,TEL,INVALID");
        FetchHint hint(strHint);

        QVERIFY(!hint.isEmpty());
        QCOMPARE(hint.fields(), QStringList() << "N" << "TEL");
    }

    void testParseFieldNames()
    {
        QList<QContactDetail::DetailType> fields = FetchHint::parseFieldNames(QStringList() << "ADR" << "BDAY" << "N" << "TEL");
        QList<QContactDetail::DetailType> expectedFields;
        expectedFields << QContactDetail::TypeAddress
                       << QContactDetail::TypeBirthday
                       << QContactDetail::TypeName
                       << QContactDetail::TypePhoneNumber;

        QCOMPARE(fields, expectedFields);
    }
};

QTEST_MAIN(FetchHintTest)

#include "fetch-hint-test.moc"
