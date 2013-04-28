#ifndef __GALERA_CONTACTS_UTILS_H__
#define __GALERA_CONTACTS_UTILS_H__

#include <QtCore/QString>

#include <folks/folks.h>

namespace galera
{

class ContactsUtils
{
public:
    static QByteArray serializeIndividual(FolksIndividual *individual);
};

} //namespace
#endif
