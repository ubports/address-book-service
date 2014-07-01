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

#include "qcontactfetchrequest-data.h"

#include <QtCore/QDebug>
#include <QtContacts/QContactManagerEngine>

using namespace QtContacts;
namespace galera
{

QContactFetchRequestData::QContactFetchRequestData(QContactAbstractRequest *request,
                                                   QDBusInterface *view,
                                                   const FetchHint &hint)
    : QContactRequestData(request),
      m_runningParser(0),
      m_view(0),
      m_offset(0),
      m_hint(hint)
{
    if (view) {
        updateView(view);
    }
}

QContactFetchRequestData::~QContactFetchRequestData()
{
    delete m_runningParser;
}

int QContactFetchRequestData::offset() const
{
    return m_offset;
}

QDBusInterface* QContactFetchRequestData::view() const
{
    return m_view.data();
}

void QContactFetchRequestData::setVCardParser(QObject *parser)
{
    m_runningParser = parser;
}

void QContactFetchRequestData::clearVCardParser()
{
    m_runningParser = 0;
}

void QContactFetchRequestData::updateView(QDBusInterface* view)
{
    m_view = QSharedPointer<QDBusInterface>(view, QContactFetchRequestData::deleteView);
}

QStringList QContactFetchRequestData::fields() const
{
    return m_hint.fields();
}

void QContactFetchRequestData::updateOffset(int offset)
{
    m_offset += offset;
}

QList<QContact> QContactFetchRequestData::result() const
{
    return m_result.values();
}

void QContactFetchRequestData::update(QContactAbstractRequest::State state,
                                      QContactManager::Error error,
                                      QMap<int, QContactManager::Error> errorMap)
{
    if (error != QtContacts::QContactManager::NoError) {
        m_result.clear();
    }
    QContactRequestData::update(state, error, errorMap);
}

void QContactFetchRequestData::update(QList<QContact> result,
                                      QContactAbstractRequest::State state,
                                      QContactManager::Error error,
                                      QMap<int, QContactManager::Error> errorMap)
{
    m_result.unite(QSet<QContact>::fromList(result));
    QContactRequestData::update(state, error, errorMap);
}

void QContactFetchRequestData::notifyError(QContactFetchRequest *request, QContactManager::Error error)
{
    QContactManagerEngine::updateContactFetchRequest(request,
                                                     QList<QContact>(),
                                                     error,
                                                     QContactAbstractRequest::FinishedState);
}

void QContactFetchRequestData::updateRequest(QContactAbstractRequest::State state, QContactManager::Error error, QMap<int, QContactManager::Error> errorMap)
{
    QContactManagerEngine::updateContactFetchRequest(static_cast<QContactFetchRequest*>(m_request.data()),
                                                     m_result.values(),
                                                     error,
                                                     state);
}

void QContactFetchRequestData::deleteView(QDBusInterface *view)
{
    if (view) {
        view->call("close");
        view->deleteLater();
    }
}

} //namespace
