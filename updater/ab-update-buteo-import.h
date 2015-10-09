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

#include <QtCore/QObject>
#include <QtCore/QMap>
#include <QtCore/QMutex>
#include <QtCore/QEventLoop>
#include <QtDBus/QDBusInterface>

#include <Accounts/Account>

#include "ab-update-module.h"

class AccountInfo
{
public:
    AccountInfo(quint32 _accountId, const QString &_accountName, bool _syncEnabled, const QString &_oldSourceId, const QString &_newSourceId, bool _emptySource);
    AccountInfo(const AccountInfo &other);
    void enableSync(const QString &syncService, bool enable=true);

    quint32 accountId;
    QString accountName;
    bool syncEnabled;
    QString oldSourceId;
    QString newSourceId;
    QString syncProfile;
    bool emptySource;
    bool removeAfterUpdate;
};

class ButeoImport : public ABUpdateModule
{
    Q_OBJECT
public:
    ButeoImport(QObject *parent = 0);
    ~ButeoImport();

    QString name() const override;
    bool needsUpdate() override;
    bool prepareToUpdate() override;
    bool update() override;
    bool requireInternetConnection() override;
    bool canUpdate() override;
    bool commit() override;
    bool rollback() override;
    bool markAsUpdate() override;
    ImportError lastError() const override;

private Q_SLOTS:
    void onProfileChanged(const QString &profileName, int changeType, const QString &profileAsXml);
    void onSyncStatusChanged(const QString &aProfileName, int aStatus, const QString &aMessage, int aMoreDetails);
    void onError(const QString &accountName, int errorCode, bool unlock);
    void onEnableAccountsReplied(const QString &reply);
    void onOldContactsSyncFinished(const QString &accountName, const QString &serviceName);
    void onOldContactsSyncError(const QString &accountName, const QString &serviceName, const QString &error);

private:
    QList<AccountInfo> m_accounts;
    QList<int> m_disabledAccounts;
    QList<int> m_syncEvolutionQueue;
    QMap<int, QString> m_buteoQueue;
    QStringList m_failToSyncProfiles;

    QScopedPointer<QDBusInterface> m_buteoInterface;
    QScopedPointer<QDBusInterface> m_syncMonitorInterface;

    QMutex m_importLock;
    ImportError m_lastError;

    QString createProfileForAccount(quint32 id);
    bool prepareButeo();
    bool createAccounts(QList<quint32> ids);
    bool removeProfile(const QString &profileId);
    bool buteoRemoveProfile(const QString &profileId) const;
    bool removeSources(const QStringList &sources);
    bool restoreService();

    QStringList runningSyncs() const;
    QString profileName(const QString &xml) const;
    QString profileName(quint32 accountId) const;
    bool startSync(const QString &profile) const;
    bool matchFavorites();
    bool checkOldAccounts();
    bool syncOldContacts();
    void syncOldContactsContinue();
    bool continueUpdate();
    ImportError parseError(int errorCode) const;
    void askAboutDisabledAccounts();

    static void sourceInfo(Accounts::Account *account, QString &oldSourceId, QString &newSourceId, bool &isEmpty);
};
