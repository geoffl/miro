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

"""miro.plat.options -- Holds platform-specific command line options.
"""

from miro.prefs import Pref

SHOW_TRAYICON = Pref(
    key="showTrayicon",
    default=True,
    platformSpecific=False)

# empty string means it hasn't been set yet.  there's code in
# get_main_window_dimensions that figures this out and sets the
# default.
WINDOW_DIMENSIONS = Pref(
    key="windowdimensions",
    default="",
    platformSpecific=False)

WINDOW_MAXIMIZED = Pref(
    key="windowmaximized",
    default=False,
    platformSpecific=False)

WINDOWS_ICON = Pref(
    key='windowsIcon',
    default=None,
    # this is platform specific, but if we set this to True then it
    # won't look up the value in the theme's app.config file
    platformSpecific=False)

GSTREAMER_IMAGESINK = Pref(
    key="DefaultGstreamerImagesink",
    default="autovideosink",
    platformSpecific=False)

GSTREAMER_AUDIOSINK = Pref(
    key="DefaultGstreamerAudiosink",
    default="autoaudiosink",
    platformSpecific=False)
