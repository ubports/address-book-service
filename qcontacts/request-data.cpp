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

#include "request-data.h"

using namespace QtContacts;
namespace galera
{

RequestData::RequestData(QContactAbstractRequest *request,
                         QDBusInterface *view,
                         QDBusPendingCallWatcher *watcher)
    : m_offset(0)
{
    m_request = QSharedPointer<QContactAbstractRequest>(request, RequestData::deleteRequest);

    if (view) {
        m_view = QSharedPointer<QDBusInterface>(view, RequestData::deleteView);
    }

    if (watcher) {
        m_watcher = QSharedPointer<QDBusPendingCallWatcher>(watcher, RequestData::deleteWatcher);
    }
}

RequestData::~RequestData()
{
}

QContactAbstractRequest* RequestData::request() const
{
    return m_request.data();
}

int RequestData::offset() const
{
    return m_offset;
}

QDBusInterface* RequestData::view() const
{
    return m_view.data();
}

void RequestData::updateWatcher(QDBusPendingCallWatcher *watcher)
{
    m_watcher.clear();
    if (watcher) {
        m_watcher = QSharedPointer<QDBusPendingCallWatcher>(watcher, RequestData::deleteWatcher);
    }
}

void RequestData::updateOffset(int offset)
{
    m_offset += offset;
}

void RequestData::setResults(QList<QContact> result)
{
    m_result = result;
}

QList<QContact> RequestData::result() const
{
    return m_result;
}

void RequestData::appendResult(QList<QContact> result)
{
    m_result += result;
}

void RequestData::registerMetaType()
{
    qRegisterMetaType<galera::RequestData*>();
}

void RequestData::deleteRequest(QContactAbstractRequest *obj)
{
    //nothing
}

void RequestData::deleteView(QDBusInterface *view)
{
    if (view) {
        view->call("close");
        view->deleteLater();
    }
}

void RequestData::deleteWatcher(QDBusPendingCallWatcher *watcher)
{
    if (watcher) {
        watcher->deleteLater();
    }
}

} //namespace
