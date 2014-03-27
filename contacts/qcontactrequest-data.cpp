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

#include "qcontactrequest-data.h"

#include <QtCore/QDebug>
#include <QtContacts/QContactManagerEngine>

using namespace QtContacts;
namespace galera
{

QContactRequestData::QContactRequestData(QContactAbstractRequest *request,
                                         QDBusPendingCallWatcher *watcher)
    : m_request(request),
      m_canceled(false),
      m_eventLoop(0)
{
    updateWatcher(watcher);
}

QContactRequestData::~QContactRequestData()
{
    if (!m_request.isNull() && m_canceled) {
        update(QContactAbstractRequest::CanceledState);
    }
    m_request.clear();
}

QContactAbstractRequest* QContactRequestData::request() const
{
    return m_request.data();
}

bool QContactRequestData::isLive() const
{
    return !m_request.isNull() &&
           (m_request->state() == QContactAbstractRequest::ActiveState);
}

void QContactRequestData::cancel()
{
    m_watcher.clear();
    m_canceled = true;
}

bool QContactRequestData::canceled() const
{
    return m_canceled;
}

void QContactRequestData::wait()
{
    if (m_eventLoop) {
        qWarning() << "Recursive wait call";
        Q_ASSERT(false);
    }

    if (isLive()) {
        QEventLoop eventLoop;
        m_eventLoop = &eventLoop;
        eventLoop.exec();
        m_eventLoop = 0;
    }
}

void QContactRequestData::finish(QContactManager::Error error)
{
    update(QContactAbstractRequest::FinishedState, error, m_errorMap);
}

void QContactRequestData::updateWatcher(QDBusPendingCallWatcher *watcher)
{
    m_watcher.clear();
    if (watcher) {
        m_watcher = QSharedPointer<QDBusPendingCallWatcher>(watcher, QContactRequestData::deleteWatcher);
    }
}

void QContactRequestData::update(QContactAbstractRequest::State state,
                                 QContactManager::Error error,
                                 QMap<int, QContactManager::Error> errorMap)
{
    if (!isLive()) {
        return;
    }

    updateRequest(state, error, errorMap);

    if (m_eventLoop && (state != QContactAbstractRequest::ActiveState)) {
        m_eventLoop->quit();
    }
}

void QContactRequestData::deleteWatcher(QDBusPendingCallWatcher *watcher)
{
    if (watcher) {
        watcher->deleteLater();
    }
}

} //namespace
