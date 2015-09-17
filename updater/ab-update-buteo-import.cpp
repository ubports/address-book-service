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

#include "config.h"

#define BUTEO_DBUS_SERVICE_NAME   "com.meego.msyncd"
#define BUTEO_DBUS_OBJECT_PATH    "/synchronizer"
#define BUTEO_DBUS_INTERFACE      "com.meego.msyncd"

using namespace QtContacts;

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

bool ButeoImport::loadAccounts(QList<quint32> &accountsToUpdate, QList<quint32> &newAccounts)
{
    // check whitch account already has a source
    Accounts::Manager mgr;
    accountsToUpdate = mgr.accountList("contacts");

    qDebug() << "Accounts" << accountsToUpdate;
    // check which account does not have a source
    QMap<QString, quint32> srcs = sources();
    for(QMap<QString, uint>::const_iterator i = srcs.begin();
        i != srcs.end();
        i++) {
        qDebug() << "Source" << i.key() << "Account" << i.value();
        // remove ids that already has a source from the idList
        if (i.value() > 0) {
            newAccounts << i.value();
            accountsToUpdate.removeOne(i.value());
        }
    }

    // no accounts to update
    if (accountsToUpdate.isEmpty() && newAccounts.isEmpty()) {
        return true;
    }

    if (!prepareButeo()) {
        qWarning() << "Fail to connect with buteo service";
        return false;
    }

    // check if all new accounts has a sync profile
    Q_FOREACH(const quint32 &accountId, newAccounts) {
        QDBusReply<QStringList> reply = m_buteoInterface->call("syncProfilesByKey",
                                                               "accountid",
                                                               QString::number(accountId));
        if (reply.value().isEmpty()) {
            qDebug() << "Account does not have a sync profile" << accountId;
            accountsToUpdate << accountId;
            newAccounts.removeOne(accountId);
        }
    }

    return true;
}

bool ButeoImport::enableContactsService(quint32 accountId)
{
    Accounts::Manager mgr;
    QScopedPointer<Accounts::Account> account(mgr.account(accountId));

    if (account) {
        Q_FOREACH(Accounts::Service service, account->services()) {
            if (service.serviceType() == "contacts") {
                account->selectService(service);
                account->setEnabled(true);
                account->syncAndBlock();
            }
        }
        return true;
    } else {
        return false;
    }
}

