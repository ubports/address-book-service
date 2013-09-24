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

            // Export custom property PIDMAP
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
                }
            }

            if (toBeAdded->size() == 0) {
                return;
            }

            // export detailUir as PID field
            if (!detail.detailUri().isEmpty()) {
                QVersitProperty &prop = toBeAdded->last();
                QMultiHash<QString, QString> params = prop.parameters();
                params.insert(galera::VCardParser::PidFieldName, detail.detailUri());
                prop.setParameters(params);
            }

            switch (detail.type()) {
                case QContactDetail::TypeAvatar:
                {
                    QContactAvatar avatar = static_cast<QContactAvatar>(detail);
                    QVersitProperty &prop = toBeAdded->last();
                    prop.insertParameter(QStringLiteral("VALUE"), QStringLiteral("URL"));
                    prop.setValue(avatar.imageUrl().toString(QUrl::RemoveUserInfo));
                    break;
                }
                case QContactDetail::TypePhoneNumber:
                {
                    QString prefName = galera::VCardParser::PreferredActionNames[QContactDetail::TypePhoneNumber];
                    QContactDetail prefPhone = contact.preferredDetail(prefName);
                    if (prefPhone == detail) {
                        QVersitProperty &prop = toBeAdded->last();
                        QMultiHash<QString, QString> params = prop.parameters();
                        params.insert(galera::VCardParser::PrefParamName, "1");
                        prop.setParameters(params);
                    }
                    break;
                }
                default:
                    break;
            }
        }

        virtual void contactProcessed(const QContact& contact, QVersitDocument* document)
        {
            Q_UNUSED(contact);
            document->removeProperties("X-QTPROJECT-EXTENDED-DETAIL");
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

            if (!*alreadyProcessed) {
                return;
            }

            QString pid = property.parameters().value(galera::VCardParser::PidFieldName);
            if (!pid.isEmpty()) {
                QContactDetail &det = updatedDetails->last();
                det.setDetailUri(pid);
            }

            if (updatedDetails->size() == 0) {
                return;
            }

            // Remove empty phone and address subtypes
            QContactDetail &det = updatedDetails->last();
            switch (det.type()) {
                case QContactDetail::TypePhoneNumber:
                {
                    QContactPhoneNumber phone = static_cast<QContactPhoneNumber>(det);
                    if (phone.subTypes().isEmpty()) {
                        det.setValue(QContactPhoneNumber::FieldSubTypes, QVariant());
                    }
                    if (property.parameters().contains(galera::VCardParser::PrefParamName)) {
                        m_prefferedPhone = phone;
                    }
                    break;
                }
                case QContactDetail::TypeAvatar:
                {
                    QString value = property.parameters().value("VALUE");
                    if (value == "URL") {
                        det.setValue(QContactAvatar::FieldImageUrl, QUrl(property.value()));
                    }
                    break;
                }
                default:
                    break;
            }
        }

        virtual void documentProcessed(const QVersitDocument& document, QContact* contact)
        {
            Q_UNUSED(document);
            Q_UNUSED(contact);
            if (!m_prefferedPhone.isEmpty()) {
                contact->setPreferredDetail(galera::VCardParser::PreferredActionNames[QContactDetail::TypePhoneNumber],
                                            m_prefferedPhone);
                m_prefferedPhone = QContactDetail();
            }
        }
    private:
        QContactDetail m_prefferedPhone;
    };
}


namespace galera
{

const QString VCardParser::PidMapFieldName = QStringLiteral("CLIENTPIDMAP");
const QString VCardParser::PidFieldName = QStringLiteral("PID");
const QString VCardParser::PrefParamName = QStringLiteral("PREF");

static QMap<QtContacts::QContactDetail::DetailType, QString> prefferedActions()
{
    QMap<QtContacts::QContactDetail::DetailType, QString> values;

    values.insert(QContactDetail::TypeAddress, QStringLiteral("ADR"));
    values.insert(QContactDetail::TypeEmailAddress, QStringLiteral("EMAIL"));
    values.insert(QContactDetail::TypeNote, QStringLiteral("NOTE"));
    values.insert(QContactDetail::TypeOnlineAccount, QStringLiteral("IMPP"));
    values.insert(QContactDetail::TypeOrganization, QStringLiteral("ORG"));
    values.insert(QContactDetail::TypePhoneNumber, QStringLiteral("TEL"));
    values.insert(QContactDetail::TypeUrl, QStringLiteral("URL"));

    return values;
}
const QMap<QtContacts::QContactDetail::DetailType, QString> VCardParser::PreferredActionNames = prefferedActions();


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
