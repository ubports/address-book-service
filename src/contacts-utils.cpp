#include "contacts-utils.h"

#include <QVersitDocument>
#include <QVersitProperty>
#include <QVersitWriter>


using namespace QtVersit;
namespace galera
{

QByteArray ContactsUtils::serializeIndividual(FolksIndividual *individual)
{
    QVersitProperty prop;
    QVersitDocument vcard(QVersitDocument::VCard30Type);

    //UID
    prop.setName("UID");
    prop.setValue(folks_individual_get_id(individual));
    vcard.addProperty(prop);

    GeeIterator *iter;
    GeeSet *personas = folks_individual_get_personas(individual);

    QList<QVersitProperty> clientPidMapList;
    iter = gee_iterable_iterator(GEE_ITERABLE(personas));
    while(gee_iterator_next(iter)) {
        FolksPersona *persona = FOLKS_PERSONA(gee_iterator_get(iter));

        prop.clear();
        prop.setName("CLIENTPIDMAP");
        prop.setValue(QString("%1;%2").arg(clientPidMapList.count() + 1)
                                      .arg(folks_persona_get_uid(persona)));
        clientPidMapList << prop;
    }


    Q_FOREACH(QVersitProperty p, clientPidMapList) {
        vcard.addProperty(p);
    }

    QByteArray result;
    QVersitWriter writer(&result);
    writer.startWriting(vcard);
    writer.waitForFinished();

    return result;
}

} //namespace

