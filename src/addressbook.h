#ifndef __GALERA_ADDRESSBOOK_H__
#define __GALERA_ADDRESSBOOK_H__

#include "source.h"

#include <QtCore/QObject>
#include <QtCore/QSet>
#include <QtCore/QString>
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

#include <QtCore/QStringList>

#include <QtDBus/QtDBus>

#include <folks/folks.h>
#include <glib.h>
#include <glib-object.h>

namespace galera
{
class View;
class ContactsMap;
class AddressBookAdaptor;

class AddressBook: public QObject, protected QDBusContext
{
    Q_OBJECT
public:
    AddressBook(QObject *parent=0);
    virtual ~AddressBook();

    static QString objectPath();
    bool registerObject(QDBusConnection &connection);

    // Adaptor
    SourceList availableSources();
    QString createContact(const QString &contact, const QString &source, const QDBusMessage &message);
    QString linkContacts(const QStringList &contacts);
    View *query(const QString &clause, const QString &sort, const QStringList &sources);
    int removeContacts(const QStringList &contactIds, const QDBusMessage &message);
    QStringList sortFields();
    bool unlinkContacts(const QString &parent, const QStringList &contacts);
    QStringList updateContacts(const QStringList &contacts, const QDBusMessage &message);

private Q_SLOTS:
    void viewClosed();

private:
    FolksIndividualAggregator *m_individualAggregator;
    ContactsMap *m_contacts;
    AddressBookAdaptor *m_adaptor;
    QSet<View*> m_views;

    void prepareFolks();
    QString removeContact(FolksIndividual *individual);
    QString addContact(FolksIndividual *individual);


    static void updateContacts(const QString &error, void *userData);

    static void individualsChangedCb(FolksIndividualAggregator *individualAggregator,
                                     GeeMultiMap *changes,
                                     AddressBook *self);
    static void aggregatorPrepareCb(GObject *source,
                                    GAsyncResult *res,
                                    AddressBook *self);
    static void createContactDone(FolksIndividualAggregator *individualAggregator,
                                  GAsyncResult *res,
                                  QDBusMessage *msg);
    static void removeContactContinue(FolksIndividualAggregator *individualAggregator,
                                      GAsyncResult *result,
                                      void *data);
};

} //namespace

#endif

