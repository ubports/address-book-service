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
#include <QtContacts/QContact>

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

AddressBook::AddressBook(QObject *parent)
    : QObject(parent),
      m_contacts(new ContactsMap),
      m_ready(false),
      m_adaptor(0)
{
    prepareFolks();
}

AddressBook::~AddressBook()
{
    // destructor
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
        } else {
            qDebug() << "Object registered:" << objectPath();
        }
    }

    return (m_adaptor != 0);
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
                                        (GAsyncReadyCallback) AddressBook::aggregatorPrepareCb,
                                        this);
}

SourceList AddressBook::availableSources()
{
    //TODO
    QList<Source> sources;
    sources << Source("Facebook", false) << Source("Telepathy", true) << Source("Google", true);
    return sources;
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

int AddressBook::removeContacts(const QStringList &contactIds, const QDBusMessage &message)
{
    RemoveContactsData *data = new RemoveContactsData;
    data->m_addressbook = this;
    data->m_message = message;
    data->m_request = contactIds;
    data->m_sucessCount = 0;
    removeContactContinue(0, 0, data);
    return 0;
}

void AddressBook::removeContactContinue(FolksIndividualAggregator *individualAggregator,
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
                                                          (GAsyncReadyCallback) removeContactContinue,
                                                          data);
        } else {
            qWarning() << "ContactId not found:" << contactId;
            removeContactContinue(individualAggregator, 0, data);
        }
    } else {
        QDBusMessage reply = removeData->m_message.createReply(removeData->m_sucessCount);
        QDBusConnection::sessionBus().send(reply);
        delete removeData;
    }
}

QStringList AddressBook::sortFields()
{
    //TODO
    QStringList fields;
    fields << "id" << "name" << "full-name";
    return fields;
}

bool AddressBook::unlinkContacts(const QString &parent, const QStringList &contacts)
{
    //TODO
    return false;
}

QStringList AddressBook::updateContacts(const QStringList &contacts, const QDBusMessage &message)
{
    UpdateContactsData *data = 0;

    if (!contacts.isEmpty()) {
        data = new UpdateContactsData;
        data->m_contacts = VCardParser::vcardToContact(contacts);
        data->m_request = contacts;
        data->m_currentIndex = -1;
        data->m_addressbook = this;
        data->m_message = message;

    }
    updateContacts("", data);

    return QStringList();
}

void AddressBook::updateContacts(const QString &error, void *userData)
{
    qDebug() << Q_FUNC_INFO << userData;
    UpdateContactsData *data = static_cast<UpdateContactsData*>(userData);
    QDBusMessage reply;

    if (data) {
        if (!error.isEmpty()) {
            data->m_result << error;
        } else if (data->m_currentIndex > -1) {
            data->m_result << data->m_request[data->m_currentIndex];
        }

        if (!data->m_contacts.isEmpty()) {
            QContact newContact = data->m_contacts.takeFirst();
            data->m_currentIndex++;

            ContactEntry *entry = data->m_addressbook->m_contacts->value(newContact.detail<QContactGuid>().guid());
            if (entry) {
                entry->individual()->update(newContact, updateContacts, userData);
            } else {
                updateContacts("Contact not found!", userData);
            }
            return;
        }
        folks_persona_store_flush(folks_individual_aggregator_get_primary_store(data->m_addressbook->m_individualAggregator), 0, 0);
        reply = data->m_message.createReply(data->m_result);
    } else {
        reply = data->m_message.createReply(QStringList());
    }

    QDBusConnection::sessionBus().send(reply);
    delete data;
}


QString AddressBook::removeContact(FolksIndividual *individual)
{
    Q_ASSERT(m_contacts->contains(individual));
    ContactEntry *ci = m_contacts->take(individual);
    if (ci) {
        QString id = QString::fromUtf8(folks_individual_get_id(individual));
        delete ci;
        return id;
    }
    return QString();
}

QString AddressBook::addContact(FolksIndividual *individual)
{
    Q_ASSERT(!m_contacts->contains(individual));
    m_contacts->insert(new ContactEntry(new QIndividual(individual, m_individualAggregator)));
    //TODO: Notify view
    return QString::fromUtf8(folks_individual_get_id(individual));
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
    iter = gee_iterable_iterator(GEE_ITERABLE(removed));

    while(gee_iterator_next(iter)) {
        FolksIndividual *individual = FOLKS_INDIVIDUAL(gee_iterator_get(iter));
        if (individual) {
            QString id = self->removeContact(individual);
            if(!id.isEmpty()) {
                removedIds << id;
            }
            g_object_unref(individual);
        }
    }
    g_object_unref (iter);

    GeeCollection *added = gee_multi_map_get_values(changes);
    iter = gee_iterable_iterator(GEE_ITERABLE(added));
    while(gee_iterator_next(iter)) {
        FolksIndividual *individual = FOLKS_INDIVIDUAL(gee_iterator_get(iter));
        if (individual) {
            QString id = self->addContact(individual);
            if(!id.isEmpty()) {
                addedIds << id;
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
    qDebug() << "Added" << addedIds;
}

void AddressBook::aggregatorPrepareCb(GObject *source,
                                      GAsyncResult *res,
                                      AddressBook *self)
{
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


} //namespace
