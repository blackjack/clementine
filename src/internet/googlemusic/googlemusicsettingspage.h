#ifndef GOOGLEMUSICSETTINGSPAGE_H
#define GOOGLEMUSICSETTINGSPAGE_H

#include "ui/settingspage.h"

class QAuthenticator;
class QNetworkReply;

class NetworkAccessManager;
class Ui_GoogleMusicSettingsPage;

class GoogleMusicSettingsPage : public SettingsPage {
  Q_OBJECT
public:
  GoogleMusicSettingsPage(SettingsDialog* dialog);
  ~GoogleMusicSettingsPage();

  void Load();
  void Save();

private slots:
  void Login();
  void Logout();

  void LoggedIn(QByteArray auth, QByteArray xt, QByteArray sjsaid);
  void LoginFailed();

private:
  QNetworkReply* CheckRedirect(QNetworkReply* reply);
  void UpdateLoginState();

private:
  NetworkAccessManager* network_;
  Ui_GoogleMusicSettingsPage* ui_;

  QByteArray auth_;
  QByteArray xt_cookie_;
  QByteArray sjsaid_cookie_;
  bool logged_in_;
};

#endif
