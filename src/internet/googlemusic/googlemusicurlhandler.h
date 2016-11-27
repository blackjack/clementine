#ifndef GOOGLEMUSICURLHANDLER_H
#define GOOGLEMUSICURLHANDLER_H

#include "core/urlhandler.h"
#include "ui/iconloader.h"

class GoogleMusicService;
class Application;


class GoogleMusicUrlHandler : public UrlHandler
{
public:
  GoogleMusicUrlHandler( Application* app, GoogleMusicService* service );

  QString scheme() const { return "googlemusic"; }
  QIcon icon() const { return IconLoader::Load( "googlemusic", IconLoader::IconType::Provider ); }
  LoadResult StartLoading( const QUrl& url );

  void CancelTask();
  void TrackLoadFinished( QUrl url );

private:
  Application* app_;
  GoogleMusicService* service_;
  int task_id_;

  QUrl last_original_url_;
};

#endif
