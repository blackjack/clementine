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

#include "librarysearchprovider.h"
#include "core/logging.h"
#include "covers/albumcoverloader.h"
#include "library/librarybackend.h"
#include "library/libraryquery.h"
#include "library/sqlrow.h"
#include "playlist/songmimedata.h"


LibrarySearchProvider::LibrarySearchProvider(LibraryBackendInterface* backend,
                                             const QString& name,
                                             const QString& id,
                                             const QIcon& icon,
                                             bool enabled_by_default,
                                             QObject* parent)
  : BlockingSearchProvider(parent),
    backend_(backend)
{
  Hints hints = WantsSerialisedArtQueries | ArtIsInSongMetadata |
                CanGiveSuggestions;

  if (!enabled_by_default) {
    hints |= DisabledByDefault;
  }

  Init(name, id, icon, hints);
}

SearchProvider::ResultList LibrarySearchProvider::Search(int id, const QString& query) {
  QueryOptions options;
  options.set_filter(query);

  LibraryQuery q(options);
  q.SetColumnSpec("%songs_table.ROWID, " + Song::kColumnSpec);

  if (!backend_->ExecQuery(&q)) {
    return ResultList();
  }

  const QStringList tokens = TokenizeQuery(query);

  QMultiMap<QString, Song> albums;
  QSet<QString> albums_with_non_track_matches;

  ResultList ret;

  while (q.Next()) {
    Song song;
    song.InitFromQuery(q, true);

    QString album_key = song.album();
    if (song.is_compilation() && !song.albumartist().isEmpty()) {
      album_key.prepend(song.albumartist() + " - ");
    } else if (!song.is_compilation()) {
      album_key.prepend(song.artist());
    }

    globalsearch::MatchQuality quality = MatchQuality(tokens, song.title());

    if (quality != globalsearch::Quality_None) {
      // If the query matched in the song title then we're interested in this
      // as an individual song.
      Result result(this);
      result.type_ = globalsearch::Type_Track;
      result.metadata_ = song;
      result.match_quality_ = quality;
      ret << result;
    } else {
      // Otherwise we record this as being an interesting album.
      albums_with_non_track_matches.insert(album_key);
    }

    albums.insertMulti(album_key, song);
  }

  // Add any albums that contained least one song that wasn't matched on the
  // song title.
  foreach (const QString& key, albums_with_non_track_matches) {
    Result result(this);
    result.type_ = globalsearch::Type_Album;
    result.metadata_ = albums.value(key);
    result.album_size_ = albums.count(key);
    result.match_quality_ =
        qMin(
          MatchQuality(tokens, result.metadata_.albumartist()),
          qMin(MatchQuality(tokens, result.metadata_.artist()),
               MatchQuality(tokens, result.metadata_.album())));

    result.album_songs_ = albums.values(key);
    SortSongs(&result.album_songs_);

    ret << result;
  }

  return ret;
}

void LibrarySearchProvider::LoadTracksAsync(int id, const Result& result) {
  SongList ret;

  switch (result.type_) {
  case globalsearch::Type_Track:
    // This is really easy - we just emit the track again.
    ret << result.metadata_;
    break;

  case globalsearch::Type_Album: {
    // Find all the songs in this album.
    LibraryQuery query;
    query.SetColumnSpec("ROWID, " + Song::kColumnSpec);
    query.AddCompilationRequirement(result.metadata_.is_compilation());
    query.AddWhere("album", result.metadata_.album());

    if (!result.metadata_.is_compilation())
      query.AddWhere("artist", result.metadata_.artist());

    if (!backend_->ExecQuery(&query)) {
      break;
    }

    while (query.Next()) {
      Song song;
      song.InitFromQuery(query, true);
      ret << song;
    }
  }

  default:
    break;
  }

  SortSongs(&ret);

  SongMimeData* mime_data = new SongMimeData;
  mime_data->backend = backend_;
  mime_data->songs = ret;

  emit TracksLoaded(id, mime_data);
}

QString LibrarySearchProvider::GetSuggestion() {
  // We'd like to use order by random(), but that's O(n) in sqlite, so instead
  // get the largest ROWID and pick a couple of random numbers within that
  // range.

  LibraryQuery q;
  q.SetColumnSpec("ROWID");
  q.SetOrderBy("ROWID DESC");
  q.SetIncludeUnavailable(true);
  q.SetLimit(1);

  if (!backend_->ExecQuery(&q) || !q.Next()) {
    return QString();
  }

  const int largest_rowid = q.Value(0).toInt();

  for (int attempt=0 ; attempt<10 ; ++attempt) {
    LibraryQuery q;
    q.SetColumnSpec("artist, album");
    q.SetIncludeUnavailable(true);
    q.AddWhere("ROWID", qrand() % largest_rowid);
    q.SetLimit(1);

    if (!backend_->ExecQuery(&q) || !q.Next()) {
      continue;
    }

    const QString artist = q.Value(0).toString();
    const QString album  = q.Value(1).toString();

    if (!artist.isEmpty() && !album.isEmpty())
      return (qrand() % 2 == 0) ? artist : album;
    else if (!artist.isEmpty())
      return artist;
    else if (!album.isEmpty())
      return album;
  }

  return QString();
}
