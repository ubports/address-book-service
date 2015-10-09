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

#include "ab-update-buteo-import.h"
#include "ab-notify-message.h"
#include "ab-i18n.h"

#include <Accounts/Manager>
#include <Accounts/Account>
#include <Accounts/AccountService>

#include <QtCore/QDebug>
#include <QtCore/QScopedPointer>
#include <QtCore/QTimer>

#include <QtDBus/QDBusReply>

#include <QtXml/QDomDocument>

#include <QtContacts/QContactManager>
#include <QtContacts/QContact>
#include <QtContacts/QContactGuid>
#include <QtContacts/QContactExtendedDetail>
#include <QtContacts/QContactDetailFilter>
#include <QtContacts/QContactIntersectionFilter>
#include <QtContacts/QContactFavorite>
#include <QtContacts/QContactName>
#include <QtContacts/QContactPhoneNumber>
#include <QtContacts/QContactEmailAddress>
#include <QtContacts/QContactDisplayLabel>
#include <QtContacts/QContactSyncTarget>

#include "config.h"

#define UOA_CONTACTS_SERVICE_TYPE   "contacts"
#define BUTEO_UOA_SERVICE_NAME      "google-contacts"
#define SYNCEVO_UOA_SERVICE_NAME    "google-carddav"

#define BUTEO_DBUS_SERVICE_NAME     "com.meego.msyncd"
#define BUTEO_DBUS_OBJECT_PATH      "/synchronizer"
#define BUTEO_DBUS_INTERFACE        "com.meego.msyncd"

#define SYNCMONITOR_DBUS_SERVICE_NAME    "com.canonical.SyncMonitor"
#define SYNCMONITOR_DBUS_OBJECT_PATH     "/com/canonical/SyncMonitor"
#define SYNCMONITOR_DBUS_INTERFACE       "com.canonical.SyncMonitor"

#define TRANSFER_ICON           "/usr/share/icons/suru/status/scalable/transfer-progress.svg"

using namespace QtContacts;


AccountInfo::AccountInfo(quint32 _accountId,
                         const QString &_accountName,
                         bool _syncEnabled,
                         const QString &_oldSourceId,
                         const QString &_newSourceId,
                         bool _emptySource)
    : accountId(_accountId),
      accountName(_accountName),
      syncEnabled(_syncEnabled),
      oldSourceId(_oldSourceId),
      newSourceId(_newSourceId),
      emptySource(_emptySource),
      removeAfterUpdate(true)
{
}

AccountInfo::AccountInfo(const AccountInfo &other)
    : accountId(other.accountId),
      accountName(other.accountName),
      syncEnabled(other.syncEnabled),
      oldSourceId(other.oldSourceId),
      newSourceId(other.newSourceId),
      emptySource(other.emptySource),
      removeAfterUpdate(other.removeAfterUpdate)
{
}

void AccountInfo::enableSync(const QString &syncService, bool enable)
{
    Accounts::Manager mgr;
    QScopedPointer<Accounts::Account> account(mgr.account(accountId));
    Accounts::Service service = mgr.service(syncService);
    if (!service.isValid()) {
        qWarning() << "Fail to enable" << syncService << "for account" << accountId << accountName;
    } else {
        account->selectService(service);
        if (account->enabled() != enable) {
            account->setEnabled(enable);
            account->syncAndBlock();
            syncEnabled = enable;
        }
    }
}

ButeoImport::ButeoImport(QObject *parent)
    : ABUpdateModule(parent)
{
}

ButeoImport::~ButeoImport()
{
}

QString ButeoImport::name() const
{
    return QStringLiteral("Buteo");
}

QStringList ButeoImport::runningSyncs() const
{
    if (m_buteoInterface.isNull()) {
        return QStringList();
    }

    QDBusReply<QStringList> result = m_buteoInterface->call("runningSyncs");
    if (result.error().isValid()) {
        qWarning() << "Fail to retrieve running syncs" << result.error();
        return QStringList();
    }

    return result.value();
}

