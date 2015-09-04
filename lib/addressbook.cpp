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
#include "addressbook.h"
#include "addressbook-adaptor.h"
#include "view.h"
#include "contacts-map.h"
#include "qindividual.h"
#include "dirtycontact-notify.h"
#include "e-source-ubuntu.h"

#include "common/vcard-parser.h"

#include <QtCore/QPair>
#include <QtCore/QUuid>

#include <QtContacts/QContactExtendedDetail>

#include <signal.h>
#include <sys/socket.h>

#include <folks/folks-eds.h>

// Ubuntu
#include <messaging-menu-app.h>
#include <messaging-menu-message.h>
#include <url-dispatcher.h>

namespace C {
#include <libintl.h>
}

#define MESSAGING_MENU_SOURCE_ID "address-book-service"

using namespace QtContacts;

namespace
{

class CreateContactData
{
public:
    QDBusMessage m_message;
    QContact m_contact;
    galera::AddressBook *m_addressbook;
};

class UpdateContactsData
{
public:
    QList<QContact> m_contacts;
    QStringList m_request;
    int m_currentIndex;
    QStringList m_result;
    galera::AddressBook *m_addressbook;
    QDBusMessage m_message;
};

class RemoveContactsData
{
public:
    QStringList m_request;
    galera::AddressBook *m_addressbook;
    QDBusMessage m_message;
    int m_sucessCount;
    bool m_softRemoval;
};

class CreateSourceData
{
public:
    QString m_sourceId;
    QString m_sourceName;
    QString m_applicationId;
    QString m_providerName;
    uint m_accountId;
    bool m_setAsPrimary;
    galera::AddressBook *m_addressbook;
    QDBusMessage m_message;
    ESource *m_source;
};

class RemoveSourceData
{
public:
    galera::AddressBook *m_addressbook;
    QDBusMessage m_message;
};

ESource* create_esource_from_data(CreateSourceData &data, ESourceRegistry **registry)
{
    GError *error = NULL;
    ESource *source = e_source_new_with_uid(data.m_sourceId.toUtf8().data(), NULL, &error);
    if (error) {
        qWarning() << "Fail to create source" << error->message;
        g_error_free(error);
        return 0;
    }

    e_source_set_parent(source, "contacts-stub");
    e_source_set_display_name(source, data.m_sourceName.toUtf8().data());

    if (data.m_accountId > 0) {
        ESourceUbuntu *ubuntu_ex = E_SOURCE_UBUNTU(e_source_get_extension(source, E_SOURCE_EXTENSION_UBUNTU));
        e_source_ubuntu_set_account_id(ubuntu_ex, data.m_accountId);
        e_source_ubuntu_set_application_id(ubuntu_ex, data.m_applicationId.toUtf8());
        e_source_ubuntu_set_autoremove(ubuntu_ex, TRUE);
        data.m_providerName = QString::fromUtf8(e_source_ubuntu_get_account_provider(ubuntu_ex));
    }

    ESourceAddressBook *ext = E_SOURCE_ADDRESS_BOOK(e_source_get_extension(source, E_SOURCE_EXTENSION_ADDRESS_BOOK));
    e_source_backend_set_backend_name(E_SOURCE_BACKEND(ext), "local");

    *registry = e_source_registry_new_sync(NULL, &error);
    if (error) {
        qWarning() << "Fail to change default contact address book" << error->message;
        g_error_free(error);
        g_object_unref(source);
        return 0;
    }

    return source;
}

}

