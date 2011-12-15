#include "vkontakteworker.h"
#include "vkontakteservice.h"
#include "core/throttlednetworkmanager.h"
#include "core/closure.h"
#include "playlist/playlist.h"

#include <QUrl>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QStandardItem>
#include <QTimer>

VkontakteSearchWorker::VkontakteSearchWorker(QObject *parent)
  : VkontakteApiWorker(parent)
{
  timer_ = new QTimer(this);
}

void VkontakteSearchWorker::Search(const QString &text, bool now) {
  pending_search_ = text;
  if (now) {
    DoSearch();
  } else {
    if (!timer_->isActive())
      timer_->singleShot(1000/VkontakteService::kMaxRequestsPerSecond,this,SLOT(DoSearch()));
  }
}

void VkontakteSearchWorker::DoSearch() {
  if (!pending_search_.isEmpty()) {
    SearchAsync(pending_search_);
  }
}

void VkontakteWorker::MultiAddToMyTracks(const QList<QUrl> &urls, const QString &user_id) {
  left_to_add_=0;
  QRegExp rx(VkontakteService::kCommentInfoRegexp);
  foreach (QUrl url, urls) {
    if (url.hasFragment() && rx.indexIn(url.fragment()) >-1) {
      QString song_id = rx.cap(1);
      QString owner_id = rx.cap(2);
      if (owner_id!=user_id) {
        ++left_to_add_;
        AddToMyTracksAsync(song_id,owner_id);
      }
    }
  }
  if (left_to_add_ == 0) {
    emit MultiAddedToMyTracks();
    return;
  }
  connect(this,SIGNAL(AddedToMyTracks(QString)),SLOT(DecreaseLeftToAddCounter()));
}

void VkontakteWorker::DecreaseLeftToAddCounter() {
  --left_to_add_;
  if (left_to_add_==0)
    emit MultiAddedToMyTracks();
}

void VkontakteWorker::MultiRemoveFromMyTracks(const QList<QUrl> &urls, const QString &user_id) {
  left_to_remove_=0;
  QRegExp rx(VkontakteService::kCommentInfoRegexp);
  foreach (QUrl url, urls) {
    if (url.hasFragment() && rx.indexIn(url.fragment()) >-1) {
      QString song_id = rx.cap(1);
      QString owner_id = rx.cap(2);
      if (owner_id==user_id) {
        ++left_to_remove_;
        RemoveFromMyTracksAsync(song_id,owner_id);
      }
    }
  }
  if (left_to_remove_ == 0) {
    emit MultiRemovedFromMyTracks();
    return;
  }
  connect(this,SIGNAL(RemovedFromMyTracks()),SLOT(DecreaseLeftToRemoveCounter()));
}

void VkontakteWorker::DecreaseLeftToRemoveCounter() {
  --left_to_remove_;
  if (left_to_remove_==0)
    emit MultiRemovedFromMyTracks();
}


void VkontakteWorker::MultiMoveToAlbum(const QList<QUrl>& songs, const QString& user_id, const QString& album_id) {
  target_album_id_ = album_id;
  QRegExp rx(VkontakteService::kCommentInfoRegexp);
  QList<QUrl> songs_to_add;
  foreach (QUrl url, songs) {
    if (url.hasFragment() && rx.indexIn(url.fragment()) >-1) {
      QString song_id = rx.cap(1);
      QString owner_id = rx.cap(2);
      QString alb_id = rx.cap(3);

      if (owner_id!=user_id) {
        songs_to_add << url;
      } else {
        if (album_id != alb_id)
          songs_ids_to_move_+=song_id+',';
      }
    }
  }
  if (!songs_to_add.isEmpty()) {
    MultiAddToMyTracks(songs_to_add,user_id);
    connect(this,SIGNAL(AddedToMyTracks(QString)),SLOT(AppendAddedSongToMove(QString)));
    connect(this,SIGNAL(MultiAddedToMyTracks()),SLOT(MoveAddedSongsToAlbum()));
  }
  else {
    MoveAddedSongsToAlbum();
  }
  connect(this,SIGNAL(MovedToAlbum()),SIGNAL(MultiMovedToAlbum()));
}

void VkontakteWorker::AppendAddedSongToMove(const QString &song_id) {
  songs_ids_to_move_+=song_id+",";
}

void VkontakteWorker::MoveAddedSongsToAlbum() {
  if (!songs_ids_to_move_.isEmpty())
    songs_ids_to_move_.remove(songs_ids_to_move_.size()-1,1); //remove last comma
  MoveToAlbumAsync(songs_ids_to_move_,target_album_id_);
}