QString ButeoImport::profileName(const QString &xml) const
{
    QDomDocument doc;
    QString errorMsg;
    int errorLine;
    int errorColumn;

    if (doc.setContent(xml, &errorMsg, &errorLine, &errorColumn)) {
       QDomNodeList profileElements = doc.elementsByTagName("profile");
       if (!profileElements.isEmpty()) {
             QDomElement e = profileElements.item(0).toElement();
             return e.attribute("name");
       } else {
           qWarning() << "Invalid profile" << xml;
       }
    } else {
        qWarning() << "Fail to parse profile:" << xml;
        qWarning() << "Error:" << errorMsg << errorLine << errorColumn;
    }
    return QString();
}

QString ButeoImport::profileName(quint32 accountId) const
{
    if (m_buteoInterface.isNull()) {
        return QString();
    }

    QDBusReply<QStringList> reply = m_buteoInterface->call("syncProfilesByKey",
                                                           "accountid",
                                                           QString::number(accountId));
    if (!reply.value().isEmpty()) {
        return this->profileName(reply.value().first());
    }
    return QString();
}

bool ButeoImport::startSync(const QString &profile) const
{
    if (m_buteoInterface.isNull()) {
        qWarning() << "Buteo interface is not valid";
        return false;
    }

    QDBusReply<bool> result = m_buteoInterface->call("startSync", profile);
    if (result.error().isValid()) {
        qWarning() << "Fail to start sync for profile" << profile << result.error().message();
        return false;
    }

    return result.value();
}

bool ButeoImport::matchFavorites()
{
    QMap<QString, QString> parameters;
    parameters.insert(ADDRESS_BOOK_SHOW_INVISIBLE_PROP, "true");
    QScopedPointer<QContactManager> manager(new QContactManager("galera", parameters));

    // load old favorites
    QContactDetailFilter folksFavorite;
    folksFavorite.setDetailType(QContactDetail::TypeFavorite, QContactFavorite::FieldFavorite);
    folksFavorite.setValue(true);

    QContactFetchHint hint;
    hint.setDetailTypesHint(QList<QContactDetail::DetailType>()
                            << QContactDetail::TypeName
                            << QContactDetail::TypeEmailAddress
                            << QContactDetail::TypePhoneNumber);
    QList<QContact> favorites = manager->contacts(folksFavorite, QList<QContactSortOrder>(), hint);
    qDebug() << "number of folks favorites" << favorites.size();

    // try to match with new contacts
    QList<QContact> toUpdate;

    Q_FOREACH(const QContact &f, favorites) {
        QContactIntersectionFilter iFilter;

        qDebug() << "Try to match contact" << f;
        // No favorite
        QContactDetailFilter noFavorite;
        noFavorite.setDetailType(QContactDetail::TypeFavorite, QContactFavorite::FieldFavorite);
        noFavorite.setValue(false);
        iFilter.append(noFavorite);

        // By name
        QContactDetailFilter firstNameFilter;
        firstNameFilter.setDetailType(QContactDetail::TypeName, QContactName::FieldFirstName);
        firstNameFilter.setValue(f.detail<QContactName>().firstName());
        iFilter.append(firstNameFilter);

        QContactDetailFilter lastNameFilter;
        lastNameFilter.setDetailType(QContactDetail::TypeName, QContactName::FieldLastName);
        lastNameFilter.setValue(f.detail<QContactName>().lastName());
        iFilter.append(lastNameFilter);

        // By Email
        Q_FOREACH(const QContactEmailAddress &e, f.details<QContactEmailAddress>()) {
            QContactDetailFilter emailFilter;
            emailFilter.setDetailType(QContactDetail::TypeEmailAddress, QContactEmailAddress::FieldEmailAddress);
            emailFilter.setValue(e.emailAddress());
            iFilter.append(emailFilter);
        }

        // By Phone
        Q_FOREACH(const QContactPhoneNumber &p, f.details<QContactPhoneNumber>()) {
            QContactDetailFilter phoneFilter;
            phoneFilter.setDetailType(QContactDetail::TypePhoneNumber, QContactPhoneNumber::FieldNumber);
            phoneFilter.setValue(p.number());
            iFilter.append(phoneFilter);
        }

        QList<QContact> contacts = manager->contacts(iFilter);
        qDebug() << "Number of contacts that match with old favorite" << contacts.size();

        Q_FOREACH(QContact c, contacts) {
            qDebug() << "Mark new contact as favorite" << c;
            QContactFavorite fav = c.detail<QContactFavorite>();
            fav.setFavorite(true);
            c.saveDetail(&fav);
            toUpdate << c;
        }
    }


    if (!toUpdate.isEmpty()) {
        if (!manager->saveContacts(&toUpdate)) {
            qWarning() << "Fail to save favorite contacts";
            return false;
        }
    }
    return true;

}