QString ButeoImport::accountName(quint32 accountId)
{
    Accounts::Manager mgr;
    QScopedPointer<Accounts::Account> account(mgr.account(accountId));
    return account ? account->displayName() : QString();
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
        qWarning() << "Buteo inteface is not valid";
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
    QMap<QString, QString> paramenters;
    paramenters.insert(ADDRESS_BOOK_SHOW_INVISIBLE_PROP, "true");
    QScopedPointer<QContactManager> manager(new QContactManager("galera", paramenters));

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

bool ButeoImport::needsUpdate()
{
    // check settings
    QSettings settings;
    if (settings.value(SETTINGS_BUTEO_KEY, false).toBool()) {
        qDebug() << "Buteo has already been updated.";
        return false;
    }

    // check if we have old sources that need to be updated
    QList<quint32> accountsToUpdate;
    QList<quint32> newAccounts;
    if (loadAccounts(accountsToUpdate, newAccounts)) {
        if (accountsToUpdate.isEmpty() && newAccounts.isEmpty()) {
            qDebug() << "No account to update";
            // update settings key
            QSettings settings;
            settings.setValue(SETTINGS_BUTEO_KEY, true);
            settings.sync();
            return false;
        }
        qDebug() << accountsToUpdate.size() << "accounts, to update";
        qDebug() << newAccounts.size() << "accounts, to sync";
    } else {
        qWarning() << "Fail to load online accounts";
    }

    return true;
}

bool ButeoImport::prepareToUpdate()
{
    return true;
}

bool ButeoImport::update()
{
    if (!m_importLock.tryLock()) {
        qWarning() << "Fail to lock import mutex";
        error("", ButeoImport::InernalError);
        return false;
    }

    m_initialAccountToProfiles.clear();
    m_pendingAccountToProfiles.clear();
    m_failToSyncProfiles.clear();

    QList<quint32> accountsToUpdate;
    QList<quint32> newAccounts;
    if (!loadAccounts(accountsToUpdate, newAccounts)) {
        // fail to load accounts information
        m_importLock.unlock();
        qWarning() << "Fail to load accounts information";
        error("", ButeoImport::OnlineAccountNotFound);
        return false;
    }

    if (accountsToUpdate.isEmpty() && newAccounts.isEmpty()) {
        qDebug() << "No accounts to update";
        // if there is not account to update just commit the update
        m_importLock.unlock();
        Q_EMIT updated();
        return true;
    }


    if (!accountsToUpdate.isEmpty()) {
        qDebug() << "Will create buteo profile for" << accountsToUpdate << "accounts";
        m_initialAccountToProfiles = createProfileForAccounts(accountsToUpdate);
        if (m_initialAccountToProfiles.isEmpty()) {
            // fail to create profiles
            m_importLock.unlock();
            qWarning() << "Fail to create profiles";
            error("", ButeoImport::FailToCreateButeoProfiles);
            return false;
        }
    }

    // start sync for new accounts to make sure that they are all in sync
    Q_FOREACH(quint32 accountId, newAccounts) {
        QString profileName = this->profileName(accountId);
        if (!profileName.isEmpty()) {
            qDebug() << "Manually start sync for" << profileName;
            m_initialAccountToProfiles.insert(accountId, profileName);
            if (!startSync(profileName)) {
                qWarning() << "Fail to start sync" << profileName;
                m_initialAccountToProfiles.remove(accountId);
            }
        }
    }

    m_pendingAccountToProfiles = m_initialAccountToProfiles;

    return true;
}

QMap<QString, quint32> ButeoImport::sources() const
{
    QMap<QString, quint32> result;
    QScopedPointer<QContactManager> manager(new QContactManager("galera"));
    QContactDetailFilter sourceFilter;
    sourceFilter.setDetailType(QContactDetail::TypeType, QContactType::FieldType);
    sourceFilter.setValue( QContactType::TypeGroup);
    Q_FOREACH(const QContact &c, manager->contacts(sourceFilter)) {
        uint accountId = 0;
        // skip local source
        if (c.id().toString().endsWith("source@system-address-book")) {
            continue;
        }

        Q_FOREACH(const QContactExtendedDetail &xDet, c.details<QContactExtendedDetail>()) {
            if (xDet.name() == "ACCOUNT-ID") {
                if (xDet.data().isValid()) {
                    accountId = xDet.data().toString().toUInt();
                }
                break;
            }
        }

        result.insert(c.id().toString(), accountId);
    }

    return result;
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

QMap<quint32, QString> ButeoImport::createProfileForAccounts(QList<quint32> ids)
{
    QMap<quint32, QString> map;
    if (m_buteoInterface.isNull()) {
        qWarning() << "Buteo interface is not valid";
        return map;
    }

    Q_FOREACH(quint32 id, ids) {
        if (!enableContactsService(id)) {
            qWarning() << "Fail to enable contacts service for acccount:" << id;
            continue;
        }

        QDBusReply<QString> result = m_buteoInterface->call("createSyncProfileForAccount", id);
        if (result.error().isValid()) {
            qWarning() << "Fail to create profile for account" << id << result.error();
        } else if (!result.value().isEmpty()) {
            qDebug() << "Profile created" << result.value() << id;
            map.insert(id, result.value());
        } else {
            qWarning() << "Fail to create profile for account" << id;
        }
    }
    return map;
}

bool ButeoImport::removeProfile(const QString &profileId)
{
    if (m_buteoInterface.isNull()) {
        qWarning() << "Buteo interface is not valid";
        return false;
    }

    // check for account
    quint32 accountId = m_initialAccountToProfiles.key(profileId, 0);
    if (accountId == 0) {
        qWarning() << "Fail to find account related with profile" << profileId;
        return false;
    }

    // check for source
    QMap<QString, quint32> listOfSources = sources();
    QString sourceId = listOfSources.key(accountId, "");

    // remove source
    if (!sourceId.isEmpty()) {
        QScopedPointer<QContactManager> manager(new QContactManager("galera"));
        if (!manager->removeContact(QContactId::fromString(sourceId))) {
            qWarning() << "Fail to remove contact source:" << sourceId;
            return false;
        } else {
            qDebug() << "Source removed" << sourceId;
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

bool ButeoImport::commit()
{
    Q_ASSERT(m_accountToProfiles.isEmpty());

    // update new favorites
    matchFavorites();

    // remove old sources
    QStringList oldSources;
    QMap<QString, quint32> srcs = sources();

    for(QMap<QString, uint>::const_iterator i = srcs.begin();
        i != srcs.end();
        i++) {
        if (i.value() == 0) {
            qDebug() << "Remove source" << i.key();
            oldSources << i.key();
        }
    }
    removeSources(oldSources);

    // all acconts synced
    m_importLock.unlock();

    // update settings key
    QSettings settings;
    settings.setValue(SETTINGS_BUTEO_KEY, true);
    settings.sync();

    // disable address-book-service safe-mode
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
    } else {
        qDebug() << "Server safe mode disabled";
    }

    // WORKAROUND: wait 4 secs to fire update done, this is necessary because the contacts will be set as favorite
    // just after the signal be fired and the changes can not have the same timestamp that the creation
    QTimer::singleShot(4000, this, SIGNAL(updated()));
    return true;
}

bool ButeoImport::roolback()
{
    // remove all profiles and new sources
    Q_FOREACH(const QString &profile, m_failToSyncProfiles) {
        removeProfile(profile);
    }
    m_failToSyncProfiles.clear();
    return true;
}

void ButeoImport::error(const QString &accountName, ButeoImport::ImportError errorCode)
{
    m_lastError = errorCode;
    Q_EMIT updateError(accountName, errorCode);
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
        qDebug() << "Profile created" << profileName;
        break;
    case 1:
        break;
    case 2:
        {
            quint32 accountId = m_initialAccountToProfiles.key(profileName, 0);
            if (accountId > 0) {
                qDebug() << "Profile removed" << accountId << profileName;
                m_pendingAccountToProfiles.remove(accountId);
                if (m_pendingAccountToProfiles.isEmpty()) {
                    // all acconts removed
                    m_importLock.unlock();
                }
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

    if (!m_pendingAccountToProfiles.values().contains(profileName)) {
        qDebug() << "Profile not found" << profileName;
        return;
    }
    quint32 accountId = m_pendingAccountToProfiles.key(profileName, 0);
    qDebug() << "SyncStatus"
             << "\n\tProfile:" << profileName
             << "\n\tAccount:" << accountId
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
        qWarning() << "Sync error for account:" << accountId  << "and profile" << profileName;
        m_failToSyncProfiles << profileName;
        m_lastError = parseError(moreDetails);
        break;
    case 4:
        qDebug() << "Sync finished for account:" << accountId  << "and profile" << profileName;
        break;
    case 5:
        qWarning() << "Sync aborted for account:" << accountId  << "and profile" << profileName;
        m_failToSyncProfiles << profileName;
        break;
    }

    if (accountId > 0) {
        m_pendingAccountToProfiles.remove(accountId);
        if (m_pendingAccountToProfiles.isEmpty()) {
            qDebug() << "All accounts has fineshed the sync, number of accounts that fail to sync:" << m_failToSyncProfiles.size();
            if (m_failToSyncProfiles.isEmpty()) {
                Q_EMIT updated();
            } else {
                error("", m_lastError);
            }
        }
    }
}
