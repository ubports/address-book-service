#include "view.h"
#include "view-adaptor.h"
#include "contacts-map.h"
#include "qindividual.h"

namespace galera
{

View::View(QString clause, QString sort, QStringList sources, ContactsMap *allContacts, QObject *parent)
    : QObject(parent),
      m_clause(clause),
      m_sort(sort),
      m_sources(sources),
      m_adaptor(0),
      m_allContacts(allContacts)
{
    //TODO: run this async
    applyFilter();
}

View::~View()
{
    close();
}

void View::close()
{
    Q_EMIT m_adaptor->contactsRemoved(0, m_contacts.count());
    Q_EMIT closed();
    m_contacts.clear();
}

QString View::contactDetails(const QStringList &fields, const QString &id)
{
    return QString();
}

QStringList View::contactsDetails(const QStringList &fields, int startIndex, int pageSize)
{
    if (startIndex < 0) {
        startIndex = 0;
    }

    if ((pageSize < 0) || (pageSize >= m_contacts.count())) {
        pageSize = m_contacts.count() -1;
    }

    QStringList result;
    for(int i=startIndex; i < pageSize; i++) {
        result << QIndividual(m_contacts[i]->individual()).toString();
    }

    return result;
}

int View::count()
{
    return m_contacts.count();
}

void View::sort(const QString &field)
{
}

QString View::objectPath()
{
    return "/com/canonical/galera/View";
}

QString View::dynamicObjectPath() const
{
    return objectPath() + "/" + QString::number((long)this);
}

bool View::registerObject(QDBusConnection &connection)
{
    if (!m_adaptor) {
        m_adaptor = new ViewAdaptor(connection, this);
        if (!connection.registerObject(dynamicObjectPath(), this))
        {
            qWarning() << "Could not register object!" << objectPath();
            delete m_adaptor;
            m_adaptor = 0;
        } else {
            qDebug() << "Object registered:" << objectPath();
        }
    }

    return (m_adaptor != 0);
}

QObject *View::adaptor() const
{
    return m_adaptor;
}

bool View::checkContact(ContactEntry *entry)
{
    //TODO: check query filter
    return true;
}

bool View::appendContact(ContactEntry *entry)
{
    if (checkContact(entry)) {
        m_contacts << entry;
        return true;
    }
    return false;
}

void View::applyFilter()
{
    Q_FOREACH(ContactEntry *entry, m_allContacts->values())
    {
        appendContact(entry);
    }
}

} //namespace
