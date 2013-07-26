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

#include "source.h"

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

class AddressBook: public QObject
{
    Q_OBJECT
public:
    AddressBook(QObject *parent=0);
    virtual ~AddressBook();

    static QString objectPath();
    bool registerObject(QDBusConnection &connection);
    void shutdown();

    // Adaptor
    QString linkContacts(const QStringList &contacts);
    View *query(const QString &clause, const QString &sort, const QStringList &sources);
    QStringList sortFields();
    bool unlinkContacts(const QString &parent, const QStringList &contacts);
    bool isReady() const;

    static int init();

Q_SIGNALS:
    void stopped();

public Q_SLOTS:
    SourceList availableSources(const QDBusMessage &message);
    QString createContact(const QString &contact, const QString &source, const QDBusMessage &message);
    int removeContacts(const QStringList &contactIds, const QDBusMessage &message);
    QStringList updateContacts(const QStringList &contacts, const QDBusMessage &message);
    void updateContactsDone(galera::QIndividual *individual, const QString &error);

private Q_SLOTS:
    void viewClosed();
    void individualChanged(QIndividual *individual);

    // Unix signal handlers.
    void handleSigQuit();

private:
    FolksIndividualAggregator *m_individualAggregator;
    ContactsMap *m_contacts;
    QSet<View*> m_views;
    bool m_ready;
    AddressBookAdaptor *m_adaptor;

    // Update command
    QDBusMessage m_updateCommandReplyMessage;
    QStringList m_updateCommandResult;
    QStringList m_updatedIds;
    QList<QtContacts::QContact> m_updateCommandPendingContacts;

    // Unix signals
    static int m_sigQuitFd[2];

    QSocketNotifier *m_snQuit;
    void setupUnixSignals();

    // Unix signal handlers.
    void prepareUnixSignals();
    static void quitSignalHandler(int unused);

    void prepareFolks();
    QString removeContact(FolksIndividual *individual);
    QString addContact(FolksIndividual *individual);

    static void individualsChangedCb(FolksIndividualAggregator *individualAggregator,
                                     GeeMultiMap *changes,
                                     AddressBook *self);
    static void isQuiescentChanged(GObject *source,
                                   GParamSpec *param,
                                   AddressBook *self);
    static void availableSourcesDone(FolksBackendStore *backendStore,
                                     GAsyncResult *res,
                                     QDBusMessage *message);
    static void prepareFolksDone(GObject *source,
                                 GAsyncResult *res,
                                 AddressBook *self);
    static void createContactDone(FolksIndividualAggregator *individualAggregator,
                                  GAsyncResult *res,
                                  QDBusMessage *msg);
    static void removeContactDone(FolksIndividualAggregator *individualAggregator,
                                  GAsyncResult *result,
                                  void *data);
};

} //namespace

#endif

