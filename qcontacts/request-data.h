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

#include <QtCore/QList>
#include <QtCore/QSharedPointer>

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
                QDBusPendingCallWatcher *watcher=0);

    ~RequestData();

    QtContacts::QContactAbstractRequest* request() const;
    QDBusInterface* view() const;

    void updateWatcher(QDBusPendingCallWatcher *watcher);

    void updateOffset(int offset);
    int offset() const;

    void setResults(QList<QtContacts::QContact> result);
    void appendResult(QList<QtContacts::QContact> result);
    QList<QtContacts::QContact> result() const;

    static void registerMetaType();

private:
    QSharedPointer<QtContacts::QContactAbstractRequest> m_request;
    QSharedPointer<QDBusInterface> m_view;
    QSharedPointer<QDBusPendingCallWatcher> m_watcher;
    QList<QtContacts::QContact> m_result;
    int m_offset;

    static void deleteRequest(QtContacts::QContactAbstractRequest *obj);
    static void deleteView(QDBusInterface *view);
    static void deleteWatcher(QDBusPendingCallWatcher *watcher);
};

}

Q_DECLARE_METATYPE(galera::RequestData*)

#endif