bool ButeoImport::checkOldAccounts()
{
    // check if the user has account with contacts and disabled sync
    m_disabledAccounts.clear();

    for(int i=0; i < m_accounts.size(); i++) {
        const AccountInfo &acc = m_accounts.at(i);
        if (!acc.syncEnabled && !acc.emptySource) {
            qDebug() << "Account need to be enabled:" << acc.accountId << acc.accountName;
            m_disabledAccounts << i;
            break;
        }
    }

    askAboutDisabledAccounts();
}

void ButeoImport::askAboutDisabledAccounts()
{
    if (m_disabledAccounts.isEmpty()) {
        syncOldContacts();
        return;
    }
    const AccountInfo &acc = m_accounts.at(m_disabledAccounts.first());

    QMap<QString, QString> updateOptions;
    updateOptions.insert("enable", _("Enable Sync"));
    updateOptions.insert("disable", _("Keep Disabled"));

    ABNotifyMessage *msg = new ABNotifyMessage(true, this);
    connect(msg, SIGNAL(questionReplied(QString)), this, SLOT(onEnableAccountsReplied(QString)));
    msg->askQuestion(_("Contact Sync Upgrade"),
                     TRANSFER_ICON,
                     QString(_("Google account %1 currently has contact sync disabled. We need to enable it to proceed with the contact sync upgrade.")).arg(acc.accountName),
                     updateOptions);
}

void ButeoImport::onEnableAccountsReplied(const QString &reply)
{
    AccountInfo &acc = m_accounts[m_disabledAccounts.takeFirst()];
    qDebug() << "Account" << acc.accountId << acc.accountName << reply;
    if (reply == "enable") {
        acc.enableSync(SYNCEVO_UOA_SERVICE_NAME);
    } else if (reply == "disable") {
        acc.removeAfterUpdate = false;
    }

    askAboutDisabledAccounts();
}

bool ButeoImport::syncOldContacts()
{
    //call syncevoltuion
    if (m_syncMonitorInterface.isNull()) {
        m_syncMonitorInterface.reset(new QDBusInterface(SYNCMONITOR_DBUS_SERVICE_NAME,
                                                        SYNCMONITOR_DBUS_OBJECT_PATH,
                                                        SYNCMONITOR_DBUS_INTERFACE));

        if (!m_buteoInterface->isValid()) {
            m_buteoInterface.reset();
            qWarning() << "Fail to connect with sync-monitor";
            return false;
        }

        connect(m_syncMonitorInterface.data(), SIGNAL(syncFinished(QString, QString)),
                SLOT(onOldContactsSyncFinished(QString,QString)), Qt::UniqueConnection);
        connect(m_syncMonitorInterface.data(), SIGNAL(syncError(QString, QString, QString)),
                SLOT(onOldContactsSyncError(QString,QString,QString)), Qt::UniqueConnection);
    }

    m_syncEvolutionQueue.clear();
    for(int i=0; i < m_accounts.size(); i++) {
        const AccountInfo &accInfo = m_accounts[i];
        // if the account is disabled or the new source was already created we do not need to sync
        if (accInfo.syncEnabled &&
            accInfo.newSourceId.isEmpty() &&
            !accInfo.oldSourceId.isEmpty()) {
            qDebug() << "SyncEvolution: Prepare to sync" << accInfo.accountId << accInfo.accountName;
            m_syncEvolutionQueue << i;
        } else {
            qDebug() << "SyncEvolution: Skip sync for disabled account" << accInfo.accountId << accInfo.accountName;
        }
    }

    syncOldContactsContinue();
}

void ButeoImport::syncOldContactsContinue()
{
    if (m_syncEvolutionQueue.isEmpty()) {
        continueUpdate();
        return;
    }

    const AccountInfo &accInfo = m_accounts[m_syncEvolutionQueue.takeFirst()];
    QDBusReply<void> result = m_syncMonitorInterface->call("syncAccount", accInfo.accountId, "contacts");
    if (result.error().isValid()) {
        qWarning() << "SyncEvolution: Fail to start account sync" << accInfo.accountId  << accInfo.accountName << result.error();
        onError("", ButeoImport::InitialSyncError, true);
    } else {
        qDebug() << "SyncEvolution: Syncing" << accInfo.accountId << accInfo.accountName;
    }
}

