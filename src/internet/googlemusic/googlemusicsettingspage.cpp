#include "googlemusicsettingspage.h"
#include "googlemusicservice.h"
#include "googlemusicclient.h"

#include "core/network.h"
#include "ui/iconloader.h"
#include "internet/core/internetmodel.h"
#include "ui_googlemusicsettingspage.h"

#include <QMessageBox>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QNetworkCookieJar>
#include <QSettings>
#include <QtDebug>


GoogleMusicSettingsPage::GoogleMusicSettingsPage(SettingsDialog* dialog)
  : SettingsPage(dialog),
    network_(new NetworkAccessManager(this)),
    ui_(new Ui_GoogleMusicSettingsPage),
    logged_in_(false)
{
  ui_->setupUi(this);
  setWindowIcon(IconLoader::Load("googlemusic", IconLoader::IconType::Provider));

  connect(ui_->login, SIGNAL(clicked()), SLOT(Login()));
  connect(ui_->login_state, SIGNAL(LoginClicked()), SLOT(Login()));
  connect(ui_->login_state, SIGNAL(LogoutClicked()), SLOT(Logout()));

  ui_->login_state->AddCredentialField(ui_->username);
  ui_->login_state->AddCredentialField(ui_->password);
  ui_->login_state->AddCredentialGroup(ui_->login_container);
}

GoogleMusicSettingsPage::~GoogleMusicSettingsPage() {
  delete ui_;
}

void GoogleMusicSettingsPage::UpdateLoginState() {
  ui_->login_state->SetLoggedIn(logged_in_ ? LoginStateWidget::LoggedIn
                                           : LoginStateWidget::LoggedOut,
                                ui_->username->text());
}

void GoogleMusicSettingsPage::Login() {
  ui_->login_state->SetLoggedIn(LoginStateWidget::LoginInProgress);

  GoogleAuthorizer* auth = new GoogleAuthorizer(this);
  auth->Authorize(ui_->username->text(),ui_->password->text());

  connect(auth, SIGNAL(Authorized(QByteArray,QByteArray,QByteArray)),
          this, SLOT(LoggedIn(QByteArray,QByteArray,QByteArray)));
  connect(auth, SIGNAL(Error()), this, SLOT(LoginFailed()));

}

void GoogleMusicSettingsPage::Logout() {
  logged_in_ = false;
  InternetModel::Service<GoogleMusicService>()->ForgetCredentials();
  UpdateLoginState();
}

void GoogleMusicSettingsPage::LoggedIn(QByteArray auth, QByteArray xt, QByteArray sjsaid)
{
    auth_ = auth;
    xt_cookie_ = xt;
    sjsaid_cookie_ = sjsaid;
    logged_in_ = true;
    Save();
    UpdateLoginState();

    sender()->deleteLater();
}

void GoogleMusicSettingsPage::LoginFailed()
{
    logged_in_ = false;
    QMessageBox::warning(this, tr("Authentication failed"), tr("Your Google Music credentials were incorrect"));
    UpdateLoginState();

    sender()->deleteLater();
}

void GoogleMusicSettingsPage::Load() {
  QSettings s;
  s.beginGroup(GoogleMusicService::kSettingsGroup);

  ui_->username->setText(s.value("username").toString());
  ui_->password->setText(s.value("password").toString());

  auth_ = s.value("auth").toByteArray();
  xt_cookie_ = s.value("xt_cookie").toByteArray();
  sjsaid_cookie_ = s.value("sjsaid_cookie").toByteArray();

  logged_in_ = s.value("logged_in",
                       !auth_.isEmpty() &&
                       !xt_cookie_.isEmpty() &&
                       !sjsaid_cookie_.isEmpty()).toBool();

  UpdateLoginState();
}

void GoogleMusicSettingsPage::Save() {
  QSettings s;
  s.beginGroup(GoogleMusicService::kSettingsGroup);

  s.setValue("username", ui_->username->text());
  s.setValue("password", ui_->password->text());
  s.setValue("auth",auth_);
  s.setValue("xt_cookie",xt_cookie_);
  s.setValue("sjsaid_cookie",sjsaid_cookie_);
  s.setValue("logged_in", logged_in_);

  InternetModel::Service<GoogleMusicService>()->ReloadSettings();
}


