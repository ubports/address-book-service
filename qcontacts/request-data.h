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

#ifndef __GALERA_REQUEST_DATA_H__
#define __GALERA_REQUEST_DATA_H__

#include <common/fetch-hint.h>

#include <QtCore/QList>
#include <QtCore/QSharedPointer>
#include <QtCore/QPointer>
#include <QtCore/QEventLoop>

#include <QtContacts/QContactAbstractRequest>

#include <QtDBus/QDBusInterface>
#include <QtDBus/QDBusPendingCallWatcher>

namespace galera
{
class RequestData
{
public:
    RequestData(QtContacts::QContactAbstractRequest *request,
                QDBusInterface *view,
                const FetchHint &hint,
                QDBusPendingCallWatcher *watcher=0);

    RequestData(QtContacts::QContactAbstractRequest *request,
                QDBusPendingCallWatcher *watcher=0);


    ~RequestData();

    QtContacts::QContactAbstractRequest* request() const;
    QDBusInterface* view() const;
    QStringList fields() const;


    void updateWatcher(QDBusPendingCallWatcher *watcher);

    void updateOffset(int offset);
    int offset() const;
    bool isLive() const;
    void cancel();
    bool canceled() const;
    void wait();

    QList<QtContacts::QContact> result() const;

    void setError(QtContacts::QContactManager::Error error);
    void update(QList<QtContacts::QContact> result,
                QtContacts::QContactAbstractRequest::State state,
                QtContacts::QContactManager::Error error = QtContacts::QContactManager::NoError,
                QMap<int, QtContacts::QContactManager::Error> errorMap = QMap<int, QtContacts::QContactManager::Error>());
    void update(QtContacts::QContactAbstractRequest::State state,
                QtContacts::QContactManager::Error error = QtContacts::QContactManager::NoError,
                QMap<int, QtContacts::QContactManager::Error> errorMap = QMap<int, QtContacts::QContactManager::Error>());

    static void setError(QtContacts::QContactAbstractRequest *request,
                         QtContacts::QContactManager::Error error = QtContacts::QContactManager::UnspecifiedError);
    static void registerMetaType();

private:
    QPointer<QtContacts::QContactAbstractRequest> m_request;
    QSharedPointer<QDBusInterface> m_view;
    QSharedPointer<QDBusPendingCallWatcher> m_watcher;
    QList<QtContacts::QContact> m_result;
    int m_offset;
    FetchHint m_hint;
    bool m_canceled;
    QEventLoop *m_eventLoop;

    void init(QtContacts::QContactAbstractRequest *request, QDBusInterface *view, QDBusPendingCallWatcher *watcher);
    static void deleteRequest(QtContacts::QContactAbstractRequest *obj);
    static void deleteView(QDBusInterface *view);
    static void deleteWatcher(QDBusPendingCallWatcher *watcher);
};

}

Q_DECLARE_METATYPE(galera::RequestData*)

#endif
