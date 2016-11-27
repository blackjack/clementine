#include "googlemusicservice.h"

#include "googlemusicurlhandler.h"
#include "core/application.h"
#include "core/taskmanager.h"

GoogleMusicUrlHandler::GoogleMusicUrlHandler( Application* app, GoogleMusicService* service )
  : UrlHandler( service ),
    app_( app ),
    service_( service ),
    task_id_( -1 )
{
}

UrlHandler::LoadResult GoogleMusicUrlHandler::StartLoading( const QUrl& url )
{
  LoadResult ret( url );

  if ( task_id_ != -1 ) {
    return ret;
  }

  // Start loading the station
  service_->GetSongUrl( url );

  // Save the URL so we can emit it in the finished signal later
  last_original_url_ = url;

  // Tell the user what's happening
  task_id_ = app_->task_manager()->StartTask( tr( "Loading stream" ) );

  ret.type_ = LoadResult::WillLoadAsynchronously;
  return ret;
}

void GoogleMusicUrlHandler::TrackLoadFinished( QUrl url )
{
  if ( task_id_ == -1 ) {
    return;
  }

  // Stop the spinner in the status bar
  CancelTask();

  emit AsyncLoadComplete( LoadResult(
                            last_original_url_, LoadResult::TrackAvailable, url ) );
}

void GoogleMusicUrlHandler::CancelTask()
{
  app_->task_manager()->SetTaskFinished( task_id_ );
  task_id_ = -1;
}
