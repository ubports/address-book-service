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

#ifndef __GALERA_QCONTACTFETCHREQUEST_DATA_H__
#define __GALERA_QCONTACTFETCHREQUEST_DATA_H__

#include "qcontactrequest-data.h"
#include <common/fetch-hint.h>

#include <QtCore/QList>
#include <QtCore/QSet>
#include <QtCore/QSharedPointer>

#include <QtContacts/QContactAbstractRequest>
#include <QtContacts/QContactFetchRequest>

#include <QtDBus/QDBusInterface>

namespace galera
{
class QContactFetchRequestData : public QContactRequestData
{
public:
    QContactFetchRequestData(QtContacts::QContactAbstractRequest *request,
                             QDBusInterface *view,
                             const FetchHint &hint = FetchHint());
    ~QContactFetchRequestData();

    QStringList fields() const;

    void updateOffset(int offset);
    int offset() const;

    void updateView(QDBusInterface *view);
    QDBusInterface* view() const;

    void setVCardParser(QObject *parser);
    void clearVCardParser();

    QList<QtContacts::QContact> result() const;

    void update(QList<QtContacts::QContact> result,
                QtContacts::QContactAbstractRequest::State state,
                QtContacts::QContactManager::Error error = QtContacts::QContactManager::NoError,
                QMap<int, QtContacts::QContactManager::Error> errorMap = QMap<int, QtContacts::QContactManager::Error>());

    static void notifyError(QtContacts::QContactFetchRequest *request,
                            QtContacts::QContactManager::Error error = QtContacts::QContactManager::NotSupportedError);

protected:
    QSet<QtContacts::QContact> m_result;

    virtual void updateRequest(QtContacts::QContactAbstractRequest::State state,
                               QtContacts::QContactManager::Error error,
                               QMap<int, QtContacts::QContactManager::Error> errorMap);

    virtual void update(QtContacts::QContactAbstractRequest::State state,
                QtContacts::QContactManager::Error error = QtContacts::QContactManager::NoError,
                QMap<int, QtContacts::QContactManager::Error> errorMap = QMap<int, QtContacts::QContactManager::Error>());

private:
    QObject *m_runningParser;
    QSharedPointer<QDBusInterface> m_view;
    int m_offset;
    FetchHint m_hint;

    static void deleteView(QDBusInterface *view);
};

}

#endif
