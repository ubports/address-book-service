#include "qcontact-engineid.h"

using namespace QtContacts;

namespace galera
{

GaleraEngineId::GaleraEngineId()
    : m_contactId(0)
{
    //qDebug() << Q_FUNC_INFO;
}

GaleraEngineId::GaleraEngineId(quint32 contactId, const QString &managerUri)
    : m_contactId(contactId),
      m_managerUri(managerUri)
{
    //qDebug() << Q_FUNC_INFO;
}

GaleraEngineId::~GaleraEngineId()
{
    //qDebug() << Q_FUNC_INFO;
}

GaleraEngineId::GaleraEngineId(const GaleraEngineId &other)
    : m_contactId(other.m_contactId),
      m_managerUri(other.m_managerUri)
{
    //qDebug() << Q_FUNC_INFO;
}

GaleraEngineId::GaleraEngineId(const QMap<QString, QString> &parameters, const QString &engineIdString)
{
    //qDebug() << Q_FUNC_INFO;
    m_contactId = engineIdString.toInt();
    m_managerUri = QContactManager::buildUri("galera", parameters);
}

bool GaleraEngineId::isEqualTo(const QtContacts::QContactEngineId *other) const
{
    //qDebug() << Q_FUNC_INFO;
    if (m_contactId != static_cast<const GaleraEngineId*>(other)->m_contactId)
        return false;
    return true;
}

bool GaleraEngineId::isLessThan(const QtContacts::QContactEngineId *other) const
{
    //qDebug() << Q_FUNC_INFO;
    const GaleraEngineId *otherPtr = static_cast<const GaleraEngineId*>(other);
    if (m_managerUri < otherPtr->m_managerUri)
        return true;
    if (m_contactId < otherPtr->m_contactId)
        return true;
    return false;
}

QString GaleraEngineId::managerUri() const
{
    //qDebug() << Q_FUNC_INFO;
    return m_managerUri;
}

QString GaleraEngineId::toString() const
{
    //qDebug() << Q_FUNC_INFO;
    return QString::number(m_contactId);
}

QtContacts::QContactEngineId* GaleraEngineId::clone() const
{
    //qDebug() << Q_FUNC_INFO;
    return new GaleraEngineId(m_contactId, m_managerUri);
}

#ifndef QT_NO_DEBUG_STREAM
QDebug& GaleraEngineId::debugStreamOut(QDebug &dbg) const
{
    dbg.nospace() << "EngineId(" << m_managerUri << "," << m_contactId << ")";
    return dbg.maybeSpace();
}
#endif

uint GaleraEngineId::hash() const
{
    //qDebug() << Q_FUNC_INFO;
    return m_contactId;
}

}
