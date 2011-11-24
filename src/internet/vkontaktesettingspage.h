#ifndef VKONTAKTESETTINGSPAGE_H
#define VKONTAKTESETTINGSPAGE_H

#include <QWidget>
#include <QWebView>
#include "ui/settingspage.h"

class Ui_VkontakteSettingsPage;
class VkontakteService;

struct VkontakteAuthResult {
 bool success;
 QString access_token;
 time_t expire_date;
 QString user_id;
 QString error;
};

class VkontakteAuth: public QWebView {
  Q_OBJECT
public:
  VkontakteAuth(QString app_id, QWidget* parent = 0);
  void Login();
signals:
  void LoginFinished(VkontakteAuthResult result);
private slots:
  void slotUrlChanged(QUrl url);
private:
  QString app_id_;
  bool result_sent_;
  void closeEvent(QCloseEvent *event);
};

class VkontakteSettingsPage : public SettingsPage {
  Q_OBJECT
public:
  VkontakteSettingsPage(SettingsDialog* dialog);
  ~VkontakteSettingsPage();

  void Load();
  void Save();

private slots:
  void Login();
  void Logout();
  void LoginFinished(VkontakteAuthResult result);

private:
  void UpdateLoginState();

private:
  Ui_VkontakteSettingsPage* ui_;
  VkontakteService* service_;
  VkontakteAuth* auth_;

  static const char* kSettingGroup;
  bool logged_in_;
  QString app_id_;
  QString access_token_;
  time_t expire_date_;
  QString user_id_;
};

#endif // VKONTAKTESETTINGSPAGE_H
