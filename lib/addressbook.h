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

#ifndef __GALERA_ADDRESSBOOK_H__
#define __GALERA_ADDRESSBOOK_H__

#include "common/source.h"

#include <QtCore/QObject>
#include <QtCore/QSet>
#include <QtCore/QString>
#include <QtCore/QStringList>

#include <QtDBus/QtDBus>

#include <QtContacts/QContact>

#include <folks/folks.h>
#include <glib.h>
#include <glib-object.h>

namespace galera
{
class View;
class ContactsMap;
class AddressBookAdaptor;
class QIndividual;
class DirtyContactsNotify;

class AddressBook: public QObject
{
    Q_OBJECT
public:
    AddressBook(QObject *parent=0);
    virtual ~AddressBook();

    static QString objectPath();
    bool start(QDBusConnection connection);

    // Adaptor
    QString linkContacts(const QStringList &contacts);
    View *query(const QString &clause, const QString &sort, const QStringList &sources);
    QStringList sortFields();
    bool unlinkContacts(const QString &parent, const QStringList &contacts);
    bool isReady() const;

    static int init();

Q_SIGNALS:
    void stopped();
    void ready();

public Q_SLOTS:
    bool start();
    void shutdown();
    SourceList availableSources(const QDBusMessage &message);
    Source source(const QDBusMessage &message);
    Source createSource(const QString &sourceId, const QDBusMessage &message);
    QString createContact(const QString &contact, const QString &source, const QDBusMessage &message = QDBusMessage());
    int removeContacts(const QStringList &contactIds, const QDBusMessage &message);
    QStringList updateContacts(const QStringList &contacts, const QDBusMessage &message);
    void updateContactsDone(const QString &contactId, const QString &error);

private Q_SLOTS:
    void viewClosed();
    void individualChanged(QIndividual *individual);

    // Unix signal handlers.
    void handleSigQuit();

private:
    FolksIndividualAggregator *m_individualAggregator;
    ContactsMap *m_contacts;
    QSet<View*> m_views;
    AddressBookAdaptor *m_adaptor;
    // timer to avoid send several updates at the same time
    DirtyContactsNotify *m_notifyContactUpdate;

    bool m_ready;
    int m_individualsChangedDetailedId;
    int m_notifyIsQuiescentHandlerId;
    QDBusConnection m_connection;

    // Update command
    QDBusMessage m_updateCommandReplyMessage;
    QStringList m_updateCommandResult;
    QStringList m_updatedIds;
    QStringList m_updateCommandPendingContacts;

    // Unix signals
    static int m_sigQuitFd[2];
    QSocketNotifier *m_snQuit;

    // dbus service name
    QString m_serviceName;


    // Disable copy contructor
    AddressBook(const AddressBook&);

    void getSource(const QDBusMessage &message, bool onlyTheDefault);

    void setupUnixSignals();

    // Unix signal handlers.
    void prepareUnixSignals();
    static void quitSignalHandler(int unused);

    void prepareFolks();
    bool registerObject(QDBusConnection &connection);
    QString removeContact(FolksIndividual *individual);
    QString addContact(FolksIndividual *individual);
    FolksPersonaStore *getFolksStore(const QString &source);

    static void availableSourcesDoneListAllSources(FolksBackendStore *backendStore,
                                                   GAsyncResult *res,
                                                   QDBusMessage *msg);
    static void availableSourcesDoneListDefaultSource(FolksBackendStore *backendStore,
                                                      GAsyncResult *res,
                                                      QDBusMessage *msg);
    static SourceList availableSourcesDoneImpl(FolksBackendStore *backendStore,
                                               GAsyncResult *res);
    static void individualsChangedCb(FolksIndividualAggregator *individualAggregator,
                                     GeeMultiMap *changes,
                                     AddressBook *self);
    static void isQuiescentChanged(GObject *source,
                                   GParamSpec *param,
                                   AddressBook *self);
    static void prepareFolksDone(GObject *source,
                                 GAsyncResult *res,
                                 AddressBook *self);
    static void createContactDone(FolksIndividualAggregator *individualAggregator,
                                  GAsyncResult *res,
                                  void *data);
    static void removeContactDone(FolksIndividualAggregator *individualAggregator,
                                  GAsyncResult *result,
                                  void *data);
    static void addAntiLinksDone(FolksAntiLinkable *antilinkable,
                                  GAsyncResult *result,
                                  void *data);
    static void createSourceDone(GObject *source,
                                 GAsyncResult *res,
                                 void *data);

    static void addGlobalAntilink(FolksPersona *persona,
                                  GAsyncReadyCallback antilinkReady,
                                  void *data);
    static void addGlobalAntilinkDone(FolksAntiLinkable *antilinkable,
                                      GAsyncResult *result,
                                      void *data);

    friend class DirtyContactsNotify;
};

} //namespace

#endif

