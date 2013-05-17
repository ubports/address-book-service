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

#include "qcontact-engineid.h"

using namespace QtContacts;

namespace galera
{

GaleraEngineId::GaleraEngineId()
    : m_contactId("")
{
    //qDebug() << Q_FUNC_INFO;
}

GaleraEngineId::GaleraEngineId(const QString &contactId, const QString &managerUri)
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
    qDebug() << Q_FUNC_INFO << engineIdString;
    m_contactId = engineIdString;
    m_managerUri = QContactManager::buildUri("memory", parameters);
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
    return m_contactId;
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
    return qHash(m_contactId);
}

}
