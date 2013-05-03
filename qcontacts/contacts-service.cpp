#include "contacts-service.h"

#include "qcontact-engineid.h"

#include <QtDBus/QDBusInterface>
#include <QtDBus/QDBusMessage>
#include <QtDBus/QDBusPendingCallWatcher>
#include <QtDBus/QDBusPendingReply>

#include <QtContacts/QContactChangeSet>
#include <QtContacts/QContactName>
#include <QtContacts/QContactPhoneNumber>

#include <QtVersit/QVersitReader>
#include <QtVersit/QVersitContactImporter>

#define GALERA_SERVICE_NAME             "com.canonical.galera"
#define GALERA_ADDRESSBOOK_OBJECT_PATH  "/com/canonical/galera/AddressBook"
#define GALERA_ADDRESSBOOK_IFACE_NAME   "com.canonical.galera.AddressBook"
#define GALERA_VIEW_IFACE_NAME          "com.canonical.galera.View"

#define FETCH_PAGE_SIZE                 30

namespace galera
{

class RequestData
{
public:
    QtContacts::QContactAbstractRequest *m_request;
    QDBusInterface *m_view;
    QDBusPendingCallWatcher *m_watcher;
    int m_offset;

    RequestData()
        : m_request(0), m_view(0), m_watcher(0), m_offset(0)
    {}

