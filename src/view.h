#ifndef __GALERA_VIEW_H__
#define __GALERA_VIEW_H__

#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtDBus/QtDBus>

namespace galera
{
class ContactEntry;
class ViewAdaptor;
class ContactsMap;

class View : public QObject
{
     Q_OBJECT
public:
    View(QString clause, QString sort, QStringList sources, ContactsMap *allContacts, QObject *parent);
    ~View();

    static QString objectPath();
    QString dynamicObjectPath() const;
    QObject *adaptor() const;
    bool registerObject(QDBusConnection &connection);
    void unregisterObject(QDBusConnection &connection);

    // contacts
    bool appendContact(ContactEntry *entry);

    // Adaptor
    QString contactDetails(const QStringList &fields, const QString &id);
    QStringList contactsDetails(const QStringList &fields, int startIndex, int pageSize);
    int count();
    void sort(const QString &field);
    void close();

Q_SIGNALS:
    void closed();

private:
    QString m_clause;
    QString m_sort;
    QStringList m_sources;
    QList<ContactEntry*> m_contacts;
    ContactsMap *m_allContacts;
    ViewAdaptor *m_adaptor;

    void applyFilter();
    bool checkContact(ContactEntry *entry);
};

} //namespace

#endif

