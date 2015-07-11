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

    /* The list of services that we will create accounts for */
    GSList *known_services;

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

static ESource *
ubuntu_sources_new_source (EUbuntuSources *extension)
{
    ESourceRegistryServer *server;
    ESource *source;
    GFile *file;
    GError *local_error = NULL;

    /* This being a brand new data source, creating the instance
     * should never fail but we'll check for errors just the same. */
    server = ubuntu_sources_get_server (extension);
    file = e_server_side_source_new_user_file (NULL);
    source = e_server_side_source_new (server, file, &local_error);
    e_source_set_parent (source, "local-stub");
    g_object_unref (file);

    if (local_error != NULL) {
        g_warn_if_fail (source == NULL);
        g_warning ("%s: %s", G_STRFUNC, local_error->message);
        g_error_free (local_error);
    }

    return source;
}

static GHashTable *
ubuntu_sources_new_account_services (EUbuntuSources *extension,
                                     AgAccount *ag_account)
{
    GHashTable *account_services;
    GList *list, *link;

    account_services = g_hash_table_new_full (
        (GHashFunc) g_str_hash,
        (GEqualFunc) g_str_equal,
        (GDestroyNotify) g_free,
        (GDestroyNotify) g_object_unref);

    /* Populate the hash table with AgAccountService instances by
     * service type.  There should only be one AgService per type.
     *
     * XXX We really should not have to create AgAccountService
     *     instances ourselves.  The AgAccount itself should own
     *     them and provide functions for listing them.  Instead
     *     it only provides functions for listing its AgServices,
     *     which is decidedly less useful. */
    list = ag_account_list_services (ag_account);
    for (link = list; link != NULL; link = g_list_next (link)) {
        AgService *ag_service = link->data;
        const gchar *service_type;

        service_type = ag_service_get_service_type (ag_service);
        g_debug("Found service: %s", service_type);
        GSList *valid_service =  g_slist_find_custom(extension->known_services,
                                                     service_type,
                                                     (GCompareFunc) g_strcmp0);

        if (valid_service) {
            g_hash_table_insert (
                account_services,
                g_strdup (service_type),
                ag_account_service_new (ag_account, ag_service));
        } else {
            g_debug("%s is not a valid service", service_type);
        }
    }
    ag_service_list_free (list);

    return account_services;
}

static const gchar *
ubuntu_sources_get_backend_name (const gchar *uoa_provider_name)
{
    const gchar *eds_backend_name = NULL;

    /* This is a mapping between AgAccount provider names and
     * ESourceCollection backend names.  It requires knowledge
     * of other registry modules, possibly even from 3rd party
     * packages.
     *
     * FIXME Put the EDS backend name in the .service config
     *       files so we're not hard-coding the providers we
     *       support.  This isn't GNOME Online Accounts. */

    if (g_strcmp0 (uoa_provider_name, "google") == 0)
        eds_backend_name = "google";

    if (g_strcmp0 (uoa_provider_name, "windows-live") == 0)
        eds_backend_name = "outlook";

    if (g_strcmp0 (uoa_provider_name, "yahoo") == 0)
        eds_backend_name = "yahoo";

    return eds_backend_name;
}

static gboolean
ubuntu_sources_provider_name_to_backend_name (GBinding *binding,
                                              const GValue *source_value,
                                              GValue *target_value,
                                              gpointer unused)
{
    const gchar *provider_name;
    const gchar *backend_name;

    provider_name = g_value_get_string (source_value);
    backend_name = ubuntu_sources_get_backend_name (provider_name);
    g_return_val_if_fail (backend_name != NULL, FALSE);
    g_value_set_string (target_value, backend_name);

    return TRUE;
}

static void
ubuntu_sources_config_collection (EUbuntuSources *extension,
                                  ESource *source,
                                  AgAccount *ag_account,
                                  GHashTable *account_services)
{
    AgAccountService *ag_account_service;
    ESourceExtension *source_extension;

    g_object_bind_property (
        ag_account, "display-name",
        source, "display-name",
        G_BINDING_SYNC_CREATE);

    g_object_bind_property (
        ag_account, "enabled",
        source, "enabled",
        G_BINDING_SYNC_CREATE);

    source_extension = e_source_get_extension (source,
                                               E_SOURCE_EXTENSION_UOA);

    g_object_bind_property (
        ag_account, "id",
        source_extension, "account-id",
        G_BINDING_SYNC_CREATE);

    source_extension = e_source_get_extension (source,
                                               E_SOURCE_EXTENSION_COLLECTION);

    g_object_bind_property_full (
        ag_account, "provider",
        source_extension, "backend-name",
        G_BINDING_SYNC_CREATE,
        ubuntu_sources_provider_name_to_backend_name,
        NULL,
        NULL, (GDestroyNotify) NULL);

    ag_account_service = g_hash_table_lookup (account_services,
                                              E_AG_SERVICE_TYPE_CALENDAR);
    if (ag_account_service != NULL) {
        g_object_bind_property (ag_account_service, "enabled",
                                source_extension, "calendar-enabled",
                                G_BINDING_SYNC_CREATE);
    }

    ag_account_service = g_hash_table_lookup (account_services,
                                              E_AG_SERVICE_TYPE_CONTACTS);
    if (ag_account_service != NULL) {
        g_object_bind_property (ag_account_service, "enabled",
                                source_extension, "contacts-enabled",
                                G_BINDING_SYNC_CREATE);
    }

    /* Stash the AgAccountService hash table in the ESource
     * to keep the property bindings alive.  The hash table
     * will be destroyed along with the ESource. */
    g_object_set_data_full (G_OBJECT (source),
                            "ag-account-services",
                            g_hash_table_ref (account_services),
                            (GDestroyNotify) g_hash_table_unref);

    /* The data source should not be removable by clients. */
    e_server_side_source_set_removable (E_SERVER_SIDE_SOURCE (source),
                                        FALSE);
}

