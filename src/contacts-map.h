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

#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QHash>

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
    ContactEntry *value(FolksIndividual *individual) const;
    ContactEntry *value(const QString &id) const;
    ContactEntry *take(FolksIndividual *individual);
    void insert(ContactEntry *entry);
    int size() const;


    QList<ContactEntry*> values() const;

private:
    QHash<FolksIndividual *, ContactEntry*> m_individualsToEntry;
    QHash<QString, ContactEntry*> m_idToEntry;
};

} //namespace
#endif
