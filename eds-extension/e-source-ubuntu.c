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

/**
 * SECTION: e-source-ubuntu
 * @short_description: #ESource extension for Ubuntu applications
 **/

#include "e-source-ubuntu.h"

#include <glib.h>
#include <gio/gio.h>
#include <libedataserver/libedataserver.h>
#include <libaccounts-glib/accounts-glib.h>

#define E_SOURCE_UBUNTU_GET_PRIVATE(obj) \
    (G_TYPE_INSTANCE_GET_PRIVATE \
    ((obj), E_TYPE_SOURCE_UBUNTU, ESourceUbuntuPrivate))

struct _ESourceUbuntuPrivate {
    GMutex property_lock;

    guint account_id;
    gchar *application_id;
    gboolean auto_remove;
    gboolean writable;
    gchar *provider;
    gchar *metadata;
    AgAccount *account;
};

enum {
    PROP_0,
    PROP_ACCOUNT_ID,
    PROP_APPLICATION_ID,
    PROP_AUTOREMOVE,
    PROP_ACCOUNT_PROVIDER,
    PROP_WRITABLE,
    PROP_METADATA
};

G_DEFINE_TYPE (
    ESourceUbuntu,
    e_source_ubuntu,
    E_TYPE_SOURCE_EXTENSION)

