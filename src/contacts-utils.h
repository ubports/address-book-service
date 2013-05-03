#ifndef __GALERA_CONTACTS_UTILS_H__
#define __GALERA_CONTACTS_UTILS_H__

#include <QtCore/QString>
#include <QVersitProperty>

#include <folks/folks.h>

namespace galera
{

class ContactsUtils
{
public:
    static QByteArray serializeIndividual(FolksIndividual *individual);

private:
    static QList<QtVersit::QVersitProperty> parsePersona(int index, FolksPersona *persona);
    static QtVersit::QVersitProperty createProperty(int sourceIndex,
                                                    int index,
                                                    const QString &name,
                                                    const QString &value);
};

} //namespace
#endif