static void
ubuntu_sources_remove_collection (EUbuntuSources *extension,
                                  ESource *source)
{
    GError *local_error = NULL;

    /* This removes the entire subtree rooted at source.
     * Deletes the corresponding on-disk key files too. */
    e_source_remove_sync (source, NULL, &local_error);

    if (local_error != NULL) {
        g_warning ("%s: %s", G_STRFUNC, local_error->message);
        g_error_free (local_error);
    }
}

static void
ubuntu_sources_config_source (EUbuntuSources *extension,
                              ESource *source,
                              AgAccount *ag_account)
{
    GHashTable *account_services;

    account_services = ubuntu_sources_new_account_services (extension,
                                                            ag_account);
    g_debug("Number of services for account: %d", g_hash_table_size (account_services));

    if (g_hash_table_size (account_services) > 0) {
        ubuntu_sources_config_collection (extension,
                                          source,
                                          ag_account,
                                          account_services);
    } else {
        g_debug ("Removing source %s because it does not contain any valid service",
                 e_source_get_display_name (source));
        ubuntu_sources_remove_collection (extension, source);
    }

    g_hash_table_unref (account_services);
}

static void
ubuntu_sources_create_collection (EUbuntuSources *extension,
                                  AgAccount *ag_account)
{
    ESourceRegistryServer *server;
    ESource *collection_source;
    GHashTable *account_services;
    const gchar *parent_uid;


    server = ubuntu_sources_get_server (extension);

    collection_source = ubuntu_sources_new_source (extension);
    g_return_if_fail (E_IS_SOURCE (collection_source));

    /* Configure parent/child relationships. */
    parent_uid = e_source_get_uid (collection_source);

    /* Now it's our turn. */
    account_services = ubuntu_sources_new_account_services (extension,
                                                            ag_account);

    if (g_hash_table_size (account_services) == 0) {
        g_hash_table_unref (account_services);
        g_object_unref (collection_source);
        return;
    }

    ubuntu_sources_config_collection (extension,
                                      collection_source,
                                      ag_account,
                                      account_services);
    g_hash_table_unref (account_services);

    /* Export the new source collection. */
    e_source_registry_server_add_source (server, collection_source);

    g_hash_table_insert (
        extension->uoa_to_eds,
        GUINT_TO_POINTER (ag_account->id),
        g_strdup (parent_uid));

    g_object_unref (collection_source);
}

static void
ubuntu_sources_account_created_cb (AgManager *ag_manager,
                                   AgAccountId ag_account_id,
                                   EUbuntuSources *extension)
{
    AgAccount *ag_account;
    ESourceRegistryServer *server;
    const gchar *source_uid;

    server = ubuntu_sources_get_server (extension);

    ag_account = ag_manager_get_account (ag_manager, ag_account_id);
    g_return_if_fail (ag_account != NULL);

    source_uid = g_hash_table_lookup (extension->uoa_to_eds,
                                      GUINT_TO_POINTER (ag_account_id));
    if (source_uid == NULL) {
        g_debug ("Will create a new source for account: %d", ag_account_id);
        ubuntu_sources_create_collection (extension, ag_account);
    }

    g_object_unref (ag_account);
}

static void
ubuntu_sources_account_deleted_cb (AgManager *ag_manager,
                                   AgAccountId ag_account_id,
                                   EUbuntuSources *extension)
{
    ESource *source = NULL;
    ESourceRegistryServer *server;
    const gchar *source_uid;

    server = ubuntu_sources_get_server (extension);

    source_uid = g_hash_table_lookup (
        extension->uoa_to_eds,
        GUINT_TO_POINTER (ag_account_id));

    if (source_uid != NULL)
        source = e_source_registry_server_ref_source (
            server, source_uid);

    if (source != NULL) {
        ubuntu_sources_remove_collection (extension, source);
        g_object_unref (source);
    }
}

