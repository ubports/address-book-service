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

#include "ab-update.h"
#include "ab-update-module.h"
#include "ab-update-buteo-import.h"
#include "ab-notify-message.h"
#include "ab-i18n.h"

#include <QtCore/QDebug>
#include <QtCore/QTimer>
#include <QtCore/QCoreApplication>
#include <QtCore/QDir>
#include <QtNetwork/QNetworkConfiguration>

#define TRANSFER_ICON           "/usr/share/icons/suru/status/scalable/transfer-progress.svg"
#define TRANSFER_ICON_PAUSED    "/usr/share/icons/suru/status/scalable/transfer-paused.svg"
#define TRANSFER_ICON_ERROR     "/usr/share/icons/suru/status/scalable/transfer-error.svg"


ABUpdate::ABUpdate(QObject *parent)
    : QObject(parent),
      m_netManager(new QNetworkConfigurationManager),
      m_needsUpdate(false),
      m_isRunning(false),
      m_activeModule(-1),
      m_skipNetworkTest(false),
      m_silenceMode(false),
      m_lockFile(QDir::tempPath() + "/address-book-updater.lock")
{
    // load update modules (this can be a plugin system in the future)
    m_updateModules << new ButeoImport;
    m_lockFile.setStaleLockTime(0);
}

ABUpdate::~ABUpdate()
{
    qDeleteAll(m_updateModules);
}

QList<ABUpdateModule*> ABUpdate::needsUpdate() const
{
    QList<ABUpdateModule*> result;
    Q_FOREACH(ABUpdateModule *module, m_updateModules) {
        bool mNeedsUpdate = module->needsUpdate();
        qDebug() << "Check if module needs update" << module->name() << ":" << mNeedsUpdate;
        if (mNeedsUpdate) {
            result << module;
        } else {
            module->markAsUpdate();
        }
    }
    return result;
}

bool ABUpdate::isRunning()
{
    if (m_lock.tryLock()) {
        m_lock.unlock();
        return false;
    }
    return true;
}

void ABUpdate::skipNetworkTest()
{
    m_skipNetworkTest = true;
}

void ABUpdate::setSilenceMode(bool flag)
{
    m_silenceMode = flag;
}

void ABUpdate::startUpdate()
{
    qDebug() << "Start update...";
    if (!m_lockFile.tryLock()) {
        qWarning() << "Lock file is locked. Removing it...";
        m_lockFile.removeStaleLockFile();
    }

    if (isRunning()) {
        qWarning() << "Update already running.";
        if (!m_silenceMode) {
            ABNotifyMessage *msg = new ABNotifyMessage(true, this);
            //if force is set and the app is waiting for internet continue anyway
            msg->show(_("Account update"),
                      QString(_("%1 contact sync account upgrade already in progress")).arg("Google"),
                      TRANSFER_ICON);
        }
        return;
    }

    // check if any module needs a upgrade
    qDebug() << "check modules to update";
    QList<ABUpdateModule*> modulesToUpdate = needsUpdate();
    if (modulesToUpdate.isEmpty()) {
        qDebug() << "No module to update.";
        notifyDone();
        return;
    }

    if (m_waitingForIntenert) {
        m_waitingForIntenert = false;
        m_netManager->disconnect(this);
    }

    m_lock.lock();
    m_modulesToUpdate = modulesToUpdate;
    qDebug() << "Modules to update" << m_modulesToUpdate.size();
    if (isOnline(false)) {
        notifyStart();
    } else {
        qWarning() << "No internet abort";
        notifyNoInternet();
        updateNextModule();
    }
}

void ABUpdate::startUpdateWhenConnected()
{
    qDebug() << "Start update when connected...";

    if (isRunning()) {
        qWarning() << "Update already running.";
        return;
    }

    if (needsUpdate().isEmpty()) {
        qDebug() << "No modules to update";
        notifyDone();
        return;
    }

    if (isOnline(true)) {
        startUpdate();
    } else {
        notifyNoInternet();
        waitForInternet();
    }
}

void ABUpdate::startUpdate(ABUpdateModule *module)
{
    qDebug() << "Start update for" << module->name();
    m_activeModule = m_updateModules.indexOf(module);
    if (!module->canUpdate()) {
        qWarning() << "Module can not be updated" << module->name();
        onModuleUpdateError("", ABUpdateModule::InernalError);
    } else {
        if (!module->prepareToUpdate()) {
            qWarning() << "Fail to prepare to update:" << module->name();
            updateNextModule();
        } else {
            connect(module,
                    SIGNAL(updated()),
                    SLOT(onModuleUpdated()));
            connect(module,
                    SIGNAL(updateError(QString,  ABUpdateModule::ImportError)),
                    SLOT(onModuleUpdateError(QString,  ABUpdateModule::ImportError)));
            module->update();
        }
    }
}

bool ABUpdate::isOnline(bool checkConnectionType) const
{
    if (m_skipNetworkTest) {
        return true;
    }

    if (!m_netManager->isOnline()) {
        return false;
    } else if (checkConnectionType) {
        QNetworkConfiguration::BearerType bearerType = m_netManager->defaultConfiguration().bearerType();
        return ((bearerType == QNetworkConfiguration::BearerWLAN) ||
                (bearerType == QNetworkConfiguration::BearerEthernet));
    } else {
        return true;
    }
}

