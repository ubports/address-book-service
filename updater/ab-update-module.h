/*
 * Copyright 2015 Canonical Ltd.
 *
 * This file is part of address-book-service.
 *
 * sync-monitor is free software; you can redistribute it and/or modify
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

#pragma once
#include <QtCore/QObject>

class ABUpdateModule : public QObject
{
    Q_OBJECT
    Q_ENUMS(ImportError)

public:
    enum ImportError {
        ApplicationAreadyUpdated = 0,
        ConnectionError,
        FailToConnectWithButeo,
        FailToCreateButeoProfiles,
        FailToAuthenticate,
        InernalError,
        InitialSyncError,
        OnlineAccountNotFound,
        SyncAlreadyRunning,
        SyncError,
        NoError
    };

    ABUpdateModule(QObject *parent = 0);
    ~ABUpdateModule() override;

    virtual QString name() const = 0;
    virtual bool needsUpdate() = 0;
    virtual bool prepareToUpdate() = 0;
    virtual bool update() = 0;
    virtual bool canUpdate() = 0;
    virtual bool requireInternetConnection() = 0;
    virtual bool commit() = 0;
    virtual bool rollback() = 0;
    virtual bool markAsUpdate() = 0;
    virtual ImportError lastError() const = 0;

Q_SIGNALS:
    void updated();
    void updateError(const QString &accountName,  ABUpdateModule::ImportError errorCode);
};
