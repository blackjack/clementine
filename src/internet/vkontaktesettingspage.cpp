#include "vkontaktesettingspage.h"
#include "ui_vkontaktesettingspage.h"
#include "vkontakteservice.h"
#include "internetmodel.h"
#include "core/closure.h"
#include <QSettings>
#include <QtDebug>
#include <QWebView>
#include <QCloseEvent>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QNetworkCookieJar>
#include <QXmlStreamReader>
#include <QImageReader>

VkontakteAuth::VkontakteAuth(const QString &app_id, QObject *parent)
  :  QObject(parent),app_id_(app_id)
{
  network_ = new QNetworkAccessManager(this);
//  connect(this,SIGNAL(urlChanged(QUrl)),SLOT(slotUrlChanged(QUrl)));
}

void VkontakteAuth::Login(const QString &username, const QString &password) {
  username_ = username;
  password_ = password;
  delete network_->cookieJar(); //clear cookies before auth
  network_->setCookieJar(new QNetworkCookieJar(network_));
  QUrl url("http://api.vkontakte.ru/oauth/authorize");
  url.addQueryItem("client_id",app_id_);
  url.addQueryItem("scope","audio,friends");
  url.addQueryItem("redirect_url","http://api.vkontakte.ru/blank.html");
  url.addQueryItem("display","wap");
  url.addQueryItem("response_type","token");
  QNetworkReply* reply = network_->get(QNetworkRequest(url));
  NewClosure(reply,SIGNAL(finished()),this,SLOT(replyGot(QNetworkReply*)),reply);
}

void VkontakteAuth::replyGot(QNetworkReply *reply) {
  reply->deleteLater();

  QVariant redirect = reply->attribute(QNetworkRequest::RedirectionTargetAttribute);
  if (!redirect.isNull()) {
      QUrl new_url = reply->url().resolved(redirect.toUrl());
      QNetworkReply* redirect_query = network_->get(QNetworkRequest(new_url));
      NewClosure(redirect_query,SIGNAL(finished()),this,SLOT(replyGot(QNetworkReply*)),redirect_query);
      return;
  }

  QString html = QString::fromUtf8(reply->readAll()).replace("&","&amp;"); //workaround to pass XML validation
  QUrl url(reply->url().toString().replace('#','?')); //in some cases url comes as http://url#item=vaule&item=value and QUrl cannot nicely work with this

  if (url.path()=="/oauth/authorize"){
      ParseForm(html);
  } else if (url.path()=="/blank.html") {
    VkontakteAuthResult result;
    if (!url.queryItemValue("error").isEmpty()) {
      result.success = false;
      result.error = url.queryItemValue("error_description").replace('+',' ');
    } else {
      result.success = true;
      result.access_token = url.queryItemValue("access_token");
      result.expire_date = QDateTime::currentDateTimeUtc().addSecs(url.queryItemValue("expires_in").toInt());
      result.user_id = url.queryItemValue("user_id");
    }
    emit LoginFinished(result);
  } else {
    VkontakteAuthResult result;
    result.success = false;
    result.error = url.queryItemValue("error_description").replace('+',' ');
    emit LoginFinished(result);
  }

}

void VkontakteAuth::ParseForm(const QString &html) {
  QXmlStreamReader reader(html);

  QUrl post_url;
  QUrl post_data;
  bool captcha_requested = false;
  while (!reader.atEnd()) {
    reader.readNext();
    if (reader.isStartElement()) {
      if (reader.name() == "form")
        post_url = QUrl(reader.attributes().value("action").toString());
      else if (reader.name() == "img") {
        QString src = reader.attributes().value("src").toString();
        if (src.contains("captcha.php")) {
          captcha_requested = true;
          RequestCaptcha( QUrl(src) );
        }
      }
      else if(reader.name() == "input") {
        QXmlStreamAttributes attrs = reader.attributes();
        if (attrs.value("type") == "hidden")
          post_data.addQueryItem(attrs.value("name").toString(),attrs.value("value").toString());
        else if (attrs.value("name") == "pass")
          post_data.addQueryItem("pass",password_);
        else if (attrs.value("name") == "email")
          post_data.addQueryItem("email",username_);
      }
      else if (reader.name() == "div" &&
               reader.attributes().value("style").toString().indexOf("#E89B88")!=-1) {
        //search for red-coloured box
        VkontakteAuthResult result;
        result.success = false;
        reader.readNext();
        if (reader.isCharacters())
          result.error = reader.text().toString();
        emit LoginFinished(result);
        return;
      }
    }
  }
  if (!captcha_requested) {
    QNetworkReply* post_reply = network_->post(QNetworkRequest(post_url),post_data.encodedQuery());
    NewClosure(post_reply,SIGNAL(finished()),this,SLOT(replyGot(QNetworkReply*)),post_reply);
  } else {
    post_url_ = post_url;
    post_data_ = post_data;
  }
}

void VkontakteAuth::RequestCaptcha(QUrl url) {
  QNetworkReply* reply = network_->get(QNetworkRequest(url));
  NewClosure(reply,SIGNAL(finished()),this,SLOT(captchaGot(QNetworkReply*)),reply);
}

void VkontakteAuth::captchaGot(QNetworkReply *reply) {
  reply->deleteLater();
  if (reply->error() == QNetworkReply::NoError) {
    QImageReader reader(reply);
    QPixmap pixmap = QPixmap::fromImageReader(&reader);
    emit CaptchaRequested(pixmap);
  }
}

void VkontakteAuth::CaptchaEntered(const QString &text) {
  post_data_.addQueryItem("captcha_key",text);
  QString data = post_data_.encodedQuery();
  QNetworkReply* post_reply = network_->post(QNetworkRequest(post_url_),post_data_.encodedQuery());
  NewClosure(post_reply,SIGNAL(finished()),this,SLOT(replyGot(QNetworkReply*)),post_reply);
}

