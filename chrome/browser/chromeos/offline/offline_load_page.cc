// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/offline/offline_load_page.h"

#include "apps/launcher.h"
#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "ash/system/tray/system_tray_delegate.h"
#include "base/i18n/rtl.h"
#include "base/metrics/histogram.h"
#include "base/prefs/pref_service.h"
#include "base/strings/string_piece.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/renderer_preferences_util.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/extensions/extension_icon_set.h"
#include "chrome/common/extensions/manifest_handlers/icons_handler.h"
#include "chrome/common/localized_error.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/interstitial_page.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/web_contents.h"
#include "grit/browser_resources.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/google_chrome_strings.h"
#include "grit/theme_resources.h"
#include "net/base/escape.h"
#include "net/base/net_errors.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/webui/jstemplate_builder.h"
#include "ui/base/webui/web_ui_util.h"

using content::BrowserThread;
using content::InterstitialPage;
using content::WebContents;

namespace {

// A utility function to set the dictionary's value given by |resource_id|.
void SetString(DictionaryValue* strings, const char* name, int resource_id) {
  strings->SetString(name, l10n_util::GetStringUTF16(resource_id));
}

}  // namespace

namespace chromeos {

OfflineLoadPage::OfflineLoadPage(WebContents* web_contents,
                                 const GURL& url,
                                 const CompletionCallback& callback)
    : callback_(callback),
      proceeded_(false),
      web_contents_(web_contents),
      url_(url) {
  net::NetworkChangeNotifier::AddConnectionTypeObserver(this);
  interstitial_page_ = InterstitialPage::Create(web_contents, true, url, this);
}

OfflineLoadPage::~OfflineLoadPage() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  net::NetworkChangeNotifier::RemoveConnectionTypeObserver(this);
}

void OfflineLoadPage::Show() {
  interstitial_page_->Show();
}

std::string OfflineLoadPage::GetHTMLContents() {
  // Use a local error page.
  int resource_id;
  base::DictionaryValue error_strings;

  // The offline page for app has icons and slightly different message.
  Profile* profile = Profile::FromBrowserContext(
      web_contents_->GetBrowserContext());
  DCHECK(profile);
  const extensions::Extension* extension = NULL;
  ExtensionService* extensions_service =
      extensions::ExtensionSystem::Get(profile)->extension_service();

  // Extension service does not exist in test.
  if (extensions_service)
    extension = extensions_service->extensions()->GetHostedAppByURL(url_);

  if (extension && !extension->from_bookmark()) {
    LocalizedError::GetAppErrorStrings(url_, extension, &error_strings);
    resource_id = IDR_OFFLINE_APP_LOAD_HTML;
  } else {
    const std::string locale = g_browser_process->GetApplicationLocale();
    const std::string accept_languages =
        profile->GetPrefs()->GetString(prefs::kAcceptLanguages);
    LocalizedError::GetStrings(net::ERR_INTERNET_DISCONNECTED,
                               net::kErrorDomain, url_, false, locale,
                               accept_languages, &error_strings);
    resource_id = IDR_OFFLINE_NET_LOAD_HTML;
  }

  const base::StringPiece template_html(
      ResourceBundle::GetSharedInstance().GetRawDataResource(
          resource_id));
  // "t" is the id of the templates root node.
  return webui::GetTemplatesHtml(template_html, &error_strings, "t");
}

 void OfflineLoadPage::OverrideRendererPrefs(
      content::RendererPreferences* prefs) {
  Profile* profile = Profile::FromBrowserContext(
      web_contents_->GetBrowserContext());
  renderer_preferences_util::UpdateFromSystemSettings(prefs, profile);
}

void OfflineLoadPage::OnProceed() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  proceeded_ = true;
  NotifyBlockingPageComplete(true);
}

void OfflineLoadPage::OnDontProceed() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  // Ignore if it's already proceeded.
  if (proceeded_)
    return;
  NotifyBlockingPageComplete(false);
}

void OfflineLoadPage::CommandReceived(const std::string& cmd) {
  std::string command(cmd);
  // The Jasonified response has quotes, remove them.
  if (command.length() > 1 && command[0] == '"') {
    command = command.substr(1, command.length() - 2);
  }
  // TODO(oshima): record action for metrics.
  if (command == "open_network_settings") {
    ash::Shell::GetInstance()->system_tray_delegate()->ShowNetworkSettings("");
  } else if (command == "open_connectivity_diagnostics") {
    Profile* profile = Profile::FromBrowserContext(
        web_contents_->GetBrowserContext());
    const extensions::Extension* extension = profile->GetExtensionService()->
        GetInstalledExtension("kodldpbjkkmmnilagfdheibampofhaom");
    apps::LaunchPlatformAppWithUrl(profile, extension, "",
                                   GURL::EmptyGURL(), GURL::EmptyGURL());

  } else {
    LOG(WARNING) << "Unknown command:" << cmd;
  }
}

void OfflineLoadPage::NotifyBlockingPageComplete(bool proceed) {
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE, base::Bind(callback_, proceed));
}

void OfflineLoadPage::OnConnectionTypeChanged(
    net::NetworkChangeNotifier::ConnectionType type) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  const bool online = type != net::NetworkChangeNotifier::CONNECTION_NONE;
  DVLOG(1) << "ConnectionTypeObserver notification received: state="
           << (online ? "online" : "offline");
  if (online) {
    net::NetworkChangeNotifier::RemoveConnectionTypeObserver(this);
    interstitial_page_->Proceed();
  }
}

}  // namespace chromeos
