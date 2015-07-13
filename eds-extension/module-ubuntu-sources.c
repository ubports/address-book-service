/*
 * module-ubuntu-online-accounts.c
 *
 * This library is free software you can redistribute it and/or modify it
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
 */

#include "e-source-ubuntu.h"

#include <libebackend/libebackend.h>
#include <libaccounts-glib/accounts-glib.h>

#define E_AG_SERVICE_TYPE_CALENDAR "calendar"
#define E_AG_SERVICE_TYPE_CONTACTS "contacts"

/* Standard GObject macros */
#define E_TYPE_UBUNTU_SOURCES \
    (e_ubuntu_sources_get_type ())
#define E_UBUNTU_SOURCES(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST \
    ((obj), E_TYPE_UBUNTU_SOURCES, EUbuntuSources))


typedef struct _EUbuntuSources EUbuntuSources;
typedef struct _EUbuntuSourcesClass EUbuntuSourcesClass;

struct _EUbuntuSources {
    EExtension parent;

    AgManager *ag_manager;
    /* AgAccountId -> ESource UID */
    GHashTable *uoa_to_eds;

};

struct _EUbuntuSourcesClass {
    EExtensionClass parent_class;
};


/* Module Entry Points */
void e_module_load (GTypeModule *type_module);
void e_module_unload (GTypeModule *type_module);

/* Forward Declarations */
GType e_ubuntu_sources_get_type (void);

G_DEFINE_TYPE (
    EUbuntuSources,
    e_ubuntu_sources,
    E_TYPE_EXTENSION)

static ESourceRegistryServer *
ubuntu_sources_get_server (EUbuntuSources *extension)
{
    EExtensible *extensible;

    extensible = e_extension_get_extensible (E_EXTENSION (extension));

    return E_SOURCE_REGISTRY_SERVER (extensible);
}

static void
ubuntu_sources_remove_collection (EUbuntuSources *extension,
                                  ESource *source)
{
    GError *local_error = NULL;
    ESourceUbuntu *ubuntu_ext;

    ubuntu_ext = e_source_get_extension (source, E_SOURCE_EXTENSION_UBUNTU);
    if (e_source_ubuntu_get_autoremove (ubuntu_ext)) {
        /* This removes the entire subtree rooted at source.
         * Deletes the corresponding on-disk key files too. */
        e_source_remove_sync (source, NULL, &local_error);

        if (local_error != NULL) {
            g_warning ("%s: %s", G_STRFUNC, local_error->message);
            g_error_free (local_error);
        }
    }
}

static void
ubuntu_sources_config_source (EUbuntuSources *extension,
                              ESource *source,
                              AgAccount *ag_account)
{
    ESourceExtension *source_extension;
    g_debug("CONFIGURE SOURCE: %s,%s", e_source_get_display_name(source),
            e_source_get_uid(source));

    g_object_bind_property (
        ag_account, "display-name",
        source, "display-name",
        G_BINDING_SYNC_CREATE);

    g_object_bind_property (
        ag_account, "enabled",
        source, "enabled",
        G_BINDING_SYNC_CREATE);

    source_extension = e_source_get_extension (source, E_SOURCE_EXTENSION_UBUNTU);

    g_object_bind_property (
        ag_account, "id",
        source_extension, "account-id",
        G_BINDING_SYNC_CREATE);

    g_debug("PROVIDER>>>: %s", ag_account_get_provider_name (ag_account));

    /* The data source should not be removable by clients. */
    e_server_side_source_set_removable (E_SERVER_SIDE_SOURCE (source),
                                        FALSE);

}