ABUpdateModule::ImportError ButeoImport::parseError(int errorCode) const
{
    switch (errorCode)
    {
    case 9: //SYNC_AUTHENTICATION_FAILURE:
        return ABUpdateModule::FailToAuthenticate;
    case 11: //SYNC_CONNECTION_ERROR:
        return ABUpdateModule::ConnectionError;
    case 14: //SYNC_PLUGIN_ERROR:
    case 15: //SYNC_PLUGIN_TIMEOUT:
        return ABUpdateModule::FailToConnectWithButeo;
    default:
        return ABUpdateModule::SyncError;
    }
}

void ButeoImport::sourceInfo(Accounts::Account *account,
                             QString &oldSourceId,
                             QString &newSourceId,
                             bool &isEmpty)
{
    QString accountName = account->displayName();

    QScopedPointer<QContactManager> manager(new QContactManager("galera"));
    QContactDetailFilter sourceFilter;
    sourceFilter.setDetailType(QContactDetail::TypeType, QContactType::FieldType);
    sourceFilter.setValue(QContactType::TypeGroup);

    Q_FOREACH(const QContact &c, manager->contacts(sourceFilter)) {
        if (c.detail<QContactDisplayLabel>().label() == accountName) {
            bool newSource = false;
            Q_FOREACH(const QContactExtendedDetail &xDet, c.details<QContactExtendedDetail>()) {
                if (xDet.name() == "ACCOUNT-ID") {
                    if (xDet.data().toUInt() == account->id()) {
                        newSource = true;
                        break;
                    }
                }
            }
            if (newSource) {
                newSourceId = c.id().toString();
            } else {
                oldSourceId = c.id().toString();
            }
        }
        if (!newSourceId.isEmpty() && !oldSourceId.isEmpty()) {
            // both sources found
            break;
        }
    }

    if (!oldSourceId.isEmpty()) {
        QMap<QString, QString> parameters;
        parameters.insert(ADDRESS_BOOK_SHOW_INVISIBLE_PROP, "true");
        QScopedPointer<QContactManager> manager(new QContactManager("galera", parameters));

        QContactDetailFilter sourceFilter;
        sourceFilter.setDetailType(QContactSyncTarget::Type,
                                   QContactSyncTarget::FieldSyncTarget + 1);
        sourceFilter.setValue(oldSourceId.replace("qtcontacts:galera::source@", ""));
        QContactFetchHint fetchHint;
        fetchHint.setMaxCountHint(1);

        QList<QContact> contacts = manager->contacts(sourceFilter, QList<QContactSortOrder>(), fetchHint);
        isEmpty = contacts.isEmpty();
    } else {
        isEmpty = true;
    }
}

bool ButeoImport::needsUpdate()
{
    // check settings
    QSettings settings;
    if (settings.value(SETTINGS_BUTEO_KEY, false).toBool()) {
        qDebug() << "Buteo has already been updated.";
        return false;
    }

    Accounts::Manager mgr;
    Accounts::AccountIdList accounts = mgr.accountList(UOA_CONTACTS_SERVICE_TYPE);
    if (accounts.isEmpty()) {
        // update settings key
        QSettings settings;
        settings.setValue(SETTINGS_BUTEO_KEY, true);
        settings.sync();
        return false;
    }
    return true;
}

