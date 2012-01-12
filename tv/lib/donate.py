# Miro - an RSS based video player application
# Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010, 2011
# Participatory Culture Foundation
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
#
# In addition, as a special exception, the copyright holders give
# permission to link the code of portions of this program with the OpenSSL
# library.
#
# You must obey the GNU General Public License in all respects for all of
# the code used other than OpenSSL. If you modify file(s) with this
# exception, you may extend this exception to your version of the file(s),
# but you are not obligated to do so. If you do not wish to do so, delete
# this exception statement from your version. If you delete this exception
# statement from all source files in the program, then also delete it here.

"""``miro.donatemanager`` -- functions for handling donation
"""

import logging
import time

from miro import app
from miro import prefs
from miro import signals

from miro.frontends.widgets import donate

from miro.plat.frontends.widgets.threads import call_on_ui_thread

class DonateManager(object):
    NOTHANKS_MULTIPLIER = [50, 100]
    URL_PATTERN = 'http://getmiro.com/give/m%s/'
    PAYMENT_URL = 'http://getmiro.com/'
    def __init__(self):
        self.donate_nothanks = app.config.get(prefs.DONATE_NOTHANKS)
        self.donate_counter = app.config.get(prefs.DONATE_COUNTER)
        self.last_donate_time = app.config.get(prefs.LAST_DONATE_TIME)
        app.backend_config_watcher.connect('changed', self.on_config_changed)
        signals.system.connect('download-complete', self.on_download_complete)
        self.donate_window = self.powertoys = None
        call_on_ui_thread(self.create_windows)

    def create_windows(self):
        self.donate_window = donate.DonateWindow()
        self.powertoys = donate.DonatePowerToys()
        self.donate_window.connect('donate-clicked', self.on_donate_clicked)

    def run_powertoys(self):
        if self.powertoys:
            self.powertoys.run_dialog()

    def on_config_changed(self, obj, key, value):
        if key == prefs.DONATE_NOTHANKS.key:
            self.donate_nothanks = value
        elif key == prefs.DONATE_COUNTER.key:
            self.donate_counter = value
        elif key == prefs.LAST_DONATE_TIME.key:
            self.last_donate_time = value

    def on_download_complete(self, obj, item):
        try:
            rearm_count = self.NOTHANKS_MULTIPLIER[self.donate_nothanks]
        except IndexError:
            rearm_count = self.NOTHANKS_MULTIPLIER[-1]

        self.donate_counter -= 1

        # In case the donate counters are borked, then reset it
        if self.donate_counter < 0:
            self.donate_counter = 0
        if self.last_donate_time < 0:
            self.last_donate_time = 0

        logging.debug('donate: on_download_complete %s %s %s',
                      self.donate_nothanks, self.donate_counter,
                      self.last_donate_time)

        show_donate = self.donate_counter == 0 and self.last_donate_time == 0

        # re-arm the countdown
        self.donate_counter = rearm_count
        app.config.set(prefs.DONATE_COUNTER, self.donate_counter)

        if show_donate:
            self.show_donate()

    def on_donate_clicked(self, obj, donate, payment_url):
        if donate:
            app.widgetapp.open_url(payment_url)
            self.last_donate_time = time.time()
            app.config.set(prefs.LAST_DONATE_TIME, self.last_donate_time)
        else:
            self.donate_nothanks += 1
            app.config.set(prefs.DONATE_NOTHANKS, self.donate_nothanks)
            self.donate_window.close()

    def shutdown(self):
        if self.donate_window:
            self.donate_window.close()
            self.donate_window = None

    def reset(self):
        for pref in [prefs.DONATE_NOTHANKS, prefs.LAST_DONATE_TIME,
                     prefs.DONATE_COUNTER]:
            app.config.set(pref, pref.default)

    def show_donate(self, url=None, payment_url=None):
        if not url:
            args = [1, 2, 3]
            try:
                url = self.URL_PATTERN % args[self.donate_nothanks]
            except IndexError:
                url = self.URL_PATTERN % 't'
        if not payment_url:
            payment_url = self.PAYMENT_URL
        if self.donate_window:
            self.donate_window.show(url, payment_url)