static void
ubuntu_sources_populate_accounts_table (EUbuntuSources *extension,
                                        GList *ag_account_ids)
{
    ESourceRegistryServer *server;
    GQueue trash = G_QUEUE_INIT;
    GList *list, *link;

    server = ubuntu_sources_get_server (extension);
    list = e_source_registry_server_list_sources (server,
                                                  E_SOURCE_EXTENSION_UOA);

    g_debug ("Found %d ubuntu accounts.", g_list_length(list));
    for (link = list; link != NULL; link = g_list_next (link)) {
        ESource *source;
        ESourceUoa *ubuntu_ext;
        AgAccount *ag_account = NULL;
        AgAccountId ag_account_id;
        const gchar *source_uid;
        GList *match;

        source = E_SOURCE (link->data);
        source_uid = e_source_get_uid (source);

        ubuntu_ext = e_source_get_extension (source, E_SOURCE_EXTENSION_UOA);
        ag_account_id = e_source_uoa_get_account_id (ubuntu_ext);

        if (ag_account_id == 0)
            continue;

        /* Verify the UOA account still exists. */
        match = g_list_find (
            ag_account_ids,
            GUINT_TO_POINTER (ag_account_id));
        if (match != NULL)
            ag_account = ag_manager_get_account (extension->ag_manager,
                                                 ag_account_id);

        /* If a matching AgAccountId was found, add it
         * to our accounts hash table.  Otherwise remove
         * the ESource after we finish looping. */
        if (ag_account != NULL) {
            g_hash_table_insert (
                extension->uoa_to_eds,
                GUINT_TO_POINTER (ag_account_id),
                g_strdup (source_uid));
            g_debug("Source %s, linked with account %d",
                    source_uid, ag_account_id);

            ubuntu_sources_config_source (extension, source, ag_account);

        } else {
            g_debug ("Account not found we will remove the source: %s",
                     source_uid);

            g_queue_push_tail (&trash, source);
        }
    }

    /* Empty the trash. */
    while (!g_queue_is_empty (&trash)) {
        GError *local_error = NULL;
        ESource *source = g_queue_pop_head (&trash);

        e_source_remove_sync (source, NULL, &local_error);
        if (local_error != NULL) {
            g_warning ("%s: %s", G_STRFUNC, local_error->message);
            g_error_free (local_error);
        }
    }

    g_list_free_full (list, (GDestroyNotify) g_object_unref);
}

static void
ubuntu_sources_bus_acquired_cb (EDBusServer *server,
                                GDBusConnection *connection,
                                EUbuntuSources *extension)
{
    GList *list, *link;
    g_debug("loading ubuntu sources");

    // We will only create sources for accounts that contains this kind of service
    extension->known_services = g_slist_append(NULL,
                                               E_AG_SERVICE_TYPE_CALENDAR);
    extension->known_services = g_slist_append(extension->known_services,
                                               E_AG_SERVICE_TYPE_CONTACTS);
    extension->ag_manager = ag_manager_new ();
    list = ag_manager_list (extension->ag_manager);

    /* This populates a hash table of UOA ID -> ESource UID strings by
     * searching through available data sources for ones with a "Ubuntu
     * Online Accounts" extension.  If such an extension is found, but
     * no corresponding AgAccount (presumably meaning the UOA account
     * was somehow deleted between E-D-S sessions) then the ESource in
     * which the extension was found gets deleted. */
    ubuntu_sources_populate_accounts_table (extension, list);

    for (link = list; link != NULL; link = g_list_next (link))
        ubuntu_sources_account_created_cb (extension->ag_manager,
                                           GPOINTER_TO_UINT (link->data),
                                           extension);
    ag_manager_list_free (list);

    /* Listen for Online Account changes. */
    g_signal_connect (extension->ag_manager, "account-created",
                      G_CALLBACK (ubuntu_sources_account_created_cb),
                      extension);
    g_signal_connect (extension->ag_manager, "account-deleted",
                      G_CALLBACK (ubuntu_sources_account_deleted_cb),
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
e_ubuntu_sources_init (EUbuntuSources *extension)
{
    extension->uoa_to_eds = g_hash_table_new_full (
        (GHashFunc) g_direct_hash,
        (GEqualFunc) g_direct_equal,
        (GDestroyNotify) NULL,
        (GDestroyNotify) g_free);
}

G_MODULE_EXPORT void
e_module_load (GTypeModule *type_module)
{
    g_type_ensure (E_TYPE_UBUNTU_SOURCES);
    //e_ubuntu_sources_register_type (type_module);
}

G_MODULE_EXPORT void
e_module_unload (GTypeModule *type_module)
{
}
