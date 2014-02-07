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

ContactEntry *ContactsMap::value(const QString &id) const
{
    return m_idToEntry[id];
}

ContactEntry *ContactsMap::take(FolksIndividual *individual)
{
    QString contactId = QString::fromUtf8(folks_individual_get_id(individual));
    return take(contactId);
}

ContactEntry *ContactsMap::take(const QString &id)
{
    QMutexLocker locker(&m_mutex);
    return m_idToEntry.take(id);
}

void ContactsMap::remove(const QString &id)
{
    QMutexLocker locker(&m_mutex);
    ContactEntry *entry = m_idToEntry.value(id,0);
    if (entry) {
        m_idToEntry.remove(id);
        delete entry;
    }
}

void ContactsMap::insert(ContactEntry *entry)
{
    QMutexLocker locker(&m_mutex);
    FolksIndividual *fIndividual = entry->individual()->individual();
    if (fIndividual) {
        m_idToEntry.insert(folks_individual_get_id(fIndividual), entry);
    }
}

int ContactsMap::size() const
{
    return m_idToEntry.size();
}

void ContactsMap::clear()
{
    QMutexLocker locker(&m_mutex);
    QList<ContactEntry*> entries = m_idToEntry.values();
    m_idToEntry.clear();
    qDeleteAll(entries);
}

void ContactsMap::lock()
{
    m_mutex.lock();
}

void ContactsMap::unlock()
{
    m_mutex.unlock();
}

QList<ContactEntry*> ContactsMap::values() const
{
    return m_idToEntry.values();
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

bool ContactsMap::contains(FolksIndividual *individual) const
{
    QString contactId = QString::fromUtf8(folks_individual_get_id(individual));
    return contains(contactId);
}

bool ContactsMap::contains(const QString &id) const
{
    return m_idToEntry.contains(id);
}

ContactEntry *ContactsMap::value(FolksIndividual *individual) const
{
    QString contactId = QString::fromUtf8(folks_individual_get_id(individual));
    return m_idToEntry.value(contactId, 0);
}

} //namespace
