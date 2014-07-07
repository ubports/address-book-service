# -*- Mode: Python; coding: utf-8; indent-tabs-mode: nil; tab-width: 4 -*-
#
# Copyright (C) 2014 Canonical
# Author: Omer Akram <omer.akram@canonical.com>
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
#

from testtools.matchers import Equals
from autopilot.matchers import Eventually

from address_book_service.tests import AddressBookServiceTestCase
from address_book_service import fixture_setup


class DummyBackendTestCase(AddressBookServiceTestCase):
    """High-level tests for the dummy backend of contacts service."""

    def setUp(self):
        dummy_backend = fixture_setup.AddressBookServiceDummyBackend()
        self.useFixture(dummy_backend)
        super(DummyBackendTestCase, self).setUp()

    def test_dummy_backend_loads_vcard(self):
        """Makes sure the dummy backend is loading the vcard."""
        list_page = self.main_window.get_contact_list_page()
        view_page = list_page.open_contact(0)

        self.assertThat(view_page.title, Eventually(Equals('UX User')))