namespace galera
{
int AddressBook::m_sigQuitFd[2] = {0, 0};
QSettings AddressBook::m_settings(SETTINGS_ORG, SETTINGS_APPLICATION);

AddressBook::AddressBook(QObject *parent)
    : QObject(parent),
      m_individualAggregator(0),
      m_contacts(0),
      m_adaptor(0),
      m_notifyContactUpdate(0),
      m_edsIsLive(false),
      m_ready(false),
      m_isAboutToQuit(false),
      m_isAboutToReload(false),
      m_individualsChangedDetailedId(0),
      m_notifyIsQuiescentHandlerId(0),
      m_connection(QDBusConnection::sessionBus()),
      m_messagingMenu(0)
{
    if (qEnvironmentVariableIsSet(ALTERNATIVE_CPIM_SERVICE_NAME)) {
        m_serviceName = qgetenv(ALTERNATIVE_CPIM_SERVICE_NAME);
        qDebug() << "Using alternative service name:" << m_serviceName;
    } else {
        m_serviceName = CPIM_SERVICE_NAME;
    }
    prepareUnixSignals();
    connectWithEDS();
    connect(this, SIGNAL(readyChanged()), SLOT(checkCompatibility()));
    connect(this, SIGNAL(safeModeChanged()), SLOT(onSafeModeChanged()));
}

AddressBook::~AddressBook()
{
    if (m_messagingMenu) {
        g_object_unref(m_messagingMenu);
        m_messagingMenu = 0;
    }

    if (m_individualAggregator) {
        qWarning() << "Addressbook destructor called while running, you should call shutdown first";
        shutdown();
        while (m_adaptor) {
            QCoreApplication::processEvents();
        }
    }

    if (m_notifyContactUpdate) {
        delete m_notifyContactUpdate;
        m_notifyContactUpdate = 0;
    }
}

QString AddressBook::objectPath()
{
    return CPIM_ADDRESSBOOK_OBJECT_PATH;
}

bool AddressBook::registerObject(QDBusConnection &connection)
{
    if (connection.interface()->isServiceRegistered(m_serviceName)) {
        qWarning() << "Galera pin service already registered";
        return false;
    } else if (!connection.registerService(m_serviceName)) {
        qWarning() << "Could not register service!" << m_serviceName;
        return false;
    }

    if (!m_adaptor) {
        m_adaptor = new AddressBookAdaptor(connection, this);
        if (!connection.registerObject(galera::AddressBook::objectPath(), this))
        {
            qWarning() << "Could not register object!" << objectPath();
            delete m_adaptor;
            m_adaptor = 0;
            if (m_notifyContactUpdate) {
                delete m_notifyContactUpdate;
                m_notifyContactUpdate = 0;
            }
        }
    }
    if (m_adaptor) {
        m_notifyContactUpdate = new DirtyContactsNotify(m_adaptor);
    }
    return (m_adaptor != 0);
}

bool AddressBook::start(QDBusConnection connection)
{
    if (registerObject(connection)) {
        m_connection = connection;
        prepareFolks();
        return true;
    }

    return false;
}

bool AddressBook::start()
{
    g_type_ensure (E_TYPE_SOURCE_UBUNTU);

    return start(QDBusConnection::sessionBus());
}

void AddressBook::unprepareFolks()
{
    // remove all contacts
    // flusing any pending notification
    m_notifyContactUpdate->flush();

    setIsReady(false);

    Q_FOREACH(View* view, m_views) {
        view->close();
    }
    m_views.clear();

    if (m_contacts) {
        delete m_contacts;
        m_contacts = 0;
    }

    qDebug() << "Will destroy aggregator" << (void*) m_individualAggregator;
    if (m_individualAggregator) {
        g_signal_handler_disconnect(m_individualAggregator,
                                    m_individualsChangedDetailedId);
        g_signal_handler_disconnect(m_individualAggregator,
                                    m_notifyIsQuiescentHandlerId);
        m_individualsChangedDetailedId = m_notifyIsQuiescentHandlerId = 0;

        // make it sync
        qDebug() << "call unprepare";
        folks_individual_aggregator_unprepare(m_individualAggregator,
                                              AddressBook::folksUnprepared,
                                              this);
    }
}

void AddressBook::checkCompatibility()
{
    QByteArray envSafeMode = qgetenv(ADDRESS_BOOK_SAFE_MODE);
    if (!envSafeMode.isEmpty()) {
        return;
    }
    GError *gError = NULL;
    ESourceRegistry *r = e_source_registry_new_sync(NULL, &gError);
    if (gError) {
        qWarning() << "Fail to check compatibility" << gError->message;
        g_error_free(gError);
        return;
    }

    bool enableSafeMode = false;
    GList *sources = e_source_registry_list_sources(r, E_SOURCE_EXTENSION_ADDRESS_BOOK);
    for(GList *l = sources; l != NULL; l = l->next) {
        ESource *s = E_SOURCE(l->data);
        if ((strcmp(e_source_get_uid(s), "system-address-book") != 0)  &&
            !e_source_has_extension(s, E_SOURCE_EXTENSION_UBUNTU)) {
            qDebug() << "Source does not contains UBUNTU extension" << QString::fromUtf8(e_source_get_display_name(s));
            enableSafeMode = true;
            break;
        }
    }

    g_list_free_full(sources, g_object_unref);
    g_object_unref(r);

    if (enableSafeMode) {
        qWarning() << "Enabling safe mode";
        setSafeMode(true);
    } else {
        qDebug() << "Safe mode not necessary";
    }

    // check safe mode status
    onSafeModeChanged();
}

void AddressBook::shutdown()
{
    m_isAboutToQuit = true;
    unprepareFolks();
}

void AddressBook::continueShutdown()
{
    qDebug() << "Folks is not running anymore";
    if (m_adaptor) {
        if (m_connection.interface() &&
            m_connection.interface()->isValid()) {

            m_connection.unregisterObject(objectPath());
            if (m_connection.interface()->isServiceRegistered(m_serviceName)) {
                m_connection.unregisterService(m_serviceName);
            }
        }

        delete m_adaptor;
        m_adaptor = 0;
        Q_EMIT stopped();
    }
}

void AddressBook::setIsReady(bool isReady)
{
    if (isReady != m_ready) {
        m_ready = isReady;
        if (m_adaptor) {
            Q_EMIT readyChanged();
        }
    }
}

void AddressBook::prepareFolks()
{
    qDebug() << "Initialize folks";
    m_contacts = new ContactsMap;
    m_individualAggregator = folks_individual_aggregator_dup();
    gboolean ready;
    g_object_get(G_OBJECT(m_individualAggregator), "is-quiescent", &ready, NULL);
    m_notifyIsQuiescentHandlerId = g_signal_connect(m_individualAggregator,
                                          "notify::is-quiescent",
                                          (GCallback) AddressBook::isQuiescentChanged,
                                          this);

    m_individualsChangedDetailedId = g_signal_connect(m_individualAggregator,
                                          "individuals-changed-detailed",
                                          (GCallback) AddressBook::individualsChangedCb,
                                          this);

    folks_individual_aggregator_prepare(m_individualAggregator,
                                        (GAsyncReadyCallback) AddressBook::prepareFolksDone,
                                        this);
    if (ready) {
        qDebug() << "Folks is already in quiescent mode";
        setIsReady(ready);
    }
}

void AddressBook::unprepareEds()
{
    FolksBackendStore *store = folks_backend_store_dup();
    FolksBackend *edsBackend = folks_backend_store_dup_backend_by_name(store, "eds");
    if (edsBackend && folks_backend_get_is_prepared(edsBackend)) {
        qDebug() << "WILL unprepare EDS";
        folks_backend_unprepare(edsBackend,
                                AddressBook::edsUnprepared,
                                this);
    } else {
        qDebug() << "Eds not prepared will restart folks";
        prepareFolks();
    }
}

void AddressBook::connectWithEDS()
{
    // we need to keep it update with the EDS dbus service name
    static const QString evolutionServiceName(EVOLUTION_ADDRESSBOOK_SERVICE_NAME);

    // Check if eds was disabled manually
    // If eds was disabled we should skip the check
    if (qEnvironmentVariableIsSet("FOLKS_BACKENDS_ALLOWED")) {
        QString allowedBackends = qgetenv("FOLKS_BACKENDS_ALLOWED");
        if (!allowedBackends.contains("eds")) {
            m_edsIsLive = true;
            return;
        }
    }

    // check if service is already registered
    // We will try register a EDS service if its fails this mean that the service is already registered
    m_edsIsLive = !QDBusConnection::sessionBus().registerService(evolutionServiceName);
    if (!m_edsIsLive) {
        // if we succeed we need to unregister it
        QDBusConnection::sessionBus().unregisterService(evolutionServiceName);
    }

    m_edsWatcher = new QDBusServiceWatcher(evolutionServiceName,
                                           QDBusConnection::sessionBus(),
                                           QDBusServiceWatcher::WatchForOwnerChange,
                                           this);
    connect(m_edsWatcher, SIGNAL(serviceOwnerChanged(QString,QString,QString)),
            this, SLOT(onEdsServiceOwnerChanged(QString,QString,QString)));


    // WORKAROUND: Will ceck for EDS after the service get ready
    connect(this, SIGNAL(readyChanged()), SLOT(checkForEds()));
}

SourceList AddressBook::availableSources(const QDBusMessage &message)
{
    getSource(message, false);
    return SourceList();
}

Source AddressBook::source(const QDBusMessage &message)
{
    getSource(message, true);
    return Source();
}

Source AddressBook::createSource(const QString &sourceName,
                                 uint accountId,
                                 bool setAsPrimary,
                                 const QDBusMessage &message)
{
    CreateSourceData *data = new CreateSourceData;
    data->m_addressbook = this;
    data->m_message = message;
    data->m_sourceName = sourceName;
    data->m_setAsPrimary = setAsPrimary;
    data->m_accountId = accountId;

    FolksPersonaStore *store = folks_individual_aggregator_get_primary_store(m_individualAggregator);
    QString personaStoreTypeId("dummy");
    if (store) {
        personaStoreTypeId = QString::fromUtf8(folks_persona_store_get_type_id(store));
    }
    if (personaStoreTypeId == "dummy") {
        FolksBackendStore *backendStore = folks_backend_store_dup();
        FolksBackend *dummy = folks_backend_store_dup_backend_by_name(backendStore, "dummy");

        GeeMap *stores = folks_backend_get_persona_stores(dummy);
        GeeSet *storesKeys = gee_map_get_keys(stores);
        GeeSet *storesIds = (GeeSet*) gee_hash_set_new(G_TYPE_STRING,
                                                       (GBoxedCopyFunc) g_strdup, g_free,
                                                       NULL, NULL, NULL, NULL, NULL, NULL);

        gee_collection_add_all(GEE_COLLECTION(storesIds), GEE_COLLECTION(storesKeys));
        gee_collection_add(GEE_COLLECTION(storesIds), sourceName.toUtf8().constData());
        folks_backend_set_persona_stores(dummy, storesIds);

        g_object_unref(storesIds);
        g_object_unref(backendStore);
        g_object_unref(dummy);

        Source src(sourceName, sourceName, QString(), QString(), data->m_accountId, false, false);
        QDBusMessage reply = message.createReply(QVariant::fromValue<Source>(src));
        QDBusConnection::sessionBus().send(reply);
    } else if (personaStoreTypeId == "eds") {
        data->m_sourceId = QUuid::createUuid().toString().remove("{").remove("}");
        ESourceRegistry *registry = NULL;
        ESource *source = create_esource_from_data(*data, &registry);
        if (source) {
            data->m_source = source;
            e_source_registry_commit_source(registry,
                                            source,
                                            NULL,
                                            (GAsyncReadyCallback) AddressBook::createSourceDone,
                                            data);
        } else {
            delete data;
            QDBusMessage reply = message.createReply(QVariant::fromValue<Source>(Source()));
            QDBusConnection::sessionBus().send(reply);
        }
    } else {
        qWarning() << "Not supported, create sources on persona store with type id:" << personaStoreTypeId;
        delete data;
        QDBusMessage reply = message.createReply(QVariant::fromValue<Source>(Source()));
        QDBusConnection::sessionBus().send(reply);
    }
    return Source();
}

void AddressBook::removeSource(const QString &sourceId, const QDBusMessage &message)
{
    FolksBackendStore *bs = folks_backend_store_dup();
    FolksBackend *backend = folks_backend_store_dup_backend_by_name(bs, "eds");
    bool error = false;
    if (backend) {
        GeeMap *storesMap = folks_backend_get_persona_stores(backend);
        GeeCollection *stores = gee_map_get_values(storesMap);
        GeeIterator *i = gee_iterable_iterator(GEE_ITERABLE(stores));
        RemoveSourceData *rData = 0;
        while (gee_iterator_next(i)) {
            FolksPersonaStore *ps = FOLKS_PERSONA_STORE(gee_iterator_get(i));
            if (g_strcmp0(folks_persona_store_get_id(ps), sourceId.toUtf8().constData()) == 0) {
                rData = new RemoveSourceData;
                rData->m_addressbook = this;
                rData->m_message = message;
                edsf_persona_store_remove_address_book(EDSF_PERSONA_STORE(ps), AddressBook::removeSourceDone, rData);
                g_object_unref(ps);
                break;
            }
            g_object_unref(ps);
        }

        g_object_unref(backend);
        g_object_unref(stores);

        if (!rData) {
            qWarning() << "Source not found to remove:" << sourceId;
            error = true;
        }
    } else {
        qWarning() << "Fail to create eds backend during the source removal:" << sourceId;
        error = true;
    }

    g_object_unref(bs);

    if (error) {
        QDBusMessage reply = message.createReply(false);
        QDBusConnection::sessionBus().send(reply);
    }
}

void AddressBook::removeSourceDone(GObject *source,
                                   GAsyncResult *res,
                                   void *data)
{
    GError *error = 0;
    bool result = true;
    edsf_persona_store_remove_address_book_finish(res, &error);
    if (error) {
        qWarning() << "Fail to remove source" << error->message;
        g_error_free(error);
        result = false;
    }

    RemoveSourceData *rData = static_cast<RemoveSourceData*>(data);
    QDBusMessage reply = rData->m_message.createReply(result);
    QDBusConnection::sessionBus().send(reply);
    delete rData;
}

void AddressBook::folksUnprepared(GObject *source, GAsyncResult *res, void *data)
{
    AddressBook *self = static_cast<AddressBook*>(data);
    GError *error = NULL;
    folks_individual_aggregator_unprepare_finish(FOLKS_INDIVIDUAL_AGGREGATOR(source), res, &error);
    if (error) {
        qWarning() << "Fail to unprepare folks:" << error->message;
        g_error_free(error);
    }
    g_clear_object(&self->m_individualAggregator);

    qDebug() << "Folks unprepared" << (void*) self->m_individualAggregator;
    if (self->m_isAboutToQuit) {
        self->continueShutdown();
    } else {
        self->unprepareEds();
    }
}

void AddressBook::edsUnprepared(GObject *source, GAsyncResult *res, void *data)
{
    GError *error = NULL;
    folks_backend_unprepare_finish(FOLKS_BACKEND(source), res, &error);
    if (error) {
        qWarning() << "Fail to unprepare eds:" << error->message;
        g_error_free(error);
    }
    qDebug() << "EDS unprepared";
    folks_backend_prepare(FOLKS_BACKEND(source),
                          AddressBook::edsPrepared,
                          data);
}

void AddressBook::edsPrepared(GObject *source, GAsyncResult *res, void *data)
{
    AddressBook *self = static_cast<AddressBook*>(data);
    GError *error = NULL;
    folks_backend_prepare_finish(FOLKS_BACKEND(source), res, &error);
    if (error) {
        qWarning() << "Fail to prepare eds:" << error->message;
        g_error_free(error);
    }
    // remove reference created by parent function
    g_object_unref(source);
    // will start folks again
    self->prepareFolks();
}

void AddressBook::onSafeModeMessageActivated(MessagingMenuMessage *message,
                                             const char *actionId,
                                             GVariant *param,
                                             AddressBook *self)
{
    qDebug() << "mesage clicked launching address-book-app";
    url_dispatch_send("application:///address-book-app.desktop", NULL, NULL);
}

bool AddressBook::isSafeMode()
{
    QByteArray envSafeMode = qgetenv(ADDRESS_BOOK_SAFE_MODE);
    if (!envSafeMode.isEmpty()) {
        return (envSafeMode.toLower() == "on" ? true : false);
    } else {
        return m_settings.value(SETTINGS_SAFE_MODE_KEY, false).toBool();
    }
}

void AddressBook::setSafeMode(bool flag)
{
    QByteArray envSafeMode = qgetenv(ADDRESS_BOOK_SAFE_MODE);
    if (!envSafeMode.isEmpty()) {
        return;
    }

    if (m_settings.value(SETTINGS_SAFE_MODE_KEY, false).toBool() != flag) {
        m_settings.setValue(SETTINGS_SAFE_MODE_KEY, flag);
        if (!flag) {
            // make all contacts visible
            Q_FOREACH(ContactEntry *entry, m_contacts->values()) {
                entry->individual()->setVisible(true);
            }
            // clear invisible sources list
            m_settings.setValue(SETTINGS_INVISIBLE_SOURCES, QStringList());
        }
        m_settings.sync();
        Q_EMIT safeModeChanged();
    }
}

void AddressBook::createSourceDone(GObject *source,
                                   GAsyncResult *res,
                                   void *data)
{
    CreateSourceData *cData = static_cast<CreateSourceData*>(data);
    GError *error = 0;
    Source src;
    e_source_registry_commit_source_finish(E_SOURCE_REGISTRY(source), res, &error);
    if (error) {
        qWarning() << "Fail to create source" << error->message;
        g_error_free(error);
    } else {
        // set as primary if necessary
        if (cData->m_setAsPrimary) {
            e_source_registry_set_default_address_book(E_SOURCE_REGISTRY(source), cData->m_source);
        }
        src = Source(cData->m_sourceId,
                     cData->m_sourceName,
                     cData->m_applicationId,
                     cData->m_providerName,
                     cData->m_accountId,
                     false,
                     cData->m_setAsPrimary);
        // if in safe mode source will be invisible, we use that to avoid invalid states
        if (isSafeMode()) {
            qDebug() << "Source will be invisible until safe mode is gone" << cData->m_sourceId << e_source_get_uid(cData->m_source);
            QStringList iSources = cData->m_addressbook->m_settings.value(SETTINGS_INVISIBLE_SOURCES).toStringList();
            iSources << e_source_get_uid(cData->m_source);
            cData->m_addressbook->m_settings.setValue(SETTINGS_INVISIBLE_SOURCES, iSources);
            cData->m_addressbook->m_settings.sync();
        }
    }
    g_object_unref(source);
    QDBusMessage reply = cData->m_message.createReply(QVariant::fromValue<Source>(src));
    QDBusConnection::sessionBus().send(reply);
    delete cData;
}

void AddressBook::getSource(const QDBusMessage &message, bool onlyTheDefault)
{
    FolksBackendStore *backendStore = folks_backend_store_dup();
    QDBusMessage *msg = new QDBusMessage(message);

    if (folks_backend_store_get_is_prepared(backendStore)) {
        if (onlyTheDefault) {
            availableSourcesDoneListDefaultSource(backendStore, 0, msg);
        } else {
            availableSourcesDoneListAllSources(backendStore, 0, msg);
        }
    } else {
        if (onlyTheDefault) {
            folks_backend_store_prepare(backendStore,
                                        (GAsyncReadyCallback) availableSourcesDoneListDefaultSource,
                                        msg);
        } else {
            folks_backend_store_prepare(backendStore,
                                        (GAsyncReadyCallback) availableSourcesDoneListAllSources,
                                        msg);
        }
    }

    g_object_unref(backendStore);
}

void AddressBook::availableSourcesDoneListAllSources(FolksBackendStore *backendStore,
                                                     GAsyncResult *res,
                                                     QDBusMessage *msg)
{
    SourceList list = availableSourcesDoneImpl(backendStore, res);
    QDBusMessage reply = msg->createReply(QVariant::fromValue<SourceList>(list));
    QDBusConnection::sessionBus().send(reply);
    delete msg;
}

void AddressBook::availableSourcesDoneListDefaultSource(FolksBackendStore *backendStore,
                                                        GAsyncResult *res,
                                                        QDBusMessage *msg)
{
    Source defaultSource;
    SourceList list = availableSourcesDoneImpl(backendStore, res);
    if (list.count() > 0) {
        defaultSource = list[0];
    }
    QDBusMessage reply = msg->createReply(QVariant::fromValue<Source>(defaultSource));
    QDBusConnection::sessionBus().send(reply);
    delete msg;
}

SourceList AddressBook::availableSourcesDoneImpl(FolksBackendStore *backendStore, GAsyncResult *res)
{
    if (res) {
        folks_backend_store_prepare_finish(backendStore, res);
    }
    static QStringList backendBlackList;

    // these backends are not fully supported yet
    if (backendBlackList.isEmpty()) {
        backendBlackList << "telepathy"
                         << "bluez"
                         << "ofono"
                         << "key-file";
    }

    GeeCollection *backends = folks_backend_store_list_backends(backendStore);

    SourceList result;

    GeeIterator *iter = gee_iterable_iterator(GEE_ITERABLE(backends));
    while(gee_iterator_next(iter)) {
        FolksBackend *backend = FOLKS_BACKEND(gee_iterator_get(iter));
        QString backendName = QString::fromUtf8(folks_backend_get_name(backend));
        if (backendBlackList.contains(backendName)) {
            continue;
        }

        GeeMap *stores = folks_backend_get_persona_stores(backend);
        GeeCollection *values =  gee_map_get_values(stores);
        GeeIterator *backendIter = gee_iterable_iterator(GEE_ITERABLE(values));

        while(gee_iterator_next(backendIter)) {
            FolksPersonaStore *store = FOLKS_PERSONA_STORE(gee_iterator_get(backendIter));

            QString id = QString::fromUtf8(folks_persona_store_get_id(store));
            QString displayName = QString::fromUtf8(folks_persona_store_get_display_name(store));

            bool canWrite = folks_persona_store_get_can_add_personas(store) &&
                            folks_persona_store_get_can_remove_personas(store);
            bool isPrimary = folks_persona_store_get_is_primary_store(store);

            uint accountId = 0;
            QString applicationId;
            QString providerName;

            // FIXME: Due a bug on Folks we can not rely on folks_persona_store_get_is_primary_store
            // see main.cpp:68
            if (strcmp(folks_backend_get_name(backend), "eds") == 0) {
                GError *error = 0;
                ESourceRegistry *r = e_source_registry_new_sync(NULL, &error);
                if (error) {
                    qWarning() << "Failt to check default source:" << error->message;
                    g_error_free(error);
                } else {
                    ESource *defaultSource = e_source_registry_ref_default_address_book(r);
                    ESource *source = edsf_persona_store_get_source(EDSF_PERSONA_STORE(store));
                    isPrimary = e_source_equal(defaultSource, source);
                    g_object_unref(defaultSource);
                    g_object_unref(r);

                    if (e_source_has_extension(source, E_SOURCE_EXTENSION_UBUNTU)) {
                        ESourceUbuntu *ubuntu_ex = E_SOURCE_UBUNTU(e_source_get_extension(source, E_SOURCE_EXTENSION_UBUNTU));
                        if (ubuntu_ex) {
                            applicationId = QString::fromUtf8(e_source_ubuntu_get_application_id(ubuntu_ex));
                            providerName = QString::fromUtf8(e_source_ubuntu_get_account_provider(ubuntu_ex));
                            accountId = e_source_ubuntu_get_account_id(ubuntu_ex);
                        }
                    } else {
                        qDebug() << "SOURCE DOES NOT HAVE UBUNTU EXTENSION:"
                                 << QString::fromUtf8(e_source_get_display_name(source));
                    }
                }
            }

            // If running on safe mode only the system-address-book is writable
            if (isSafeMode() && (id != "system-address-book")) {
                qDebug() << "Running safe mode for source" << id << displayName;
                canWrite = false;
            }

            result << Source(id, displayName, applicationId, providerName, accountId, !canWrite, isPrimary);
            g_object_unref(store);
        }

        g_object_unref(backendIter);
        g_object_unref(backend);
        g_object_unref(values);
    }
    g_object_unref(iter);
    return result;
}

QString AddressBook::createContact(const QString &contact, const QString &source, const QDBusMessage &message)
{
    ContactEntry *entry = m_contacts->valueFromVCard(contact);
    if (entry) {
        qWarning() << "Contact exists";
    } else {
        QContact qcontact = VCardParser::vcardToContact(contact);
        if (!qcontact.isEmpty()) {
            GHashTable *details = QIndividual::parseDetails(qcontact);
            Q_ASSERT(details);
            CreateContactData *data = new CreateContactData;
            data->m_message = message;
            data->m_addressbook = this;
            data->m_contact = qcontact;
            FolksPersonaStore *store = getFolksStore(source);
            folks_individual_aggregator_add_persona_from_details(m_individualAggregator,
                                                                 NULL, //parent
                                                                 store,
                                                                 details,
                                                                 (GAsyncReadyCallback) createContactDone,
                                                                 (void*) data);
            g_hash_table_destroy(details);
            g_object_unref(store);
            return "";
        }
    }

    if (message.type() != QDBusMessage::InvalidMessage) {
        QDBusMessage reply = message.createReply(QString());
        QDBusConnection::sessionBus().send(reply);
    }
    return "";
}

FolksPersonaStore * AddressBook::getFolksStore(const QString &source)
{
    FolksPersonaStore *result = 0;

    if (!source.isEmpty()) {
        FolksBackendStore *backendStore = folks_backend_store_dup();
        GeeCollection *backends = folks_backend_store_list_backends(backendStore);

        GeeIterator *iter = gee_iterable_iterator(GEE_ITERABLE(backends));
        while((result == 0) && gee_iterator_next(iter)) {
            FolksBackend *backend = FOLKS_BACKEND(gee_iterator_get(iter));
            GeeMap *stores = folks_backend_get_persona_stores(backend);
            GeeCollection *values =  gee_map_get_values(stores);
            GeeIterator *backendIter = gee_iterable_iterator(GEE_ITERABLE(values));

            while(gee_iterator_next(backendIter)) {
                FolksPersonaStore *store = FOLKS_PERSONA_STORE(gee_iterator_get(backendIter));

                QString id = QString::fromUtf8(folks_persona_store_get_id(store));
                if (id == source) {
                    result = store;
                    break;
                }
                g_object_unref(store);
            }

            g_object_unref(backendIter);
            g_object_unref(backend);
            g_object_unref(values);
        }
        g_object_unref(iter);
        g_object_unref(backendStore);
    }

    if (!result)  {
        result = folks_individual_aggregator_get_primary_store(m_individualAggregator);
        Q_ASSERT(result);
        g_object_ref(result);
    }
    return result;
}

QString AddressBook::linkContacts(const QStringList &contacts)
{
    //TODO
    return "";
}

View *AddressBook::query(const QString &clause, const QString &sort, const QStringList &sources)
{
    View *view = new View(clause, sort, sources, m_ready ? m_contacts : 0, this);
    m_views << view;
    connect(view, SIGNAL(closed()), this, SLOT(viewClosed()));
    return view;
}

void AddressBook::viewClosed()
{
    m_views.remove(qobject_cast<View*>(QObject::sender()));
}

void AddressBook::individualChanged(QIndividual *individual)
{
    if (individual->isVisible()) {
        m_notifyContactUpdate->insertChangedContacts(QSet<QString>() << individual->id());
    }
}

void AddressBook::onEdsServiceOwnerChanged(const QString &name, const QString &oldOwner, const QString &newOwner)
{
    if (newOwner.isEmpty()) {
        m_edsIsLive = false;
        m_isAboutToReload = true;
        qWarning() << "EDS died: restarting service" << m_individualsChangedDetailedId;
        unprepareFolks();
    } else {
        m_edsIsLive = true;
    }
}

void AddressBook::onSafeModeChanged()
{
    qDebug() << "onSafeModeChanged" << isSafeMode() << (void*) m_messagingMenu;
    if (isSafeMode()) {
        if (m_messagingMenu) {
            return;
        }

        GIcon *icon = g_icon_new_for_string("address-book-app", NULL);
        m_messagingMenu = messaging_menu_app_new("address-book-app.desktop");
        messaging_menu_app_register(m_messagingMenu);
        messaging_menu_app_append_source(m_messagingMenu, MESSAGING_MENU_SOURCE_ID, icon, C::gettext("Address book service"));
        g_object_unref(icon);

        icon = g_themed_icon_new("address-book-app");
        MessagingMenuMessage *message = messaging_menu_message_new("address-book-service-safe-mode",
                                                                   icon,
                                                                   C::gettext("Update required"),
                                                                   NULL,
                                                                   C::gettext("Your Contacts app needs to be upgraded. Only local contacts will be editable until upgrade is complete"),
                                                                   QDateTime::currentMSecsSinceEpoch() * 1000); // the value is expected to be in microseconds

        g_signal_connect(message, "activate", G_CALLBACK(&AddressBook::onSafeModeMessageActivated), this);
        messaging_menu_app_append_message(m_messagingMenu, message, MESSAGING_MENU_SOURCE_ID, true);
        qDebug() << "Put safe message on messaging-menu";

        g_object_unref(icon);
        g_object_unref(message);
    } else {
        if (!m_messagingMenu) {
            return;
        }
        messaging_menu_app_unregister(m_messagingMenu);
        g_object_unref(m_messagingMenu);
    }
}

int AddressBook::removeContacts(const QStringList &contactIds, const QDBusMessage &message)
{
    RemoveContactsData *data = new RemoveContactsData;
    data->m_addressbook = this;
    data->m_message = message;
    data->m_request = contactIds;
    data->m_sucessCount = 0;
    data->m_softRemoval = true;
    removeContactDone(0, 0, data);
    return 0;
}

void AddressBook::removeContactDone(FolksIndividualAggregator *individualAggregator,
                                    GAsyncResult *result,
                                    void *data)
{
    GError *error = 0;
    RemoveContactsData *removeData = static_cast<RemoveContactsData*>(data);

   qDebug() << "WILL DELETE" << removeData  ->m_request;

    if (result) {
        folks_individual_aggregator_remove_individual_finish(individualAggregator, result, &error);
        if (error) {
            qWarning() << "Fail to remove contact:" << error->message;
            g_error_free(error);
        } else {
            removeData->m_sucessCount++;
        }
    }


    if (!removeData->m_request.isEmpty()) {
        QString contactId = removeData->m_request.takeFirst();
        ContactEntry *entry = removeData->m_addressbook->m_contacts->value(contactId);
        if (entry) {
            if (removeData->m_softRemoval && entry->individual()->markAsDeleted()) {
                removeContactDone(individualAggregator, 0, data);
                // since this will not be removed we need to send a removal singal
                removeData->m_addressbook->m_notifyContactUpdate->insertRemovedContacts(QSet<QString>() << entry->individual()->id());
            } else {
                folks_individual_aggregator_remove_individual(individualAggregator,
                                                              entry->individual()->individual(),
                                                              (GAsyncReadyCallback) removeContactDone,
                                                              data);
            }
        } else {
            removeContactDone(individualAggregator, 0, data);
        }
    } else {
        QDBusMessage reply = removeData->m_message.createReply(removeData->m_sucessCount);
        QDBusConnection::sessionBus().send(reply);
        delete removeData;
    }
}

QStringList AddressBook::sortFields()
{
    return SortClause::supportedFields();
}

bool AddressBook::unlinkContacts(const QString &parent, const QStringList &contacts)
{
    //TODO
    return false;
}

bool AddressBook::isReady() const
{
    return m_ready && m_edsIsLive;
}

QStringList AddressBook::updateContacts(const QStringList &contacts, const QDBusMessage &message)
{
    //TODO: support multiple update contacts calls
    Q_ASSERT(m_updateCommandPendingContacts.isEmpty());
    if (!processUpdates()) {
        qWarning() << "Fail to process pending updates";
        QDBusMessage reply = m_updateCommandReplyMessage.createReply(QStringList());
        QDBusConnection::sessionBus().send(reply);
        return QStringList();
    }

    m_updatedIds.clear();
    m_updateCommandReplyMessage = message;
    m_updateCommandResult = contacts;
    m_updateCommandPendingContacts = contacts;

    updateContactsDone("", "");
    return QStringList();
}

void AddressBook::purgeContacts(const QDateTime &since, const QString &sourceId, const QDBusMessage &message)
{
    RemoveContactsData *data = new RemoveContactsData;
    data->m_addressbook = this;
    data->m_message = message;
    data->m_sucessCount = 0;
    data->m_softRemoval = false;

    Q_FOREACH(const ContactEntry *entry, m_contacts->values()) {
        if (entry->individual()->deletedAt() > since) {
            QContactSyncTarget syncTarget = entry->individual()->contact().detail<QContactSyncTarget>();
            if (syncTarget.value(QContactSyncTarget::FieldSyncTarget + 1).toString() == sourceId) {
                data->m_request << entry->individual()->id();
            }
        }
    }

    removeContactDone(0, 0, data);
}

void AddressBook::updateContactsDone(const QString &contactId,
                                     const QString &error)
{
    int currentContactIndex = m_updateCommandResult.size() - m_updateCommandPendingContacts.size() - 1;

    if (!error.isEmpty()) {
        // update the result with the error
        if (currentContactIndex >= 0 &&
            currentContactIndex < m_updateCommandResult.size()) {
            m_updateCommandResult[currentContactIndex] = error;
        } else {
            qWarning() << "Invalid contact changed index" << currentContactIndex <<
                          "Contact list size" << m_updateCommandResult.size();
        }
    } else if (!contactId.isEmpty()){
        // update the result with the new contact info
        ContactEntry *entry = m_contacts->value(contactId);
        Q_ASSERT(entry);
        m_updatedIds << contactId;
        QContact contact = entry->individual()->contact();
        QString vcard = VCardParser::contactToVcard(contact);
        if (!vcard.isEmpty()) {
            m_updateCommandResult[currentContactIndex] = vcard;
        } else {
            m_updateCommandResult[currentContactIndex] = "";
        }
        // update contact position on map
        m_contacts->updatePosition(entry);
    }

    if (!m_updateCommandPendingContacts.isEmpty()) {
        QString vCard = m_updateCommandPendingContacts.takeFirst();
        QContact newContact = VCardParser::vcardToContact(vCard);
        ContactEntry *entry = m_contacts->value(newContact.detail<QContactGuid>().guid());
        if (entry) {
            entry->individual()->update(newContact, this,
                                        SLOT(updateContactsDone(QString,QString)));
        } else {
            qWarning() << "Contact not found for update:" << vCard;
            updateContactsDone("", "Contact not found!");
        }
    } else {
        QDBusMessage reply = m_updateCommandReplyMessage.createReply(m_updateCommandResult);
        QDBusConnection::sessionBus().send(reply);

        // notify about the changes
        m_notifyContactUpdate->insertChangedContacts(m_updatedIds.toSet());

        // clear command data
        m_updatedIds.clear();
        m_updateCommandResult.clear();
        m_updateCommandReplyMessage = QDBusMessage();
        m_updateLock.unlock();
    }
}

QString AddressBook::removeContact(FolksIndividual *individual, bool *visible)
{
    QString contactId = QString::fromUtf8(folks_individual_get_id(individual));
    ContactEntry *ci = m_contacts->take(contactId);
    if (ci) {
        *visible = ci->individual()->isVisible();
        delete ci;
        return contactId;
    }
    return QString();
}

QString AddressBook::addContact(FolksIndividual *individual, bool visible)
{
    QString id = QString::fromUtf8(folks_individual_get_id(individual));
    ContactEntry *entry = m_contacts->value(id);
    if (entry) {
        entry->individual()->setIndividual(individual);
        entry->individual()->setVisible(visible);
    } else {
        QIndividual *i = new QIndividual(individual, m_individualAggregator);
        i->addListener(this, SLOT(individualChanged(QIndividual*)));
        i->setVisible(visible);
        m_contacts->insert(new ContactEntry(i));
        //TODO: Notify view
    }

    return id;
}

void AddressBook::individualsChangedCb(FolksIndividualAggregator *individualAggregator,
                                       GeeMultiMap *changes,
                                       AddressBook *self)
{
    Q_UNUSED(individualAggregator);

    QSet<QString> removedIds;
    QSet<QString> addedIds;
    QSet<QString> updatedIds;
    QStringList invisibleSources;

    if (isSafeMode()) {
        invisibleSources = self->m_settings.value(SETTINGS_INVISIBLE_SOURCES).toStringList();
    }

    GeeSet *removed = gee_multi_map_get_keys(changes);
    GeeIterator *iter = gee_iterable_iterator(GEE_ITERABLE(removed));
    while(gee_iterator_next(iter)) {
        FolksIndividual *individual = FOLKS_INDIVIDUAL(gee_iterator_get(iter));
        if (!individual) {
            continue;
        }

        bool visible = true;
        QString cId = self->removeContact(individual, &visible);
        if (visible && !cId.isEmpty()) {
            removedIds << cId;
        }
        g_object_unref(individual);
    }
    g_object_unref(iter);

    GeeCollection *added = gee_multi_map_get_values(changes);
    iter = gee_iterable_iterator(GEE_ITERABLE(added));
    while(gee_iterator_next(iter)) {
        FolksIndividual *individual = FOLKS_INDIVIDUAL(gee_iterator_get(iter));

        if (!individual) {
            continue;
        }

        QString id = QString::fromUtf8(folks_individual_get_id(individual));
        if (addedIds.contains(id)) {
            g_object_unref(individual);
            continue;
        }

        bool visible = true;
        if (!invisibleSources.isEmpty()) {
            GeeSet *personas = folks_individual_get_personas(individual);
            GeeIterator *iter = gee_iterable_iterator(GEE_ITERABLE(personas));
            if (gee_iterator_next(iter)) {
                FolksPersona *persona = FOLKS_PERSONA(gee_iterator_get(iter));
                FolksPersonaStore *ps = folks_persona_get_store(persona);
                g_object_unref(persona);
                visible = !invisibleSources.contains(folks_persona_store_get_id(ps));
            }
            g_object_unref(iter);
        }

        bool exists = self->m_contacts->contains(id);
        QString cId = self->addContact(individual, visible);
        if (visible && exists) {
            updatedIds <<  cId;
        } else if (visible) {
            addedIds << cId;
        }

        g_object_unref(individual);
    }
    g_object_unref(iter);

    g_object_unref(removed);
    g_object_unref(added);

    if (!removedIds.isEmpty()) {
        self->m_notifyContactUpdate->insertRemovedContacts(removedIds);
    }

    if (!addedIds.isEmpty()) {
        self->m_notifyContactUpdate->insertAddedContacts(addedIds);
    }

    if (!updatedIds.isEmpty()) {
        self->m_notifyContactUpdate->insertChangedContacts(updatedIds);
    }
}

void AddressBook::prepareFolksDone(GObject *source,
                                      GAsyncResult *res,
                                      AddressBook *self)
{
    Q_UNUSED(source);
    Q_UNUSED(res);
    Q_UNUSED(self);
}

void AddressBook::createContactDone(FolksIndividualAggregator *individualAggregator,
                                    GAsyncResult *res,
                                    void *data)
{
    CreateContactData *createData = static_cast<CreateContactData*>(data);

    FolksPersona *persona;
    GError *error = NULL;
    QDBusMessage reply;
    persona = folks_individual_aggregator_add_persona_from_details_finish(individualAggregator, res, &error);
    if (error != NULL) {
        qWarning() << "Failed to create individual from contact:" << error->message;
        reply = createData->m_message.createErrorReply("Failed to create individual from contact", error->message);
        g_clear_error(&error);
    } else if (persona == NULL) {
        qWarning() << "Failed to create individual from contact: Persona already exists";
        reply = createData->m_message.createErrorReply("Failed to create individual from contact", "Contact already exists");
    } else {
        QIndividual::setExtendedDetails(persona,
                                        createData->m_contact.details(QContactExtendedDetail::Type),
                                        QDateTime::currentDateTime());
        FolksIndividual *individual = folks_persona_get_individual(persona);
        ContactEntry *entry = createData->m_addressbook->m_contacts->value(QString::fromUtf8(folks_individual_get_id(individual)));
        if (entry) {
            // We will need to reload contact due the extended details
            entry->individual()->flush();
            QString vcard = VCardParser::contactToVcard(entry->individual()->contact());
            if (createData->m_message.type() != QDBusMessage::InvalidMessage) {
                reply = createData->m_message.createReply(vcard);
            }
        } else if (createData->m_message.type() != QDBusMessage::InvalidMessage) {
            reply = createData->m_message.createErrorReply("", "Failed to retrieve the new contact");
        }
    }
    //TODO: use dbus connection
    if (createData->m_message.type() != QDBusMessage::InvalidMessage) {
        QDBusConnection::sessionBus().send(reply);
    }
    delete createData;
}

void AddressBook::isQuiescentChanged(GObject *source, GParamSpec *param, AddressBook *self)
{
    Q_UNUSED(param);
    gboolean ready = false;
    g_object_get(source, "is-quiescent", &ready, NULL);
    if (self) {
        self->setIsReady(ready);
    }
}

void AddressBook::quitSignalHandler(int)
 {
     char a = 1;
     ::write(m_sigQuitFd[0], &a, sizeof(a));
}

bool AddressBook::processUpdates()
{
    int timeout = 10;
    while(!m_updateLock.tryLock(1000)) {
        if (timeout <= 0) {
            return false;
        }
        QCoreApplication::processEvents();
        timeout--;
    }
    return true;
}

int AddressBook::init()
{
    struct sigaction quit = { { 0 } };
    Source::registerMetaType();

    quit.sa_handler = AddressBook::quitSignalHandler;
    sigemptyset(&quit.sa_mask);
    quit.sa_flags |= SA_RESTART;

    if (sigaction(SIGQUIT, &quit, 0) > 0)
        return 1;

    return 0;
}

void AddressBook::prepareUnixSignals()
{
    if (::socketpair(AF_UNIX, SOCK_STREAM, 0, m_sigQuitFd)) {
       qFatal("Couldn't create HUP socketpair");
    }

    m_snQuit = new QSocketNotifier(m_sigQuitFd[1], QSocketNotifier::Read, this);
    connect(m_snQuit, SIGNAL(activated(int)), this, SLOT(handleSigQuit()));
}

void AddressBook::handleSigQuit()
{
    m_snQuit->setEnabled(false);
    char tmp;
    ::read(m_sigQuitFd[1], &tmp, sizeof(tmp));

    shutdown();

    m_snQuit->setEnabled(true);
}

// WORKAROUND: For some strange reason sometimes EDS does not start with the service request
// we will try to reload folks if this happen
void AddressBook::checkForEds()
{
    if (!m_ready) {
        return;
    }

    // Use maxRetry value to avoid infinite loop
    static const int maxRetry = 10;
    static int retryCount = 0;

    qDebug() << "Check for EDS attempt number " << retryCount;
    if (retryCount >= maxRetry) {
        // abort when reach the maxRetry
        qWarning() << QDateTime::currentDateTime().toString() << "Fail to start EDS the service will abort";
        QTimer::singleShot(500, this, SLOT(shutdown()));
        return;
    }
    retryCount++;

    if (!m_edsIsLive) {
        // wait some ms to restart folks, this increase 1s for each retryCount
        m_isAboutToReload = true;
        QTimer::singleShot(1000 * retryCount, this, SLOT(unprepareFolks()));
        qWarning() << QDateTime::currentDateTime().toString() << "EDS did not start, trying to reload folks";
    } else {
        retryCount = 0;
    }
}

} //namespace
