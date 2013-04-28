#include "addressbook-adaptor.h"
#include "addressbook.h"
#include "view.h"

namespace galera
{

AddressBookAdaptor::AddressBookAdaptor(const QDBusConnection &connection, AddressBook *parent)
    : QDBusAbstractAdaptor(parent),
      m_addressBook(parent),
      m_connection(connection)
{
    setAutoRelaySignals(true);
}

AddressBookAdaptor::~AddressBookAdaptor()
{
    // destructor
}

SourceList AddressBookAdaptor::availableSources()
{
    return m_addressBook->availableSources();
}

QString AddressBookAdaptor::createContact(const QString &contact, const QString &source)
{
    return m_addressBook->createContact(contact, source);
}

QDBusObjectPath AddressBookAdaptor::query(const QString &clause, const QString &sort, const QStringList &sources)
{
    View *v = m_addressBook->query(clause, sort, sources);
    v->registerObject(m_connection);
    return QDBusObjectPath(v->dynamicObjectPath());
}

bool AddressBookAdaptor::removeContacts(const QStringList &contactIds)
{
    return m_addressBook->removeContacts(contactIds);
}

QStringList AddressBookAdaptor::sortFields()
{
    return m_addressBook->sortFields();
}

QString AddressBookAdaptor::linkContacts(const QStringList &contactsIds)
{
    return m_addressBook->linkContacts(contactsIds);
}

bool AddressBookAdaptor::unlinkContacts(const QString &parentId, const QStringList &contactsIds)
{
    return m_addressBook->unlinkContacts(parentId, contactsIds);
}

bool AddressBookAdaptor::updateContact(const QString &contact)
{
    return m_addressBook->updateContact(contact);

}

} //namespace
