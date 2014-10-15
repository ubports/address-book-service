/*
 * Copyright (C) 2012 Digia Plc and/or its subsidiary(-ies).
 * Copyright (C) 2013 Canonical Ltd.
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
 *
 * Based on QContactManagerEngine code by Digia
 */

#include "contact-less-than.h"
#include "contacts-map.h"
#include "qindividual.h"

#include <QtCore/QTime>
#include <QtCore/QDebug>

#include <QtContacts/QContactManagerEngine>
#include <QtContacts/QContactDisplayLabel>
#include <QtContacts/QContactTag>

using namespace QtContacts;

namespace galera {

ContactLessThan::ContactLessThan(const galera::SortClause &sortClause)
    : m_sortClause(sortClause)
{
}

bool ContactLessThan::operator()(ContactEntry *entryA, ContactEntry *entryB)
{
    qDebug() << "Compare" << m_sortClause.toContactSortOrder();
    int r = QContactManagerEngine::compareContact(entryA->individual()->contact(),
                                                 entryB->individual()->contact(),
                                                 m_sortClause.toContactSortOrder());
    qDebug() << "A:" << entryA->individual()->contact().detail<QContactDisplayLabel>().label()
                     << "\t" << entryA->individual()->contact().detail<QContactTag>().tag();
    qDebug() << "B:" << entryB->individual()->contact().detail<QContactDisplayLabel>().label()
                     << "\t" << entryB->individual()->contact().detail<QContactTag>().tag();
    qDebug() << "A < B" << r;
    return (r <= 0);
}

} // namespace
