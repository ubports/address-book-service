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

#ifndef __GALERA_GEE_UTILS_H__
#define __GALERA_GEE_UTILS_H__

#include <QtCore/QMultiMap>

#include <QtContacts/QContactDetail>

#include <gio/gio.h>
#include <folks/folks.h>


static guint _folks_abstract_field_details_hash_data_func (gconstpointer v, gpointer self)
{
    const FolksAbstractFieldDetails *constDetails = static_cast<const FolksAbstractFieldDetails*>(v);
    return folks_abstract_field_details_hash_static (const_cast<FolksAbstractFieldDetails*>(constDetails));
}

static int _folks_abstract_field_details_equal_data_func (gconstpointer a, gconstpointer b, gpointer self)
{
    const FolksAbstractFieldDetails *constDetailsA = static_cast<const FolksAbstractFieldDetails*>(a);
    const FolksAbstractFieldDetails *constDetailsB = static_cast<const FolksAbstractFieldDetails*>(b);
    return folks_abstract_field_details_equal_static (const_cast<FolksAbstractFieldDetails*>(constDetailsA), const_cast<FolksAbstractFieldDetails*>(constDetailsB));
}

#define SET_AFD_NEW() \
    GEE_SET(gee_hash_set_new(FOLKS_TYPE_ABSTRACT_FIELD_DETAILS, \
                             (GBoxedCopyFunc) g_object_ref, g_object_unref, \
                             _folks_abstract_field_details_hash_data_func, \
                             NULL, \
                             NULL, \
                             _folks_abstract_field_details_equal_data_func, \
                             NULL, \
                             NULL))

#define SET_PERSONA_NEW() \
    GEE_SET(gee_hash_set_new(FOLKS_TYPE_PERSONA, \
                             (GBoxedCopyFunc) g_object_ref, g_object_unref, \
                             NULL, \
                             NULL, \
                             NULL, \
                             NULL, \
                             NULL, \
                             NULL))

#define GEE_MULTI_MAP_AFD_NEW(FOLKS_TYPE) \
    GEE_MULTI_MAP(gee_hash_multi_map_new(G_TYPE_STRING,\
                                         (GBoxedCopyFunc) g_strdup, g_free, \
                                         FOLKS_TYPE, \
                                         (GBoxedCopyFunc)g_object_ref, g_object_unref, \
                                         NULL, \
                                         NULL, \
                                         NULL, \
                                         NULL, \
                                         NULL, \
                                         NULL, \
                                         _folks_abstract_field_details_hash_data_func, \
                                         NULL, \
                                         NULL, \
                                         _folks_abstract_field_details_equal_data_func, \
                                         NULL, \
                                         NULL))

class GeeUtils
{
public:
    static GValue* gValueSliceNew(GType type);
    static void gValueSliceFree(GValue *value);
    static void personaDetailsInsert(GHashTable *details, FolksPersonaDetail key, gpointer value);
    static GValue* asvSetStrNew(QMultiMap<QString, QString> providerUidMap);
}; // class

#endif
