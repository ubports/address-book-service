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

#include "qcontactcollectionfetchrequest-data.h"

#include <QtCore/QDebug>
#include <QtContacts/QContactManagerEngine>

using namespace QtContacts;
namespace galera
{

QContactCollectionFetchRequestData::QContactCollectionFetchRequestData(QContactAbstractRequest *request)
    : QContactRequestData(request)
{
}

QList<QContactCollection> QContactCollectionFetchRequestData::result() const
{
    return m_result;
}

void QContactCollectionFetchRequestData::update(QList<QContactCollection> result,
                                                QContactAbstractRequest::State state,
                                                QContactManager::Error error)
{
    m_result = result;
    QContactRequestData::update(state, error);
}

void QContactCollectionFetchRequestData::notifyError(QContactCollectionFetchRequest *request, QContactManager::Error error)
{
    QContactManagerEngine::updateCollectionFetchRequest(request,
                                                        QList<QContactCollection>(),
                                                        error,
                                                        QContactAbstractRequest::FinishedState);
}

void QContactCollectionFetchRequestData::updateRequest(QContactAbstractRequest::State state, QContactManager::Error error, QMap<int, QContactManager::Error> errorMap)
{
    QContactManagerEngine::updateCollectionFetchRequest(static_cast<QContactCollectionFetchRequest*>(m_request.data()),
                                                        m_result,
                                                        error,
                                                        state);
}

} //namespace