bool ButeoImport::prepareToUpdate()
{
    // populate accounts map;
    Accounts::Manager mgr;
    Accounts::AccountIdList accounts = mgr.accountList(UOA_CONTACTS_SERVICE_TYPE);

    m_accounts.clear();
    qDebug() << "Loading account information";
    Q_FOREACH(Accounts::AccountId accountId, accounts) {
         QScopedPointer<Accounts::Account> acc(mgr.account(accountId));

         QString oldSourceId;
         QString newSourceId;
         bool isEmpty;

         sourceInfo(acc.data(), oldSourceId, newSourceId, isEmpty);

         Accounts::Service service = mgr.service(SYNCEVO_UOA_SERVICE_NAME);
         acc->selectService(service);

         AccountInfo accInfo(accountId, acc->displayName(), acc->isEnabled(), oldSourceId, newSourceId, isEmpty);
         accInfo.syncProfile = profileName(accountId);
         qDebug() << "\tAccount:" << accountId << acc->displayName()
                  << "\n\t\tEnabled" << acc->isEnabled()
                  << "\n\t\tOld source:" << oldSourceId << "isEmpty" << isEmpty
                  << "\n\t\tNew source:" << newSourceId
                  << "\n\t\tSync profile:" << accInfo.syncProfile;
         m_accounts << accInfo;
    }

    return true;
}

bool ButeoImport::update()
{
    if (!m_importLock.tryLock()) {
        qWarning() << "Fail to lock import mutex";
        onError("", ButeoImport::InernalError, false);
        return false;
    }

    if (m_accounts.isEmpty()) {
        qDebug() << "No accounts to update";
        // if there is not account to update just commit the update
        m_importLock.unlock();
        Q_EMIT updated();
        return true;
    }

    return checkOldAccounts();
}

bool ButeoImport::continueUpdate()
{
    m_failToSyncProfiles.clear();

    // enable buteo sync service if necessary
    Q_FOREACH(AccountInfo info, m_accounts) {
        if (info.syncEnabled) {
            info.enableSync(BUTEO_UOA_SERVICE_NAME);
        }
    }

    for(int i=0; i < m_accounts.size(); i++) {
        AccountInfo &accInfo = m_accounts[i];
        if (accInfo.syncProfile.isEmpty()) {
            qDebug() << "Will create buteo profile for" << accInfo.accountId << "account";
            accInfo.syncProfile = createProfileForAccount(accInfo.accountId);
            if (accInfo.syncProfile.isEmpty()) {
                // fail to create profiles
                qWarning() << "Fail to create profiles";
                onError("", ButeoImport::FailToCreateButeoProfiles, true);
                return false;
            } else {
                m_buteoQueue.insert(i, accInfo.syncProfile);
            }
        } else {
            m_buteoQueue.insert(i, accInfo.syncProfile);
            if (!startSync(accInfo.syncProfile)) {
                qWarning() << "Fail to start sync" << accInfo.syncProfile;
                m_buteoQueue.remove(i);
            }
        }
    }

    return true;
}

bool ButeoImport::prepareButeo()
{
    if (!m_buteoInterface.isNull()) {
        return true;
    }

    m_buteoInterface.reset(new QDBusInterface(BUTEO_DBUS_SERVICE_NAME,
                                              BUTEO_DBUS_OBJECT_PATH,
                                              BUTEO_DBUS_INTERFACE));

    if (!m_buteoInterface->isValid()) {
        m_buteoInterface.reset();
        qWarning() << "Fail to connect with syncfw";
        return false;
    }

    connect(m_buteoInterface.data(),
            SIGNAL(signalProfileChanged(QString, int, QString)),
            SLOT(onProfileChanged(QString, int, QString)));
    connect(m_buteoInterface.data(),
            SIGNAL(syncStatus(QString,int,QString,int)),
            SLOT(onSyncStatusChanged(QString,int,QString,int)));

    return true;
}

QString ButeoImport::createProfileForAccount(quint32 id)
{
    if (m_buteoInterface.isNull()) {
        qWarning() << "Buteo interface is not valid";
        return QString();
    }

    QDBusReply<QString> result = m_buteoInterface->call("createSyncProfileForAccount", id);
    if (result.error().isValid()) {
        qWarning() << "Fail to create profile for account" << id << result.error();
    } else if (!result.value().isEmpty()) {
        qDebug() << "Profile created" << result.value() << id;
        return result.value();
    } else {
        qWarning() << "Fail to create profile for account" << id;
    }
    return QString();
}

