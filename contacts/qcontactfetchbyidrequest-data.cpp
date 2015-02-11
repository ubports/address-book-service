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

#include "qcontactfetchbyidrequest-data.h"

#include <QtCore/QDebug>

#include <QtContacts/QContactManagerEngine>

using namespace QtContacts;
namespace galera
{

QContactFetchByIdRequestData::QContactFetchByIdRequestData(QContactFetchByIdRequest *request,
                                                           QDBusInterface *view)
    : QContactFetchRequestData(request, view)
{
}

void QContactFetchByIdRequestData::notifyError(QContactFetchByIdRequest *request,
                                               QContactManager::Error error)
{
    QContactManagerEngine::updateContactFetchByIdRequest(request,
                                                         QList<QContact>(),
                                                         error,
                                                         QMap<int, QContactManager::Error>(),
                                                         QContactAbstractRequest::FinishedState);
}

void QContactFetchByIdRequestData::updateRequest(QContactAbstractRequest::State state, QContactManager::Error error, QMap<int, QContactManager::Error> errorMap)
{
    QList<QtContacts::QContact> result;
    // send all results only in the finished state, this will avoid a contact update in every updateContactFetchRequest
    switch(state)
    {
    case QContactAbstractRequest::FinishedState:
        result = m_allResults;
        break;
    default:
        result = m_result;
    }

    QContactManagerEngine::updateContactFetchByIdRequest(static_cast<QContactFetchByIdRequest*>(m_request.data()),
                                                         result,
                                                         error,
                                                         errorMap,
                                                         state);
}


} //namespace
