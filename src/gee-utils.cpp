#include "gee-utils.h"

#include <QDebug>

GValue* GeeUtils::gValueSliceNew(GType type)
{
    GValue *retval = g_slice_new0(GValue);
    g_value_init(retval, type);

    return retval;
}

void GeeUtils::gValueSliceFree(GValue *value)
{
    g_value_unset(value);
    g_slice_free(GValue, value);
}

void GeeUtils::personaDetailsInsert(GHashTable *details, FolksPersonaDetail key, gpointer value)
{
    g_hash_table_insert(details, (gpointer) folks_persona_store_detail_key(key), value);
}

GValue* GeeUtils::asvSetStrNew(QMultiMap<QString, QString> providerUidMap)
{
    GeeMultiMap *hashSet = GEE_MULTI_MAP_AFD_NEW(FOLKS_TYPE_IM_FIELD_DETAILS);
    GValue *retval = gValueSliceNew (G_TYPE_OBJECT);
    g_value_take_object (retval, hashSet);

    QList<QString> keys = providerUidMap.keys();
    Q_FOREACH(const QString& key, keys) {
        QList<QString> values = providerUidMap.values(key);
        Q_FOREACH(const QString& value, values) {
            FolksImFieldDetails *imfd;

            imfd = folks_im_field_details_new (value.toUtf8().data(), NULL);

            gee_multi_map_set(hashSet,
                              key.toUtf8().data(),
                              imfd);
            g_object_unref(imfd);
        }
    }

    return retval;
}

