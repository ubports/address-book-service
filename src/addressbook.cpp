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

#include "addressbook.h"
#include "addressbook-adaptor.h"
#include "view.h"
#include "contacts-map.h"
#include "qindividual.h"

#include "common/vcard-parser.h"

#include <QtCore/QPair>
#include <QtCore/QUuid>

#include <signal.h>
#include <sys/socket.h>

using namespace QtContacts;

namespace
{
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

}

namespace galera
{
//this timeout represents how long the server will wait for changes on the contact before notify the client
#define NOTIFY_CONTACTS_TIMEOUT 500

int AddressBook::m_sigQuitFd[2] = {0, 0};

// this is a helper class uses a timer with a small timeout to notify the client about
// any contact change notification. This class should be used instead of emit the signal directly
// this will avoid notify about the contact update several times when updating different fields simultaneously
// With that we can reduce the dbus traffic and skip some client calls to query about the new contact info.
class DirtyContactsNotify
{
public:
    DirtyContactsNotify(AddressBookAdaptor *adaptor)
    {
        m_timer.setInterval(NOTIFY_CONTACTS_TIMEOUT);
        m_timer.setSingleShot(true);
        QObject::connect(&m_timer, &QTimer::timeout,
                         [=]() {
                            Q_EMIT adaptor->contactsUpdated(m_ids);
                            m_ids.clear();
                         });
    }

    void append(QStringList ids)
    {
        Q_FOREACH(QString id, ids) {
            if (!m_ids.contains(id)) {
                m_ids << id;
            }
        }

        m_timer.start();
    }

private:
    QTimer m_timer;
    QStringList m_ids;
};

AddressBook::AddressBook(QObject *parent)
    : QObject(parent),
      m_contacts(new ContactsMap),
      m_ready(false),
      m_adaptor(0),
      m_notifyContactUpdate(0)
{
    prepareUnixSignals();
    prepareFolks();
}

AddressBook::~AddressBook()
{
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
        } else {
            qDebug() << "Object registered:" << objectPath();
        }
    }
    if (m_adaptor) {
        m_notifyContactUpdate = new DirtyContactsNotify(m_adaptor);
    }
    return (m_adaptor != 0);
}

void AddressBook::shutdown()
{
    m_ready = false;

    Q_FOREACH(View* view, m_views) {
        view->close();
    }

    if (m_contacts) {
        delete m_contacts;
        m_contacts = 0;
    }

    if (m_individualAggregator) {
        g_clear_object(&m_individualAggregator);
    }

    if (m_adaptor) {
        //FIXME: use the connection used in register funcion
        QDBusConnection connection = QDBusConnection::sessionBus();
        connection.unregisterObject(objectPath());
        delete m_adaptor;
        m_adaptor = 0;
    }

    Q_EMIT stopped();
}

void AddressBook::prepareFolks()
{
    //TODO: filter EDS (FolksBackendStore)
    m_individualAggregator = folks_individual_aggregator_new();
    g_object_get(G_OBJECT(m_individualAggregator), "is-quiescent", &m_ready, NULL);
    if (m_ready) {
        AddressBook::isQuiescentChanged(G_OBJECT(m_individualAggregator), NULL, this);
    }
    g_signal_connect(m_individualAggregator,
                     "notify::is-quiescent",
                     (GCallback)AddressBook::isQuiescentChanged,
                     this);

    g_signal_connect(m_individualAggregator,
                     "individuals-changed-detailed",
                     (GCallback) AddressBook::individualsChangedCb,
                     this);

    folks_individual_aggregator_prepare(m_individualAggregator,
                                        (GAsyncReadyCallback) AddressBook::prepareFolksDone,
                                        this);
}

SourceList AddressBook::availableSources(const QDBusMessage &message)
{
    FolksBackendStore *backendStore = folks_backend_store_dup();
    QDBusMessage *msg = new QDBusMessage(message);

    if (folks_backend_store_get_is_prepared(backendStore)) {
        availableSourcesDone(backendStore, 0, msg);
    } else {
        folks_backend_store_prepare(backendStore, (GAsyncReadyCallback) availableSourcesDone, msg);
    }
    return SourceList();
}

