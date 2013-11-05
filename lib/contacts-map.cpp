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
    Q_ASSERT(individual);
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
    clear();
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

void ContactsMap::remove(const QString &id)
{
    ContactEntry *entry = m_idToEntry[id];
    if (entry) {
        m_individualsToEntry.remove(entry->individual()->individual());
        m_idToEntry.remove(id);
        delete entry;
    }
}

bool ContactsMap::contains(FolksIndividual *individual) const
{
    return m_individualsToEntry.contains(individual);
}

void ContactsMap::insert(ContactEntry *entry)
{
    FolksIndividual *fIndividual = entry->individual()->individual();
    m_idToEntry.insert(folks_individual_get_id(fIndividual), entry);
    m_individualsToEntry.insert(fIndividual, entry);
}

int ContactsMap::size() const
{
    return m_idToEntry.size();
}

void ContactsMap::clear()
{
    QList<ContactEntry*> entries = m_idToEntry.values();
    m_idToEntry.clear();
    m_individualsToEntry.clear();

    Q_FOREACH(ContactEntry *entry, entries) {
        delete entry;
    }
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