    ~RequestData()
    {
        m_request = 0;
        m_watcher->deleteLater();
        m_view->call("close");
        m_view->deleteLater();
    }
};

GaleraContactsService::GaleraContactsService(const QString &managerUri)
    : m_selfContactId(),
      m_nextContactId(1),
      m_managerUri(managerUri)

{
    m_iface = QSharedPointer<QDBusInterface>(new QDBusInterface(GALERA_SERVICE_NAME,
                                                                 GALERA_ADDRESSBOOK_OBJECT_PATH,
                                                                 GALERA_ADDRESSBOOK_IFACE_NAME));
}

GaleraContactsService::GaleraContactsService(const GaleraContactsService &other)
    : m_selfContactId(other.m_selfContactId),
      m_nextContactId(other.m_nextContactId),
      m_iface(other.m_iface),
      m_managerUri(other.m_managerUri)
{
}

GaleraContactsService::~GaleraContactsService()
{
}

void GaleraContactsService::fetchContacts(QtContacts::QContactFetchRequest *request)
{
    qDebug() << Q_FUNC_INFO;
    //QContactFetchRequest *r = static_cast<QContactFetchRequest*>(request);
    //QContactFilter filter = r->filter();
    //QList<QContactSortOrder> sorting = r->sorting();
    //QContactFetchHint fetchHint = r->fetchHint();
    //QContactManager::Error operationError = QContactManager::NoError;

    QDBusMessage result = m_iface->call("query", "", "", QStringList());
    if (result.type() == QDBusMessage::ErrorMessage) {
        qWarning() << result.errorName() << result.errorMessage();
        QContactManagerEngine::updateContactFetchRequest(request, QList<QContact>(),
                                                         QContactManager::UnspecifiedError,
                                                         QContactAbstractRequest::FinishedState);
        return;
    }
    RequestData *requestData = new RequestData;
    QDBusObjectPath viewObjectPath = result.arguments()[0].value<QDBusObjectPath>();
    requestData->m_view = new QDBusInterface(GALERA_SERVICE_NAME,
                                           viewObjectPath.path(),
                                           GALERA_VIEW_IFACE_NAME);

    requestData->m_request = request;
    m_pendingRequests << requestData;
    fetchContactsPage(requestData);
}

void GaleraContactsService::fetchContactsPage(RequestData *request)
{
    qDebug() << Q_FUNC_INFO << request->m_offset;
    // Load contacs async
    QDBusPendingCall pcall = request->m_view->asyncCall("contactsDetails", QStringList(), request->m_offset, FETCH_PAGE_SIZE);
    if (pcall.isError()) {
        qWarning() << pcall.error().name() << pcall.error().message();
        QContactManagerEngine::updateContactFetchRequest(static_cast<QContactFetchRequest*>(request->m_request), QList<QContact>(),
                                                         QContactManager::UnspecifiedError,
                                                         QContactAbstractRequest::FinishedState);
        destroyRequest(request);
        return;
    }
    request->m_watcher = new QDBusPendingCallWatcher(pcall, 0);
    QObject::connect(request->m_watcher, &QDBusPendingCallWatcher::finished,
                     [=](QDBusPendingCallWatcher *call) {
                        this->fetchContactsDone(request, call);
                     });
}

void GaleraContactsService::fetchContactsDone(RequestData *request, QDBusPendingCallWatcher *call)
{
    qDebug() << Q_FUNC_INFO;
    QDBusPendingReply<QStringList> reply = *call;
    if (reply.isError()) {
        qWarning() << reply.error().name() << reply.error().message();
        QContactManagerEngine::updateContactFetchRequest(static_cast<QContactFetchRequest*>(request->m_request),
                                                         QList<QContact>(),
                                                         QContactManager::UnspecifiedError,
                                                         QContactAbstractRequest::FinishedState);
        destroyRequest(request);
    } else {
        QList<QContact> contacts;
        const QStringList vcards = reply.value();
        QContactAbstractRequest::State opState = QContactAbstractRequest::FinishedState;

        if (vcards.size() == FETCH_PAGE_SIZE) {
            opState = QContactAbstractRequest::ActiveState;
        }

        if (!vcards.isEmpty()) {
            QtVersit::QVersitReader reader(vcards.join("").toLocal8Bit());
            reader.startReading();
            reader.waitForFinished();

            QtVersit::QVersitContactImporter importer;
            importer.importDocuments(reader.results());

            contacts = importer.contacts();
            QList<QContactId> contactsIds;

            QList<QContact>::iterator contact;
            for (contact = contacts.begin(); contact != contacts.end(); ++contact) {
                if (!contact->isEmpty()) {
                    quint32 nextId = m_nextContactId;
                    GaleraEngineId *engineId = new GaleraEngineId(++nextId, m_managerUri);
                    QContactId newId = QContactId(engineId);

                    contact->setId(newId);
                    contactsIds << newId;
                    m_nextContactId = nextId;

#if 0
                    if (m_nextContactId < 10) {
                        qDebug() << "VCARD" << vcards.at(m_nextContactId-1);
                        qDebug() << "Imported contact" << contact->id()
                                 << "Name:" << contact->detail<QContactName>().firstName()
                                 << "Phone" << contact->detail<QContactPhoneNumber>().number();
                        Q_FOREACH(QContactDetail det, contact->details()) {
                            qDebug() << det;
                        }
                    }
#endif
                }
            }

            m_contacts += contacts;
            m_contactIds += contactsIds;
        }

        QContactManagerEngine::updateContactFetchRequest(static_cast<QContactFetchRequest*>(request->m_request),
                                                         m_contacts,
                                                         QContactManager::NoError,
                                                         opState);

        qDebug() << "CONTACTS SIZE:" << vcards.size();
        if (opState == QContactAbstractRequest::ActiveState) {
            request->m_offset += FETCH_PAGE_SIZE;
            request->m_watcher->deleteLater();
            request->m_watcher = 0;
            fetchContactsPage(request);
        } else {
            destroyRequest(request);
        }
    }
}

void GaleraContactsService::addRequest(QtContacts::QContactAbstractRequest *request)
{
    qDebug() << Q_FUNC_INFO << request->state();
    Q_ASSERT(request->state() == QContactAbstractRequest::ActiveState);
    switch (request->type()) {
        case QContactAbstractRequest::ContactFetchRequest:
            fetchContacts(static_cast<QContactFetchRequest*>(request));
            break;
        case QContactAbstractRequest::ContactFetchByIdRequest:
        case QContactAbstractRequest::ContactIdFetchRequest:
        case QContactAbstractRequest::ContactSaveRequest:
        case QContactAbstractRequest::ContactRemoveRequest:
        case QContactAbstractRequest::RelationshipFetchRequest:
        case QContactAbstractRequest::RelationshipRemoveRequest:
        case QContactAbstractRequest::RelationshipSaveRequest:
        break;

        default: // unknown request type.
        break;
    }
}

void GaleraContactsService::destroyRequest(RequestData *request)
{
    qDebug() << Q_FUNC_INFO;
    m_pendingRequests.remove(request);
    delete request;
}

} //namespace
