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
    QString sourceName;
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
      m_contacts(new ContactsMap),
      m_adaptor(0),
      m_notifyContactUpdate(0),
      m_ready(false),
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
}

AddressBook::~AddressBook()
{
    shutdown();
    // destructor
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

void AddressBook::shutdown()
{
    m_ready = false;

    Q_FOREACH(View* view, m_views) {
        view->close();
    }
    m_views.clear();

    if (m_contacts) {
        delete m_contacts;
        m_contacts = 0;
    }

    if (m_individualAggregator) {
        g_signal_handler_disconnect(m_individualAggregator,
                                    m_individualsChangedDetailedId);
        g_signal_handler_disconnect(m_individualAggregator,
                                    m_notifyIsQuiescentHandlerId);
        m_individualsChangedDetailedId = m_notifyIsQuiescentHandlerId = 0;
        g_clear_object(&m_individualAggregator);
    }

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
    m_individualAggregator = FOLKS_INDIVIDUAL_AGGREGATOR_DUP();
    g_object_get(G_OBJECT(m_individualAggregator), "is-quiescent", &m_ready, NULL);
    if (m_ready) {
        AddressBook::isQuiescentChanged(G_OBJECT(m_individualAggregator), NULL, this);
    }
    m_notifyIsQuiescentHandlerId = g_signal_connect(m_individualAggregator,
                                          "notify::is-quiescent",
                                          (GCallback)AddressBook::isQuiescentChanged,
                                          this);

    m_individualsChangedDetailedId = g_signal_connect(m_individualAggregator,
                                          "individuals-changed-detailed",
                                          (GCallback) AddressBook::individualsChangedCb,
                                          this);

    folks_individual_aggregator_prepare(m_individualAggregator,
                                        (GAsyncReadyCallback) AddressBook::prepareFolksDone,
                                        this);
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

Source AddressBook::createSource(const QString &sourceId, const QDBusMessage &message)
{
    CreateSourceData *data = new CreateSourceData;
    data->m_addressbook = this;
    data->m_message = message;
    data->sourceName = sourceId;

    FolksPersonaStore *store = folks_individual_aggregator_get_primary_store(m_individualAggregator);
    QString personaStoreTypeId  = QString::fromUtf8(folks_persona_store_get_type_id (store));
    if ( personaStoreTypeId == "dummy") {
        qDebug() << "Create source on dummy" << sourceId;
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
        src = Source(cData->sourceName, cData->sourceName, false, false);
    }
    QDBusMessage reply = cData->m_message.createReply(QVariant::fromValue<Source>(src));
    QDBusConnection::sessionBus().send(reply);
    delete cData;
}

void AddressBook::addGlobalAntilink(FolksPersona *persona, GAsyncReadyCallback antilinkReady, void *data)
{
    if (FOLKS_IS_ANTI_LINKABLE(persona)) {
        GeeSet *oldAntiLinks = folks_anti_linkable_get_anti_links(FOLKS_ANTI_LINKABLE(persona));
        if (!gee_collection_contains(GEE_COLLECTION(oldAntiLinks), "*")) {
            GeeHashSet *antiLinks = gee_hash_set_new(G_TYPE_STRING,
                                                     (GBoxedCopyFunc) g_strdup,
                                                     g_free,
                                                     NULL, NULL, NULL, NULL, NULL, NULL);
            gee_collection_add(GEE_COLLECTION(antiLinks), "*");
            folks_anti_linkable_change_anti_links(FOLKS_ANTI_LINKABLE(persona),
                                                  GEE_SET(antiLinks),
                                                  antilinkReady,
                                                  data);
            g_object_unref(antiLinks);
        }
    } else {
        g_object_unref(persona);
    }
}

void AddressBook::addGlobalAntilinkDone(FolksAntiLinkable *antilinkable, GAsyncResult *result, void *data)
{
    GError *error = 0;
    folks_anti_linkable_change_anti_links_finish(antilinkable, result, &error);
    if (error) {
        qWarning() << "Fail to anti link pesona" << folks_persona_get_display_id(FOLKS_PERSONA(antilinkable)) << error->message;
        g_error_free(error);
    }
    g_object_unref(antilinkable);
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

void AddressBook::addAntiLinksDone(FolksAntiLinkable *antilinkable,
                                   GAsyncResult *result,
                                   void *data)
{
    CreateContactData *createData = static_cast<CreateContactData*>(data);
    QDBusMessage reply;

    GError *error = 0;
    folks_anti_linkable_change_anti_links_finish(antilinkable, result, &error);
    if (error) {
        qWarning() << "Fail to anti link pesona" << folks_persona_get_display_id(FOLKS_PERSONA(antilinkable)) << error->message;
        reply = createData->m_message.createErrorReply("Fail to anti link pesona:", error->message);
        g_error_free(error);
    } else {
        qDebug() << "Send back contact: create OK";
        FolksIndividual *individual = folks_persona_get_individual(FOLKS_PERSONA(antilinkable));
        // return the result/contactId to the client
        reply = createData->m_message.createReply(QString::fromUtf8(folks_individual_get_id(individual)));
    }
    QDBusConnection::sessionBus().send(reply);
    delete createData;
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
    qDebug() << Q_FUNC_INFO;
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

    // add anti lik for any new contact, if the contact is already linked ignore it
    // because this could be manually linked before
    GeeSet *personas = folks_individual_get_personas(individual);
    int size = 0;
    FolksPersona **personasArray = (FolksPersona **) gee_collection_to_array(GEE_COLLECTION(personas), &size);
    if (gee_collection_get_size(GEE_COLLECTION(personas)) == 1) {
        FolksPersona **personasArray = (FolksPersona **) gee_collection_to_array(GEE_COLLECTION(personas), &size);
        addGlobalAntilink(personasArray[0],
                         (GAsyncReadyCallback) AddressBook::addGlobalAntilinkDone,
                          0);
    }
    g_free(personasArray);
    return id;
}

void AddressBook::individualsChangedCb(FolksIndividualAggregator *individualAggregator,
                                       GeeMultiMap *changes,
                                       AddressBook *self)
{
    qDebug() << Q_FUNC_INFO;
    Q_UNUSED(individualAggregator);

    QSet<QString> removedIds;
    QSet<QString> addedIds;
    QSet<QString> updatedIds;
    QSet<QString> ignoreIds;

    GeeSet *keys = gee_multi_map_get_keys(changes);
    GeeIterator *iter = gee_iterable_iterator(GEE_ITERABLE(keys));

    while(gee_iterator_next(iter)) {
        FolksIndividual *individualKey = FOLKS_INDIVIDUAL(gee_iterator_get(iter));
        GeeCollection *values = gee_multi_map_get(changes, individualKey);
        GeeIterator *iterV;

        iterV = gee_iterable_iterator(GEE_ITERABLE(values));
        while(gee_iterator_next(iterV)) {
            FolksIndividual *individualValue = FOLKS_INDIVIDUAL(gee_iterator_get(iterV));

            // contact added
            if (individualKey == 0) {
                addedIds << self->addContact(individualValue);
            } else if (individualValue != 0) {
                QString idValue = QString::fromUtf8(folks_individual_get_id(individualValue));
                QString idKey = QString::fromUtf8(folks_individual_get_id(individualKey));
                // after adding a anti link folks emit a signal with the same value in both key and value,
                // we can ignore this
                if (idValue == idKey) {
                    // individual object has changed
                    ignoreIds << self->addContact(individualValue);
                } else {
                    if (self->m_contacts->value(idValue)) {
                        updatedIds << self->addContact(individualValue);
                    } else {
                        addedIds << self->addContact(individualValue);
                    }
                }
            }

            if (individualValue) {
                g_object_unref(individualValue);
            }
        }

        g_object_unref(iterV);
        g_object_unref(values);

        if (individualKey) {
            QString id = QString::fromUtf8(folks_individual_get_id(individualKey));
            if (!ignoreIds.contains(id) &&
                !addedIds.contains(id) &&
                !updatedIds.contains(id)) {
                removedIds << self->removeContact(individualKey);
            }
            g_object_unref(individualKey);
        }
    }

    g_object_unref(keys);

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

} //namespace
