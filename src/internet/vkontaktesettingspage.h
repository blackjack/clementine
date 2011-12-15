#ifndef VKONTAKTESETTINGSPAGE_H
#define VKONTAKTESETTINGSPAGE_H

#include <QWidget>
#include <QTextBrowser>
#include "ui/settingspage.h"

class Ui_VkontakteSettingsPage;
class VkontakteService;
class QNetworkAccessManager;
class QNetworkReply;

struct VkontakteAuthResult {
 bool success;
 QString access_token;
 QDateTime expire_date;
 QString user_id;
 QString error;
};

class VkontakteAuth: public QObject {
  Q_OBJECT
public:
  VkontakteAuth(const QString& app_id, QObject* parent = 0);
  void Login(const QString& username, const QString& password);
  void CaptchaEntered(const QString& text);
signals:
  void CaptchaRequested(const QPixmap& img);
  void LoginFinished(VkontakteAuthResult result);
private slots:
  void replyGot(QNetworkReply* reply);
  void captchaGot(QNetworkReply* reply);
private:
  QString app_id_;
  QString username_;
  QString password_;
  QUrl post_data_;
  QUrl post_url_;
  QNetworkAccessManager* network_;
  void RequestCaptcha(QUrl url);
  void ParseForm(const QString& html);
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
  void CaptchaRequested(const QPixmap& pix);
  void CaptchaEntered();
  void FullNameReceived(const QString& full_name);
private:
  void UpdateLoginState();

private:
  Ui_VkontakteSettingsPage* ui_;
  VkontakteService* service_;
  VkontakteAuth* auth_;

  static const char* kSettingGroup;
  bool logged_in_;
  QString login_error_;
  QString app_id_;
  QString access_token_;
  QDateTime expire_date_;
  QString user_id_;
  QString full_name_;
};

#endif // VKONTAKTESETTINGSPAGE_H
