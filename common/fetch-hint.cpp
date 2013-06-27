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

#include "fetch-hint.h"

#include <QtContacts/qcontactdetails.h>

using namespace QtContacts;

namespace galera
{

FetchHint::FetchHint()
{
    //empty
}

FetchHint::FetchHint(const QtContacts::QContactFetchHint &hint)
    : m_hint(hint)
{
    update();
}

bool FetchHint::isEmpty() const
{
    return m_strHint.isEmpty();
}

FetchHint::FetchHint(const QString &hint)
    : m_hint(buildFilter(hint))
{
    update();
}

FetchHint::FetchHint(const FetchHint &other)
    : m_hint(other.m_hint)
{
    update();
}

QString FetchHint::toString() const
{
    return m_strHint;
}

QStringList FetchHint::fields() const
{
    return m_fields;
}

void FetchHint::update()
{
    m_strHint.clear();
    m_fields.clear();

    QMap<QString, QtContacts::QContactDetail::DetailType> map = contactFieldNames();
    Q_FOREACH(QContactDetail::DetailType type, m_hint.detailTypesHint()) {
        QString fieldName = map.key(type, "");
        if (!fieldName.isEmpty()) {
            m_fields << fieldName;
        }
    }

    if (!m_fields.isEmpty()) {
        m_strHint = QString("FIELDS:") + m_fields.join(",");
    }
}

QtContacts::QContactFetchHint FetchHint::toContactFetchHint() const
{
    return m_hint;
}

QMap<QString, QtContacts::QContactDetail::DetailType> FetchHint::contactFieldNames()
{
    static QMap<QString, QContactDetail::DetailType> map;
    if (map.isEmpty()) {
        map.insert("ADR", QContactAddress::Type);
        map.insert("BDAY", QContactBirthday::Type);
        map.insert("EMAIL", QContactEmailAddress::Type);
        map.insert("FN", QContactDisplayLabel::Type);
        map.insert("GENDER", QContactGender::Type);
        map.insert("N", QContactName::Type);
        map.insert("NICKNAME", QContactNickname::Type);
        map.insert("NOTE", QContactNote::Type);
        map.insert("ORG", QContactOrganization::Type);
        map.insert("PHOTO", QContactAvatar::Type);
        map.insert("TEL", QContactPhoneNumber::Type);
        map.insert("URL", QContactUrl::Type);
    }
    return map;
}

QList<QContactDetail::DetailType> FetchHint::parseFieldNames(QStringList fieldNames)
{
    QList<QContactDetail::DetailType> result;
    const QMap<QString, QtContacts::QContactDetail::DetailType> map = contactFieldNames();
    Q_FOREACH(QString fieldName, fieldNames) {
        if (map.contains(fieldName)) {
            result << map[fieldName];
        }
    }
    return result;
}


// Parse string
// Format: <KEY0>:VALUE0,VALUE1;<KEY1>:VALUE0,VALUE1
QtContacts::QContactFetchHint FetchHint::buildFilter(const QString &originalHint)
{
    QString hint = QString(originalHint).replace(" ","");
    QContactFetchHint result;
    QStringList groups = hint.split(";");
    Q_FOREACH(QString group, groups) {
        QStringList values = group.split(":");

        if (values.count() == 2) {
            if (values[0] == "FIELDS") {
                QList<QContactDetail::DetailType> fields;
                QMap<QString, QtContacts::QContactDetail::DetailType> map = contactFieldNames();
                Q_FOREACH(QString field, values[1].split(",")) {
                    if (map.contains(field)) {
                        fields << map[field];
                    }
                }
                result.setDetailTypesHint(fields);
            }
        } else {
            qWarning() << "invalid fech hint: " << values;
        }
    }

    return result;
}

}


