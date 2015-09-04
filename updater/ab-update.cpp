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
      m_silenceMode(false)
{
    // load update modules (this can be a plugin system in the future)
    m_updateModules << new ButeoImport;
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
    if (isRunning()) {
        qWarning() << "Update already running.";
        if (!m_silenceMode) {
            ABNotifyMessage *msg = new ABNotifyMessage(true, this);
            if (m_waitingForIntenert) {
                msg->show(_("Account update"),
                          QString(_("%1 contact sync account upgrade is waiting for internet connection.")).arg("Google"),
                          TRANSFER_ICON);
            } else {
                msg->show(_("Account update"),
                          QString(_("%1 contact sync account upgrade already in progress")).arg("Google"),
                          TRANSFER_ICON);
            }
        }
        return;
    }

    // check if any module needs a upgrade
    QList<ABUpdateModule*> modulesToUpdate = needsUpdate();
    if (modulesToUpdate.isEmpty()) {
        qDebug() << "No module to update.";
        notifyDone();
        return;
    }


    m_lock.lock();
    m_modulesToUpdate = modulesToUpdate;
    if (!isOnline()) {
        notifyNoInternet();
        waitForInternet();
    } else {
        notifyStart();
        updateNextModule();
    }
}

void ABUpdate::startUpdate(ABUpdateModule *module)
{
    qDebug() << "Start update for" << module->name();
    m_activeModule = m_updateModules.indexOf(module);
    if (!isOnline()) {
        m_modulesToUpdate << module;
        waitForInternet();
    } else if (!module->canUpdate()) {
        qWarning() << "Module can not be updated" << module->name();
        onModuleUpdateError(_("Internal error"));
    } else {
        connect(module,
                SIGNAL(updated()),
                SLOT(onModuleUpdated()));
        connect(module,
                SIGNAL(updateError(QString,ButeoImport::ImportError)),
                SLOT(onModuleUpdateError(QString)));
        module->update();
    }
}

bool ABUpdate::isOnline() const
{
    return (m_skipNetworkTest || m_netManager->isOnline());
}

void ABUpdate::waitForInternet()
{
    qDebug() << "Not internet connection wait before start upgrade";
    m_waitingForIntenert = true;
    connect(m_netManager.data(), SIGNAL(onlineStateChanged(bool)), SLOT(onOnlineStateChanged(bool)));
}

void ABUpdate::updateNextModule()
{
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
        onModuleUpdateError(_("Internal Error"));
        return;
    }

    qDebug() << "Update complete for:" << module->name();
    if (m_silenceMode) {
        updateNextModule();
    } else {
        ABNotifyMessage *msg = new ABNotifyMessage(true, this);
        msg->show(_("Account update"),
                  _("Contact sync upgrade complete."),
                  TRANSFER_ICON);
        connect(msg, SIGNAL(messageClosed()), SLOT(updateNextModule()));
    }
}

void ABUpdate::onModuleUpdateError(const QString &errorMessage)
{
    ABUpdateModule *module = m_updateModules.at(m_activeModule);
    module->disconnect(this);

    qWarning() << "Fail to update module" << module->name() << module->lastError();
    module->roolback();

    if (m_silenceMode) {
        updateNextModule();
    } else {
        ABNotifyMessage *msg = new ABNotifyMessage(true, this);
        msg->show(_("Account update"),
                  QString(_("Could not complete %1 contact sync account upgrade.\nOnly local contacts will be editable until upgrade is complete.\nTo retry, open Contacts app and press the sync button.")).arg("Google"),
                  TRANSFER_ICON_ERROR);

        connect(msg, SIGNAL(messageClosed()), SLOT(updateNextModule()));
    }
}

void ABUpdate::onOnlineStateChanged(bool isOnline)
{
    if (isOnline) {
        m_waitingForIntenert = false;
        qDebug() << "Network is online resume upddate process.";
        m_netManager->disconnect(this);
        notifyStart();
        updateNextModule();
    }
}

void ABUpdate::notifyStart()
{
    if (!m_silenceMode) {
        ABNotifyMessage *msg = new ABNotifyMessage(true, this);
        msg->show(_("Account update"),
                  QString(_("%1 contact sync account upgrade in progress")).arg("Google"),
                  TRANSFER_ICON);
    }
}

void ABUpdate::notifyNoInternet()
{
    if (!m_silenceMode) {
        ABNotifyMessage *msg = new ABNotifyMessage(true, this);
        msg->show(_("Account update"),
                  QString(_("%1 contact sync account needs upgrade. Please connect with the internet to start the update.")).arg("Google"),
                  TRANSFER_ICON_PAUSED);
    }
}

void ABUpdate::notifyDone()
{
    m_activeModule = -1;
    m_lock.unlock();
    updateDone();
}
