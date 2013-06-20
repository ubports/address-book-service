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

#include "vcard-parser.h"

#include <QtVersit/QVersitDocument>
#include <QtVersit/QVersitWriter>
#include <QtVersit/QVersitReader>
#include <QtVersit/QVersitContactExporter>
#include <QtVersit/QVersitContactImporter>
#include <QtVersit/QVersitProperty>

#include <QtContacts/QContactDetail>
#include <QtContacts/QContactExtendedDetail>

using namespace QtVersit;
using namespace QtContacts;

namespace
{
    class ContactExporterDetailHandler : public QVersitContactExporterDetailHandlerV2
    {
    public:
        virtual ~ContactExporterDetailHandler()
        {
            //nothing
        }

        virtual void detailProcessed(const QContact& contact,
                                     const QContactDetail& detail,
                                     const QVersitDocument& document,
                                     QSet<int>* processedFields,
                                     QList<QVersitProperty>* toBeRemoved,
                                     QList<QVersitProperty>* toBeAdded)
        {
            Q_UNUSED(contact);
            Q_UNUSED(document);
            Q_UNUSED(processedFields);
            Q_UNUSED(toBeRemoved);

            if (detail.type() == QContactDetail::TypeExtendedDetail) {
                const QContactExtendedDetail *extendedDetail = static_cast<const QContactExtendedDetail *>(&detail);
                if (extendedDetail->name() == galera::VCardParser::PidMapFieldName) {
                    QVersitProperty prop;
                    prop.setName(extendedDetail->name());
                    QStringList value;
                    value << extendedDetail->value(QContactExtendedDetail::FieldData).toString()
                          << extendedDetail->value(QContactExtendedDetail::FieldData + 1).toString();
                    prop.setValueType(QVersitProperty::CompoundType);
                    prop.setValue(value);
                    *toBeAdded << prop;

                    //TODO remove original property
                }
            }

            if (!detail.detailUri().isEmpty()) {
                if (toBeAdded->size() > 0) {
                    QVersitProperty &prop = toBeAdded->last();
                    QMultiHash<QString, QString> params = prop.parameters();
                    params.insert(galera::VCardParser::PidFieldName, detail.detailUri());
                    prop.setParameters(params);
                }
            }
        }

        virtual void contactProcessed(const QContact& contact, QVersitDocument* document)
        {
            //nothing
        }
    };


    class  ContactImporterPropertyHandler : public QVersitContactImporterPropertyHandlerV2
    {
    public:
        virtual ~ContactImporterPropertyHandler()
        {
            //nothing
        }

        virtual void propertyProcessed(const QVersitDocument& document,
                                       const QVersitProperty& property,
                                       const QContact& contact,
                                       bool *alreadyProcessed,
                                       QList<QContactDetail>* updatedDetails)
        {
            Q_UNUSED(document);
            Q_UNUSED(contact);

            if (!*alreadyProcessed && (property.name() == galera::VCardParser::PidMapFieldName)) {
                QContactExtendedDetail detail;
                detail.setName(property.name());
                QStringList value = property.value<QString>().split(";");
                detail.setValue(QContactExtendedDetail::FieldData, value[0]);
                detail.setValue(QContactExtendedDetail::FieldData + 1, value[1]);
                *updatedDetails  << detail;
                *alreadyProcessed = true;
            }

            QString pid = property.parameters().value(galera::VCardParser::PidFieldName);
            if (!pid.isEmpty()) {
                QContactDetail &det = updatedDetails->last();
                det.setDetailUri(pid);
            }

            // Remove empty phone and address subtypes
            // Remove this after this fix get merged: https://codereview.qt-project.org/#change,59156
            if (updatedDetails->size() > 0) {
                QContactDetail &det = updatedDetails->last();
                switch (det.type()) {
                    case QContactDetail::TypePhoneNumber:
                    {
                        QContactPhoneNumber phone = static_cast<QContactPhoneNumber>(det);
                        if (phone.subTypes().size() == 0) {
                            det.setValue(QContactPhoneNumber::FieldSubTypes, QVariant());
                        }
                        break;
                    }
                    case QContactDetail::TypeAddress:
                    {
                        QContactAddress addr = static_cast<QContactAddress>(det);
                        if (addr.subTypes().size() == 0) {
                            det.setValue(QContactAddress::FieldSubTypes, QVariant());
                        }
                        break;
                    }
                    default:
                        break;
                }
            }
        }

        virtual void documentProcessed(const QVersitDocument& document, QContact* contact)
        {
            //nothing
        }
    };
}


namespace galera
{

const QString VCardParser::PidMapFieldName = QString("CLIENTPIDMAP");
const QString VCardParser::PidFieldName = QString("PID");

QStringList VCardParser::contactToVcard(QList<QtContacts::QContact> contacts)
{
    QStringList result;
    QVersitContactExporter contactExporter;
    contactExporter.setDetailHandler(new ContactExporterDetailHandler);
    if (!contactExporter.exportContacts(contacts, QVersitDocument::VCard30Type)) {
        qDebug() << "Fail to export contacts" << contactExporter.errors();
        return result;
    }

    QByteArray vcard;
    Q_FOREACH(QVersitDocument doc, contactExporter.documents()) {
        vcard.clear();
        QVersitWriter writer(&vcard);
        if (!writer.startWriting(doc)) {
            qWarning() << "Fail to write contacts" << doc;
        } else {
            writer.waitForFinished();
            result << QString::fromUtf8(vcard);
        }
    }

    // TODO: check if is possible to write all contacts and split the result in a stringlist
    return result;
}


QtContacts::QContact VCardParser::vcardToContact(const QString &vcard)
{
    QVersitReader reader(vcard.toUtf8());
    reader.startReading();
    reader.waitForFinished();

    QVersitContactImporter importer;
    importer.importDocuments(reader.results());
    if (importer.errorMap().size() > 0) {
        qWarning() << importer.errorMap();
        return QContact();
    } else {
        return importer.contacts()[0];
    }
}

QList<QtContacts::QContact> VCardParser::vcardToContact(const QStringList &vcardList)
{
    QString vcards = vcardList.join("\n");
    QVersitReader reader(vcards.toUtf8());
    if (!reader.startReading()) {
        qWarning() << "Fail to read docs";
        return QList<QtContacts::QContact>();
    } else {
        reader.waitForFinished();
        QList<QVersitDocument> documents = reader.results();

        QVersitContactImporter contactImporter;
        contactImporter.setPropertyHandler(new ContactImporterPropertyHandler);
        if (!contactImporter.importDocuments(documents)) {
            qWarning() << "Fail to import contacts";
            return QList<QtContacts::QContact>();
        }

        return contactImporter.contacts();
    }
}

} //namespace