bool ButeoImport::removeProfile(const QString &profileId)
{
    if (m_buteoInterface.isNull()) {
        qWarning() << "Buteo interface is not valid";
        return false;
    }

    // check for account
    quint32 accountId = 0;
    QString newSourceId;

    for (int i=0; i < m_accounts.size(); i++) {
        AccountInfo acc = m_accounts[i];
        if (acc.syncProfile == profileId) {
            accountId = acc.accountId;
            newSourceId = acc.newSourceId;
            break;
        }
    }
    if (accountId != 0) {
        qWarning() << "Fail to find account related with profile" << profileId;
        return false;
    }

    // remove source
    if (!newSourceId.isEmpty()) {
        QScopedPointer<QContactManager> manager(new QContactManager("galera"));
        if (!manager->removeContact(QContactId::fromString(newSourceId))) {
            qWarning() << "Fail to remove contact source:" << newSourceId;
            return false;
        } else {
            qDebug() << "Source removed" << newSourceId;
        }
    } else {
        qDebug() << "No source was created for account" << accountId;
    }

    // remove profile
    return buteoRemoveProfile(profileId);
}

bool ButeoImport::buteoRemoveProfile(const QString &profileId) const
{
    QDBusReply<bool> result = m_buteoInterface->call("removeProfile", profileId);
    if (result.error().isValid()) {
        qWarning() << "Fail to remove profile" << profileId << result.error();
        return false;
    }
    return true;
}

bool ButeoImport::removeSources(const QStringList &sources)
{
    if (sources.isEmpty()) {
        return true;
    }

    bool result = true;
    QScopedPointer<QContactManager> manager(new QContactManager("galera"));
    Q_FOREACH(const QString &source, sources) {
        if (!manager->removeContact(QContactId::fromString(source))) {
            qWarning() << "Fail to remove source" << source;
            result = false;
        }
    }

    return result;
}

bool ButeoImport::restoreService()
{
    QDBusMessage setSafeMode = QDBusMessage::createMethodCall("com.canonical.pim",
                                                              "/com/canonical/pim/AddressBook",
                                                              "org.freedesktop.DBus.Properties",
                                                              "Set");
    QList<QVariant> args;
    args << "com.canonical.pim.AddressBook"
         << "safeMode"
         << QVariant::fromValue(QDBusVariant(false));
    setSafeMode.setArguments(args);
    QDBusReply<void> reply = QDBusConnection::sessionBus().call(setSafeMode);
    if (reply.error().isValid()) {
        qWarning() << "Fail to disable safe-mode" << reply.error().message();
        return false;
    } else {
        qDebug() << "Server safe mode disabled";
        return true;
    }
}

bool ButeoImport::commit()
{
    Q_ASSERT(m_buteoQueue.isEmpty());

    // update new favorites
    matchFavorites();

    QStringList sourceToRemove;

    for(int i=0; i < m_accounts.size(); i++) {
        AccountInfo &accInfo = m_accounts[i];
        if (accInfo.removeAfterUpdate) {
            sourceToRemove << accInfo.oldSourceId;
        }
        // disable old syncevolution service
        if (accInfo.syncEnabled) {
            accInfo.enableSync(SYNCEVO_UOA_SERVICE_NAME, false);
        }
    }

    removeSources(sourceToRemove);

    m_accounts.clear();
    // all accounts synced
    m_importLock.unlock();

    // update settings key
    QSettings settings;
    settings.setValue(SETTINGS_BUTEO_KEY, true);
    settings.sync();

    // disable address-book-service safe-mode
    restoreService();

    // WORKAROUND: wait 4 secs to fire update done, this is necessary because the contacts will be set as favorite
    // just after the signal be fired and the changes can not have the same timestamp that the creation
    QTimer::singleShot(4000, this, SIGNAL(updated()));
    return true;
}

bool ButeoImport::rollback()
{
    // remove all profiles and new sources
    Q_FOREACH(const QString &profile, m_failToSyncProfiles) {
        removeProfile(profile);
    }
    m_failToSyncProfiles.clear();
    return true;
}

bool ButeoImport::markAsUpdate()
{
    return restoreService();
}

void ButeoImport::onError(const QString &accountName, int errorCode, bool unlock)
{
    if (unlock) {
        m_importLock.unlock();
    }
    m_lastError = ABUpdateModule::ImportError(errorCode);
    Q_EMIT updateError(accountName, m_lastError);
}



void ButeoImport::onOldContactsSyncFinished(const QString &accountName, const QString &serviceName)
{
    qDebug() << "SyncEvolution: Sync done" << accountName << serviceName;
    if (serviceName == "contacts") {
        syncOldContactsContinue();
    }
}

