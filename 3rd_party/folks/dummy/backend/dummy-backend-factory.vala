/*
 * Copyright (C) 2009 Zeeshan Ali (Khattak) <zeeshanak@gnome.org>.
 * Copyright (C) 2009 Nokia Corporation.
 * Copyright (C) 2011, 2013 Collabora Ltd.
 *
 * This library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Zeeshan Ali (Khattak) <zeeshanak@gnome.org>
 *          Travis Reitter <travis.reitter@collabora.co.uk>
 *          Marco Barisione <marco.barisione@collabora.co.uk>
 *          Raul Gutierrez Segales <raul.gutierrez.segales@collabora.co.uk>
 *
 * This file was originally part of Rygel.
 */

using Folks;
using Gee;

/**
 * The dummy backend module entry point.
 *
 * @backend_store a store to add the dummy backends to
 * @since UNRELEASED
 */
public void module_init (BackendStore backend_store)
{
  string[] writable_properties =
    {
      Folks.PersonaStore.detail_key (PersonaDetail.AVATAR),
      Folks.PersonaStore.detail_key (PersonaDetail.BIRTHDAY),
      Folks.PersonaStore.detail_key (PersonaDetail.EMAIL_ADDRESSES),
      Folks.PersonaStore.detail_key (PersonaDetail.FULL_NAME),
      Folks.PersonaStore.detail_key (PersonaDetail.GENDER),
      Folks.PersonaStore.detail_key (PersonaDetail.IM_ADDRESSES),
      Folks.PersonaStore.detail_key (PersonaDetail.IS_FAVOURITE),
      Folks.PersonaStore.detail_key (PersonaDetail.NICKNAME),
      Folks.PersonaStore.detail_key (PersonaDetail.PHONE_NUMBERS),
      Folks.PersonaStore.detail_key (PersonaDetail.POSTAL_ADDRESSES),
      Folks.PersonaStore.detail_key (PersonaDetail.ROLES),
      Folks.PersonaStore.detail_key (PersonaDetail.STRUCTURED_NAME),
      Folks.PersonaStore.detail_key (PersonaDetail.LOCAL_IDS),
      Folks.PersonaStore.detail_key (PersonaDetail.LOCATION),
      Folks.PersonaStore.detail_key (PersonaDetail.WEB_SERVICE_ADDRESSES),
      Folks.PersonaStore.detail_key (PersonaDetail.NOTES),
      Folks.PersonaStore.detail_key (PersonaDetail.URLS),
      Folks.PersonaStore.detail_key (PersonaDetail.GROUPS),
      null
    };

  /* Create a new persona store. */
  var dummy_persona_store =
      new FolksDummy.PersonaStore ("dummy-store", "Dummy personas", writable_properties);
  dummy_persona_store.persona_type = typeof (FolksDummy.FullPersona);

  /* Register it with the backend. */
  var persona_stores = new HashSet<PersonaStore> ();
  persona_stores.add (dummy_persona_store);

  var backend = new FolksDummy.Backend ();
  backend.register_persona_stores (persona_stores);
  dummy_persona_store.reach_quiescence();
  backend_store.add_backend (backend);
}

/**
 * The dummy backend module exit point.
 *
 * @param backend_store the store to remove the backends from
 * @since UNRELEASED
 */
public void module_finalize (BackendStore backend_store)
{
  /* FIXME: No backend_store.remove_backend() API exists. */
}
