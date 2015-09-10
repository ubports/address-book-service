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

#ifndef __GALERA_CONTACTS_MAP_PRIV_H__
#define __GALERA_CONTACTS_MAP_PRIV_H__

#include "common/sort-clause.h"

#include <QtCore/QString>
#include <QtCore/QHash>
#include <QtCore/QReadWriteLock>

#include <QtContacts/QContactPhoneNumber>

#include <folks/folks.h>
#include <glib.h>
#include <glib-object.h>

namespace galera
{

class QIndividual;

class ContactEntry
{
public:
    ContactEntry(QIndividual *individual);
    ~ContactEntry();

    QIndividual *individual() const;

private:
    ContactEntry();
    ContactEntry(const ContactEntry &other);

    QIndividual *m_individual;
};


class ContactsMap
{
public:
    ContactsMap();
    ~ContactsMap();

    ContactEntry *valueFromVCard(const QString &vcard) const;

    bool contains(FolksIndividual *individual) const;
    bool contains(const QString &id) const;

    ContactEntry *value(FolksIndividual *individual) const;
    ContactEntry *value(const QString &id) const;
    QList<ContactEntry*> valueByPhone(const QString &phone) const;
    QList<ContactEntry*> values(const QStringList &ids) const;

    ContactEntry *take(FolksIndividual *individual);
    ContactEntry *take(const QString &id);

    void remove(const QString &id);
    void insert(ContactEntry *entry);
    void updatePosition(ContactEntry *entry);
    int size() const;
    void clear();
    void lockForRead();
    void unlock();
    QList<ContactEntry*> values() const;
    QList<QtContacts::QContact> contacts() const;
    QStringList keys() const;

    void sertSort(const SortClause &clause);
    SortClause sort() const;

    static SortClause defaultSort();

private:
    QHash<QString, ContactEntry*> m_idToEntry;
    QMultiMap<QString, ContactEntry*> m_phoneToEntry;
    // sorted contacts
    QList<ContactEntry*> m_contacts;
    SortClause m_sortClause;
    QReadWriteLock m_mutex;

    void removeData(ContactEntry *entry, bool del);
    void insertData(ContactEntry *entry);
    void insertdata(const QList<QtContacts::QContactPhoneNumber> &numbers, ContactEntry *entry);
    QString minimalNumber(const QString &phone) const;
};

} //namespace
#endif