void AddressBook::availableSourcesDone(FolksBackendStore *backendStore, GAsyncResult *res, QDBusMessage *message)
{
    if (res) {
        folks_backend_store_prepare_finish(backendStore, res);
    }
    GeeCollection *backends = folks_backend_store_list_backends(backendStore);
    SourceList result;

    GeeIterator *iter = gee_iterable_iterator(GEE_ITERABLE(backends));
    while(gee_iterator_next(iter)) {
        FolksBackend *backend = FOLKS_BACKEND(gee_iterator_get(iter));

        GeeMap *stores = folks_backend_get_persona_stores(backend);
        GeeCollection *values =  gee_map_get_values(stores);
        GeeIterator *backendIter = gee_iterable_iterator(GEE_ITERABLE(values));

        while(gee_iterator_next(backendIter)) {
            FolksPersonaStore *store = FOLKS_PERSONA_STORE(gee_iterator_get(backendIter));

            QString id = QString::fromUtf8(folks_persona_store_get_id(store));
            bool canWrite = folks_persona_store_get_is_writeable(store);
            result << Source(id, !canWrite);

            g_object_unref(store);
        }

        g_object_unref(backendIter);
        g_object_unref(backend);
        g_object_unref(values);
    }
    g_object_unref(iter);

    QDBusMessage reply = message->createReply(QVariant::fromValue<SourceList>(result));
    QDBusConnection::sessionBus().send(reply);
    delete message;
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
            //TOOD: lookup for source and use the correct store
            QDBusMessage *cpy = new QDBusMessage(message);
            folks_individual_aggregator_add_persona_from_details(m_individualAggregator,
                                                                 NULL, //parent
                                                                 folks_individual_aggregator_get_primary_store(m_individualAggregator),
                                                                 details,
                                                                 (GAsyncReadyCallback) createContactDone,
                                                                 (void*) cpy);
            g_hash_table_destroy(details);
            return "";
        }
    }

    QDBusMessage reply = message.createReply(QString());
    QDBusConnection::sessionBus().send(reply);
    return "";
}

QString AddressBook::linkContacts(const QStringList &contacts)
{
    //TODO
    return "";
}

View *AddressBook::query(const QString &clause, const QString &sort, const QStringList &sources)
{
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
    m_notifyContactUpdate->append(QStringList() << individual->id());
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
            qDebug() << "contact removed";
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
            qWarning() << "ContactId not found:" << contactId;
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
    m_updateCommandPendingContacts << VCardParser::vcardToContact(contacts);

    updateContactsDone(0, QString());

    return QStringList();
}

void AddressBook::updateContactsDone(galera::QIndividual *individual, const QString &error)
{
    Q_UNUSED(individual);
    qDebug() << Q_FUNC_INFO;

    int currentContactIndex = m_updateCommandResult.size() - m_updateCommandPendingContacts.size() - 1;

    if (!error.isEmpty()) {
        // update the result with the error
        m_updateCommandResult[currentContactIndex] = error;
    } else if (individual){
        // update the result with the new contact info
        m_updatedIds << individual->id();
        QStringList newContacts = VCardParser::contactToVcard(QList<QContact>() << individual->contact());
        if (newContacts.length() == 1) {
            m_updateCommandResult[currentContactIndex] = newContacts[0];
        } else {
            m_updateCommandResult[currentContactIndex] = "";
        }
    }

    if (!m_updateCommandPendingContacts.isEmpty()) {
        QContact newContact = m_updateCommandPendingContacts.takeFirst();
        ContactEntry *entry = m_contacts->value(newContact.detail<QContactGuid>().guid());
        if (entry) {
            entry->individual()->update(newContact, this, SLOT(updateContactsDone(galera::QIndividual*,QString)));
        } else {
            updateContactsDone(0, "Contact not found!");
        }
    } else {
        folks_persona_store_flush(folks_individual_aggregator_get_primary_store(m_individualAggregator), 0, 0);
        QDBusMessage reply = m_updateCommandReplyMessage.createReply(m_updateCommandResult);
        QDBusConnection::sessionBus().send(reply);

        // notify about the changes
        m_notifyContactUpdate->append(m_updatedIds);

        // clear command data
        m_updatedIds.clear();
        m_updateCommandResult.clear();
        m_updateCommandReplyMessage = QDBusMessage();
    }
}


