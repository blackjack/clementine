/* This file is part of Clementine.
   Copyright 2011, David Sansome <me@davidsansome.com>

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

#include "groovesharksearchprovider.h"

#include <QIcon>

#include "core/logging.h"
#include "covers/albumcoverloader.h"
#include "internet/groovesharkservice.h"
#include "internet/internetsongmimedata.h"

GroovesharkSearchProvider::GroovesharkSearchProvider(QObject* parent)
    : service_(NULL) {
}

void GroovesharkSearchProvider::Init(GroovesharkService* service) {
  service_ = service;
  SearchProvider::Init("Grooveshark", "grooveshark",
                       QIcon(":providers/grooveshark.png"),
                       WantsDelayedQueries | ArtIsProbablyRemote | CanShowConfig);

  connect(service_, SIGNAL(SimpleSearchResults(int, SongList)),
          SLOT(SearchDone(int, SongList)));
  connect(service_, SIGNAL(AlbumSearchResult(int, SongList)),
          SLOT(AlbumSearchResult(int, SongList)));
  connect(service_, SIGNAL(AlbumSongsLoaded(int, SongList)),
          SLOT(AlbumSongsLoaded(int, SongList)));

  cover_loader_ = new BackgroundThreadImplementation<AlbumCoverLoader, AlbumCoverLoader>(this);
  cover_loader_->Start(true);
  cover_loader_->Worker()->SetDesiredHeight(kArtHeight);
  cover_loader_->Worker()->SetPadOutputImage(true);
  cover_loader_->Worker()->SetScaleOutputImage(true);

  connect(cover_loader_->Worker().get(),
          SIGNAL(ImageLoaded(quint64, QImage)),
          SLOT(AlbumArtLoaded(quint64, QImage)));
}

void GroovesharkSearchProvider::SearchAsync(int id, const QString& query) {
  const int service_id = service_->SimpleSearch(query);
  pending_searches_[service_id] = PendingState(id, TokenizeQuery(query));;

  const int album_id = service_->SearchAlbums(query);
  pending_searches_[album_id] = PendingState(id, TokenizeQuery(query));
}

void GroovesharkSearchProvider::SearchDone(int id, const SongList& songs) {
  // Map back to the original id.
  const PendingState state = pending_searches_.take(id);
  const int global_search_id = state.orig_id_;

  SongList songs_copy(songs);
  SortSongs(&songs_copy);

  ResultList ret;
  foreach (const Song& song, songs_copy) {
    Result result(this);
    result.type_ = globalsearch::Type_Track;
    result.metadata_ = song;
    result.match_quality_ = MatchQuality(state.tokens_, song.title());

    ret << result;
  }

  emit ResultsAvailable(global_search_id, ret);
  MaybeSearchFinished(global_search_id);
}

void GroovesharkSearchProvider::AlbumSearchResult(int id, const SongList& songs) {
  // Map back to the original id.
  const PendingState state = pending_searches_.take(id);
  const int global_search_id = state.orig_id_;

  ResultList ret;
  foreach (const Song& s, songs) {
    Result result(this);
    result.type_ = globalsearch::Type_Album;
    result.metadata_ = s;
    result.match_quality_ =
        qMin(MatchQuality(state.tokens_, s.album()),
             MatchQuality(state.tokens_, s.artist()));

    ret << result;
  }

  emit ResultsAvailable(global_search_id, ret);
  MaybeSearchFinished(global_search_id);
}

void GroovesharkSearchProvider::MaybeSearchFinished(int id) {
  if (pending_searches_.keys(PendingState(id, QStringList())).isEmpty()) {
    emit SearchFinished(id);
  }
}


void GroovesharkSearchProvider::LoadArtAsync(int id, const Result& result) {
  quint64 loader_id = cover_loader_->Worker()->LoadImageAsync(result.metadata_);
  cover_loader_tasks_[loader_id] = id;
}

void GroovesharkSearchProvider::AlbumArtLoaded(quint64 id, const QImage& image) {
  if (!cover_loader_tasks_.contains(id)) {
    return;
  }
  int original_id = cover_loader_tasks_.take(id);
  emit ArtLoaded(original_id, image);
}

void GroovesharkSearchProvider::LoadTracksAsync(int id, const Result& result) {
  SongList ret;

  switch (result.type_) {
    case globalsearch::Type_Track: {
      ret << result.metadata_;
      SortSongs(&ret);

      InternetSongMimeData* mime_data = new InternetSongMimeData(service_);
      mime_data->songs = ret;

      emit TracksLoaded(id, mime_data);
      break;
    }

    case globalsearch::Type_Album:
      FetchAlbum(id, result);
      break;

    default:
      Q_ASSERT(0);
  }

}

bool GroovesharkSearchProvider::IsLoggedIn() {
  return (service_ && service_->IsLoggedIn());
}

void GroovesharkSearchProvider::ShowConfig() {
  service_->ShowConfig();
}

void GroovesharkSearchProvider::FetchAlbum(int id, const Result& result) {
  service_->FetchSongsForAlbum(id, result.metadata_.url());
}

void GroovesharkSearchProvider::AlbumSongsLoaded(int id, const SongList& songs) {
  InternetSongMimeData* mime_data = new InternetSongMimeData(service_);
  mime_data->songs = songs;
  SortSongs(&mime_data->songs);

  emit TracksLoaded(id, mime_data);
}