static void
source_ubuntu_set_property (GObject *object,
                            guint property_id,
                            const GValue *value,
                            GParamSpec *pspec)
{
    switch (property_id) {
        case PROP_ACCOUNT_ID:
            e_source_ubuntu_set_account_id(E_SOURCE_UBUNTU (object),
                                           g_value_get_uint(value));
            return;

        case PROP_APPLICATION_ID:
            e_source_ubuntu_set_application_id (E_SOURCE_UBUNTU (object),
                                                g_value_get_string (value));
            return;

        case PROP_AUTOREMOVE:
            e_source_ubuntu_set_autoremove (E_SOURCE_UBUNTU (object),
                                            g_value_get_boolean (value));
            return;
        case PROP_WRITABLE:
            e_source_ubuntu_set_writable (E_SOURCE_UBUNTU (object),
                                          g_value_get_boolean (value));
            return;

        case PROP_METADATA:
            e_source_ubuntu_set_metadata (E_SOURCE_UBUNTU (object),
                                          g_value_get_string (value));
            return;
    }

    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
source_ubuntu_get_property (GObject *object,
                         guint property_id,
                         GValue *value,
                         GParamSpec *pspec)
{
    switch (property_id) {
        case PROP_ACCOUNT_ID:
            g_value_set_uint (value,
                              e_source_ubuntu_get_account_id (E_SOURCE_UBUNTU (object)));
            return;

        case PROP_APPLICATION_ID:
            g_value_take_string (value,
                                 e_source_ubuntu_dup_application_id (E_SOURCE_UBUNTU (object)));
            return;

        case PROP_AUTOREMOVE:
            g_value_set_boolean (value,
                                 e_source_ubuntu_get_autoremove (E_SOURCE_UBUNTU (object)));
            return;

        case PROP_ACCOUNT_PROVIDER:
            g_value_take_string (value,
                                 e_source_ubuntu_dup_account_provider (E_SOURCE_UBUNTU (object)));
            return;

        case PROP_WRITABLE:
            g_value_set_boolean (value,
                                 e_source_ubuntu_get_writable (E_SOURCE_UBUNTU (object)));
            return;

        case PROP_METADATA:
            g_value_take_string (value,
                                 e_source_ubuntu_dup_metadata(E_SOURCE_UBUNTU (object)));
            return;
    }

    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
source_ubuntu_finalize (GObject *object)
{
    ESourceUbuntuPrivate *priv;

    priv = E_SOURCE_UBUNTU_GET_PRIVATE (object);

    g_mutex_clear (&priv->property_lock);
    if (priv->account) {
        g_object_unref (priv->account);
    }

    g_free (priv->application_id);
    g_free (priv->provider);
    g_free (priv->metadata);

    /* Chain up to parent's finalize() method. */
    G_OBJECT_CLASS (e_source_ubuntu_parent_class)->finalize (object);
}

static void
e_source_ubuntu_class_init (ESourceUbuntuClass *klass)
{
    GObjectClass *object_class;
    ESourceExtensionClass *extension_class;

    g_type_class_add_private (klass, sizeof (ESourceUbuntuPrivate));

    object_class = G_OBJECT_CLASS (klass);
    object_class->set_property = source_ubuntu_set_property;
    object_class->get_property = source_ubuntu_get_property;
    object_class->finalize = source_ubuntu_finalize;

    extension_class = E_SOURCE_EXTENSION_CLASS (klass);
    extension_class->name = E_SOURCE_EXTENSION_UBUNTU;

    g_object_class_install_property (
        object_class,
        PROP_ACCOUNT_ID,
        g_param_spec_uint (
            "account-id",
            "Account ID",
            "Online Account ID",
            0, G_MAXUINT, 0,
            G_PARAM_READWRITE |
            G_PARAM_CONSTRUCT |
            G_PARAM_STATIC_STRINGS |
            E_SOURCE_PARAM_SETTING));

    g_object_class_install_property (
        object_class,
        PROP_APPLICATION_ID,
        g_param_spec_string (
            "application-id",
            "Application ID",
            "Ubuntu application ID",
            NULL,
            G_PARAM_READWRITE |
            G_PARAM_CONSTRUCT |
            G_PARAM_STATIC_STRINGS |
            E_SOURCE_PARAM_SETTING));

    g_object_class_install_property (
        object_class,
        PROP_AUTOREMOVE,
        g_param_spec_boolean (
            "auto-remove",
            "Autoremove source",
            "Autoremove source",
            TRUE,
            G_PARAM_READWRITE |
            G_PARAM_CONSTRUCT |
            G_PARAM_STATIC_STRINGS |
            E_SOURCE_PARAM_SETTING));

    g_object_class_install_property (
        object_class,
        PROP_WRITABLE,
        g_param_spec_boolean (
            "writable",
            "Writable source",
            "Writable source",
            TRUE,
            G_PARAM_READWRITE |
            G_PARAM_CONSTRUCT |
            G_PARAM_STATIC_STRINGS |
            E_SOURCE_PARAM_SETTING));

    g_object_class_install_property (
        object_class,
        PROP_METADATA,
        g_param_spec_string (
            "metadata",
            "metadata",
            "Source metadata",
            NULL,
            G_PARAM_READWRITE |
            G_PARAM_CONSTRUCT |
            G_PARAM_STATIC_STRINGS |
            E_SOURCE_PARAM_SETTING));

    g_object_class_install_property (
        object_class,
        PROP_ACCOUNT_PROVIDER,
        g_param_spec_string (
            "account-provider",
            "Provider name",
            "Online Account Provider name",
            NULL,
            G_PARAM_READABLE |
            G_PARAM_STATIC_STRINGS));
}

static void
e_source_ubuntu_init (ESourceUbuntu *extension)
{
    extension->priv = E_SOURCE_UBUNTU_GET_PRIVATE (extension);
    g_mutex_init (&extension->priv->property_lock);
}

/**
 * e_source_ubuntu_get_account_id:
 * @extension: an #ESourceUbuntu
 *
 * Returns the identifier uint of the Online Account associated
 * with the #ESource to which @extension belongs.
 *
 * Returns: the associated Online Account ID
 *
 **/
guint
e_source_ubuntu_get_account_id (ESourceUbuntu *extension)
{
    g_return_val_if_fail (E_IS_SOURCE_UBUNTU (extension), 0);

    return extension->priv->account_id;
}


/**
 * e_source_ubuntu_set_account_id:
 * @extension: an #ESourceUbuntu
 * @account_id: (allow-none): the associated Online Account ID, or %NULL
 *
 * Sets the identifier guint of the Online Account associated
 * with the #ESource to which @extension belongs.
 *
 * The internal copy of @account_id is automatically stripped of leading
 * and trailing whitespace.  If the resulting string is empty, %NULL is set
 * instead.
 **/
void
e_source_ubuntu_set_account_id (ESourceUbuntu *extension,
                                guint account_id)
{
    g_return_if_fail (E_IS_SOURCE_UBUNTU (extension));

    g_mutex_lock (&extension->priv->property_lock);

    if (extension->priv->account_id == account_id) {
        g_mutex_unlock (&extension->priv->property_lock);
        return;
    }

    if (extension->priv->account) {
        g_object_unref (extension->priv->account);
        extension->priv->account = 0;
    }

    extension->priv->account_id = account_id;
    if (account_id != 0) {
        AgManager *manager = ag_manager_new ();
        extension->priv->account = ag_manager_get_account (manager, account_id);
    }

    g_mutex_unlock (&extension->priv->property_lock);

    g_object_notify (G_OBJECT (extension), "account-id");
}

/**
 * e_source_ubuntu_get_application_id:
 * @extension: an #ESourceUbuntu
 *
 * Returns the id string of the application associated
 * with the #ESource to which @extension belongs. Can be %NULL or an empty
 * string for accounts not supporting this property.
 *
 * Returns: the associated application-id
 *
 **/
const gchar *
e_source_ubuntu_get_application_id (ESourceUbuntu *extension)
{
    g_return_val_if_fail (E_IS_SOURCE_UBUNTU (extension), NULL);

    return extension->priv->application_id;
}

/**
 * e_source_ubuntu_dup_application_id:
 * @extension: an #ESourceUbuntu
 *
 * Thread-safe variation of e_source_ubuntu_get_application_id().
 * Use this function when accessing @extension from multiple threads.
 *
 * The returned string should be freed with g_free() when no longer needed.
 *
 * Returns: a newly-allocated copy of #ESourceUbuntu:application-id
 *
 * Since: 3.8
 **/
gchar *
e_source_ubuntu_dup_application_id (ESourceUbuntu *extension)
{
    const gchar *application_id;
    gchar *duplicate;

    g_return_val_if_fail (E_IS_SOURCE_UBUNTU (extension), NULL);

    g_mutex_lock (&extension->priv->property_lock);

    application_id = e_source_ubuntu_get_application_id (extension);
    duplicate = g_strdup (application_id);

    g_mutex_unlock (&extension->priv->property_lock);

    return duplicate;
}

/**
 * e_source_ubuntu_set_application_id:
 * @extension: an #ESourceUbuntu
 * @application_id: (allow-none): the associated application ID, or %NULL
 *
 * Sets the id of the application associated
 * with the #ESource to which @extension belongs.
 *
 * The internal copy of @calendar_url is automatically stripped of leading
 * and trailing whitespace.  If the resulting string is empty, %NULL is set
 * instead.
 *
 **/
void
e_source_ubuntu_set_application_id (ESourceUbuntu *extension,
                                    const gchar *application_id)
{
    g_return_if_fail (E_IS_SOURCE_UBUNTU (extension));

    g_mutex_lock (&extension->priv->property_lock);

    if (g_strcmp0 (extension->priv->application_id, application_id) == 0) {
        g_mutex_unlock (&extension->priv->property_lock);
        return;
    }

    g_free (extension->priv->application_id);
    extension->priv->application_id = e_util_strdup_strip (application_id);

    g_mutex_unlock (&extension->priv->property_lock);

    g_object_notify (G_OBJECT (extension), "application-id");
}

const gchar *
e_source_ubuntu_get_account_provider (ESourceUbuntu *extension)
{
    g_return_val_if_fail (E_IS_SOURCE_UBUNTU (extension), NULL);

    if (extension->priv->account) {
        return ag_account_get_provider_name (extension->priv->account);
    }

    return NULL;
}

gchar *
e_source_ubuntu_dup_account_provider (ESourceUbuntu *extension)
{
    const gchar *provider;
    gchar *duplicate;

    g_return_val_if_fail (E_IS_SOURCE_UBUNTU (extension), NULL);

    g_mutex_lock (&extension->priv->property_lock);

    provider = e_source_ubuntu_get_account_provider (extension);
    duplicate = g_strdup (provider);

    g_mutex_unlock (&extension->priv->property_lock);

    return duplicate;
}

gboolean
e_source_ubuntu_get_autoremove(ESourceUbuntu *extension)
{
    g_return_val_if_fail (E_IS_SOURCE_UBUNTU (extension), FALSE);

    return extension->priv->auto_remove;
}

void
e_source_ubuntu_set_autoremove(ESourceUbuntu *extension,
                               gboolean flag)
{
    g_return_if_fail (E_IS_SOURCE_UBUNTU (extension));

    g_mutex_lock (&extension->priv->property_lock);

    if (extension->priv->auto_remove == flag) {
        g_mutex_unlock (&extension->priv->property_lock);
        return;
    }

    extension->priv->auto_remove = flag;

    g_mutex_unlock (&extension->priv->property_lock);

    g_object_notify (G_OBJECT (extension), "auto-remove");
}

/**
 * e_source_ubuntu_get_writable:
 * @extension: an #ESourceUbuntu
 *
 * This can be used as extra flag.
 * For example for sources that are created from read-only remote sources.
 *
 * Returns: True if the source is marked as writable or False if not.
 *
 **/
gboolean
e_source_ubuntu_get_writable(ESourceUbuntu *extension)
{
    g_return_val_if_fail (E_IS_SOURCE_UBUNTU (extension), FALSE);

    return extension->priv->writable;
}

/**
 * e_source_ubuntu_set_writable:
 * @extension: an #ESourceUbuntu
 *
 * This can be used as extra flag.
 * For example for sources that are created from read-only remote sources.
 *
 * Sets if the source should be considered writable or not.
 *
 **/
void
e_source_ubuntu_set_writable(ESourceUbuntu *extension,
                               gboolean flag)
{
    g_return_if_fail (E_IS_SOURCE_UBUNTU (extension));

    g_mutex_lock (&extension->priv->property_lock);

    if (extension->priv->writable == flag) {
        g_mutex_unlock (&extension->priv->property_lock);
        return;
    }

    extension->priv->writable = flag;

    g_mutex_unlock (&extension->priv->property_lock);

    g_object_notify (G_OBJECT (extension), "writable");
}

/**
 * e_source_ubuntu_get_metadata:
 * @extension: an #ESourceUbuntu
 *
 * Returns the metadata string of the application associated
 * with the #ESource to which @extension belongs. Can be %NULL or an empty
 * string.
 *
 * Returns: the associated metadata
 *
 **/
const gchar *
e_source_ubuntu_get_metadata (ESourceUbuntu *extension)
{
    g_return_val_if_fail (E_IS_SOURCE_UBUNTU (extension), NULL);

    return extension->priv->metadata;
}

/**
 * e_source_ubuntu_dup_metadata:
 * @extension: an #ESourceUbuntu
 *
 * Thread-safe variation of e_source_ubuntu_get_metadata().
 * Use this function when accessing @extension from multiple threads.
 *
 * The returned string should be freed with g_free() when no longer needed.
 *
 * Returns: a newly-allocated copy of #ESourceUbuntu:metadata
 *
 **/
gchar *
e_source_ubuntu_dup_metadata (ESourceUbuntu *extension)
{
    const gchar *metadata;
    gchar *duplicate;

    g_return_val_if_fail (E_IS_SOURCE_UBUNTU (extension), NULL);

    g_mutex_lock (&extension->priv->property_lock);

    metadata = e_source_ubuntu_get_metadata(extension);
    duplicate = g_strdup (metadata);

    g_mutex_unlock (&extension->priv->property_lock);

    return duplicate;
}

/**
 * e_source_ubuntu_set_metadata:
 * @extension: an #ESourceUbuntu
 * @metadata: (allow-none): the associated metadata, or %NULL
 *
 * Sets the metadata associated with the #ESource to which @extension belongs.
 *
 * The internal copy of @metadata is automatically stripped of leading
 * and trailing whitespace.  If the resulting string is empty, %NULL is set
 * instead.
 *
 **/
void
e_source_ubuntu_set_metadata (ESourceUbuntu *extension,
                              const gchar *metadata)
{
    g_return_if_fail (E_IS_SOURCE_UBUNTU (extension));

    g_mutex_lock (&extension->priv->property_lock);

    if (g_strcmp0 (extension->priv->metadata, metadata) == 0) {
        g_mutex_unlock (&extension->priv->property_lock);
        return;
    }

    g_free (extension->priv->metadata);
    extension->priv->metadata = e_util_strdup_strip (metadata);

    g_mutex_unlock (&extension->priv->property_lock);

    g_object_notify (G_OBJECT (extension), "metadata");
}