void VkontakteApiWorker::GetTracksAsync(const QString& user_id) {
  QUrl request("https://api.vkontakte.ru/method/audio.get.xml");
  request.addQueryItem("uid",user_id);
  request.addQueryItem("access_token",access_token_);

  NetworkReply* reply = network_->throttledGet(QNetworkRequest(request));
  connect(reply,SIGNAL(finished(QNetworkReply*)),SLOT(GetTracksFinished(QNetworkReply*)));
}

void VkontakteApiWorker::GetTracksFinished(QNetworkReply* reply) {
  if (reply->error() != QNetworkReply::NoError) {
    emit Error(reply->errorString());
    return;
  }

  QDomElement root = GetRootElement(reply);
  if (root.isNull())
    return;

  SongList songs;
  QDomNodeList tracks = root.elementsByTagName("audio");
  for(int i = 0; i< tracks.size(); ++i) {
    songs << ParseSong(tracks.at(i).firstChildElement());
  }
  emit TracksGot(songs);
  reply->deleteLater();
}

void VkontakteApiWorker::GetFriendsAsync(const QString& user_id) {
  QUrl request("https://api.vkontakte.ru/method/friends.get.xml");
  request.addQueryItem("uid",user_id);
  request.addQueryItem("fields","uid,first_name,last_name");
  request.addQueryItem("access_token",access_token_);
  NetworkReply* reply = network_->throttledGet(QNetworkRequest(request));
  connect(reply,SIGNAL(finished(QNetworkReply*)),SLOT(GetFriendsFinished(QNetworkReply*)));
}

void VkontakteApiWorker::GetFriendsFinished(QNetworkReply* reply) {
  if (reply->error() != QNetworkReply::NoError) {
    emit Error(reply->errorString());
    return;
  }

  QList<QStringList> result;
  QDomElement root = GetRootElement(reply);
  if (root.isNull())
    return;
  QDomNodeList users = root.elementsByTagName("user");
  for(int i = 0; i< users.size(); ++i) {
    QDomElement info = users.at(i).firstChildElement();
    QString first_name, last_name, uid;
    while (!info.isNull()) {
      if (info.tagName() == "uid") {
        uid = info.text();
      } else if (info.tagName() == "first_name") {
        first_name = info.text();
      } else if (info.tagName() == "last_name") {
        last_name = info.text();
      }
      info = info.nextSiblingElement();
    }
    QStringList item;

    if (first_name.isEmpty() || last_name.isEmpty())
      first_name+last_name;
    else
      item << uid << first_name+" "+last_name;

    result << item;
  }
  emit FriendsGot(result);
  reply->deleteLater();
}


void VkontakteApiWorker::GetAlbumsAsync(const QString& user_id) {
  QUrl request("https://api.vkontakte.ru/method/audio.getAlbums.xml");
  request.addQueryItem("uid",user_id);
  request.addQueryItem("count",QString::number(VkontakteService::kMaxAlbumsPerRequest));
  request.addQueryItem("access_token",access_token_);
  NetworkReply* reply = network_->throttledGet(QNetworkRequest(request));
  connect(reply,SIGNAL(finished(QNetworkReply*)),SLOT(GetAlbumsFinished(QNetworkReply*)));
}

void VkontakteApiWorker::GetAlbumsFinished(QNetworkReply *reply) {
  if (reply->error() != QNetworkReply::NoError) {
    emit Error(reply->errorString());
    return;
  }

  QDomElement root = GetRootElement(reply);
  if (root.isNull())
    return;

  QList<QStringList> result; //list_of_owner_album_title
  QDomNodeList albums = root.elementsByTagName("album");
  for(int i = 0; i< albums.size(); ++i) {
    QDomElement info = albums.at(i).firstChildElement();
    QString owner_id,album_id,title;
    while (!info.isNull()) {
      if (info.tagName() == "owner_id") {
        owner_id=info.text();
      } else if (info.tagName() == "album_id") {
        album_id=info.text();
      } else if (info.tagName() == "title") {
        title = ReplaceXMLChars(info.text());
      }
      info = info.nextSiblingElement();
    }

    QStringList item;
    item << owner_id << album_id << title;
    result << item;
  }
  emit AlbumsGot(result);
  reply->deleteLater();
}


void VkontakteApiWorker::SearchAsync(const QString& text, int offset, int count) {
  QUrl request("https://api.vkontakte.ru/method/audio.search.xml");
  request.addQueryItem("q",text);
  request.addQueryItem("sort","2");
  request.addQueryItem("count",QString::number(count));
  request.addQueryItem("auto_complete","1");
  request.addQueryItem("offset",QString::number(offset));
  request.addQueryItem("access_token",access_token_);

  NetworkReply* reply = network_->throttledGet(QNetworkRequest(request));
  connect (reply,SIGNAL(finished(QNetworkReply*)),SLOT(SearchFinished(QNetworkReply*)));
}