QString AddressBook::removeContact(FolksIndividual *individual)
{
    if (m_contacts->contains(individual)) {
        ContactEntry *ci = m_contacts->take(individual);
        if (ci) {
            QString id = QString::fromUtf8(folks_individual_get_id(individual));
            qDebug() << "Remove contact" << id;
            delete ci;
            return id;
        }
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
        Q_ASSERT(!m_contacts->contains(individual));
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
    qDebug() << Q_FUNC_INFO;
    QStringList removedIds;
    QStringList addedIds;

    GeeIterator *iter;
    GeeSet *removed = gee_multi_map_get_keys(changes);
    GeeCollection *added = gee_multi_map_get_values(changes);

    iter = gee_iterable_iterator(GEE_ITERABLE(added));
    while(gee_iterator_next(iter)) {
        FolksIndividual *individual = FOLKS_INDIVIDUAL(gee_iterator_get(iter));
        if (individual) {
            // add contact to the map
            addedIds << self->addContact(individual);
            g_object_unref(individual);
        }
    }
    g_object_unref (iter);


    iter = gee_iterable_iterator(GEE_ITERABLE(removed));
    while(gee_iterator_next(iter)) {
        FolksIndividual *individual = FOLKS_INDIVIDUAL(gee_iterator_get(iter));
        if (individual) {
            QString id = QString::fromUtf8(folks_individual_get_id(individual));
            if (!addedIds.contains(id)) {
                // delete from contact map
                removedIds << self->removeContact(individual);
            }
            g_object_unref(individual);
        }
    }
    g_object_unref (iter);

    //TODO: check for linked and unliked contacts
    if (!removedIds.isEmpty() && self->m_ready) {
        Q_EMIT self->m_adaptor->contactsRemoved(removedIds);
    }

    if (!addedIds.isEmpty() && self->m_ready) {
        Q_EMIT self->m_adaptor->contactsAdded(addedIds);
    }

    g_object_unref(added);
    g_object_unref(removed);
    qDebug() << "Added" << addedIds;
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
                                    QDBusMessage *msg)
{
    qDebug() << "Create Contact Done" << msg;
    FolksPersona *persona;
    GError *error = NULL;
    QDBusMessage reply;
    persona = folks_individual_aggregator_add_persona_from_details_finish(individualAggregator, res, &error);
    if (error != NULL) {
        qWarning() << "Failed to create individual from contact:" << error->message;
        reply = msg->createErrorReply("Failed to create individual from contact", error->message);
        g_clear_error(&error);
    } else if (persona == NULL) {
        qWarning() << "Failed to create individual from contact: Persona already exists";
        reply = msg->createErrorReply("Failed to create individual from contact", "Contact already exists");
    } else {
        qDebug() << "Persona created:" << persona;
        FolksIndividual *individual;
        individual = folks_persona_get_individual(persona);
        reply = msg->createReply(QString::fromUtf8(folks_individual_get_id(individual)));
    }
    //TODO: use dbus connection
    QDBusConnection::sessionBus().send(reply);
    delete msg;
}

void AddressBook::isQuiescentChanged(GObject *source, GParamSpec *param, AddressBook *self)
{
    Q_UNUSED(source);
    Q_UNUSED(param);

    g_object_get(source, "is-quiescent", &self->m_ready, NULL);
    if (self->m_ready) {
        Q_EMIT self->m_adaptor->ready();
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
