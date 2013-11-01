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

#include "config.h"
#include "dummy-backend.h"
#include "scoped-loop.h"

#include "lib/qindividual.h"

#include <QtCore/QDir>
#include <QtCore/QDebug>

DummyBackendProxy::DummyBackendProxy()
    : m_adaptor(0),
      m_backend(0),
      m_primaryPersonaStore(0),
      m_backendStore(0),
      m_isReady(false)
{
}

DummyBackendProxy::~DummyBackendProxy()
{
    shutdown();
    g_object_unref(m_primaryPersonaStore);
    g_object_unref(m_backend);
    g_object_unref(m_backendStore);
}

void DummyBackendProxy::start(bool useDBus)
{
    initEnviroment();
    initFolks();
    if (useDBus) {
        registerObject();
    }
}

void DummyBackendProxy::shutdown()
{
    m_isReady = false;

    if (m_adaptor) {
        QDBusConnection connection = QDBusConnection::sessionBus();

        connection.unregisterObject(DUMMY_OBJECT_PATH);
        connection.unregisterService(DUMMY_SERVICE_NAME);

        delete m_adaptor;
        m_adaptor = 0;
        Q_EMIT stopped();
    }
}

void DummyBackendProxy::initFolks()
{
    ScopedEventLoop loop(&m_eventLoop);
    m_backendStore = folks_backend_store_dup();
    folks_backend_store_load_backends(m_backendStore,
                                      (GAsyncReadyCallback) DummyBackendProxy::backendStoreLoaded,
                                      this);

    loop.exec();

    loop.reset(&m_eventLoop);
    folks_backend_store_enable_backend(m_backendStore, "dummy",
                                       (GAsyncReadyCallback) DummyBackendProxy::backendEnabled,
                                       this);
    loop.exec();

    m_backend = DUMMYF_BACKEND(folks_backend_store_dup_backend_by_name(m_backendStore, "dummy"));

    Q_ASSERT(m_backend != 0);
    configurePrimaryStore();
    m_isReady = true;
    Q_EMIT ready();
}

bool DummyBackendProxy::isReady() const
{
    return m_isReady;
}

QString DummyBackendProxy::createContact(const QtContacts::QContact &qcontact)
{
    ScopedEventLoop loop(&m_eventLoop);

    FolksIndividualAggregator *fia = FOLKS_INDIVIDUAL_AGGREGATOR_DUP();
    folks_individual_aggregator_prepare(fia,
                                        (GAsyncReadyCallback) DummyBackendProxy::individualAggregatorPrepared,
                                        this);

    loop.exec();

    GHashTable *details = galera::QIndividual::parseDetails(qcontact);
    loop.reset(&m_eventLoop);
    folks_individual_aggregator_add_persona_from_details(fia,
                                                         NULL, //parent
                                                         FOLKS_PERSONA_STORE(m_primaryPersonaStore),
                                                         details,
                                                         (GAsyncReadyCallback) DummyBackendProxy::individualAggregatorAddedPersona,
                                                         this);

    loop.exec();
    g_object_unref(fia);
    return QString();
}

void DummyBackendProxy::configurePrimaryStore()
{
    static const char* writableProperties[] = {
        folks_persona_store_detail_key(FolksPersonaDetail::FOLKS_PERSONA_DETAIL_BIRTHDAY),
        folks_persona_store_detail_key(FolksPersonaDetail::FOLKS_PERSONA_DETAIL_EMAIL_ADDRESSES),
        folks_persona_store_detail_key(FolksPersonaDetail::FOLKS_PERSONA_DETAIL_PHONE_NUMBERS),
        0
    };

    m_primaryPersonaStore = dummyf_persona_store_new("dummy-store",
                                                     "Dummy personas",
                                                     const_cast<char**>(writableProperties), 4);
    dummyf_persona_store_set_persona_type(m_primaryPersonaStore, DUMMYF_TYPE_FAT_PERSONA);

    GeeHashSet *personaStores = gee_hash_set_new(FOLKS_TYPE_PERSONA_STORE,
                                                 (GBoxedCopyFunc) g_object_ref, g_object_unref,
                                                 NULL, NULL, NULL, NULL, NULL, NULL);

    gee_abstract_collection_add(GEE_ABSTRACT_COLLECTION(personaStores), m_primaryPersonaStore);
    dummyf_backend_register_persona_stores(m_backend, GEE_SET(personaStores), true);
    dummyf_persona_store_reach_quiescence(m_primaryPersonaStore);
    g_object_unref(personaStores);
}

void DummyBackendProxy::backendEnabled(FolksBackendStore *backendStore,
                                       GAsyncResult *res,
                                       DummyBackendProxy *self)
{
    folks_backend_store_enable_backend_finish(backendStore, res);
    self->m_eventLoop->quit();
    self->m_eventLoop = 0;
}