static void
ubuntu_sources_account_deleted_cb (AgManager *ag_manager,
                                   AgAccountId ag_account_id,
                                   EUbuntuSources *extension)
{
    ESource *source = NULL;
    ESourceRegistryServer *server;
    GSList *eds_id_list;
    GSList *link;

    server = ubuntu_sources_get_server (extension);

    eds_id_list = g_hash_table_lookup (extension->uoa_to_eds,
                                       GUINT_TO_POINTER (ag_account_id));

    g_debug("Sources registered for account: %d", g_slist_length (eds_id_list));

    for (link = eds_id_list; link != NULL; link = g_slist_next (link)) {
        const gchar *source_uid = link->data;

        source = e_source_registry_server_ref_source (server, source_uid);
        if (source != NULL) {
            ubuntu_sources_remove_collection (extension, source);
            g_object_unref (source);
        }
    }

    g_hash_table_remove (extension->uoa_to_eds,
                         GUINT_TO_POINTER (ag_account_id));
}

static gboolean
ubuntu_sources_register_source (EUbuntuSources *extension,
                                ESource *source)
{
    ESourceUbuntu *ubuntu_ext;
    AgAccountId ag_account_id;
    AgAccount *ag_account;

    g_debug("REgister new source: %s/%s", e_source_get_display_name(source),
            e_source_get_uid(source));

    if (!e_source_has_extension (source, E_SOURCE_EXTENSION_UBUNTU)) {
        return;
    }

    ubuntu_ext = e_source_get_extension (source, E_SOURCE_EXTENSION_UBUNTU);
    ag_account_id = e_source_ubuntu_get_account_id (ubuntu_ext);
    ag_account = ag_manager_get_account (extension->ag_manager,
                                         ag_account_id);

    if (ag_account) {
        GSList *eds_id_list;
        GSList *match;
        const gchar *source_uid;

        eds_id_list = g_hash_table_lookup (extension->uoa_to_eds,
                                           GUINT_TO_POINTER (ag_account_id));

        source_uid = e_source_get_uid (source);
        match = g_slist_find(eds_id_list, source_uid);
        if (match) {
            g_object_unref (ag_account);
            g_debug ("Source Already registered");
            return;
        }


        eds_id_list = g_slist_append (eds_id_list, g_strdup (source_uid));
        g_hash_table_insert (extension->uoa_to_eds,
                             GUINT_TO_POINTER (ag_account_id),
                             eds_id_list);

        ubuntu_sources_config_source (extension, source, ag_account);

        g_object_unref (ag_account);

        g_debug("Source %s, linked with account %d", source_uid, ag_account_id);
        return TRUE;
    }

    return FALSE;
}

static void
ubuntu_sources_populate_accounts_table (EUbuntuSources *extension)
{
    ESourceRegistryServer *server;
    GQueue trash = G_QUEUE_INIT;
    GList *list, *link;

    server = ubuntu_sources_get_server (extension);
    list = e_source_registry_server_list_sources (server,
                                                  E_SOURCE_EXTENSION_UBUNTU);

    g_debug ("Found %d ubuntu accounts.", g_list_length(list));
    for (link = list; link != NULL; link = g_list_next (link)) {
        ESource *source;
        source = E_SOURCE (link->data);

        /* If a matching AgAccountId was found, add it
         * to our accounts hash table.  Otherwise remove
         * the ESource after we finish looping. */
        if (!ubuntu_sources_register_source (extension, source)) {
            g_debug ("Account not found we will remove the source: %s",
                     e_source_get_display_name (source));

            g_queue_push_tail (&trash, source);
        }
    }

    /* Empty the trash. */
    while (!g_queue_is_empty (&trash)) {
        ESource *source = g_queue_pop_head (&trash);
        ubuntu_sources_remove_collection (extension, source);
    }

    g_list_free_full (list, (GDestroyNotify) g_object_unref);
    g_debug("ubuntu_sources_populate_accounts_table:END");
}

static void
ubuntu_source_source_added_cb (ESourceRegistryServer *server,
                               ESource *source,
                               EUbuntuSources *extension)
{
    ubuntu_sources_register_source (extension, source);
}

