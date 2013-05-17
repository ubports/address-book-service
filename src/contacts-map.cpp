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

#include "contacts-map.h"
#include "qindividual.h"

#include <QtCore/QDebug>

namespace galera
{

//ContactInfo
ContactEntry::ContactEntry(QIndividual *individual)
    : m_individual(individual)
{
}

ContactEntry::~ContactEntry()
{
     delete m_individual;
}

QIndividual *ContactEntry::individual() const
{
    return m_individual;
}


//ContactMap
ContactsMap::ContactsMap()
{
}


ContactsMap::~ContactsMap()
{
}

ContactEntry *ContactsMap::value(FolksIndividual *individual) const
{
    return m_individualsToEntry[individual];
}

ContactEntry *ContactsMap::value(const QString &id) const
{
    return m_idToEntry[id];
}

ContactEntry *ContactsMap::take(FolksIndividual *individual)
{
    if (m_individualsToEntry.remove(individual)) {
        return m_idToEntry.take(folks_individual_get_id(individual));
    }
    return 0;
}

bool ContactsMap::contains(FolksIndividual *individual) const
{
    return m_individualsToEntry.contains(individual);
}

void ContactsMap::insert(FolksIndividual *individual, ContactEntry *entry)
{
    m_idToEntry.insert(folks_individual_get_id(individual), entry);
    m_individualsToEntry.insert(individual, entry);
}

QList<ContactEntry*> ContactsMap::values() const
{
    return m_individualsToEntry.values();
}

ContactEntry *ContactsMap::valueFromVCard(const QString &vcard) const
{
    //GET UID
    int startIndex = vcard.indexOf("UID:");
    if (startIndex) {
        startIndex += 4; // "UID:"
        int endIndex = vcard.indexOf("\r\n", startIndex);

        QString id = vcard.mid(startIndex, endIndex - startIndex);
        return m_idToEntry[id];
    }
    return 0;
}

} //namespace
