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

#ifndef __GALERA_QCONTACTREQUEST_DATA_H__
#define __GALERA_QCONTACTREQUEST_DATA_H__

#include <common/fetch-hint.h>

#include <QtCore/QList>
#include <QtCore/QSharedPointer>
#include <QtCore/QEventLoop>
#include <QtCore/QPointer>

#include <QtContacts/QContactAbstractRequest>

#include <QtDBus/QDBusInterface>
#include <QtDBus/QDBusPendingCallWatcher>

namespace galera
{
class QContactRequestData
{
public:
    QContactRequestData(QtContacts::QContactAbstractRequest *request, QDBusPendingCallWatcher *watcher = 0);
    virtual ~QContactRequestData();

    QtContacts::QContactAbstractRequest* request() const;

    void updateWatcher(QDBusPendingCallWatcher *watcher);

    bool isLive() const;
    void cancel();
    bool canceled() const;
    void wait();
    void finish(QtContacts::QContactManager::Error error = QtContacts::QContactManager::NoError);

    virtual void update(QtContacts::QContactAbstractRequest::State state,
                QtContacts::QContactManager::Error error = QtContacts::QContactManager::NoError,
                QMap<int, QtContacts::QContactManager::Error> errorMap = QMap<int, QtContacts::QContactManager::Error>());

protected:
    QPointer<QtContacts::QContactAbstractRequest> m_request;
    QMap<int, QtContacts::QContactManager::Error> m_errorMap;

    virtual void updateRequest(QtContacts::QContactAbstractRequest::State state,
                               QtContacts::QContactManager::Error error,
                               QMap<int, QtContacts::QContactManager::Error> errorMap) = 0;

private:
    QSharedPointer<QDBusPendingCallWatcher> m_watcher;
    bool m_canceled;
    QEventLoop *m_eventLoop;

    void init(QtContacts::QContactAbstractRequest *request,
              QDBusInterface *view,
              QDBusPendingCallWatcher *watcher);

    static void deleteRequest(QtContacts::QContactAbstractRequest *obj);
    static void deleteWatcher(QDBusPendingCallWatcher *watcher);
};

}

Q_DECLARE_METATYPE(galera::QContactRequestData*)

#endif
