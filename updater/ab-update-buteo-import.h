/*
 * Copyright (C) 2015 Canonical, Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 3.
 *
 * This program is distributed in the hope that it will be useful,
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

#include "ab-update-module.h"

class ButeoImport : public ABUpdateModule
{
    Q_OBJECT
public:
    ButeoImport(QObject *parent = 0);
    ~ButeoImport();

    QString name() const override;
    bool needsUpdate() override;
    bool update() override;
    bool requireInternetConnection() override;
    bool canUpdate() override;
    bool commit() override;
    bool roolback() override;
    ImportError lastError() const override;

Q_SIGNALS:
    void updateError(const QString &accountName, ButeoImport::ImportError errorCode);

private Q_SLOTS:
    void onProfileChanged(const QString &profileName, int changeType, const QString &profileAsXml);
    void onSyncStatusChanged(const QString &aProfileName, int aStatus, const QString &aMessage, int aMoreDetails);

private:
    QScopedPointer<QDBusInterface> m_buteoInterface;
    QMap<quint32, QString> m_accountToProfiles;
    QStringList m_failToSyncProfiles;
    QMutex m_importLock;
    ImportError m_lastError;

    QMap<QString, quint32> sources() const;
    QMap<quint32, QString> createProfileForAccounts(QList<quint32> ids);
    bool prepareButeo();
    bool createAccounts(QList<quint32> ids);
    bool removeProfile(const QString &profileId);
    bool removeSources(const QStringList &sources);
    bool restoreSession(const QStringList &activeSyncs);
    void error(const QString &accountName, ImportError errorCode);
    bool loadAccounts(QList<quint32> &accountsToUpdate, QList<quint32> &newAccounts);
    bool enableContactsService(quint32 accountId);
    QString accountName(quint32 accountId);
    QStringList runningSyncs() const;
    QString profileName(const QString &xml) const;
    QString profileName(quint32 accountId) const;
    bool startSync(const QString &profile) const;
    bool matchFavorites();
};
