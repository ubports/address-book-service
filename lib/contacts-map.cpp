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

#include "contact-less-than.h"
#include "contacts-map.h"
#include "qindividual.h"

#include <QtCore/QDebug>

#include <QtContacts/QContactSortOrder>
#include <QtContacts/QContactDisplayLabel>
#include <QtContacts/QContactTag>
#include <QtContacts/QContactPhoneNumber>

#include <phonenumbers/phonenumberutil.h>
#include <phonenumbers/region_code.h>

using namespace QtContacts;

namespace galera
{

//ContactInfo
ContactEntry::ContactEntry(QIndividual *individual)
    : m_individual(individual)
{
    Q_ASSERT(individual);
}

ContactEntry::~ContactEntry()
{
    delete m_individual;
}

QIndividual *ContactEntry::individual() const
{
    return m_individual;
}

//ContactMap
ContactsMap::ContactsMap()
    : m_sortClause(defaultSort())
{
}

ContactsMap::~ContactsMap()
{
    clear();
}

ContactEntry *ContactsMap::value(const QString &id) const
{
    return m_idToEntry[id];
}

QSet<ContactEntry *> ContactsMap::valueByPhone(const QString &phone) const
{
    if (phone.isEmpty()) {
        return values().toSet();
    }

    return m_phoneToEntry.values(minimalNumber(phone)).toSet();
}

ContactEntry *ContactsMap::take(FolksIndividual *individual)
{
    QString contactId = QString::fromUtf8(folks_individual_get_id(individual));
    return take(contactId);
}

ContactEntry *ContactsMap::take(const QString &id)
{
    QWriteLocker locker(&m_mutex);
    ContactEntry *entry = m_idToEntry.take(id);
    removeData(entry, false);
    return entry;
}

void ContactsMap::remove(const QString &id)
{
    QWriteLocker locker(&m_mutex);
    ContactEntry *entry = m_idToEntry.take(id);
    removeData(entry, true);
}

void ContactsMap::insert(ContactEntry *entry)
{
    QWriteLocker locker(&m_mutex);
    insertData(entry);
}

void ContactsMap::updatePosition(ContactEntry *entry)
{
    QWriteLocker locker(&m_mutex);
    if (!m_sortClause.isEmpty()) {
        int oldPos = m_contacts.indexOf(entry);

        ContactEntryLessThan lessThan(m_sortClause);
        QList<ContactEntry*>::iterator it(std::upper_bound(m_contacts.begin(), m_contacts.end(), entry, lessThan));

        if (it != m_contacts.end()) {
            int newPos = std::distance(m_contacts.begin(), it);
            if (oldPos != newPos) {
                m_contacts.move(oldPos, newPos);
            }
        } else if (oldPos != (m_contacts.size() - 1)) {
            m_contacts.move(oldPos, m_contacts.size() -1);
        }
    }

    // update phone number map
    Q_FOREACH(const QString &key, m_phoneToEntry.keys(entry)) {
        m_phoneToEntry.remove(key, entry);
    }
    insertdata(entry->individual()->contact().details<QContactPhoneNumber>(), entry);
}

int ContactsMap::size() const
{
    return m_idToEntry.size();
}

void ContactsMap::clear()
{
    QWriteLocker locker(&m_mutex);
    QList<ContactEntry*> entries = m_idToEntry.values();
    m_idToEntry.clear();
    m_phoneToEntry.clear();
    m_contacts.clear();
    qDeleteAll(entries);
}

void ContactsMap::lockForRead()
{
    m_mutex.lockForRead();
}

void ContactsMap::unlock()
{
    m_mutex.unlock();
}

QList<ContactEntry*> ContactsMap::values() const
{
    return m_contacts;
}

QList<QContact> ContactsMap::contacts() const
{
    QList<QContact> result;
    Q_FOREACH(ContactEntry *e, m_contacts) {
        result << e->individual()->contact();
    }
    return result;
}

QStringList ContactsMap::keys() const
{
    return m_idToEntry.keys();
}

void ContactsMap::sertSort(const SortClause &clause)
{
    if (clause.toContactSortOrder() != m_sortClause.toContactSortOrder()) {
        m_sortClause = clause;
        if (!m_sortClause.isEmpty()) {
            ContactEntryLessThan lessThan(m_sortClause);
            qSort(m_contacts.begin(), m_contacts.end(), lessThan);
        }
    }
}

SortClause ContactsMap::sort() const
{
    return m_sortClause;
}

SortClause ContactsMap::defaultSort()
{
    static SortClause clause("");
    if (clause.isEmpty()) {
        // create a default sort, this sort is used by the most commom case
        QList<QContactSortOrder> cClauseList;

        QContactSortOrder cClauseTag;
        cClauseTag.setCaseSensitivity(Qt::CaseInsensitive);
        cClauseTag.setDetailType(QContactDetail::TypeTag, QContactTag::FieldTag);
        cClauseTag.setBlankPolicy(QContactSortOrder::BlanksLast);
        cClauseTag.setDirection(Qt::AscendingOrder);
        cClauseList << cClauseTag;

        QContactSortOrder cClauseName;
        cClauseName.setCaseSensitivity(Qt::CaseInsensitive);
        cClauseName.setDetailType(QContactDetail::TypeDisplayLabel, QContactDisplayLabel::FieldLabel);
        cClauseName.setBlankPolicy(QContactSortOrder::BlanksLast);
        cClauseName.setDirection(Qt::AscendingOrder);
        cClauseList << cClauseName;
        clause = SortClause(cClauseList);
    }

    return clause;
}

ContactEntry *ContactsMap::valueFromVCard(const QString &vcard) const
{
    //GET UID
    int startIndex = vcard.indexOf("UID:");
    if (startIndex) {
        startIndex += 4; // "UID:"
        int endIndex = vcard.indexOf("\r\n", startIndex);

        QString id = vcard.mid(startIndex, endIndex - startIndex);
        return m_idToEntry[id];
    }
    return 0;
}

bool ContactsMap::contains(FolksIndividual *individual) const
{
    QString contactId = QString::fromUtf8(folks_individual_get_id(individual));
    return contains(contactId);
}

bool ContactsMap::contains(const QString &id) const
{
    return m_idToEntry.contains(id);
}

ContactEntry *ContactsMap::value(FolksIndividual *individual) const
{
    QString contactId = QString::fromUtf8(folks_individual_get_id(individual));
    return m_idToEntry.value(contactId, 0);
}

void ContactsMap::removeData(ContactEntry *entry, bool del)
{
    if (entry) {
        Q_FOREACH(const QString &key,  m_phoneToEntry.keys(entry)) {
            m_phoneToEntry.remove(key, entry);
        }
        m_contacts.removeOne(entry);
        if (del) {
            delete entry;
        }
    }

}

void ContactsMap::insertData(ContactEntry *entry)
{
    FolksIndividual *fIndividual = entry->individual()->individual();


    if (fIndividual) {
        // fill id map
        m_idToEntry.insert(folks_individual_get_id(fIndividual), entry);

        // fill contact list
        if (!m_sortClause.isEmpty()) {
            ContactEntryLessThan lessThan(m_sortClause);
            QList<ContactEntry*>::iterator it(std::upper_bound(m_contacts.begin(), m_contacts.end(), entry, lessThan));
            m_contacts.insert(it, entry);
        } else {
            m_contacts.append(entry);
        }

        // fill phone map
        insertdata(entry->individual()->contact().details<QContactPhoneNumber>(), entry);
    }
}

void ContactsMap::insertdata(const QList<QContactPhoneNumber> &numbers, ContactEntry *entry)
{
    Q_FOREACH(const QContactPhoneNumber &phone, numbers) {
        QString mNumber = minimalNumber(phone.number());
        if (!mNumber.isEmpty()) {
            m_phoneToEntry.insert(mNumber, entry);
        }
    }
}

QString ContactsMap::minimalNumber(const QString &phone) const
{
    static i18n::phonenumbers::PhoneNumberUtil *phonenumberUtil = i18n::phonenumbers::PhoneNumberUtil::GetInstance();

    std::string stdPreprocessedPhone(phone.toStdString());
    phonenumberUtil->NormalizeDiallableCharsOnly(&stdPreprocessedPhone);

    if (stdPreprocessedPhone.length() <= 7) {
        return QString::fromStdString(stdPreprocessedPhone);
    }

    return QString::fromStdString(stdPreprocessedPhone).right(7);
}

} //namespace