static void
ubuntu_sources_bus_acquired_cb (EDBusServer *server,
                                GDBusConnection *connection,
                                EUbuntuSources *extension)
{
    g_debug("loading ubuntu sources");

    extension->ag_manager = ag_manager_new ();

    /* This populates a hash table of UOA ID -> ESource UID strings by
     * searching through available data sources for ones with a "Ubuntu
     * Source" extension.  If such an extension is found, but
     * no corresponding AgAccount (presumably meaning the UOA account
     * was somehow deleted between E-D-S sessions) then the ESource in
     * which the extension was found gets deleted. */
    ubuntu_sources_populate_accounts_table (extension);

    /* Listen for Online Account changes. */
    g_signal_connect (extension->ag_manager, "account-deleted",
                      G_CALLBACK (ubuntu_sources_account_deleted_cb),
                      extension);

    ESourceRegistryServer *registry_server;
    registry_server = ubuntu_sources_get_server (extension);
    g_signal_connect (registry_server, "source_added",
                      G_CALLBACK (ubuntu_source_source_added_cb),
                      extension);
}

static void
ubuntu_sources_dispose (GObject *object)
{
    EUbuntuSources *extension;

    extension = E_UBUNTU_SOURCES (object);

    if (extension->ag_manager != NULL) {
        g_signal_handlers_disconnect_matched (
            extension->ag_manager,
            G_SIGNAL_MATCH_DATA,
            0, 0, NULL, NULL, object);
        g_object_unref (extension->ag_manager);
        extension->ag_manager = NULL;
    }

    /* Chain up to parent's dispose() method. */
    G_OBJECT_CLASS (e_ubuntu_sources_parent_class)->dispose (object);
}

static void
ubuntu_sources_finalize (GObject *object)
{
    EUbuntuSources *extension;

    extension = E_UBUNTU_SOURCES (object);

    g_hash_table_destroy (extension->uoa_to_eds);

    /* Chain up to parent's finalize() method. */
    G_OBJECT_CLASS (e_ubuntu_sources_parent_class)->finalize (object);
}

static void
ubuntu_sources_constructed (GObject *object)
{
    EExtension *extension;
    EExtensible *extensible;

    extension = E_EXTENSION (object);
    extensible = e_extension_get_extensible (extension);

    /* Wait for the registry service to acquire its well-known
     * bus name so we don't do anything destructive beforehand.
     * Run last so that all the sources get loaded first. */

    g_signal_connect_after (
        extensible, "bus-acquired",
        G_CALLBACK (ubuntu_sources_bus_acquired_cb),
        extension);

    /* Chain up to parent's constructed() method. */
    G_OBJECT_CLASS (e_ubuntu_sources_parent_class)->constructed (object);
}

static void
e_ubuntu_sources_class_init (EUbuntuSourcesClass *klass)
{
    GObjectClass *object_class;
    EExtensionClass *extension_class;

    object_class = G_OBJECT_CLASS (klass);
    object_class->dispose = ubuntu_sources_dispose;
    object_class->finalize = ubuntu_sources_finalize;
    object_class->constructed = ubuntu_sources_constructed;

    extension_class = E_EXTENSION_CLASS (klass);
    extension_class->extensible_type = E_TYPE_SOURCE_REGISTRY_SERVER;
}

static void
e_ubuntu_sources_destroy_eds_id_slist (GSList *eds_id_list)
{
    g_slist_free_full (eds_id_list, g_free);
}

static void
e_ubuntu_sources_init (EUbuntuSources *extension)
{
    extension->uoa_to_eds = g_hash_table_new_full (
        (GHashFunc) g_direct_hash,
        (GEqualFunc) g_direct_equal,
        (GDestroyNotify) NULL,
        (GDestroyNotify) e_ubuntu_sources_destroy_eds_id_slist);
}

G_MODULE_EXPORT void
e_module_load (GTypeModule *type_module)
{
    g_type_ensure (E_TYPE_SOURCE_UBUNTU);
    g_type_ensure (E_TYPE_UBUNTU_SOURCES);
}

G_MODULE_EXPORT void
e_module_unload (GTypeModule *type_module)
{
}
