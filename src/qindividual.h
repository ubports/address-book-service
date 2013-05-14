#ifndef __GALERA_QINDIVIDUAL_H__
#define __GALERA_QINDIVIDUAL_H__

#include <QtCore/QString>
#include <QtCore/QList>
#include <QtCore/QMultiHash>
#include <QVersitProperty>

#include <QtContacts/QContact>
#include <QtContacts/QContactDetail>

#include <folks/folks.h>

namespace galera
{

typedef QList<QtVersit::QVersitProperty> PropertyList;
class QIndividual
{
public:
    enum Field {
        All = 0x0000,
        Name = 0x0001,
        FullName = 0x0002,
        NickName = 0x0004,
        Birthday = 0x0008,
        Photo = 0x0016,
        Role = 0x0032,
        Email = 0x0064,
        Phone = 0x0128,
        Address = 0x0256,
        Im = 0x512,
        TimeZone = 0x1024,
        Url = 0x2048
    };
    Q_DECLARE_FLAGS(Fields, Field)

    QIndividual(FolksIndividual *individual);
    ~QIndividual();    

    QtContacts::QContact &contact();
    QtContacts::QContact copy(Fields fields = QIndividual::All);
    bool update(const QString &vcard);
    bool update(const QtContacts::QContact &contact);

    static GHashTable *parseDetails(const QtContacts::QContact &contact);

private:
    FolksIndividual *m_individual;
    QtContacts::QContact m_contact;

    bool fieldsContains(Fields fields, Field value) const;
    QList<QtVersit::QVersitProperty> parseFieldList(const QString &fieldName, GeeSet *values) const;
    QMultiHash<QString, QString> parseDetails(FolksAbstractFieldDetails *details) const;
    void updateContact();


    // QContact
    QtContacts::QContactDetail getUid() const;
    QList<QtContacts::QContactDetail> getClientPidMap() const;
    QtContacts::QContactDetail getName() const;
    QtContacts::QContactDetail getNickname() const;
    QtContacts::QContactDetail getBirthday() const;
    QtContacts::QContactDetail getPhoto() const;
    QList<QtContacts::QContactDetail> getRoles() const;
    QList<QtContacts::QContactDetail> getEmails() const;
    QList<QtContacts::QContactDetail> getPhones() const;
    QList<QtContacts::QContactDetail> getAddresses() const;
    QList<QtContacts::QContactDetail> getIms() const;
    QtContacts::QContactDetail getTimeZone() const;
    QList<QtContacts::QContactDetail> getUrls() const;

    static void parseParameters(QtContacts::QContactDetail &detail, FolksAbstractFieldDetails *fd);
    static void parsePhoneParameters(QtContacts::QContactDetail &phone, const QStringList &parameters);
    static void parseAddressParameters(QtContacts::QContactDetail &address, const QStringList &parameters);
    static void parseOnlineAccountParameters(QtContacts::QContactDetail &im, const QStringList &parameters);

    static void parseContext(FolksAbstractFieldDetails *fd, const QtContacts::QContactDetail &detail);
    static QStringList parseContext(const QtContacts::QContactDetail &detail);
    static QStringList parsePhoneContext(const QtContacts::QContactDetail &detail);
    static QStringList parseAddressContext(const QtContacts::QContactDetail &detail);
    static QStringList parseOnlineAccountContext(const QtContacts::QContactDetail &detail);

    static QStringList listParameters(FolksAbstractFieldDetails *details);
    static QStringList listContext(const QtContacts::QContactDetail &detail);
    static QList<int> contextsFromParameters(QStringList &parameters);

    static int onlineAccountProtocolFromString(const QString &protocol);
    static QString onlineAccountProtocolFromEnum(int protocol);


    // glib callbacks
    static void folksIndividualAggregatorAddPersonaFromDetailsDone(GObject *source,
                                                                   GAsyncResult *res,
                                                                   QIndividual *individual);
    friend class QIndividualUtils;
};

} //namespace
#endif