void DummyBackendProxy::backendStoreLoaded(FolksBackendStore *backendStore,
                                           GAsyncResult *res,
                                           DummyBackendProxy *self)
{
    GError *error = 0;
    folks_backend_store_load_backends_finish(backendStore, res, &error);
    checkError(error);

    self->m_eventLoop->quit();
    self->m_eventLoop = 0;
}

void DummyBackendProxy::checkError(GError *error)
{
    if (error) {
        qWarning() << error->message;
        g_error_free(error);
    }
    Q_ASSERT(error == 0);
}

void DummyBackendProxy::mkpath(const QString &path)
{
    QDir dir;
    Q_ASSERT(dir.mkpath(path));
}

void DummyBackendProxy::initEnviroment()
{
    Q_ASSERT(m_tmpDir.isValid());
    QString tmpFullPath = QString("%1/folks-test").arg(m_tmpDir.path());

    qputenv("FOLKS_BACKENDS_ALLOWED", "dummy");
    qputenv("FOLKS_PRIMARY_STORE", "dummy");

    qDebug() << "setting up in transient directory:" << tmpFullPath;

    // home
    qputenv("HOME", tmpFullPath.toUtf8().data());

    // cache
    QString cacheDir = QString("%1/.cache").arg(tmpFullPath);

    mkpath(cacheDir);
    qputenv("XDG_CACHE_HOME", cacheDir.toUtf8().data());

    // config
    QString configDir = QString("%1/.config").arg(tmpFullPath);
    mkpath(configDir);
    qputenv("XDG_CONFIG_HOME", configDir.toUtf8().data());

    // data
    QString dataDir = QString("%1/.local/share").arg(tmpFullPath);
    mkpath(dataDir);
    qputenv("XDG_DATA_HOME", dataDir.toUtf8().data());
    mkpath(QString("%1/folks").arg(dataDir));

    // runtime
    QString runtimeDir = QString("%1/XDG_RUNTIME_DIR").arg(tmpFullPath);
    mkpath(runtimeDir);
    qputenv("XDG_RUNTIME_DIR", runtimeDir.toUtf8().data());

    qputenv("XDG_DESKTOP_DIR", "");
    qputenv("XDG_DOCUMENTS_DIR", "");
    qputenv("XDG_DOWNLOAD_DIR", "");
    qputenv("XDG_MUSIC_DIR", "");
    qputenv("XDG_PICTURES_DIR", "");
    qputenv("XDG_PUBLICSHARE_DIR", "");
    qputenv("XDG_TEMPLATES_DIR", "");
    qputenv("XDG_VIDEOS_DIR", "");
}

bool DummyBackendProxy::registerObject()
{
    QDBusConnection connection = QDBusConnection::sessionBus();

    if (!connection.registerService(DUMMY_SERVICE_NAME)) {
        qWarning() << "Could not register service!" << DUMMY_SERVICE_NAME;
        return false;
    }

    m_adaptor = new DummyBackendAdaptor(connection, this);
    if (!connection.registerObject(DUMMY_OBJECT_PATH, this))
    {
        qWarning() << "Could not register object!" << DUMMY_OBJECT_PATH;
        delete m_adaptor;
        m_adaptor = 0;
    } else {
        qDebug() << "Object registered:" << QString(DUMMY_OBJECT_PATH);
    }

    return (m_adaptor != 0);
}

void DummyBackendProxy::individualAggregatorPrepared(FolksIndividualAggregator *fia,
                                                     GAsyncResult *res,
                                                     DummyBackendProxy *self)
{
    GError *error = 0;

    folks_individual_aggregator_prepare_finish(fia, res, &error);
    checkError(error);

    self->m_eventLoop->quit();
    self->m_eventLoop = 0;
}

void DummyBackendProxy::individualAggregatorAddedPersona(FolksIndividualAggregator *fia,
                                                         GAsyncResult *res,
                                                         DummyBackendProxy *self)
{
    GError *error = 0;
    folks_individual_aggregator_add_persona_from_details_finish(fia, res, &error);
    checkError(error);

    self->m_eventLoop->quit();
    self->m_eventLoop = 0;
}

DummyBackendAdaptor::DummyBackendAdaptor(const QDBusConnection &connection, DummyBackendProxy *parent)
    : QDBusAbstractAdaptor(parent),
      m_connection(connection),
      m_proxy(parent)
{
    if (m_proxy->isReady()) {
        Q_EMIT ready();
    }
    connect(m_proxy, SIGNAL(ready()), this, SIGNAL(ready()));
}

DummyBackendAdaptor::~DummyBackendAdaptor()
{
}

bool DummyBackendAdaptor::isReady()
{
    return m_proxy->isReady();
}

bool DummyBackendAdaptor::ping()
{
    return true;
}

void DummyBackendAdaptor::quit()
{
    QMetaObject::invokeMethod(m_proxy, "shutdown", Qt::QueuedConnection);
}


