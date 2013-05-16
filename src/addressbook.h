#ifndef __GALERA_ADDRESSBOOK_H__
#define __GALERA_ADDRESSBOOK_H__

#include "source.h"

#include <QtCore/QObject>
#include <QtCore/QSet>
#include <QtCore/QString>
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
    bool removeContacts(const QStringList &contactIds);
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
};

} //namespace

#endif

