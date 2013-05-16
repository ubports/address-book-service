/*
 * Copyright 2013 Canonical Ltd.
 *
 * This file is part of contact-service-app.
 *
 * ontact-service-app is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 3.
 *
 * webbrowser-app is distributed in the hope that it will be useful,
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

#include "vcard/vcard-parser.h"

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

}

namespace galera
{

AddressBook::AddressBook(QObject *parent)
    : QObject(parent),
      m_adaptor(0),
      m_contacts(new ContactsMap)

{
    prepareFolks();
}

AddressBook::~AddressBook()
{
    // destructor
}

QString AddressBook::objectPath()
{
    return "/com/canonical/galera/AddressBook";
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
    setDelayedReply(true);
    ContactEntry *entry = m_contacts->valueFromVCard(contact);
    if (entry) {
        qWarning() << "Contact exists";
        return "";
    } else {
        QContact qcontact = VCardParser::vcardToContact(contact);
        if (!qcontact.isEmpty()) {
            GHashTable *details = QIndividual::parseDetails(qcontact);
            //TOOD: lookup for source and use the correct store
            message.setDelayedReply(true);
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

bool AddressBook::removeContacts(const QStringList &contactIds)
{
    //TODO
    return false;
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
    if (!contacts.isEmpty()) {
        setDelayedReply(true);
        message.setDelayedReply(true);

        UpdateContactsData *data = new UpdateContactsData;
        data->m_contacts = VCardParser::vcardToContact(contacts);
        data->m_request = contacts;
        data->m_currentIndex = -1;
        data->m_addressbook = this;
        data->m_message = message;

        updateContacts("", data);
    }

    return QStringList();
}

void AddressBook::updateContacts(const QString &error, void *userData)
{
    qDebug() << Q_FUNC_INFO << userData;
    UpdateContactsData *data = static_cast<UpdateContactsData*>(userData);

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

    qDebug() << "Flush";
    folks_persona_store_flush(folks_individual_aggregator_get_primary_store(data->m_addressbook->m_individualAggregator), 0, 0);
    QDBusMessage reply = data->m_message.createReply(data->m_result);
    QDBusConnection::sessionBus().send(reply);
    delete data;
}


QString AddressBook::removeContact(FolksIndividual *individual)
{
    Q_ASSERT(m_contacts->contains(individual));
    ContactEntry *ci = m_contacts->take(individual);
    if (ci) {
        QString id = QString::fromUtf8(folks_individual_get_id(individual));
        //TODO: Notify view
        delete ci;
    }
    return QString();
}

QString AddressBook::addContact(FolksIndividual *individual)
{
    qDebug() << "Add contact" << folks_individual_get_id(individual);
    Q_ASSERT(!m_contacts->contains(individual));
    m_contacts->insert(individual, new ContactEntry(new QIndividual(individual, m_individualAggregator)));
    //TODO: Notify view
    return QString::fromUtf8(folks_individual_get_id(individual));
}

void AddressBook::individualsChangedCb(FolksIndividualAggregator *individualAggregator,
                                       GeeMultiMap *changes,
                                       AddressBook *self)
{
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

    if (!removedIds.isEmpty()) {
        Q_EMIT self->m_adaptor->contactsRemoved(removedIds);
    }

    if (!addedIds.isEmpty()) {
        Q_EMIT self->m_adaptor->contactsCreated(addedIds);
    }
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

} //namespace
