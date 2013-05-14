#ifndef __GALERA_QCONTACT_ENGINEID_H__
#define __GALERA_QCONTACT_ENGINEID_H__

#include <qcontactengineid.h>

namespace galera
{

class GaleraEngineId : public QtContacts::QContactEngineId
{
public:
    GaleraEngineId();
    ~GaleraEngineId();
    GaleraEngineId(const QString &contactId, const QString &managerUri);
    GaleraEngineId(const GaleraEngineId &other);
    GaleraEngineId(const QMap<QString, QString> &parameters, const QString &engineIdString);

    bool isEqualTo(const QtContacts::QContactEngineId *other) const;
    bool isLessThan(const QtContacts::QContactEngineId *other) const;

    QString managerUri() const;

    QContactEngineId* clone() const;

    QString toString() const;

#ifndef QT_NO_DEBUG_STREAM
    QDebug& debugStreamOut(QDebug &dbg) const;
#endif
    uint hash() const;

private:
    QString m_contactId;
    QString m_managerUri;
};

} //namespace

#endif
