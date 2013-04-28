#include "view-adaptor.h"
#include "view.h"

namespace galera
{

ViewAdaptor::ViewAdaptor(const QDBusConnection &connection, View *parent)
    : QDBusAbstractAdaptor(parent),      
      m_view(parent),
      m_connection(connection)
{
    setAutoRelaySignals(true);
}

ViewAdaptor::~ViewAdaptor()
{
}

QString ViewAdaptor::contactDetails(const QStringList &fields, const QString &id)
{
    return m_view->contactDetails(fields, id);
}

QStringList ViewAdaptor::contactsDetails(const QStringList &fields, int startIndex, int pageSize)
{
    return m_view->contactsDetails(fields, startIndex, pageSize);
}

int ViewAdaptor::count()
{
    return m_view->count();
}

void ViewAdaptor::sort(const QString &field)
{
    return m_view->sort(field);
}

} //namespace
