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


#include <QObject>
#include <QtTest>
#include <QDebug>

#include <QtContacts>

#include <glib.h>
#include <gio/gio.h>

#include "src/contacts-map.h"
#include "src/qindividual.h"

using namespace QtContacts;
using namespace galera;

class ContactMapTest : public QObject
{
    Q_OBJECT

private:
    FolksIndividualAggregator *m_individualAggregator;
    QMutex m_mutex;
    ContactsMap m_map;
    QList<FolksIndividual*> m_individuals;

    static void folksIndividualAggregatorPrepareDone(FolksIndividualAggregator *aggregator,
                                                     GAsyncResult *res,
                                                     ContactMapTest *self)
    {

        GError *error = NULL;
        folks_individual_aggregator_prepare_finish(aggregator, res, &error);
        QVERIFY(error == NULL);
    }

    static void folksIndividualAggregatorIndividualsChangedDetailed(FolksIndividualAggregator *individualAggregator,
                                                                    GeeMultiMap *changes,
                                                                    ContactMapTest *self)
    {
        GeeIterator *iter;
        GeeSet *removed = gee_multi_map_get_keys(changes);
        iter = gee_iterable_iterator(GEE_ITERABLE(removed));

        GeeCollection *added = gee_multi_map_get_values(changes);
        iter = gee_iterable_iterator(GEE_ITERABLE(added));
        while(gee_iterator_next(iter)) {
            FolksIndividual *individual = FOLKS_INDIVIDUAL(gee_iterator_get(iter));
            if (individual) {
                self->m_map.insert(new ContactEntry(new QIndividual(individual, individualAggregator)));
                self->m_individuals << individual;
                g_object_unref(individual);
            }
        }
        g_object_unref (iter);
        self->m_mutex.unlock();
    }

    int randomIndex() const
    {
        return qrand() % m_map.size();
    }

    FolksIndividual *randomIndividual() const
    {
        return m_individuals[randomIndex()];
    }

private Q_SLOTS:
    void initTestCase()
    {
        m_mutex.lock();
        m_individualAggregator = folks_individual_aggregator_new();

        g_signal_connect(m_individualAggregator,
                         "individuals-changed-detailed",
                         (GCallback) ContactMapTest::folksIndividualAggregatorIndividualsChangedDetailed,
                         this);

        folks_individual_aggregator_prepare(m_individualAggregator,
                                            (GAsyncReadyCallback) ContactMapTest::folksIndividualAggregatorPrepareDone,
                                            this);
        while (!m_mutex.tryLock(100)) {
            QCoreApplication::processEvents();
        }
        m_mutex.unlock();
    }

    void cleanupTestCase()
    {
        g_object_unref(m_individualAggregator);
    }

    void testLookupByFolksIndividual()
    {
        QVERIFY(m_map.size() > 0);
        FolksIndividual *fIndividual = randomIndividual();

        QVERIFY(m_map.contains(fIndividual));

        ContactEntry *entry = m_map.value(fIndividual);
        QVERIFY(entry->individual()->individual() == fIndividual);
    }

    void testLookupByFolksIndividualId()
    {
        FolksIndividual *fIndividual = randomIndividual();
        QString id = QString::fromUtf8(folks_individual_get_id(fIndividual));
        ContactEntry *entry = m_map.value(id);
        QVERIFY(entry->individual()->individual() == fIndividual);
    }

    void testValues()
    {
        QList<ContactEntry*> entries = m_map.values();
        QCOMPARE(entries.size(), m_map.size());
        QCOMPARE(m_individuals.size(), m_map.size());

        Q_FOREACH(FolksIndividual *individual, m_individuals) {
            QVERIFY(m_map.contains(individual));
        }
    }

    void testTakeIndividual()
    {
        FolksIndividual *individual = folks_individual_new(0);

        QVERIFY(m_map.take(individual) == 0);
        QVERIFY(!m_map.contains(individual));

        g_object_unref(individual);

        individual = randomIndividual();
        ContactEntry *entry = m_map.take(individual);
        QVERIFY(entry->individual()->individual() == individual);

        //put it back
        m_map.insert(entry);
    }

    void testLookupByVcard()
    {
        FolksIndividual *individual = randomIndividual();
        QString id = QString::fromUtf8(folks_individual_get_id(individual));
        QString vcard = QString("BEGIN:VCARD\r\n"
                "VERSION:3.0\r\n"
                "UID:%1\r\n"
                "N:Gump;Forrest\r\n"
                "FN:Forrest Gump\r\n"
                "REV:2008-04-24T19:52:43Z\r\n").arg(id);
        ContactEntry *entry = m_map.valueFromVCard(vcard);
        QVERIFY(entry);
        QVERIFY(entry->individual()->individual() == individual);
    }
};

QTEST_MAIN(ContactMapTest)

#include "contactmap-test.moc"
