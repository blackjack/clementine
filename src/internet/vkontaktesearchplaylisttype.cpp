/* This file is part of Clementine.
   Copyright 2010, David Sansome <me@davidsansome.com>

   Clementine is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   Clementine is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with Clementine.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "vkontaktesearchplaylisttype.h"
#include "vkontakteservice.h"

const char* VkontakteSearchPlaylistType::kName = "vkontakte-search";

VkontakteSearchPlaylistType::VkontakteSearchPlaylistType(VkontakteService* service)
  : service_(service) {
}

QIcon VkontakteSearchPlaylistType::icon(Playlist* playlist) const {
  return QIcon(":providers/vkontakte.png");
}

QString VkontakteSearchPlaylistType::search_hint_text(Playlist* playlist) const {
  return QObject::tr("Search Vkontakte");
}

QString VkontakteSearchPlaylistType::empty_playlist_text(Playlist* playlist) const {
  return QObject::tr("Start typing in the search box above to find music on %1.").arg("Vkontakte");
}

bool VkontakteSearchPlaylistType::has_special_search_behaviour(Playlist* playlist) const {
  return true;
}

void VkontakteSearchPlaylistType::Search(const QString& text, Playlist* playlist) {
  service_->Search(text, playlist);
}

