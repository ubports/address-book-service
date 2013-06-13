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

#ifndef __GALERA_SORT_CLAUSE_H__
#define __GALERA_SORT_CLAUSE_H__

#include <QtCore/QString>
#include <QtCore/QList>

#include <QtContacts/QContactSortOrder>

namespace galera
{
class ContactEntry;

class SortClause
{
public:
    SortClause(const QString &sort);
    SortClause(QList<QtContacts::QContactSortOrder> sort);
    SortClause(const SortClause &other);

    QString toString() const;
    QList<QtContacts::QContactSortOrder> toContactSortOrder() const;

private:
    QList<QtContacts::QContactSortOrder> m_sortOrders;

    QtContacts::QContactSortOrder fromString(const QString &clause) const;
    QString toString(const QtContacts::QContactSortOrder &sort) const;
    void initialize() const;
};

}

#endif
