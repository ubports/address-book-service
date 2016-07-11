/*
 * Copyright (C) 2015 Canonical Ltd
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Renato Araujo Oliveira Filho <renato.filho@canonical.com>
 *
 */

#ifndef E_SOURCE_UBUNTU_H
#define E_SOURCE_UBUNTU_H

#include <libedataserver/libedataserver.h>

/* Standard GObject macros */
#define E_TYPE_SOURCE_UBUNTU \
    (e_source_ubuntu_get_type ())
#define E_SOURCE_UBUNTU(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST \
    ((obj), E_TYPE_SOURCE_UBUNTU, ESourceUbuntu))
#define E_SOURCE_UBUNTU_CLASS(cls) \
    (G_TYPE_CHECK_CLASS_CAST \
    ((cls), E_TYPE_SOURCE_UBUNTU, ESourceUbuntuClass))
#define E_IS_SOURCE_UBUNTU(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE \
    ((obj), E_TYPE_SOURCE_UBUNTU))
#define E_IS_SOURCE_UBUNTU_CLASS(cls) \
    (G_TYPE_CHECK_CLASS_TYPE \
    ((cls), E_TYPE_SOURCE_UBUNTU))
#define E_SOURCE_UBUNTU_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS \
    ((obj), E_TYPE_SOURCE_UBUNTU, ESourceUbuntuClass))

/**
 * E_SOURCE_EXTENSION_UBUNTU:
 *
 * Pass this extension name to e_source_get_extension() to access
 * #ESourceUbuntu.  This is also used as a group name in key files.
 *
 **/
#define E_SOURCE_EXTENSION_UBUNTU "Ubuntu"

G_BEGIN_DECLS

typedef struct _ESourceUbuntu ESourceUbuntu;
typedef struct _ESourceUbuntuClass ESourceUbuntuClass;
typedef struct _ESourceUbuntuPrivate ESourceUbuntuPrivate;

/**
 * ESourceUbuntu:
 *
 * Contains only private data that should be read and manipulated using the
 * functions below.
 *
 **/
struct _ESourceUbuntu {
    ESourceExtension parent;
    ESourceUbuntuPrivate *priv;
};

struct _ESourceUbuntuClass {
    ESourceExtensionClass parent_class;
};

GType           e_source_ubuntu_get_type            (void) G_GNUC_CONST;

guint           e_source_ubuntu_get_account_id      (ESourceUbuntu *extension);
void            e_source_ubuntu_set_account_id      (ESourceUbuntu *extension,
                                                     guint id);

const gchar *   e_source_ubuntu_get_application_id  (ESourceUbuntu *extension);
gchar *         e_source_ubuntu_dup_application_id  (ESourceUbuntu *extension);
void            e_source_ubuntu_set_application_id  (ESourceUbuntu *extension,
                                                     const gchar *application_id);

gboolean        e_source_ubuntu_get_autoremove      (ESourceUbuntu *extension);
void            e_source_ubuntu_set_autoremove      (ESourceUbuntu *extension,
                                                     gboolean flag);

gboolean        e_source_ubuntu_get_writable        (ESourceUbuntu *extension);
void            e_source_ubuntu_set_writable        (ESourceUbuntu *extension,
                                                     gboolean flag);

const gchar *   e_source_ubuntu_get_account_provider(ESourceUbuntu *extension);
gchar *         e_source_ubuntu_dup_account_provider(ESourceUbuntu *extension);

const gchar *   e_source_ubuntu_get_metadata        (ESourceUbuntu *extension);
gchar *         e_source_ubuntu_dup_metadata        (ESourceUbuntu *extension);
void            e_source_ubuntu_set_metadata        (ESourceUbuntu *extension,
                                                     const gchar *metadata);

G_END_DECLS

#endif /* E_SOURCE_UBUNTU_H */

