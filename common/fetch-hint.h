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

#ifndef __GALERA_FETCH_HINT_H__
#define __GALERA_FETCH_HINT_H__

#include <QtContacts/QContactFetchHint>
#include <QtContacts/QContactDetail>
#include <QtContacts/QContact>

namespace galera
{

class FetchHint
{
public:
    FetchHint(const QtContacts::QContactFetchHint &hint);
    FetchHint(const QString &hint);
    FetchHint(const FetchHint &other);
    FetchHint();

    bool isEmpty() const;
    QString toString() const;
    QStringList fields() const;
    QtContacts::QContactFetchHint toContactFetchHint() const;
    static QMap<QString, QtContacts::QContactDetail::DetailType> contactFieldNames();
    static QList<QtContacts::QContactDetail::DetailType> parseFieldNames(const QStringList &fieldNames);

private:
    QtContacts::QContactFetchHint m_hint;
    QString m_strHint;
    QStringList m_fields;



    void update();
    static QtContacts::QContactFetchHint buildFilter(const QString &originalHint);
};

}

#endif