VkontakteSettingsPage::VkontakteSettingsPage(SettingsDialog* dialog)
  : SettingsPage(dialog),
    ui_(new Ui_VkontakteSettingsPage),
    service_(InternetModel::Service<VkontakteService>()),
    logged_in_(false)
{
  ui_->setupUi(this);
  setWindowIcon(QIcon(":/providers/vkontakte.png"));

  connect(service_,SIGNAL(FullNameReceived(QString,QString)),SLOT(FullNameReceived(QString,QString)));

  ui_->captcha->hide();
  ui_->captcha_label->hide();
  ui_->captcha_desc_label->hide();
  ui_->details_label->hide();

  connect(ui_->login, SIGNAL(clicked()), ui_->login_state, SIGNAL(LoginClicked()));
  connect(ui_->login_state, SIGNAL(LoginClicked()), SLOT(Login()));
  connect(ui_->login_state, SIGNAL(LogoutClicked()), SLOT(Logout()));
  connect(ui_->captcha, SIGNAL(returnPressed()),SLOT(CaptchaEntered()));

  ui_->login_state->AddCredentialGroup(ui_->login_container);
  ui_->login_state->AddCredentialField(ui_->login);
  ui_->login_state->AddCredentialField(ui_->password);
  ui_->login_state->AddCredentialField(ui_->captcha);

  auth_ = new VkontakteAuth(VkontakteService::kApplicationId,0);
  connect(auth_,SIGNAL(CaptchaRequested(QPixmap)),SLOT(CaptchaRequested(QPixmap)));
  connect(auth_,SIGNAL(LoginFinished(VkontakteAuthResult)),SLOT(LoginFinished(VkontakteAuthResult)));
}

VkontakteSettingsPage::~VkontakteSettingsPage() {
  delete ui_;
}

void VkontakteSettingsPage::UpdateLoginState() {
  QString user = full_name_.isEmpty() ? user_id_ : full_name_;
  ui_->login_state->SetLoggedIn(logged_in_ ? LoginStateWidget::LoggedIn
                                           : LoginStateWidget::LoggedOut,
                                user);
  ui_->login->setEnabled(!logged_in_);
  QString details = logged_in_ ?
        tr("Logged until %1").arg(expire_date_.toLocalTime().toString()):
        login_error_;
  ui_->details_label->setText(details);
  ui_->details_label->setVisible(logged_in_ || !login_error_.isEmpty());
  service_->ReloadSettings();
}

void VkontakteSettingsPage::Login() {
  ui_->details_label->hide();
  ui_->login_state->SetLoggedIn(LoginStateWidget::LoginInProgress);
  auth_->Login(ui_->username->text(),ui_->password->text());
}

void VkontakteSettingsPage::Logout() {
  logged_in_ = false;
  user_id_.clear();
  full_name_.clear();
  access_token_.clear();
  expire_date_ = QDateTime();
  Save();
  UpdateLoginState();
}

void VkontakteSettingsPage::LoginFinished(VkontakteAuthResult result) {
  if (result.success) {
    user_id_ = result.user_id;
    access_token_ = result.access_token;
    expire_date_ = result.expire_date;
    logged_in_ = (QDateTime::currentDateTimeUtc()<expire_date_ && !user_id_.isEmpty() );
    service_->GetFullNameAsync(user_id_);
  }
  else {
    logged_in_ = false;
    login_error_ = result.error;
  }
  UpdateLoginState();
}

void VkontakteSettingsPage::Load() {
  QSettings s;
  s.beginGroup(VkontakteService::kSettingsGroup);
  user_id_ = s.value("user_id").toString();
  expire_date_ = s.value("expire_date").toDateTime();
  access_token_ = s.value("access_token").toString();
  ui_->username->setText(s.value("username").toString());
  ui_->password->setText(s.value("password").toString());

  logged_in_ = (QDateTime::currentDateTimeUtc()<expire_date_ && !user_id_.isEmpty() );

  service_->GetFullNameAsync(user_id_);
  UpdateLoginState();
}

void VkontakteSettingsPage::Save() {
  QSettings s;
  s.beginGroup(VkontakteService::kSettingsGroup);
  s.setValue("user_id",user_id_);
  s.setValue("expire_date",expire_date_);
  s.setValue("access_token",access_token_);
  if (logged_in_) {
    s.setValue("username",ui_->username->text());
    s.setValue("password",ui_->password->text());
  }

  service_->ReloadSettings();
}

void VkontakteSettingsPage::CaptchaRequested(const QPixmap &pix) {
  ui_->captcha_label->setPixmap(pix);
  ui_->captcha->show();
  ui_->captcha_label->show();
  ui_->captcha_desc_label->show();

  disconnect(ui_->login_state,SIGNAL(LoginClicked()),this,SLOT(Login()));
  connect(ui_->login_state,SIGNAL(LoginClicked()),this,SLOT(CaptchaEntered()));
}

void VkontakteSettingsPage::CaptchaEntered() {
  auth_->CaptchaEntered(ui_->captcha->text());
  ui_->captcha->hide();
  ui_->captcha_desc_label->hide();
  ui_->captcha_label->hide();
  disconnect(ui_->login_state,SIGNAL(LoginClicked()),this,SLOT(CaptchaEntered()));
  connect(ui_->login_state,SIGNAL(LoginClicked()),this,SLOT(Login()));
}

void VkontakteSettingsPage::FullNameReceived(const QString &user_id, const QString &full_name) {
  full_name_ = full_name;
  UpdateLoginState();
}

