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

            // export read-only info
            if (detail.accessConstraints().testFlag(QContactDetail::ReadOnly)) {
                QVersitProperty &prop = toBeAdded->last();
                QMultiHash<QString, QString> params = prop.parameters();
                params.insert(galera::VCardParser::ReadOnlyFieldName, "YES");
                prop.setParameters(params);
            }

            // export Irremovable info
            if (detail.accessConstraints().testFlag(QContactDetail::Irremovable)) {
                QVersitProperty &prop = toBeAdded->last();
                QMultiHash<QString, QString> params = prop.parameters();
                params.insert(galera::VCardParser::IrremovableFieldName, "YES");
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

            bool ro = (property.parameters().value(galera::VCardParser::ReadOnlyFieldName, "NO") == "YES");
            bool irremovable = (property.parameters().value(galera::VCardParser::IrremovableFieldName, "NO") == "YES");
            if (ro && irremovable) {
                QContactDetail &det = updatedDetails->last();
                QContactManagerEngine::setDetailAccessConstraints(&det,
                                                                  QContactDetail::ReadOnly |
                                                                  QContactDetail::Irremovable);
            } else if (ro) {
                QContactDetail &det = updatedDetails->last();
                QContactManagerEngine::setDetailAccessConstraints(&det,
                                                                  QContactDetail::ReadOnly);
            } else if (irremovable) {
                QContactDetail &det = updatedDetails->last();
                QContactManagerEngine::setDetailAccessConstraints(&det,
                                                                  QContactDetail::Irremovable);
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
const QString VCardParser::IrremovableFieldName = QStringLiteral("IRREMOVABLE");
const QString VCardParser::ReadOnlyFieldName = QStringLiteral("READ-ONLY");

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

VCardParser::VCardParser(QObject *parent)
    : QObject(parent),
      m_versitWriter(0),
      m_versitReader(0)
{
}

VCardParser::~VCardParser()
{
    if (m_versitReader) {
        m_versitReader->waitForFinished();
    }
    if (m_versitWriter) {
        m_versitWriter->waitForFinished();
    }
}

QList<QContact> VCardParser::vcardToContactSync(const QStringList &vcardList)
{
    QString vcards = vcardList.join("\r\n");
    QVersitReader reader(vcards.toUtf8());
    if (!reader.startReading()) {
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

QtContacts::QContact VCardParser::vcardToContact(const QString &vcard)
{
    QList<QContact> contacts = vcardToContactSync(QStringList() << vcard);
    if (contacts.size()) {
        return contacts[0];
    } else {
        return QContact();
    }
}

void VCardParser::vcardToContact(const QStringList &vcardList)
{
    if (m_versitReader) {
        qWarning() << "Import operation in progress.";
        return;
    }
    QString vcards = vcardList.join("\r\n");
    m_versitReader = new QVersitReader(vcards.toUtf8());
    connect(m_versitReader,
            &QVersitReader::resultsAvailable,
            this,
            &VCardParser::onReaderResultsAvailable);
    connect(m_versitReader,
            &QVersitReader::stateChanged,
            this,
            &VCardParser::onReaderStateChanged);
    m_versitReader->startReading();
}

void VCardParser::onReaderResultsAvailable()
{
    //NOTHING FOR NOW
}

QStringList VCardParser::splitVcards(const QByteArray &vcardList)
{
    QStringList result;
    int start = 0;
    static const int size = QString("END:VCARD\r\n").size();

    while(start < vcardList.size()) {
        int pos = vcardList.indexOf("END:VCARD\r\n", start);
        result << vcardList.mid(start, (pos - start) + size);
        start += (pos + size + 1);
    }

    return result;
}

void VCardParser::onReaderStateChanged(QVersitReader::State state)
{
    if (state == QVersitReader::FinishedState) {
        QList<QVersitDocument> documents = m_versitReader->results();

        QVersitContactImporter contactImporter;
        contactImporter.setPropertyHandler(new ContactImporterPropertyHandler);
        if (!contactImporter.importDocuments(documents)) {
            qWarning() << "Fail to import contacts";
            return;
        }
        Q_EMIT contactsParsed(contactImporter.contacts());

        delete m_versitReader;
        m_versitReader = 0;
    }
}

void VCardParser::contactToVcard(QList<QtContacts::QContact> contacts)
{
    QStringList result;
    if (m_versitWriter) {
        qWarning() << "Export operation in progress.";
        return;
    }

    QVersitContactExporter exporter;
    exporter.setDetailHandler(new ContactExporterDetailHandler);
    if (!exporter.exportContacts(contacts, QVersitDocument::VCard30Type)) {
        qDebug() << "Fail to export contacts" << exporter.errors();
        return;
    }

    m_versitWriter = new QVersitWriter(&m_vcardData);
    connect(m_versitWriter, &QVersitWriter::stateChanged, this, &VCardParser::onWriterStateChanged);
    m_versitWriter->startWriting(exporter.documents());
}

void VCardParser::onWriterStateChanged(QVersitWriter::State state)
{
    if (state == QVersitWriter::FinishedState) {
        QStringList vcards = VCardParser::splitVcards(m_vcardData);
        Q_EMIT vcardParsed(vcards);
        delete m_versitWriter;
        m_versitWriter = 0;
    }
}

QStringList VCardParser::contactToVcardSync(QList<QContact> contacts)
{
    QVersitContactExporter exporter;
    exporter.setDetailHandler(new ContactExporterDetailHandler);
    if (!exporter.exportContacts(contacts, QVersitDocument::VCard30Type)) {
        qDebug() << "Fail to export contacts" << exporter.errors();
        return QStringList();
    }

    QByteArray vcardData;
    QVersitWriter versitWriter(&vcardData);
    versitWriter.startWriting(exporter.documents());
    versitWriter.waitForFinished();

    return VCardParser::splitVcards(vcardData);
}

QString VCardParser::contactToVcard(const QContact &contact)
{
    QStringList vcards = VCardParser::contactToVcardSync(QList<QContact>() << contact);
    if (vcards.size()) {
        return vcards[0];
    } else {
        return QString();
    }
}

} //namespace