void VkontakteApiWorker::SearchFinished(QNetworkReply* reply) {
  if (reply->error() != QNetworkReply::NoError) {
    emit Error(reply->errorString());
    return;
  }

  QDomElement root = GetRootElement(reply);
  if (root.isNull())
    return;
  QDomNodeList tracks = root.elementsByTagName("audio");
  SongList result;

  for(int i = 0; i< tracks.size(); ++i) {
    result << ParseSong(tracks.at(i).firstChildElement());
  }
  emit SearchResultsGot(result);
  reply->deleteLater();
}

void VkontakteApiWorker::CreateAlbumAsync(const QString &title) {
  QUrl request("https://api.vkontakte.ru/method/audio.addAlbum.xml");
  request.addQueryItem("title",title);
  request.addQueryItem("access_token",access_token_);

  NetworkReply* reply = network_->throttledGet(QNetworkRequest(request));
  connect (reply,SIGNAL(finished(QNetworkReply*)),SLOT(CreateAlbumFinished(QNetworkReply*)));
}

void VkontakteApiWorker::CreateAlbumFinished(QNetworkReply* reply) {
  if (reply->error() != QNetworkReply::NoError) {
    emit Error(reply->errorString());
    return;
  }

  QDomElement root = GetRootElement(reply);
  if (root.isNull())
    return;
  emit AlbumCreated(root.elementsByTagName("album_id").at(0).toElement().text());
  reply->deleteLater();
}

void VkontakteApiWorker::DeleteAlbumAsync(const QString &album_id) {
  QUrl request("https://api.vkontakte.ru/method/audio.deleteAlbum.xml");
  request.addQueryItem("album_id",album_id);
  request.addQueryItem("access_token",access_token_);

  NetworkReply* reply = network_->throttledGet(QNetworkRequest(request));
  connect (reply,SIGNAL(finished(QNetworkReply*)),SLOT(DeleteAlbumFinished(QNetworkReply*)));
}

void VkontakteApiWorker::DeleteAlbumFinished(QNetworkReply* reply) {
  if (reply->error() != QNetworkReply::NoError) {
    emit Error(reply->errorString());
    return;
  }

  QDomElement root = GetRootElement(reply);
  if (root.isNull())
    return;
  emit AlbumDeleted();
  reply->deleteLater();
}

void VkontakteApiWorker::EditAlbumNameAsync(const QString& album_id, const QString& title) {
  QUrl request("https://api.vkontakte.ru/method/audio.editAlbum.xml");
  request.addQueryItem("album_id",album_id);
  request.addQueryItem("title",title);
  request.addQueryItem("access_token",access_token_);

  NetworkReply* reply = network_->throttledGet(QNetworkRequest(request));
  connect (reply,SIGNAL(finished(QNetworkReply*)),SLOT(EditAlbumNameFinished(QNetworkReply*)));
}

void VkontakteApiWorker::EditAlbumNameFinished(QNetworkReply* reply) {
  if (reply->error() != QNetworkReply::NoError) {
    emit Error(reply->errorString());
    return;
  }

  QDomElement root = GetRootElement(reply);
  if (root.isNull())
    return;
  emit AlbumNameEdited();
  reply->deleteLater();
}

void VkontakteApiWorker::AddToMyTracksAsync(const QString& song_id, const QString& owner_id) {
  QUrl request("https://api.vkontakte.ru/method/audio.add.xml");
  request.addQueryItem("aid",song_id);
  request.addQueryItem("oid",owner_id);
  request.addQueryItem("access_token",access_token_);

  NetworkReply* reply = network_->throttledGet(QNetworkRequest(request));
  connect (reply,SIGNAL(finished(QNetworkReply*)),SLOT(AddToMyTracksFinished(QNetworkReply*)));
}

void VkontakteApiWorker::AddToMyTracksFinished(QNetworkReply* reply) {
  if (reply->error() != QNetworkReply::NoError) {
    emit Error(reply->errorString());
    return;
  }

  QDomElement root = GetRootElement(reply);
  if (root.isNull())
    return;

  QString song_id = root.text();
  emit AddedToMyTracks(song_id);
  reply->deleteLater();
}

void VkontakteApiWorker::RemoveFromMyTracksAsync(const QString& song_id, const QString& owner_id) {
  QUrl request("https://api.vkontakte.ru/method/audio.delete.xml");
  request.addQueryItem("aid",song_id);
  request.addQueryItem("oid",owner_id);
  request.addQueryItem("access_token",access_token_);

  NetworkReply* reply = network_->throttledGet(QNetworkRequest(request));
  connect (reply,SIGNAL(finished(QNetworkReply*)),SLOT(RemoveFromMyTracksFinished(QNetworkReply*)));
}

