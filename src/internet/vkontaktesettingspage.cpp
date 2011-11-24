#include "vkontaktesettingspage.h"
#include "ui_vkontaktesettingspage.h"
#include "vkontakteservice.h"
#include "internetmodel.h"

#include <QSettings>
#include <QtDebug>
#include <QWebView>
#include <QCloseEvent>

VkontakteAuth::VkontakteAuth(QString app_id, QWidget *parent)
  :  QWebView(parent), app_id_(app_id),result_sent_(false)
{
  connect(this,SIGNAL(urlChanged(QUrl)),SLOT(slotUrlChanged(QUrl)));
}

void VkontakteAuth::Login() {
  result_sent_ = false;
  QUrl url("http://api.vkontakte.ru/oauth/authorize");
  url.addQueryItem("client_id",app_id_);
  url.addQueryItem("scope","audio,friends");
  url.addQueryItem("redirect_url","http://api.vkontakte.ru/blank.html");
  url.addQueryItem("display","popup");
  url.addQueryItem("response_type","token");
  load(url);
  show();
}

void VkontakteAuth::closeEvent(QCloseEvent *event) {
  if (!result_sent_) {
    result_sent_ = true;
    VkontakteAuthResult result;
    result.success = false;
    emit LoginFinished(result);
  }
  event->accept();
}

void VkontakteAuth::slotUrlChanged(QUrl url) {
  if (url.hasFragment())
    url = QUrl(url.toString().replace("#","?"));

  QString s = url.toString();
  if (url.path()=="/blank.html") {
    VkontakteAuthResult result;

    if (!url.queryItemValue("error").isEmpty()) {
      result.success = false;
      result.error = url.queryItemValue("error_description").replace("+"," ");
    }
    else {
      result.success = true;
      result.access_token = url.queryItemValue("access_token");
      result.expire_date = time(NULL)+url.queryItemValue("expires_in").toInt();
      result.user_id = url.queryItemValue("user_id");
    }
    result_sent_ = true;
    close();
    emit LoginFinished(result);
  } else if (url.path()=="/oauth/grant_access")
    close();
}

VkontakteSettingsPage::VkontakteSettingsPage(SettingsDialog* dialog)
  : SettingsPage(dialog),
    ui_(new Ui_VkontakteSettingsPage),
    service_(InternetModel::Service<VkontakteService>()),
    logged_in_(false),
    expire_date_(0)
{
  ui_->setupUi(this);
  setWindowIcon(QIcon(":/providers/vkontakte.png"));

  connect(ui_->login, SIGNAL(clicked()), SLOT(Login()));
  connect(ui_->login_state, SIGNAL(LoginClicked()), SLOT(Login()));
  connect(ui_->login_state, SIGNAL(LogoutClicked()), SLOT(Logout()));

  ui_->login_state->AddCredentialGroup(ui_->login);

  auth_ = new VkontakteAuth("2677518",0);
  connect(auth_,SIGNAL(LoginFinished(VkontakteAuthResult)),SLOT(LoginFinished(VkontakteAuthResult)));
}

VkontakteSettingsPage::~VkontakteSettingsPage() {
  delete ui_;
}

void VkontakteSettingsPage::UpdateLoginState() {
  ui_->login_state->SetLoggedIn(logged_in_ ? LoginStateWidget::LoggedIn
                                           : LoginStateWidget::LoggedOut,
                                user_id_);
  ui_->login->setEnabled(!logged_in_);
  QString details = logged_in_ ?
        tr("Logged until %1").arg(QDateTime::fromTime_t(expire_date_).toLocalTime().toString()):
        tr("Not logged in");
  ui_->details_label->setText(details);

  service_->ReloadSettings();
}

void VkontakteSettingsPage::Login() {
  auth_->Login();
}

void VkontakteSettingsPage::Logout() {
  logged_in_ = false;
  user_id_.clear();
  access_token_.clear();
  expire_date_ = 0;
  UpdateLoginState();
}

void VkontakteSettingsPage::LoginFinished(VkontakteAuthResult result) {
  if (result.success) {
    user_id_ = result.user_id;
    access_token_ = result.access_token;
    expire_date_ = result.expire_date;
    logged_in_ = (time(NULL)<expire_date_ && !user_id_.isEmpty() );
  }
  else
    logged_in_ = false;
  UpdateLoginState();
}

void VkontakteSettingsPage::Load() {
  QSettings s;
  s.beginGroup(VkontakteService::kSettingsGroup);
  user_id_ = s.value("user_id").toString();
  expire_date_ = s.value("expire_date",0).toInt();
  access_token_ = s.value("access_token").toString();

  logged_in_ = (time(NULL)<expire_date_ && !user_id_.isEmpty() );
  UpdateLoginState();
}

void VkontakteSettingsPage::Save() {
  QSettings s;
  s.beginGroup(VkontakteService::kSettingsGroup);
  s.setValue("user_id",user_id_);
  s.setValue("expire_date",(int)expire_date_);
  s.setValue("access_token",access_token_);

  service_->ReloadSettings();
}

