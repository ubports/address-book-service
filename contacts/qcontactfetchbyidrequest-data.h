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

#ifndef __GALERA_QCONTACTFETCHREQUESTBYID_DATA_H__
#define __GALERA_QCONTACTFETCHREQUESTBYID_DATA_H__

#include "qcontactfetchrequest-data.h"

#include <common/fetch-hint.h>
#include <QtContacts/QContactFetchByIdRequest>

namespace galera
{
class QContactFetchByIdRequestData : public QContactFetchRequestData
{
public:
    QContactFetchByIdRequestData(QtContacts::QContactFetchByIdRequest *request,
                                 QDBusInterface *view);

    static void notifyError(QtContacts::QContactFetchByIdRequest *request,
                            QtContacts::QContactManager::Error error = QtContacts::QContactManager::NotSupportedError);

protected:
    virtual void updateRequest(QtContacts::QContactAbstractRequest::State state,
                               QtContacts::QContactManager::Error error,
                               QMap<int, QtContacts::QContactManager::Error> errorMap);
};

}

#endif