void VkontakteApiWorker::RemoveFromMyTracksFinished(QNetworkReply* reply) {
  if (reply->error() != QNetworkReply::NoError) {
    emit Error(reply->errorString());
    return;
  }

  QDomElement root = GetRootElement(reply);
  if (root.isNull())
    return;

  emit RemovedFromMyTracks();
  reply->deleteLater();
}

void VkontakteApiWorker::MoveToAlbumAsync(const QString &song_ids, const QString &album_id) {
  QUrl request("https://api.vkontakte.ru/method/audio.moveToAlbum.xml");
  request.addQueryItem("aids",song_ids);
  if (!album_id.isEmpty())
    request.addQueryItem("album_id",album_id); //if no album it deletes tracks from any
  request.addQueryItem("access_token",access_token_);

  NetworkReply* reply = network_->throttledGet(QNetworkRequest(request));
  connect (reply,SIGNAL(finished(QNetworkReply*)),SLOT(MoveToAlbumFinished(QNetworkReply*)));
}


void VkontakteApiWorker::MoveToAlbumFinished(QNetworkReply* reply) {
  if (reply->error() != QNetworkReply::NoError) {
    emit Error(reply->errorString());
    return;
  }

  QDomElement root = GetRootElement(reply);
  if (root.isNull())
    return;

  emit MovedToAlbum();
  reply->deleteLater();
}

void VkontakteApiWorker::GetFullNameAsync(const QString& user_id) {
  QUrl request("https://api.vkontakte.ru/method/getProfiles.xml");
  request.addQueryItem("uids",user_id);
  request.addQueryItem("fields","first_name,last_name");
  request.addQueryItem("access_token",access_token_);
  NetworkReply* reply = network_->throttledGet(QNetworkRequest(request));

  connect(reply,SIGNAL(finished(QNetworkReply*)),SLOT(GetFullNameFinished(QNetworkReply*)));
}


void VkontakteApiWorker::GetFullNameFinished(QNetworkReply* reply) {
  if (reply->error() != QNetworkReply::NoError) {
    emit Error(reply->errorString());
    return;
  }

  QDomElement root = GetRootElement(reply);
  if (root.isNull())
    return;
  QDomElement user = root.elementsByTagName("user").at(0).toElement().firstChildElement();
  QString first_name,last_name;
  while (!user.isNull()) {
    if (user.tagName() == "first_name") {
      first_name = user.text();
    } else if (user.tagName() == "last_name") {
      last_name = user.text();
    }
    user = user.nextSiblingElement();
  }
  if (first_name.isEmpty() || last_name.isEmpty())
   emit FullNameGot(first_name+last_name);
  else
    emit FullNameGot(first_name+" "+last_name);
  reply->deleteLater();
}

QDomElement VkontakteApiWorker::GetRootElement(QNetworkReply* data) {
  QByteArray arr = data->readAll();
  QDomDocument doc;
  doc.setContent(arr.trimmed());
  QDomElement root = doc.documentElement();
  if (root.tagName()=="response") {
    return root;
  } else if (root.tagName()=="error") {
    QDomElement error_code = root.elementsByTagName("error_code").at(0).toElement();
    switch (error_code.text().toInt()) {
    case 4:
    case 5:
      emit AuthError();
    default:
      emit Error(root.elementsByTagName("error_msg").at(0).toElement().text());
    }
  }
  return QDomElement();
}

Song VkontakteApiWorker::ParseSong(QDomElement info) {
  Song song;
  QString song_id, owner_id, album_id;
  while (!info.isNull()) {
    if (info.tagName() == "artist") {
      song.set_artist(ReplaceXMLChars(info.text()));
    } else if (info.tagName() == "title") {
      song.set_title(ReplaceXMLChars(info.text()));
    } else if (info.tagName() == "url") {
      song.set_url(QUrl(info.text()));
    } else if (info.tagName() == "album") {
      album_id = info.text(); //don't set Album field because we get album_id
    } else if (info.tagName() == "aid") {
      song_id = info.text();
    } else if (info.tagName() == "owner_id") {
      owner_id = info.text();
    } else if (info.tagName() == "duration") {
      song.set_length_nanosec(info.text().toLongLong()*1e9);
    }
    info = info.nextSiblingElement();
  }
  QUrl url = song.url();
  url.setFragment(QString("song_id=%1|owner_id=%2|album_id=%3").arg(song_id,owner_id,album_id));
  song.set_url(url);
  return song;
}

QString VkontakteApiWorker::ReplaceXMLChars(QString s) {
  s.replace("&#39;","\'");
  s.replace("&quot;","\"");
  s.replace("&lt;","<");
  s.replace("&gt;",">");
  s.replace("&amp;","&");
  return s;
}
