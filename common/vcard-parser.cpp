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

#include <QtCore/QMimeDatabase>
#include <QtCore/QMimeType>

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
            if (detail.type() == QContactDetail::TypeSyncTarget) {
                QContactSyncTarget syncTarget = static_cast<QContactSyncTarget>(detail);
                QVersitProperty prop;
                prop.setValueType(QVersitProperty::CompoundType);

                prop.setName(galera::VCardParser::PidMapFieldName);
                QStringList values;
                values << syncTarget.syncTarget()
                       << syncTarget.value(QContactSyncTarget::FieldSyncTarget + 1).toString();
                prop.setValue(values);
                *toBeAdded << prop;
            }

            if (detail.type() == QContactDetail::TypeExtendedDetail) {
                QVersitProperty prop;
                prop.setName(detail.value(QContactExtendedDetail::FieldName).toString());
                prop.setValue(detail.value(QContactExtendedDetail::FieldData).toString());
                *toBeAdded << prop;
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
            if (!contact.id().isNull() &&
                contact.details<QContactGuid>().isEmpty()) {
                // translate contact id to uid vcard
                QVersitProperty prop;
                prop.setName("UID");
                prop.setValue(contact.id().toString().split("::").last());
                document->addProperty(prop);
            }
            Q_UNUSED(contact);
            document->removeProperties("X-QTPROJECT-EXTENDED-DETAIL");
        }
    };


