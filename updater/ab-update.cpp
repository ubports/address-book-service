#include "ab-update.h"
#include "ab-update-module.h"
#include "ab-update-buteo-import.h"

#include <QtCore/QDebug>
#include <QtCore/QTimer>
#include <QtCore/QCoreApplication>

ABUpdate::ABUpdate(QObject *parent)
    : QObject(parent),
      m_netManager(new QNetworkConfigurationManager),
      m_needsUpdate(false),
      m_isRunning(false),
      m_activeModule(-1),
      m_skipNetworkTest(false)
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

void ABUpdate::startUpdate()
{
    if (isRunning()) {
        qWarning() << "Update already running.";
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
    updateNextModule();
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
        notifyError(module);
        updateNextModule();
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

void ABUpdate::notifyError(ABUpdateModule *module)
{
    qWarning() << "Fail to update module" << module->name() << module->lastError();
    module->roolback();
    //TODO show OSD asking for reply
}

bool ABUpdate::isOnline() const
{
    return (m_skipNetworkTest || m_netManager->isOnline());
}

void ABUpdate::waitForInternet()
{
    qDebug() << "Not internet connection wait before start upgrade";
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
    if (module->commit()) {
        qDebug() << "Update complete for:" << module->name();
    } else {
        qDebug() << "Fail to commit final changes for module" << module->name();
        notifyError(module);
    }

    updateNextModule();
}

void ABUpdate::onModuleUpdateError(const QString &errorMessage)
{
    ABUpdateModule *module = m_updateModules.at(m_activeModule);
    module->disconnect(this);
    notifyError(module);

    updateNextModule();
}

void ABUpdate::onOnlineStateChanged(bool isOnline)
{
    if (isOnline) {
        qDebug() << "Network is online resume upddate process.";
        m_netManager->disconnect(this);

        updateNextModule();
    }
}

void ABUpdate::notifyDone()
{
    m_activeModule = -1;
    m_lock.unlock();
    Q_EMIT updateDone();
}