void ButeoImport::onOldContactsSyncError(const QString &accountName, const QString &serviceName, const QString &error)
{
    qWarning() << "SyncEvolution: Fail to sync account " << accountName << serviceName << error;
    onError("", ButeoImport::InitialSyncError, true);
}

bool ButeoImport::requireInternetConnection()
{
    return true;
}

bool ButeoImport::canUpdate()
{
    if (!prepareButeo()) {
        qWarning() << "Fail to connect with contact sync service. We can not continue the upgrade";
        return false;
    }

    // check for any running sync
    QStringList syncs = runningSyncs();
    if (!syncs.isEmpty()) {
        qWarning() << "Sync running" << syncs << "no way to check for outdated information";
        return false;
    }

    return true;
}

ButeoImport::ImportError ButeoImport::lastError() const
{
    return m_lastError;
}

void ButeoImport::onProfileChanged(const QString &profileName, int changeType, const QString &profileAsXml)
{
    Q_UNUSED(profileAsXml);
    /*
    *      0 (ADDITION): Profile was added.
    *      1 (MODIFICATION): Profile was modified.
    *      2 (DELETION): Profile was deleted.
    */
    switch(changeType) {
    case 0:
        // profile created sync should start soon
        qDebug() << "Signal profile created received" << profileName;
        break;
    case 1:
        break;
    case 2:
        {
            int index = m_buteoQueue.key(profileName, -1);
            if (index != -1) {
                AccountInfo &accInfo = m_accounts[index];
                accInfo.syncProfile = "";
                m_buteoQueue.remove(index);

                qDebug() << "Profile removed" << accInfo.accountId << profileName;
            }

            if (m_buteoQueue.isEmpty()) {
                // all accounts removed
                m_importLock.unlock();
            }
            break;
        }
    }
}

void ButeoImport::onSyncStatusChanged(const QString &profileName,
                                      int status,
                                      const QString &message,
                                      int moreDetails)
{
    Q_UNUSED(message);
    Q_UNUSED(moreDetails);

    int index = m_buteoQueue.key(profileName, -1);
    if (index == -1) {
        qDebug() << "Profile not found" << profileName;
        return;
    }

    AccountInfo &accInfo = m_accounts[index];
    qDebug() << "SyncStatus"
             << "\n\tProfile:" << profileName
             << "\n\tAccount:" << accInfo.accountId
             << "\n\tStatus:" << status
             << "\n\tMessage:" << message
             << "\n\tDetails:" << moreDetails;
    /*
    *      0 (QUEUED): Sync request has been queued or was already in the
    *          queue when sync start was requested.
    *      1 (STARTED): Sync session has been started.
    *      2 (PROGRESS): Sync session is progressing.
    *      3 (ERROR): Sync session has encountered an error and has been stopped,
    *          or the session could not be started at all.
    *      4 (DONE): Sync session was successfully completed.
    *      5 (ABORTED): Sync session was aborted.
    */
    switch(status) {
    case 0:
    case 1:
    case 2:
        return;
    case 3:
        if (!accInfo.syncEnabled) {
            // error because the accout is not enabled
        } else {
            qWarning() << "Sync error for account:" << accInfo.accountId  << "and profile" << profileName;
            m_failToSyncProfiles << profileName;
            m_lastError = parseError(moreDetails);
        }
        break;
    case 4:
        qDebug() << "Sync finished for account:" << accInfo.accountId  << "and profile" << profileName;
        break;
    case 5:
        qWarning() << "Sync aborted for account:" << accInfo.accountId  << "and profile" << profileName;
        m_failToSyncProfiles << profileName;
        break;
    }

    m_buteoQueue.remove(index);
    if (m_buteoQueue.isEmpty()) {
        qDebug() << "All accounts has fineshed the sync, number of accounts that fail to sync:" << m_failToSyncProfiles;
        if (m_failToSyncProfiles.isEmpty()) {
            Q_EMIT updated();
        } else {
            QMetaObject::invokeMethod(this, "onError", Qt::QueuedConnection,
                                      Q_ARG(QString, ""),
                                      Q_ARG(int, m_lastError),
                                      Q_ARG(bool, true));
        }
    }
}
