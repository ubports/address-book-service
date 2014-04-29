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

#ifndef __GALERA_CONTACT_LESS_THAN_H__
#define __GALERA_CONTACT_LESS_THAN_H__

#include "common/sort-clause.h"

#include <QtCore/QString>
#include <QtCore/QVariant>

#include <QtContacts/QContact>

namespace galera {

class ContactEntry;

class ContactLessThan
{
public:
    ContactLessThan(const SortClause &sortClause);

    bool operator()(ContactEntry *entryA, ContactEntry *entryB);

private:
    SortClause m_sortClause;
};

} // namespace

#endif //__GALERA_CONTACT_LESS_THAN_H__
