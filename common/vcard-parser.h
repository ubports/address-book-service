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

#ifndef __GALERA_VCARD_PARSER_H__
#define __GALERA_VCARD_PARSER_H__

#include <QtCore/QObject>
#include <QtCore/QStringList>
#include <QtCore/QList>

#include <QtContacts/QtContacts>

#include <QtVersit/QVersitWriter>
#include <QtVersit/QVersitReader>

namespace galera
{

class VCardParser : public QObject
{
    Q_OBJECT
public:
    VCardParser(QObject *parent=0);
    ~VCardParser();

    void contactToVcard(QList<QtContacts::QContact> contacts);
    void vcardToContact(const QStringList &vcardList);

    static const QString PidMapFieldName;
    static const QString PidFieldName;
    static const QString PrefParamName;
    static const QString IrremovableFieldName;
    static const QString ReadOnlyFieldName;
    static const QMap<QtContacts::QContactDetail::DetailType, QString> PreferredActionNames;

    static QtContacts::QContact vcardToContact(const QString &vcard);
    static QList<QtContacts::QContact> vcardToContactSync(const QStringList &vcardList);
    static QString contactToVcard(const QtContacts::QContact &contact);
    static QStringList contactToVcardSync(QList<QtContacts::QContact> contacts);

    static QStringList splitVcards(const QByteArray &vcardList);

Q_SIGNALS:
    void vcardParsed(QStringList vcards);
    void contactsParsed(QList<QtContacts::QContact> contacts);
    void finished();

private Q_SLOTS:
    void onWriterStateChanged(QtVersit::QVersitWriter::State state);
    void onReaderStateChanged(QtVersit::QVersitReader::State state);
    void onReaderResultsAvailable();

private:
    QtVersit::QVersitWriter *m_versitWriter;
    QtVersit::QVersitReader *m_versitReader;

    QByteArray m_vcardData;
};

}

Q_DECLARE_METATYPE(QList<QtContacts::QContact>)

#endif
