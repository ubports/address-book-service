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

#include "folks-dummy-base-test.h"

#include "src/qindividual.h"
#include "common/dbus-service-defs.h"

#include <QDebug>
#include <QTest>

ScopedEventLoop::ScopedEventLoop(QEventLoop **proxy)
{
    reset(proxy);
}

ScopedEventLoop::~ScopedEventLoop()
{
    *m_proxy = 0;
}

void ScopedEventLoop::reset(QEventLoop **proxy)
{
    *proxy = &m_eventLoop;
    m_proxy = proxy;
}

void ScopedEventLoop::exec()
{
    if (*m_proxy) {
        m_eventLoop.exec();
        *m_proxy = 0;
    }
}

void BaseDummyTest::initTestCase()
{
    initEnviroment();
    ScopedEventLoop loop(&m_eventLoop);

    m_backendStore = folks_backend_store_dup();
    folks_backend_store_load_backends(m_backendStore,
                                      (GAsyncReadyCallback) BaseDummyTest::backendStoreLoaded,
                                      this);

    loop.exec();

    loop.reset(&m_eventLoop);
    folks_backend_store_enable_backend(m_backendStore, "dummy",
                                       (GAsyncReadyCallback) BaseDummyTest::backendEnabled,
                                       this);
    loop.exec();

    m_backend = DUMMYF_BACKEND(folks_backend_store_dup_backend_by_name(m_backendStore, "dummy"));

    QVERIFY(m_backend != 0);
    configurePrimaryStore();
}

void BaseDummyTest::init()
{
    m_addressBook = new galera::AddressBook;
}

void BaseDummyTest::cleanup()
{
    QDBusConnection connection = QDBusConnection::sessionBus();
    connection.unregisterService(CPIM_SERVICE_NAME);
    delete m_addressBook;
    m_addressBook = 0;
}

void BaseDummyTest::cleanupTestCase()
{
    g_object_unref(m_primaryPersonaStore);
    g_object_unref(m_backend);
    g_object_unref(m_backendStore);
    m_primaryPersonaStore = 0;
    m_backend = 0;
    m_backendStore = 0;
}

void BaseDummyTest::configurePrimaryStore()
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

void BaseDummyTest::startService()
{
    QDBusConnection connection = QDBusConnection::sessionBus();
    if (connection.interface()->isServiceRegistered(CPIM_SERVICE_NAME)) {
        QFAIL("Pin service already registered!");
    }

    if (!connection.registerService(CPIM_SERVICE_NAME))
    {
        QFAIL("Could not register pim service!");
    } else {
        qDebug() << "Interface registered:" << CPIM_SERVICE_NAME;
    }

    m_addressBook = new galera::AddressBook;
    m_addressBook->registerObject(connection);
}

void BaseDummyTest::startServiceSync()
{
    startService();
    while(!m_addressBook->isReady()) {
        QCoreApplication::instance()->processEvents();
    }
}

void BaseDummyTest::mkpath(const QString &path)
{
    QDir dir;
    QVERIFY(dir.mkpath(path));
}

void BaseDummyTest::initEnviroment()
{
    QVERIFY(m_tmpDir.isValid());
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

void BaseDummyTest::backendEnabled(FolksBackendStore *backendStore,
                                   GAsyncResult *res,
                                   BaseDummyTest *self)
{
    folks_backend_store_enable_backend_finish(backendStore, res);
    self->m_eventLoop->quit();
    self->m_eventLoop = 0;
}


void BaseDummyTest::backendStoreLoaded(FolksBackendStore *backendStore,
                                       GAsyncResult *res,
                                       BaseDummyTest *self)
{
    GError *error = 0;
    folks_backend_store_load_backends_finish(backendStore, res, &error);
    checkError(error);

    self->m_eventLoop->quit();
    self->m_eventLoop = 0;
}

void BaseDummyTest::checkError(GError *error)
{
    if (error) {
        qWarning() << error->message;
        g_error_free(error);
    }
    QVERIFY(error == 0);
}

QString BaseDummyTest::createContact(const QtContacts::QContact &qcontact)
{
    ScopedEventLoop loop(&m_eventLoop);

    FolksIndividualAggregator *fia = folks_individual_aggregator_new();
    folks_individual_aggregator_prepare(fia,
                                        (GAsyncReadyCallback) BaseDummyTest::individualAggregatorPrepared,
                                        this);

    loop.exec();

    GHashTable *details = galera::QIndividual::parseDetails(qcontact);
    loop.reset(&m_eventLoop);
    folks_individual_aggregator_add_persona_from_details(fia,
                                                         NULL, //parent
                                                         FOLKS_PERSONA_STORE(m_primaryPersonaStore),
                                                         details,
                                                         (GAsyncReadyCallback) BaseDummyTest::individualAggregatorAddedPersona,
                                                         this);

    loop.exec();
    return QString();
}

void BaseDummyTest::individualAggregatorPrepared(FolksIndividualAggregator *fia,
                                                 GAsyncResult *res,
                                                 BaseDummyTest *self)
{
    GError *error = 0;

    folks_individual_aggregator_prepare_finish(fia, res, &error);
    checkError(error);

    self->m_eventLoop->quit();
    self->m_eventLoop = 0;
}

void BaseDummyTest::individualAggregatorAddedPersona(FolksIndividualAggregator *fia,
                                                     GAsyncResult *res,
                                                     BaseDummyTest *self)
{
    GError *error = 0;
    qDebug() << "Contact added";
    folks_individual_aggregator_add_persona_from_details_finish(fia, res, &error);
    checkError(error);

    self->m_eventLoop->quit();
    self->m_eventLoop = 0;
}

