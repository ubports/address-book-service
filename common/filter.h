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

#ifndef __GALERA_FILTER_H__
#define __GALERA_FILTER_H__

#include <QtContacts/QContactFilter>
#include <QtContacts/QContact>

namespace galera
{

class Filter
{
public:
    Filter(const QtContacts::QContactFilter &filter);
    Filter(const QString &filter);
    Filter(const Filter &other);

    QString toString() const;
    QtContacts::QContactFilter toContactFilter() const;
    bool test(const QtContacts::QContact &contact) const;
    bool isValid() const;
    bool isEmpty() const;
    QString phoneNumberToFilter() const;

private:
    QtContacts::QContactFilter m_filter;

    Filter();

    bool checkIsEmpty(const QList<QtContacts::QContactFilter> filters) const;
    bool checkIsValid(const QList<QtContacts::QContactFilter> filters) const;

    static QString phoneNumberToFilter(const QtContacts::QContactFilter &filter);
    static QString toString(const QtContacts::QContactFilter &filter);
    static QtContacts::QContactFilter buildFilter(const QString &filter);

    static QString detailFilterToString(const QtContacts::QContactFilter &filter);
    static QString unionFilterToString(const QtContacts::QContactFilter &filter);
    static QtContacts::QContactFilter parseFilter(const QtContacts::QContactFilter &filter);
    static QtContacts::QContactFilter parseIdFilter(const QtContacts::QContactFilter &filter);
    static QtContacts::QContactFilter parseUnionFilter(const QtContacts::QContactFilter &filter);
    static QtContacts::QContactFilter parseIntersectionFilter(const QtContacts::QContactFilter &filter);
    static bool testFilter(const QtContacts::QContactFilter& filter, const QtContacts::QContact &contact);
    static bool comparePhoneNumbers(const QString &phoneNumberA, const QString &phoneNumberB, QtContacts::QContactFilter::MatchFlags flags);
};

}

#endif
