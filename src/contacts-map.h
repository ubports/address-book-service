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
    ContactEntry(FolksIndividual *individual);
    ContactEntry(const ContactEntry &other);
    ~ContactEntry();

    QIndividual *individual() const;

private:
    ContactEntry();

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
    ContactEntry *take(FolksIndividual *individual);
    void insert(FolksIndividual *individual, ContactEntry *entry);


    QList<ContactEntry*> values() const;

private:
    QHash<FolksIndividual *, ContactEntry*> m_individualsToEntry;
    QHash<QString, ContactEntry*> m_idToEntry;
};

} //namespace
#endif
