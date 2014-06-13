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
    m_view = 0;
}

void ViewAdaptor::destroy()
{
    m_view = 0;
    this->deleteLater();
}

QString ViewAdaptor::contactDetails(const QStringList &fields, const QString &id)
{
    if (m_view) {
        return m_view->contactDetails(fields, id);
    } else {
        return QString();
    }
}

QStringList ViewAdaptor::contactsDetails(const QStringList &fields, int startIndex, int pageSize, const QDBusMessage &message)
{
    if (m_view) {
        message.setDelayedReply(true);
        m_view->contactsDetails(fields, startIndex, pageSize, message);
    }
    return QStringList();
}

int ViewAdaptor::count()
{
    if (m_view) {
        return m_view->count();
    } else {
        return 0;
    }
}

void ViewAdaptor::sort(const QString &field)
{
    if (m_view) {
        return m_view->sort(field);
    }
}

void ViewAdaptor::close()
{
    if (m_view) {
        return m_view->close();
    }
}

} //namespace
