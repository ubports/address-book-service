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

#include "qcontactremoverequest-data.h"

#include <QtCore/QDebug>

#include <QtContacts/QContactManagerEngine>

using namespace QtContacts;

namespace galera
{

QContactRemoveRequestData::QContactRemoveRequestData(QContactRemoveRequest *request)
    : QContactRequestData(request)
{
    Q_FOREACH(QContactId contactId, request->contactIds()) {
        m_pendingIds << contactId.toString().split(":").last();
    }
}

QStringList QContactRemoveRequestData::pendingIds() const
{
    return m_pendingIds;
}

void QContactRemoveRequestData::notifyError(QContactRemoveRequest *request, QContactManager::Error error)
{
    QContactManagerEngine::updateContactRemoveRequest(request,
                                                      error,
                                                      QMap<int, QContactManager::Error>(),
                                                      QContactAbstractRequest::FinishedState);
}

void QContactRemoveRequestData::updateRequest(QContactAbstractRequest::State state,
                                              QContactManager::Error error,
                                              QMap<int, QContactManager::Error> errorMap)
{
    QContactManagerEngine::updateContactRemoveRequest(static_cast<QContactRemoveRequest*>(m_request.data()),
                                                      error,
                                                      errorMap,
                                                      state);
}

} //namespace
