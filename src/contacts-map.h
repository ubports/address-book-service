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

class ContactEntry
{
public:
    ContactEntry(FolksIndividual *individual);
    ContactEntry(const ContactEntry &other);
    ~ContactEntry();

    FolksIndividual *individual() const;

private:
    ContactEntry();
    FolksIndividual *m_individual;
};


class ContactsMap : public QHash<FolksIndividual *, ContactEntry*>
{
public:
    ContactsMap();
    ~ContactsMap();
};

} //namespace
#endif
