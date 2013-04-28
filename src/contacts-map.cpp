#include "contacts-map.h"

namespace galera
{

//ContactInfo
ContactEntry::ContactEntry(FolksIndividual *individual)
    : m_individual(individual)
{
    g_object_ref(m_individual);
}

ContactEntry::ContactEntry(const ContactEntry &other)
{
    g_object_ref(other.m_individual);
    m_individual = other.m_individual;
}

ContactEntry::~ContactEntry()
{
    g_object_unref(m_individual);
}

FolksIndividual *ContactEntry::individual() const
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

} //namespace
