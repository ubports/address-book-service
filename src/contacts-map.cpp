#include "contacts-map.h"
#include "qindividual.h"

#include <QtCore/QDebug>

namespace galera
{

//ContactInfo
ContactEntry::ContactEntry(FolksIndividual *individual)
    : m_individual(new QIndividual(individual))
{
}

ContactEntry::ContactEntry(const ContactEntry &other)
{
    delete m_individual;
}

ContactEntry::~ContactEntry()
{
    g_object_unref(m_individual);
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
