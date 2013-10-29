/*
 * Copyright (C) 2013 Philip Withnall
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
 * Authors:
 *       Philip Withnall <philip@tecnocode.co.uk>
 */

using Folks;
using Gee;
using GLib;

/**
 * A persona subclass which represents a single contact. TODO
 *
 * Each {@link Dummy.Persona} instance represents a single EDS {@link E.Contact}.
 * When the contact is modified (either by this folks client, or a different
 * client), the {@link Dummy.Persona} remains the same, but is assigned a new
 * {@link E.Contact}. It then updates its properties from this new contact.
 *
 * @since UNRELEASED
 */
public class Dummyf.Persona : Folks.Persona
{
  private string[] _linkable_properties = new string[0];

  /**
   * {@inheritDoc}
   *
   * @since UNRELEASED
   */
  public override string[] linkable_properties
    {
      get { return this._linkable_properties; }
    }

  private string[] _writeable_properties = new string[0];

  /**
   * {@inheritDoc}
   *
   * @since UNRELEASED
   */
  public override string[] writeable_properties
    {
      get { return this._writeable_properties; }
    }

  /**
   * Create a new persona.
   *
   * Create a new persona for the {@link PersonaStore} ``store``, representing
   * the EDS contact given by ``contact``. TODO
   *
   * @param store the store which will contain the persona
   * @param contact_id TODO
   * @param is_user TODO
   *
   * @since UNRELEASED
   */
  public Persona (PersonaStore store, string contact_id, bool is_user = false,
      string[] linkable_properties = {})
    {
      var uid = Folks.Persona.build_uid (BACKEND_NAME, store.id, contact_id);
      var iid = store.id + ":" + contact_id;

      Object (display_id: contact_id,
              uid: uid,
              iid: iid,
              store: store,
              is_user: is_user);

      this._linkable_properties = linkable_properties;
      this._writeable_properties = this.store.always_writeable_properties;
    }

  /**
   * {@inheritDoc}
   *
   * @since UNRELEASED
   */
  public override void linkable_property_to_links (string prop_name,
      Folks.Persona.LinkablePropertyCallback callback)
    {
      /* TODO */
      if (prop_name == "im-addresses")
        {
          var persona = this as ImDetails;
          assert (persona != null);

          foreach (var protocol in persona.im_addresses.get_keys ())
            {
              var im_fds = persona.im_addresses.get (protocol);

              foreach (var im_fd in im_fds)
                {
                  callback (protocol + ":" + im_fd.value);
                }
            }
        }
      else if (prop_name == "local-ids")
        {
          var persona = this as LocalIdDetails;
          assert (persona != null);

          foreach (var id in persona.local_ids)
            {
              callback (id);
            }
        }
      else if (prop_name == "web-service-addresses")
        {
          var persona = this as WebServiceDetails;
          assert (persona != null);

          foreach (var web_service in persona.web_service_addresses.get_keys ())
            {
              var web_service_addresses =
                  persona.web_service_addresses.get (web_service);

              foreach (var ws_fd in web_service_addresses)
                {
                  callback (web_service + ":" + ws_fd.value);
                }
            }
        }
      else if (prop_name == "email-addresses")
        {
          var persona = this as EmailDetails;
          assert (persona != null);

          foreach (var email in persona.email_addresses)
            {
              callback (email.value);
            }
        }
      else
        {
          /* Chain up */
          base.linkable_property_to_links (prop_name, callback);
        }
    }


  /*
   * All the functions below here are to be used by testing code rather than by
   * libfolks clients. They form the interface which would normally be between
   * the Persona and a web service or backing store of some kind.
   */


  /**
   * TODO
   *
   * @since UNRELEASED
   */
  public void update_writeable_properties (string[] writeable_properties)
    {
      /* TODO: Don't update if there's no change. */
      var new_length = this.store.always_writeable_properties.length +
          writeable_properties.length;
      this._writeable_properties = new string[new_length];
      int i = 0;

      foreach (var p in this.store.always_writeable_properties)
        {
          this._writeable_properties[i++] = p;
        }
      foreach (var p in writeable_properties)
        {
          this._writeable_properties[i++] = p;
        }

      this.notify_property ("writeable-properties");
    }

  /**
   * TODO (in milliseconds)
   *
   * @since UNRELEASED
   */
  protected int property_change_delay { get; set; }

  /* TODO */
  protected delegate void ChangePropertyCallback ();

  /**
   * TODO
   *
   * @param property_name TODO
   * @param callback TODO
   * @since UNRELEASED
   */
  protected async void change_property (string property_name,
      ChangePropertyCallback callback)
    {
      if (this.property_change_delay < 0)
        {
          /* No delay. */
          callback ();
        }
      else if (this.property_change_delay == 0)
        {
          /* Idle delay. */
          Idle.add (() =>
            {
              callback ();
              this.change_property.callback ();
              return false;
            });

          yield;
        }
      else
        {
          /* Timed delay. */
          Timeout.add (this.property_change_delay, () =>
            {
              callback ();
              this.change_property.callback ();
              return false;
            });

          yield;
        }
    }
}
