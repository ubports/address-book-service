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

import os
import subprocess
import sysconfig

from fixtures import EnvironmentVariable, Fixture


class AddressBookServiceDummyBackend(Fixture):

    def setUp(self):
        super(AddressBookServiceDummyBackend, self).setUp()
        self.start_dummy_contact_service()
        self.addCleanup(self._restart_address_book_service)

    def start_dummy_contact_service(self):
        """Add environment variables to load the dummy backend
        and restart the service."""
        self._setup_environment()
        self._restart_address_book_service()

    def _setup_environment(self):
        self.useFixture(EnvironmentVariable(
            'ALTERNATIVE_CPIM_SERVICE_NAME', 'com.canonical.test.pim'))
        self.useFixture(EnvironmentVariable(
            'FOLKS_BACKEND_PATH',
            self._get_service_library_path() + 'dummy.so'))
        self.useFixture(EnvironmentVariable('FOLKS_BACKENDS_ALLOWED', 'dummy'))
        self.useFixture(EnvironmentVariable('FOLKS_PRIMARY_STORE', 'dummy'))
        self.useFixture(EnvironmentVariable(
            'ADDRESS_BOOK_SERVICE_DEMO_DATA',
            self._get_vcard_location()))

    def _restart_address_book_service(self):
        self._kill_address_book_service()

        path = self._get_service_library_path() + 'address-book-service'
        subprocess.Popen([path])

    def _get_vcard_location(self):
        local_location = os.path.abspath('vcard.vcf')
        bin_location = '/usr/share/address-book-service/data/vcard.vcf'
        if os.path.exists(local_location):
            return local_location
        else:
            return bin_location
    
    def _get_service_library_path(self):
        architecture = sysconfig.get_config_var('MULTIARCH')

        return '/usr/lib/' + architecture + '/address-book-service/'

    def _kill_address_book_service(self):
        try:
            pid = subprocess.check_output(
                ['pidof', 'address-book-service']).strip()
            subprocess.call(['kill', '-3', pid])
        except subprocess.CalledProcessError:
            # Service not running, so do nothing.
            pass
