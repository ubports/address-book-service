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

#include "common/vcard-parser.h"

#include <QtCore/QPair>
#include <QtCore/QUuid>

#include <signal.h>
#include <sys/socket.h>

#include <folks/folks-eds.h>

using namespace QtContacts;

namespace
{

class CreateContactData
{
public:
    QDBusMessage m_message;
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
};

class CreateSourceData
{
public:
    QString m_sourceName;
    bool m_setAsPrimary;
    galera::AddressBook *m_addressbook;
    QDBusMessage m_message;
};

class RemoveSourceData
{
public:
    galera::AddressBook *m_addressbook;
    QDBusMessage m_message;
};

}

namespace galera
{
int AddressBook::m_sigQuitFd[2] = {0, 0};

AddressBook::AddressBook(QObject *parent)
    : QObject(parent),
      m_individualAggregator(0),
      m_contacts(0),
      m_adaptor(0),
      m_notifyContactUpdate(0),
      m_edsIsLive(false),
      m_ready(false),
      m_isAboutToQuit(false),
      m_individualsChangedDetailedId(0),
      m_notifyIsQuiescentHandlerId(0),
      m_connection(QDBusConnection::sessionBus())
{
    if (qEnvironmentVariableIsSet(ALTERNATIVE_CPIM_SERVICE_NAME)) {
        m_serviceName = qgetenv(ALTERNATIVE_CPIM_SERVICE_NAME);
        qDebug() << "Using alternative service name:" << m_serviceName;
    } else {
        m_serviceName = CPIM_SERVICE_NAME;
    }
    prepareUnixSignals();
    connectWithEDS();
}

AddressBook::~AddressBook()
{
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
    return start(QDBusConnection::sessionBus());
}

void AddressBook::unprepareFolks()
{
    // remove all contacts
    // flusing any pending notification
    m_notifyContactUpdate->flush();

    // notify about contacts removal
    if (m_contacts) {
        m_notifyContactUpdate->insertRemovedContacts(m_contacts->keys().toSet());
        m_notifyContactUpdate->flush();
    }

    m_ready = false;

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

void AddressBook::prepareFolks()
{
    qDebug() << "Initialize folks";
    m_contacts = new ContactsMap;
    m_individualAggregator = folks_individual_aggregator_dup();
    g_object_get(G_OBJECT(m_individualAggregator), "is-quiescent", &m_ready, NULL);
    if (m_ready) {
        AddressBook::isQuiescentChanged(G_OBJECT(m_individualAggregator), NULL, this);
    }
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
}

void AddressBook::connectWithEDS()
{
    // we need to keep it update with the EDS dbus service name
    static const QString evolutionServiceName("org.gnome.evolution.dataserver.AddressBook6");

    // Check if eds was disabled manually
    // If eds was disabled we should skip the check
    if (qEnvironmentVariableIsSet("FOLKS_BACKENDS_ALLOWED")) {
        QString allowedBackends = qgetenv("FOLKS_BACKENDS_ALLOWED");
        if (!allowedBackends.contains("eds")) {
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
    connect(this, SIGNAL(ready()), SLOT(checkForEds()));
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

Source AddressBook::createSource(const QString &sourceId, bool setAsPrimary, const QDBusMessage &message)
{
    CreateSourceData *data = new CreateSourceData;
    data->m_addressbook = this;
    data->m_message = message;
    data->m_sourceName = sourceId;
    data->m_setAsPrimary = setAsPrimary;

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
        gee_collection_add(GEE_COLLECTION(storesIds), sourceId.toUtf8().constData());
        folks_backend_set_persona_stores(dummy, storesIds);

        g_object_unref(storesIds);
        g_object_unref(backendStore);
        g_object_unref(dummy);

        Source src(sourceId, sourceId, false, false);
        QDBusMessage reply = message.createReply(QVariant::fromValue<Source>(src));
        QDBusConnection::sessionBus().send(reply);
    } else if (personaStoreTypeId == "eds") {
        edsf_persona_store_create_address_book(sourceId.toUtf8().data(),
                                               (GAsyncReadyCallback) AddressBook::createSourceDone,
                                               data);
    } else {
        qWarning() << "Not supported, create sources on persona store with type id:" << personaStoreTypeId;
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
        if (gee_map_has_key(storesMap, sourceId.toUtf8().constData())) {
            EdsfPersonaStore *ps = EDSF_PERSONA_STORE(gee_map_get(storesMap, sourceId.toUtf8().constData()));

            RemoveSourceData *rData = new RemoveSourceData;
            rData->m_addressbook = this;
            rData->m_message = message;
            edsf_persona_store_remove_address_book(ps, AddressBook::removeSourceDone, rData);
            g_object_unref(ps);
        } else {
            qWarning() << "Source not found to remove:" << sourceId;
            error = true;
        }
        g_object_unref(backend);
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
        qDebug() << "Try reload it again";
        self->prepareFolks();
    }
}

void AddressBook::createSourceDone(GObject *source,
                                   GAsyncResult *res,
                                   void *data)
{
    CreateSourceData *cData = static_cast<CreateSourceData*>(data);
    GError *error = 0;
    Source src;
    edsf_persona_store_create_address_book_finish(res, &error);
    if (error) {
        qWarning() << "Fail to create source" << error->message;
        g_error_free(error);
    } else {
        src = Source(cData->m_sourceName, cData->m_sourceName, false, cData->m_setAsPrimary);
        if (cData->m_setAsPrimary) {
            ESourceRegistry *r = e_source_registry_new_sync(NULL, &error);
            if (error) {
                qWarning() << "Fail to change default contact address book" << error->message;
                g_error_free(error);
            } else {
                ESource *edsSource = 0;
                GList *sources = e_source_registry_list_sources(r, E_SOURCE_EXTENSION_ADDRESS_BOOK);
                for(GList *i = sources; i != -0; i = sources->next) {
                    edsSource = E_SOURCE(i->data);
                    if (strcmp(cData->m_sourceName.toUtf8().constData(), e_source_get_uid(edsSource)) == 0) {
                        break;
                    }
                }
                if (edsSource) {
                    e_source_registry_set_default_address_book(r, edsSource);
                } else {
                    qWarning() << "Fail to find source:" << cData->m_sourceName;
                }
                g_list_free_full(sources, g_object_unref);
            }
            g_object_unref(r);
        }
    }
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
                }
            }

            result << Source(id, displayName, !canWrite, isPrimary);

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
    // wait for the service be ready for queries
    while(!m_ready) {
        QCoreApplication::processEvents();
    }

    View *view = new View(clause, sort, sources, m_contacts, this);
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
    m_notifyContactUpdate->insertChangedContacts(QSet<QString>() << individual->id());
}

void AddressBook::onEdsServiceOwnerChanged(const QString &name, const QString &oldOwner, const QString &newOwner)
{
    if (newOwner.isEmpty()) {
        m_edsIsLive = false;
        qWarning() << "EDS died: restarting service" << m_individualsChangedDetailedId;
        unprepareFolks();
    } else {
        m_edsIsLive = true;
    }
}

int AddressBook::removeContacts(const QStringList &contactIds, const QDBusMessage &message)
{
    RemoveContactsData *data = new RemoveContactsData;
    data->m_addressbook = this;
    data->m_message = message;
    data->m_request = contactIds;
    data->m_sucessCount = 0;
    removeContactDone(0, 0, data);
    return 0;
}

void AddressBook::removeContactDone(FolksIndividualAggregator *individualAggregator,
                                    GAsyncResult *result,
                                    void *data)
{
    GError *error = 0;
    RemoveContactsData *removeData = static_cast<RemoveContactsData*>(data);

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
            folks_individual_aggregator_remove_individual(individualAggregator,
                                                          entry->individual()->individual(),
                                                          (GAsyncReadyCallback) removeContactDone,
                                                          data);
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
    return m_ready;
}

QStringList AddressBook::updateContacts(const QStringList &contacts, const QDBusMessage &message)
{
    //TODO: support multiple update contacts calls
    Q_ASSERT(m_updateCommandPendingContacts.isEmpty());

    m_updatedIds.clear();
    m_updateCommandReplyMessage = message;
    m_updateCommandResult = contacts;
    m_updateCommandPendingContacts = contacts;

    updateContactsDone("", "");
    return QStringList();
}

void AddressBook::updateContactsDone(const QString &contactId,
                                     const QString &error)
{
    int currentContactIndex = m_updateCommandResult.size() - m_updateCommandPendingContacts.size() - 1;

    if (!error.isEmpty()) {
        // update the result with the error
        m_updateCommandResult[currentContactIndex] = error;
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
        QContact newContact = VCardParser::vcardToContact(m_updateCommandPendingContacts.takeFirst());
        ContactEntry *entry = m_contacts->value(newContact.detail<QContactGuid>().guid());
        if (entry) {
            entry->individual()->update(newContact, this,
                                        SLOT(updateContactsDone(QString,QString)));
        } else {
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
    }
}

QString AddressBook::removeContact(FolksIndividual *individual)
{
    QString contactId = QString::fromUtf8(folks_individual_get_id(individual));
    ContactEntry *ci = m_contacts->take(contactId);
    if (ci) {
        delete ci;
        return contactId;
    }
    return QString();
}

QString AddressBook::addContact(FolksIndividual *individual)
{
    QString id = QString::fromUtf8(folks_individual_get_id(individual));
    ContactEntry *entry = m_contacts->value(id);
    if (entry) {
        entry->individual()->setIndividual(individual);
    } else {
        QIndividual *i = new QIndividual(individual, m_individualAggregator);
        i->addListener(this, SLOT(individualChanged(QIndividual*)));
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

    GeeSet *removed = gee_multi_map_get_keys(changes);
    GeeIterator *iter = gee_iterable_iterator(GEE_ITERABLE(removed));
    while(gee_iterator_next(iter)) {
        FolksIndividual *individual = FOLKS_INDIVIDUAL(gee_iterator_get(iter));
        if (!individual) {
            continue;
        }

        removedIds << self->removeContact(individual);
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

        if (self->m_contacts->contains(id)) {
            updatedIds << self->addContact(individual);
        } else {
            addedIds << self->addContact(individual);
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
        FolksIndividual *individual = folks_persona_get_individual(persona);
        ContactEntry *entry = createData->m_addressbook->m_contacts->value(QString::fromUtf8(folks_individual_get_id(individual)));
        if (entry) {
            QString vcard = VCardParser::contactToVcard(entry->individual()->contact());
            if (createData->m_message.type() != QDBusMessage::InvalidMessage) {
                reply = createData->m_message.createReply(vcard);
            }
        } else if (createData->m_message.type() != QDBusMessage::InvalidMessage) {
            reply = createData->m_message.createErrorReply("Failed to retrieve the new contact", error->message);
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
    Q_UNUSED(source);
    Q_UNUSED(param);

    g_object_get(source, "is-quiescent", &self->m_ready, NULL);
    if (self->m_ready && self->m_adaptor) {
        Q_EMIT self->ready();
    }
}

void AddressBook::quitSignalHandler(int)
 {
     char a = 1;
     ::write(m_sigQuitFd[0], &a, sizeof(a));
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
        QTimer::singleShot(1000 * retryCount, this, SLOT(unprepareFolks()));
        qWarning() << QDateTime::currentDateTime().toString() << "EDS did not start, trying to reload folks";
    } else {
        retryCount = 0;
    }
}

} //namespace