void ABUpdate::waitForInternet()
{
    qDebug() << "Not internet connection wait before start upgrade";
    m_waitingForIntenert = true;
    connect(m_netManager.data(),
            SIGNAL(onlineStateChanged(bool)),
            SLOT(onOnlineStateChanged()));
    connect(m_netManager.data(),
            SIGNAL(updateCompleted()),
            SLOT(onOnlineStateChanged()));
    connect(m_netManager.data(),
            SIGNAL(configurationAdded(QNetworkConfiguration)),
            SLOT(onOnlineStateChanged()));
    connect(m_netManager.data(),
            SIGNAL(configurationChanged(QNetworkConfiguration)),
            SLOT(onOnlineStateChanged()));
    connect(m_netManager.data(),
            SIGNAL(configurationRemoved(QNetworkConfiguration)),
            SLOT(onOnlineStateChanged()));
}

QString ABUpdate::errorMessage(ABUpdateModule::ImportError error) const
{
    switch (error)
    {
    case ABUpdateModule::ApplicationAreadyUpdated:
        return _("Contacts app is updated already!");
    case ABUpdateModule::ConnectionError:
        return _("Fail to connect to internet!");
    case ABUpdateModule::FailToConnectWithButeo:
        return _("Fail to connect to contact sync service!");
    case ABUpdateModule::FailToCreateButeoProfiles:
        return _("Fail to create sync profile!");
    case ABUpdateModule::FailToAuthenticate:
        return _("Fail to authenticate!");
    case ABUpdateModule::InernalError:
        return _("Internal error during the sync!");
    case ABUpdateModule::OnlineAccountNotFound:
        return _("Online account not found!");
    case ABUpdateModule::SyncAlreadyRunning:
        return _("Contact sync update already in progress!!");
    case ABUpdateModule::SyncError:
    default:
        return _("Fail to sync contacts!");
    }
}

void ABUpdate::updateNextModule()
{
    qDebug() << "Update next module" << m_modulesToUpdate.size();
    if (m_modulesToUpdate.isEmpty()) {
        notifyDone();
    } else {
        startUpdate(m_modulesToUpdate.takeFirst());
    }
}


void ABUpdate::cancelUpdate()
{
}

void ABUpdate::onModuleUpdated()
{
    ABUpdateModule *module = m_updateModules.at(m_activeModule);
    module->disconnect(this);
    if (!module->commit()) {
        qDebug() << "Fail to commit final changes for module" << module->name();
        onModuleUpdateError("", ABUpdateModule::InernalError);
        return;
    }

    qDebug() << "Update complete for:" << module->name();
    if (m_silenceMode) {
        updateNextModule();
    } else {
        ABNotifyMessage *msg = new ABNotifyMessage(true, this);
        msg->show(_("Account update"),
                  _("Contact sync update complete."),
                  TRANSFER_ICON);
        connect(msg, SIGNAL(messageClosed()), SLOT(updateNextModule()));
    }
}

void ABUpdate::onModuleUpdateError(const QString &accountName, ABUpdateModule::ImportError error)
{
    ABUpdateModule *module = m_updateModules.at(m_activeModule);
    module->disconnect(this);

    qWarning() << "Fail to update module" << module->name() << accountName << module->lastError();
    module->rollback();

    if (m_silenceMode) {
        updateNextModule();
    } else {
        ABNotifyMessage *msg = new ABNotifyMessage(true, this);
        msg->setProperty("MODULE", QVariant::fromValue<QObject*>(module));
        msg->askYesOrNo(_("Fail to update"),
                        QString(_("%1.\nDo you want to retry now?")).arg(errorMessage(error)),
                        TRANSFER_ICON_ERROR);
        connect(msg, SIGNAL(questionAccepted()), this, SLOT(onModuleUpdateRetry()));
        connect(msg, SIGNAL(questionRejected()), SLOT(onModuleUpdateNoRetry()));
        connect(msg, SIGNAL(messageClosed()), SLOT(onModuleUpdateNoRetry()));
    }
}

void ABUpdate::onModuleUpdateRetry()
{
    QObject *sender = QObject::sender();
    disconnect(sender);

    ABUpdateModule *module = qobject_cast<ABUpdateModule*>(sender->property("MODULE").value<QObject*>());
    m_modulesToUpdate << module;
    updateNextModule();
}

void ABUpdate::onModuleUpdateNoRetry()
{
    QObject *sender = QObject::sender();
    disconnect(sender);

    ABNotifyMessage *msg = new ABNotifyMessage(true, this);
    msg->show(_("Fail to update"),
              _("To retry later open Contacts app and press the sync button."),
              TRANSFER_ICON);
    connect(msg, SIGNAL(messageClosed()), SLOT(updateNextModule()));
}

void ABUpdate::onOnlineStateChanged()
{
    if (isOnline(true)) {
        qDebug() << "Network is online resume upddate process.";
        m_waitingForIntenert = false;
        m_netManager->disconnect(this);
        QTimer::singleShot(5000, this, SLOT(continueUpdateWithInternet()));
    }
}

void ABUpdate::continueUpdateWithInternet()
{
    if (isOnline(true)) {
        startUpdate();
    } else {
        waitForInternet();
    }
}

void ABUpdate::notifyStart()
{
    if (m_silenceMode) {
        updateNextModule();
    } else {
        ABNotifyMessage *msg = new ABNotifyMessage(true, this);
        msg->show(_("Account update"),
                  QString(_("%1 contact sync account upgrade in progress")).arg("Google"),
                  TRANSFER_ICON);
        connect(msg, SIGNAL(messageClosed()), SLOT(updateNextModule()));
    }
}

void ABUpdate::notifyNoInternet()
{
    if (!m_silenceMode) {
        ABNotifyMessage *msg = new ABNotifyMessage(true, this);
        msg->show(_("Account update"),
                  QString(_("%1 contact sync account needs upgrade. Please connect with the internet.")).arg("Google"),
                  TRANSFER_ICON_PAUSED);
    }
}

void ABUpdate::notifyDone()
{
    m_activeModule = -1;
    m_lock.unlock();
    qDebug() << "Send update done";
    Q_EMIT updateDone();
}
