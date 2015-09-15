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

#include "ab-update-module.h"

#include <QtCore/QObject>
#include <QtCore/QMutex>
#include <QtCore/QScopedPointer>
#include <QtNetwork/QNetworkConfigurationManager>


class ABUpdate : public QObject
{
    Q_OBJECT
public:
    ABUpdate(QObject *parent = 0);
    ~ABUpdate() override;

    QList<ABUpdateModule*> needsUpdate() const;
    bool isRunning();

    // test
    void skipNetworkTest();
    void setSilenceMode(bool flag);

public Q_SLOTS:
    void startUpdate();
    void cancelUpdate();

Q_SIGNALS:
    void onNeedsUpdateChanged();
    void onIsRunningChanged();
    void updateDone();
    void updateError(const QString &accountName, ABUpdateModule::ImportError);

private Q_SLOTS:
    void onModuleUpdated();
    void onModuleUpdateError(const QString &accountName, ABUpdateModule::ImportError);
    void onOnlineStateChanged(bool isOnline);
    void continueUpdateWithInternet();
    void updateNextModule();
    void onModuleUpdateRetry();
    void onModuleUpdateNoRetry();

private:
    QScopedPointer<QNetworkConfigurationManager> m_netManager;
    QList<ABUpdateModule*> m_updateModules;
    QList<ABUpdateModule*> m_modulesToUpdate;
    QMutex m_lock;
    bool m_needsUpdate;
    bool m_isRunning;
    bool m_waitingForIntenert;
    int m_activeModule;
    bool m_skipNetworkTest;
    bool m_silenceMode;

    void notifyStart();
    void notifyNoInternet();
    void notifyDone();

    void startUpdate(ABUpdateModule *module);
    bool isOnline() const;
    void waitForInternet();

};