class  ContactImporterPropertyHandler : public QVersitContactImporterPropertyHandlerV2
{
public:
    virtual void propertyProcessed(const QVersitDocument& document,
                                   const QVersitProperty& property,
                                   const QContact& contact,
                                   bool *alreadyProcessed,
                                   QList<QContactDetail>* updatedDetails)
    {
        Q_UNUSED(document);
        Q_UNUSED(contact);

        if (!*alreadyProcessed && (property.name() == galera::VCardParser::PidMapFieldName)) {
            QContactSyncTarget target;
            QStringList values = property.value().split(QStringLiteral(";"));
            target.setSyncTarget(values.value(0));
            target.setValue(QContactSyncTarget::FieldSyncTarget + 1, values.value(1));
            *updatedDetails  << target;
            *alreadyProcessed = true;
        }

        if (!*alreadyProcessed && (property.name().startsWith("X-"))) {
            QContactExtendedDetail xDet;
            xDet.setName(property.name());
            xDet.setData(property.value<QString>());
            *updatedDetails  << xDet;
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
        if (!m_prefferedPhone.isEmpty()) {
            contact->setPreferredDetail(galera::VCardParser::PreferredActionNames[QContactDetail::TypePhoneNumber],
                                        m_prefferedPhone);
            m_prefferedPhone = QContactDetail();
        }

        if (contact->id().isNull() &&
            !contact->detail<QContactGuid>().isEmpty()) {
            QContactId id = QContactId::fromString(
                        QString("qtcontacts:galera::%1").arg(contact->detail<QContactGuid>().guid()));
            contact->setId(id);
        }

        //update contact timestamp with X-CREATED-AT
        QContactTimestamp timestamp = contact->detail<QContactTimestamp>();
        QDateTime createdAt = timestamp.lastModified();
        Q_FOREACH(const QVersitProperty &prop, document.properties()) {
            if (prop.name() == "X-CREATED-AT") {
                createdAt = QDateTime::fromString(prop.value(), Qt::ISODate).toUTC();
                break;
            }
        }
        timestamp.setCreated(createdAt);
        contact->saveDetail(&timestamp);
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
const QString VCardParser::TagFieldName = QStringLiteral("TAG");

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
    m_exporterHandler = new ContactExporterDetailHandler;
    m_importerHandler = new ContactImporterPropertyHandler;
}

VCardParser::~VCardParser()
{
    waitForFinished();

    delete m_exporterHandler;
    delete m_importerHandler;
 }

QList<QContact> VCardParser::vcardToContactSync(const QStringList &vcardList)
{
    VCardParser parser;
    parser.vcardToContact(vcardList);
    parser.waitForFinished();

    return parser.contactsResult();
}

QtContacts::QContact VCardParser::vcardToContact(const QString &vcard)
{
    return vcardToContactSync(QStringList() << vcard).value(0, QContact());
}

void VCardParser::vcardToContact(const QStringList &vcardList)
{
    if (m_versitReader) {
        qWarning() << "Import operation in progress.";
        return;
    }
    m_vcardsResult.clear();
    m_contactsResult.clear();

    QString vcards = vcardList.join("\r\n");
    m_versitReader = new QVersitReader(vcards.toUtf8());
    connect(m_versitReader,
            SIGNAL(resultsAvailable()),
            SLOT(onReaderResultsAvailable()));
    connect(m_versitReader,
            SIGNAL(stateChanged(QVersitReader::State)),
            SLOT(onReaderStateChanged(QVersitReader::State)));
    m_versitReader->startReading();
}

void VCardParser::waitForFinished()
{
    if (m_versitReader) {
        m_versitReader->waitForFinished();
    }
    if (m_versitWriter) {
        m_versitWriter->waitForFinished();
    }

    // wait state changed events to arrive
    QCoreApplication::sendPostedEvents(this);
}

QStringList VCardParser::vcardResult() const
{
    return m_vcardsResult;
}

QList<QContact> VCardParser::contactsResult() const
{
    return m_contactsResult;
}

void VCardParser::onReaderResultsAvailable()
{
    //NOTHING FOR NOW
}

QStringList VCardParser::splitVcards(const QByteArray &vcardList)
{
    QStringList result;
    int start = 0;

    while(start < vcardList.size()) {
        int pos = vcardList.indexOf("BEGIN:VCARD", start + 1);

        if (pos == -1) {
            pos = vcardList.length();
        }
        QByteArray vcard = vcardList.mid(start, (pos - start));
        result << vcard;
        start = pos;
    }

    return result;
}

void VCardParser::onReaderStateChanged(QVersitReader::State state)
{
    if (state == QVersitReader::FinishedState) {
        QList<QVersitDocument> documents = m_versitReader->results();

        QVersitContactImporter contactImporter;
        contactImporter.setPropertyHandler(m_importerHandler);
        if (!contactImporter.importDocuments(documents)) {
            qWarning() << "Fail to import contacts";
            return;
        }
        m_contactsResult = contactImporter.contacts();
        Q_EMIT contactsParsed(contactImporter.contacts());

        delete m_versitReader;
        m_versitReader = 0;
    }
}

void VCardParser::contactToVcard(QList<QtContacts::QContact> contacts)
{
    if (m_versitWriter) {
        qWarning() << "Export operation in progress.";
        return;
    }
    m_vcardsResult.clear();
    m_contactsResult.clear();

    QVersitContactExporter exporter;
    exporter.setDetailHandler(m_exporterHandler);
    if (!exporter.exportContacts(contacts, QVersitDocument::VCard30Type)) {
        qWarning() << "Fail to export contacts" << exporter.errors();
        return;
    }

    m_versitWriter = new QVersitWriter(&m_vcardData);
    connect(m_versitWriter,
            SIGNAL(stateChanged(QVersitWriter::State)),
            SLOT(onWriterStateChanged(QVersitWriter::State)));
    m_versitWriter->startWriting(exporter.documents());
}

void VCardParser::onWriterStateChanged(QVersitWriter::State state)
{
    if (state == QVersitWriter::FinishedState) {
        QStringList vcards = VCardParser::splitVcards(m_vcardData);
        m_vcardsResult = vcards;
        Q_EMIT vcardParsed(vcards);
        delete m_versitWriter;
        m_versitWriter = 0;
    }
}

QStringList VCardParser::contactToVcardSync(QList<QContact> contacts)
{
    VCardParser parser;
    parser.contactToVcard(contacts);
    parser.waitForFinished();

    return parser.vcardResult();
}

QString VCardParser::contactToVcard(const QContact &contact)
{
    return contactToVcardSync(QList<QContact>() << contact).value(0, QString());
}

} //namespace
