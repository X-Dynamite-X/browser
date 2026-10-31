#include "../core/document_viewer.hpp"
#include "../core/fs_utils.hpp"
#include "../core/internal_pages.hpp"
#include "../core/storage.hpp"
#include "../core/url_utils.hpp"

#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dlfcn.h>
#include <memory>
#include <string>
#include <vector>

namespace {

// Dynamic WebKit2GTK C API loader (compatible with both webkit2gtk-4.1 and webkit2gtk-4.0
// without requiring -dev header packages at runtime or build time).
struct WebKitApi {
    void* handle = nullptr;

    void* (*web_context_get_default)(void) = nullptr;
    void (*web_context_set_cache_model)(void*, int) = nullptr;
    void* (*web_context_download_uri)(void*, const char*) = nullptr;

    GtkWidget* (*web_view_new)(void) = nullptr;
    void (*web_view_load_uri)(GtkWidget*, const char*) = nullptr;
    void (*web_view_load_html)(GtkWidget*, const char*, const char*) = nullptr;
    void (*web_view_go_back)(GtkWidget*) = nullptr;
    void (*web_view_go_forward)(GtkWidget*) = nullptr;
    gboolean (*web_view_can_go_back)(GtkWidget*) = nullptr;
    gboolean (*web_view_can_go_forward)(GtkWidget*) = nullptr;
    void (*web_view_reload)(GtkWidget*) = nullptr;
    void (*web_view_stop_loading)(GtkWidget*) = nullptr;
    const char* (*web_view_get_uri)(GtkWidget*) = nullptr;
    const char* (*web_view_get_title)(GtkWidget*) = nullptr;
    double (*web_view_get_estimated_load_progress)(GtkWidget*) = nullptr;
    double (*web_view_get_zoom_level)(GtkWidget*) = nullptr;
    void (*web_view_set_zoom_level)(GtkWidget*, double) = nullptr;
    void* (*web_view_get_settings)(GtkWidget*) = nullptr;

    void (*settings_set_enable_javascript)(void*, gboolean) = nullptr;
    void (*settings_set_enable_webgl)(void*, gboolean) = nullptr;
    void (*settings_set_enable_webaudio)(void*, gboolean) = nullptr;
    void (*settings_set_enable_media)(void*, gboolean) = nullptr;
    void (*settings_set_enable_mediasource)(void*, gboolean) = nullptr;
    void (*settings_set_enable_media_stream)(void*, gboolean) = nullptr;
    void (*settings_set_enable_fullscreen)(void*, gboolean) = nullptr;
    void (*settings_set_enable_smooth_scrolling)(void*, gboolean) = nullptr;
    void (*settings_set_enable_developer_extras)(void*, gboolean) = nullptr;
    void (*settings_set_allow_file_access_from_file_urls)(void*, gboolean) = nullptr;
    void (*settings_set_hardware_acceleration_policy)(void*, int) = nullptr;
    void (*settings_set_user_agent)(void*, const char*) = nullptr;

    void* (*navigation_policy_decision_get_navigation_action)(void*) = nullptr;
    void* (*navigation_action_get_request)(void*) = nullptr;
    void* (*response_policy_decision_get_request)(void*) = nullptr;
    gboolean (*response_policy_decision_is_mime_type_supported)(void*) = nullptr;
    const char* (*uri_request_get_uri)(void*) = nullptr;
    void (*policy_decision_use)(void*) = nullptr;
    void (*policy_decision_ignore)(void*) = nullptr;
    void (*policy_decision_download)(void*) = nullptr;

    void* (*download_get_request)(void*) = nullptr;
    void* (*download_get_response)(void*) = nullptr;
    uint64_t (*download_get_received_data_length)(void*) = nullptr;
    void (*download_set_destination)(void*, const char*) = nullptr;
    void (*download_set_allow_overwrite)(void*, gboolean) = nullptr;
    uint64_t (*uri_response_get_content_length)(void*) = nullptr;
    const char* (*uri_response_get_suggested_filename)(void*) = nullptr;

    bool load() {
        const char* candidates[] = {
            "libwebkit2gtk-4.1.so.0",
            "libwebkit2gtk-4.0.so.37",
            "libwebkit2gtk-4.1.so",
            "libwebkit2gtk-4.0.so"
        };
        for (const char* name : candidates) {
            handle = dlopen(name, RTLD_LAZY | RTLD_GLOBAL);
            if (handle) break;
        }
        if (!handle) return false;

#define BIND_WK(field, sym) \
    field = reinterpret_cast<decltype(field)>(dlsym(handle, sym))

        BIND_WK(web_context_get_default, "webkit_web_context_get_default");
        BIND_WK(web_context_set_cache_model, "webkit_web_context_set_cache_model");
        BIND_WK(web_context_download_uri, "webkit_web_context_download_uri");

        BIND_WK(web_view_new, "webkit_web_view_new");
        BIND_WK(web_view_load_uri, "webkit_web_view_load_uri");
        BIND_WK(web_view_load_html, "webkit_web_view_load_html");
        BIND_WK(web_view_go_back, "webkit_web_view_go_back");
        BIND_WK(web_view_go_forward, "webkit_web_view_go_forward");
        BIND_WK(web_view_can_go_back, "webkit_web_view_can_go_back");
        BIND_WK(web_view_can_go_forward, "webkit_web_view_can_go_forward");
        BIND_WK(web_view_reload, "webkit_web_view_reload");
        BIND_WK(web_view_stop_loading, "webkit_web_view_stop_loading");
        BIND_WK(web_view_get_uri, "webkit_web_view_get_uri");
        BIND_WK(web_view_get_title, "webkit_web_view_get_title");
        BIND_WK(web_view_get_estimated_load_progress, "webkit_web_view_get_estimated_load_progress");
        BIND_WK(web_view_get_zoom_level, "webkit_web_view_get_zoom_level");
        BIND_WK(web_view_set_zoom_level, "webkit_web_view_set_zoom_level");
        BIND_WK(web_view_get_settings, "webkit_web_view_get_settings");

        BIND_WK(settings_set_enable_javascript, "webkit_settings_set_enable_javascript");
        BIND_WK(settings_set_enable_webgl, "webkit_settings_set_enable_webgl");
        BIND_WK(settings_set_enable_webaudio, "webkit_settings_set_enable_webaudio");
        BIND_WK(settings_set_enable_media, "webkit_settings_set_enable_media");
        BIND_WK(settings_set_enable_mediasource, "webkit_settings_set_enable_mediasource");
        BIND_WK(settings_set_enable_media_stream, "webkit_settings_set_enable_media_stream");
        BIND_WK(settings_set_enable_fullscreen, "webkit_settings_set_enable_fullscreen");
        BIND_WK(settings_set_enable_smooth_scrolling, "webkit_settings_set_enable_smooth_scrolling");
        BIND_WK(settings_set_enable_developer_extras, "webkit_settings_set_enable_developer_extras");
        BIND_WK(settings_set_allow_file_access_from_file_urls, "webkit_settings_set_allow_file_access_from_file_urls");
        BIND_WK(settings_set_hardware_acceleration_policy, "webkit_settings_set_hardware_acceleration_policy");
        BIND_WK(settings_set_user_agent, "webkit_settings_set_user_agent");

        BIND_WK(navigation_policy_decision_get_navigation_action, "webkit_navigation_policy_decision_get_navigation_action");
        BIND_WK(navigation_action_get_request, "webkit_navigation_action_get_request");
        BIND_WK(response_policy_decision_get_request, "webkit_response_policy_decision_get_request");
        BIND_WK(response_policy_decision_is_mime_type_supported, "webkit_response_policy_decision_is_mime_type_supported");
        BIND_WK(uri_request_get_uri, "webkit_uri_request_get_uri");
        BIND_WK(policy_decision_use, "webkit_policy_decision_use");
        BIND_WK(policy_decision_ignore, "webkit_policy_decision_ignore");
        BIND_WK(policy_decision_download, "webkit_policy_decision_download");

        BIND_WK(download_get_request, "webkit_download_get_request");
        BIND_WK(download_get_response, "webkit_download_get_response");
        BIND_WK(download_get_received_data_length, "webkit_download_get_received_data_length");
        BIND_WK(download_set_destination, "webkit_download_set_destination");
        BIND_WK(download_set_allow_overwrite, "webkit_download_set_allow_overwrite");
        BIND_WK(uri_response_get_content_length, "webkit_uri_response_get_content_length");
        BIND_WK(uri_response_get_suggested_filename, "webkit_uri_response_get_suggested_filename");

#undef BIND_WK
        return web_view_new && web_view_load_uri && web_view_load_html && web_context_get_default;
    }
};

WebKitApi g_wk;

struct BrowserApp;

struct TabContext {
    BrowserApp* app = nullptr;
    int tabId = 0;
    GtkWidget* container = nullptr;
    GtkWidget* webView = nullptr;
    GtkWidget* tabLabel = nullptr;
    std::string logicalUrl = "lite://home";
    std::string activeTempUrl;
};

struct DownloadBinding {
    BrowserApp* app = nullptr;
    int recordId = 0;
};

struct BrowserApp {
    litebrowser::StorageManager storage;
    GtkWidget* window = nullptr;
    GtkWidget* notebook = nullptr;
    GtkWidget* urlEntry = nullptr;
    GtkWidget* btnBack = nullptr;
    GtkWidget* btnForward = nullptr;
    GtkWidget* btnBookmark = nullptr;
    GtkWidget* btnLiteMode = nullptr;
    GtkWidget* statusLabel = nullptr;
    std::vector<std::unique_ptr<TabContext>> tabs;
    int nextTabId = 1;

    TabContext* currentTab() {
        int page = gtk_notebook_get_current_page(GTK_NOTEBOOK(notebook));
        if (page < 0) return nullptr;
        GtkWidget* child = gtk_notebook_get_nth_page(GTK_NOTEBOOK(notebook), page);
        for (auto& t : tabs) {
            if (t->container == child) return t.get();
        }
        return nullptr;
    }
};

std::string makeUniqueDownloadPath(const std::string& baseDir, const std::string& rawName) {
    std::string safeName = rawName.empty() ? "downloaded_file" : litebrowser::fs::getFileName(rawName);
    if (safeName.empty()) safeName = "downloaded_file";
    std::string candidate = litebrowser::fs::joinPath(baseDir, safeName);
    if (!litebrowser::fs::fileExists(candidate)) return candidate;

    std::string ext = litebrowser::fs::getExtensionLower(safeName);
    std::string stem = ext.empty() ? safeName : safeName.substr(0, safeName.size() - ext.size());
    for (int i = 1; i < 1000; ++i) {
        std::string numbered = litebrowser::fs::joinPath(baseDir, stem + "(" + std::to_string(i) + ")" + ext);
        if (!litebrowser::fs::fileExists(numbered)) return numbered;
    }
    return candidate;
}

void applyEngineSettingsToView(GtkWidget* webView, const litebrowser::BrowserSettings& s) {
    if (!g_wk.web_view_get_settings) return;
    void* settings = g_wk.web_view_get_settings(webView);
    if (!settings) return;

    if (g_wk.settings_set_enable_javascript) g_wk.settings_set_enable_javascript(settings, TRUE);
    if (g_wk.settings_set_enable_media) g_wk.settings_set_enable_media(settings, TRUE);
    if (g_wk.settings_set_enable_mediasource) g_wk.settings_set_enable_mediasource(settings, TRUE);
    if (g_wk.settings_set_enable_media_stream) g_wk.settings_set_enable_media_stream(settings, TRUE);
    if (g_wk.settings_set_enable_webaudio) g_wk.settings_set_enable_webaudio(settings, TRUE);
    if (g_wk.settings_set_enable_fullscreen) g_wk.settings_set_enable_fullscreen(settings, TRUE);
    if (g_wk.settings_set_allow_file_access_from_file_urls) g_wk.settings_set_allow_file_access_from_file_urls(settings, TRUE);
    if (g_wk.settings_set_enable_developer_extras) g_wk.settings_set_enable_developer_extras(settings, !s.liteMode);
    if (g_wk.settings_set_enable_smooth_scrolling) g_wk.settings_set_enable_smooth_scrolling(settings, !s.liteMode);
    if (g_wk.settings_set_enable_webgl) g_wk.settings_set_enable_webgl(settings, TRUE);

    if (g_wk.settings_set_hardware_acceleration_policy) {
        // 0 = ON_DEMAND (saves VRAM in liteMode while still running WebGL), 2 = NEVER
        g_wk.settings_set_hardware_acceleration_policy(settings, s.liteMode ? 2 : 0);
    }
    if (g_wk.settings_set_user_agent) {
        g_wk.settings_set_user_agent(
            settings,
            "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/605.1.15 (KHTML, like Gecko) Version/17.4 Safari/605.1.15 LiteBrowser/1.0");
    }

    if (g_wk.web_context_set_cache_model) {
        // 0 = DOCUMENT_VIEWER (minimal memory footprint), 2 = WEB_BROWSER
        g_wk.web_context_set_cache_model(g_wk.web_context_get_default(), s.liteMode ? 0 : 2);
    }
}

void setStatus(BrowserApp* app, const std::string& msg) {
    if (app && app->statusLabel) {
        gtk_label_set_text(GTK_LABEL(app->statusLabel), msg.c_str());
    }
}

void updateToolbarForTab(BrowserApp* app, TabContext* tab) {
    if (!app || !tab) return;
    gtk_entry_set_text(GTK_ENTRY(app->urlEntry), tab->logicalUrl.c_str());
    if (g_wk.web_view_can_go_back) {
        gtk_widget_set_sensitive(app->btnBack, g_wk.web_view_can_go_back(tab->webView));
    }
    if (g_wk.web_view_can_go_forward) {
        gtk_widget_set_sensitive(app->btnForward, g_wk.web_view_can_go_forward(tab->webView));
    }
    bool starred = app->storage.isBookmarked(tab->logicalUrl);
    gtk_button_set_label(GTK_BUTTON(app->btnBookmark), starred ? "★" : "☆");
}

void navigateTab(TabContext* tab, const std::string& rawInput);
TabContext* createNewTab(BrowserApp* app, const std::string& initialUrl, bool switchToIt = true);
void promptOpenDocument(BrowserApp* app);

void openDownloadedItem(BrowserApp* app, int dlId) {
    auto rec = app->storage.getDownloadById(dlId);
    if (rec.id == 0 || rec.savePath.empty()) return;
    if (litebrowser::isSupportedDocumentFile(rec.savePath) && litebrowser::fs::fileExists(rec.savePath)) {
        createNewTab(app, rec.savePath, true);
        return;
    }
    std::string uri = litebrowser::fs::pathToFileUrl(rec.savePath);
    gtk_show_uri_on_window(GTK_WINDOW(app->window), uri.c_str(), GDK_CURRENT_TIME, nullptr);
}

void handleLiteAction(TabContext* tab, const std::string& actionUri) {
    BrowserApp* app = tab->app;
    std::string rest = actionUri.substr(std::strlen("lite-action://"));

    if (litebrowser::fs::startsWithIgnoreCase(rest, "open-file")) {
        promptOpenDocument(app);
    } else if (litebrowser::fs::startsWithIgnoreCase(rest, "toggle-lite")) {
        auto s = app->storage.getSettings();
        s.liteMode = !s.liteMode;
        app->storage.updateSettings(s);
        gtk_button_set_label(GTK_BUTTON(app->btnLiteMode), s.liteMode ? "⚡ خفيف: مفعّل" : "⚡ خفيف");
        for (auto& t : app->tabs) applyEngineSettingsToView(t->webView, s);
        setStatus(app, s.liteMode ? "تم تفعيل الوضع الخفيف لتوفير الذاكرة والمعالج" : "تم التبديل إلى الوضع الكامل");
        navigateTab(tab, tab->logicalUrl);
    } else if (litebrowser::fs::startsWithIgnoreCase(rest, "toggle-adblock")) {
        auto s = app->storage.getSettings();
        s.adBlockEnabled = !s.adBlockEnabled;
        app->storage.updateSettings(s);
        setStatus(app, s.adBlockEnabled ? "تم تفعيل مانع الإعلانات" : "تم إيقاف مانع الإعلانات");
        navigateTab(tab, tab->logicalUrl);
    } else if (litebrowser::fs::startsWithIgnoreCase(rest, "download?url=")) {
        std::string encoded = rest.substr(std::strlen("download?url="));
        std::string dlUrl = litebrowser::fs::urlDecode(encoded);
        if (!dlUrl.empty() && g_wk.web_context_download_uri) {
            if (!litebrowser::fs::startsWithIgnoreCase(dlUrl, "http://") &&
                !litebrowser::fs::startsWithIgnoreCase(dlUrl, "https://")) {
                dlUrl = "https://" + dlUrl;
            }
            g_wk.web_context_download_uri(g_wk.web_context_get_default(), dlUrl.c_str());
            setStatus(app, "بدأ التحميل: " + dlUrl);
            navigateTab(tab, "lite://downloads");
        }
    } else if (litebrowser::fs::startsWithIgnoreCase(rest, "open-downloads-dir")) {
        std::string uri = litebrowser::fs::pathToFileUrl(litebrowser::fs::getDownloadsDir());
        gtk_show_uri_on_window(GTK_WINDOW(app->window), uri.c_str(), GDK_CURRENT_TIME, nullptr);
    } else if (litebrowser::fs::startsWithIgnoreCase(rest, "open-downloaded?id=")) {
        int id = std::atoi(rest.c_str() + std::strlen("open-downloaded?id="));
        openDownloadedItem(app, id);
    } else if (litebrowser::fs::startsWithIgnoreCase(rest, "remove-bookmark?idx=")) {
        size_t idx = static_cast<size_t>(std::strtoul(rest.c_str() + std::strlen("remove-bookmark?idx="), nullptr, 10));
        app->storage.removeBookmarkByIndex(idx);
        navigateTab(tab, "lite://bookmarks");
    } else if (litebrowser::fs::startsWithIgnoreCase(rest, "clear-history")) {
        app->storage.clearHistory();
        navigateTab(tab, "lite://history");
    }
}

void navigateTab(TabContext* tab, const std::string& rawInput) {
    if (!tab) return;
    BrowserApp* app = tab->app;
    auto settings = app->storage.getSettings();
    auto resolved = litebrowser::resolveOmniboxInput(rawInput, settings.liteMode, settings.searchEngine);

    if (resolved.type == litebrowser::OmniboxActionType::InternalPage) {
        auto page = litebrowser::renderInternalPage(resolved.targetUrlOrPath, app->storage, tab->tabId);
        tab->logicalUrl = resolved.targetUrlOrPath;
        tab->activeTempUrl = page.tempFileUrl;
        gtk_label_set_text(GTK_LABEL(tab->tabLabel), page.title.c_str());
        g_wk.web_view_load_uri(tab->webView, page.tempFileUrl.c_str());
        updateToolbarForTab(app, tab);
        setStatus(app, page.title);
    } else if (resolved.type == litebrowser::OmniboxActionType::OpenLocalDocument) {
        setStatus(app, "جاري تجهيز المستند للقراءة...");
        auto doc = litebrowser::renderDocumentToHtml(resolved.targetUrlOrPath, tab->tabId);
        tab->logicalUrl = resolved.targetUrlOrPath;
        tab->activeTempUrl = doc.tempFileUrl;
        gtk_label_set_text(GTK_LABEL(tab->tabLabel), doc.tabTitle.c_str());
        g_wk.web_view_load_uri(tab->webView, doc.tempFileUrl.c_str());
        updateToolbarForTab(app, tab);
        setStatus(app, "تم فتح المستند: " + doc.tabTitle);
    } else {
        tab->logicalUrl = resolved.targetUrlOrPath;
        tab->activeTempUrl.clear();
        g_wk.web_view_load_uri(tab->webView, resolved.targetUrlOrPath.c_str());
        updateToolbarForTab(app, tab);
        setStatus(app, "جاري التحميل: " + resolved.targetUrlOrPath);
    }
}

void onWebViewLoadChanged(GtkWidget* webView, int loadEvent, gpointer userData) {
    auto* tab = static_cast<TabContext*>(userData);
    BrowserApp* app = tab->app;
    const char* uri = g_wk.web_view_get_uri ? g_wk.web_view_get_uri(webView) : nullptr;
    if (uri && *uri) {
        std::string u(uri);
        if (u != tab->activeTempUrl && u.find("litebrowser_session_") == std::string::npos) {
            tab->logicalUrl = u;
        }
    }
    if (loadEvent == 3) {  // WEBKIT_LOAD_FINISHED
        if (tab == app->currentTab()) {
            gtk_entry_set_progress_fraction(GTK_ENTRY(app->urlEntry), 0.0);
            updateToolbarForTab(app, tab);
        }
        const char* title = g_wk.web_view_get_title ? g_wk.web_view_get_title(webView) : nullptr;
        std::string tStr = (title && *title) ? title : tab->logicalUrl;
        app->storage.addHistory(tStr, tab->logicalUrl);
        if (tab == app->currentTab()) setStatus(app, "جاهز");
    }
}

void onWebViewNotifyTitle(GObject*, GParamSpec*, gpointer userData) {
    auto* tab = static_cast<TabContext*>(userData);
    const char* title = g_wk.web_view_get_title ? g_wk.web_view_get_title(tab->webView) : nullptr;
    if (title && *title) {
        std::string shortTitle(title);
        if (shortTitle.size() > 36) shortTitle = shortTitle.substr(0, 33) + "...";
        gtk_label_set_text(GTK_LABEL(tab->tabLabel), shortTitle.c_str());
    }
}

void onWebViewNotifyProgress(GObject*, GParamSpec*, gpointer userData) {
    auto* tab = static_cast<TabContext*>(userData);
    if (tab != tab->app->currentTab() || !g_wk.web_view_get_estimated_load_progress) return;
    double p = g_wk.web_view_get_estimated_load_progress(tab->webView);
    gtk_entry_set_progress_fraction(GTK_ENTRY(tab->app->urlEntry), (p < 1.0) ? p : 0.0);
}

gboolean onWebViewDecidePolicy(GtkWidget*, void* decision, int decisionType, gpointer userData) {
    auto* tab = static_cast<TabContext*>(userData);
    BrowserApp* app = tab->app;

    if (decisionType == 0 || decisionType == 1) {  // NAVIGATION_ACTION or NEW_WINDOW_ACTION
        if (!g_wk.navigation_policy_decision_get_navigation_action || !g_wk.navigation_action_get_request || !g_wk.uri_request_get_uri) {
            return FALSE;
        }
        void* navAct = g_wk.navigation_policy_decision_get_navigation_action(decision);
        void* req = navAct ? g_wk.navigation_action_get_request(navAct) : nullptr;
        const char* uriC = req ? g_wk.uri_request_get_uri(req) : nullptr;
        if (!uriC) return FALSE;
        std::string uri(uriC);

        if (litebrowser::fs::startsWithIgnoreCase(uri, "lite-action://")) {
            g_wk.policy_decision_ignore(decision);
            handleLiteAction(tab, uri);
            return TRUE;
        }
        if (litebrowser::fs::startsWithIgnoreCase(uri, "lite://")) {
            g_wk.policy_decision_ignore(decision);
            if (decisionType == 1) createNewTab(app, uri, true);
            else navigateTab(tab, uri);
            return TRUE;
        }
        if (app->storage.getSettings().adBlockEnabled && litebrowser::shouldBlockAdOrTracker(uri)) {
            g_wk.policy_decision_ignore(decision);
            return TRUE;
        }
        if (litebrowser::fs::startsWithIgnoreCase(uri, "file://") &&
            uri.find("litebrowser_session_") == std::string::npos) {
            std::string path = litebrowser::fs::fileUrlToPath(uri);
            if (litebrowser::isSupportedDocumentFile(path) && litebrowser::fs::fileExists(path)) {
                g_wk.policy_decision_ignore(decision);
                navigateTab(tab, path);
                return TRUE;
            }
        }
        if (decisionType == 1) {
            g_wk.policy_decision_ignore(decision);
            createNewTab(app, uri, true);
            return TRUE;
        }
    } else if (decisionType == 2) {  // WEBKIT_POLICY_DECISION_TYPE_RESPONSE
        if (g_wk.response_policy_decision_is_mime_type_supported &&
            !g_wk.response_policy_decision_is_mime_type_supported(decision)) {
            g_wk.policy_decision_download(decision);
            return TRUE;
        }
        if (g_wk.response_policy_decision_get_request && g_wk.uri_request_get_uri) {
            void* req = g_wk.response_policy_decision_get_request(decision);
            const char* uriC = req ? g_wk.uri_request_get_uri(req) : nullptr;
            if (uriC && litebrowser::isDirectDownloadExtension(uriC)) {
                g_wk.policy_decision_download(decision);
                return TRUE;
            }
        }
    }
    return FALSE;
}

gboolean onDownloadDecideDestination(void* download, const char* suggestedFilename, gpointer userData) {
    auto* bind = static_cast<DownloadBinding*>(userData);
    std::string fname = (suggestedFilename && *suggestedFilename) ? suggestedFilename : "download.bin";
    std::string savePath = makeUniqueDownloadPath(litebrowser::fs::getDownloadsDir(), fname);
    std::string destUri = litebrowser::fs::pathToFileUrl(savePath);

    const char* reqUri = "";
    if (g_wk.download_get_request && g_wk.uri_request_get_uri) {
        void* r = g_wk.download_get_request(download);
        if (r) reqUri = g_wk.uri_request_get_uri(r);
    }

    bind->recordId = bind->app->storage.addDownload(
        litebrowser::fs::getFileName(savePath), reqUri ? reqUri : "", savePath);

    if (g_wk.download_set_allow_overwrite) g_wk.download_set_allow_overwrite(download, TRUE);
    if (g_wk.download_set_destination) g_wk.download_set_destination(download, destUri.c_str());
    setStatus(bind->app, "جاري التحميل إلى: " + savePath);
    return TRUE;
}

void onDownloadReceivedData(void* download, guint64, gpointer userData) {
    auto* bind = static_cast<DownloadBinding*>(userData);
    if (bind->recordId == 0) return;
    uint64_t rec = g_wk.download_get_received_data_length ? g_wk.download_get_received_data_length(download) : 0;
    uint64_t tot = 0;
    if (g_wk.download_get_response && g_wk.uri_response_get_content_length) {
        void* resp = g_wk.download_get_response(download);
        if (resp) tot = g_wk.uri_response_get_content_length(resp);
    }
    bind->app->storage.updateDownloadProgress(bind->recordId, rec, tot);
    setStatus(bind->app, "جاري التحميل: " + litebrowser::fs::formatBytes(rec));
}

void onDownloadFinished(void*, gpointer userData) {
    auto* bind = static_cast<DownloadBinding*>(userData);
    if (bind->recordId != 0) {
        bind->app->storage.finishDownload(bind->recordId, litebrowser::DownloadStatus::Completed);
        auto rec = bind->app->storage.getDownloadById(bind->recordId);
        setStatus(bind->app, "اكتمل التحميل بنجاح: " + rec.fileName + " (يمكنك فتحه من صفحة التحميلات)");
    }
    delete bind;
}

void onDownloadFailed(void*, void*, gpointer userData) {
    auto* bind = static_cast<DownloadBinding*>(userData);
    if (bind->recordId != 0) {
        bind->app->storage.finishDownload(bind->recordId, litebrowser::DownloadStatus::Failed);
        setStatus(bind->app, "تعذّر إكمال التحميل.");
    }
    delete bind;
}

void onContextDownloadStarted(void*, void* download, gpointer userData) {
    auto* app = static_cast<BrowserApp*>(userData);
    auto* bind = new DownloadBinding{app, 0};
    g_signal_connect(G_OBJECT(download), "decide-destination", G_CALLBACK(onDownloadDecideDestination), bind);
    g_signal_connect(G_OBJECT(download), "received-data", G_CALLBACK(onDownloadReceivedData), bind);
    g_signal_connect(G_OBJECT(download), "finished", G_CALLBACK(onDownloadFinished), bind);
    g_signal_connect(G_OBJECT(download), "failed", G_CALLBACK(onDownloadFailed), bind);
}

void onCloseTabClicked(GtkButton*, gpointer userData) {
    auto* tab = static_cast<TabContext*>(userData);
    BrowserApp* app = tab->app;
    if (app->tabs.size() <= 1) {
        navigateTab(tab, "lite://home");
        return;
    }
    int pageNum = gtk_notebook_page_num(GTK_NOTEBOOK(app->notebook), tab->container);
    for (auto it = app->tabs.begin(); it != app->tabs.end(); ++it) {
        if (it->get() == tab) {
            if (pageNum >= 0) gtk_notebook_remove_page(GTK_NOTEBOOK(app->notebook), pageNum);
            app->tabs.erase(it);
            break;
        }
    }
}

TabContext* createNewTab(BrowserApp* app, const std::string& initialUrl, bool switchToIt) {
    auto tab = std::make_unique<TabContext>();
    tab->app = app;
    tab->tabId = app->nextTabId++;

    tab->webView = g_wk.web_view_new();
    applyEngineSettingsToView(tab->webView, app->storage.getSettings());

    tab->container = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_box_pack_start(GTK_BOX(tab->container), tab->webView, TRUE, TRUE, 0);

    GtkWidget* tabHeader = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    tab->tabLabel = gtk_label_new("تبويب جديد");
    GtkWidget* closeBtn = gtk_button_new_with_label("×");
    gtk_button_set_relief(GTK_BUTTON(closeBtn), GTK_RELIEF_NONE);
    gtk_box_pack_start(GTK_BOX(tabHeader), tab->tabLabel, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(tabHeader), closeBtn, FALSE, FALSE, 0);
    gtk_widget_show_all(tabHeader);
    gtk_widget_show_all(tab->container);

    TabContext* rawPtr = tab.get();
    g_signal_connect(G_OBJECT(closeBtn), "clicked", G_CALLBACK(onCloseTabClicked), rawPtr);
    g_signal_connect(G_OBJECT(tab->webView), "load-changed", G_CALLBACK(onWebViewLoadChanged), rawPtr);
    g_signal_connect(G_OBJECT(tab->webView), "notify::title", G_CALLBACK(onWebViewNotifyTitle), rawPtr);
    g_signal_connect(G_OBJECT(tab->webView), "notify::estimated-load-progress", G_CALLBACK(onWebViewNotifyProgress), rawPtr);
    g_signal_connect(G_OBJECT(tab->webView), "decide-policy", G_CALLBACK(onWebViewDecidePolicy), rawPtr);

    int idx = gtk_notebook_append_page(GTK_NOTEBOOK(app->notebook), tab->container, tabHeader);
    app->tabs.push_back(std::move(tab));

    if (switchToIt) {
        gtk_notebook_set_current_page(GTK_NOTEBOOK(app->notebook), idx);
    }
    navigateTab(rawPtr, initialUrl);
    return rawPtr;
}

void promptOpenDocument(BrowserApp* app) {
    GtkWidget* dlg = gtk_file_chooser_dialog_new(
        "فتح ملف PDF أو Word للقراءة",
        GTK_WINDOW(app->window),
        GTK_FILE_CHOOSER_ACTION_OPEN,
        "إلغاء", GTK_RESPONSE_CANCEL,
        "فتح المستند", GTK_RESPONSE_ACCEPT,
        nullptr);

    GtkFileFilter* docFilter = gtk_file_filter_new();
    gtk_file_filter_set_name(docFilter, "مستندات مدعومة (PDF, Word .docx/.doc, RTF, TXT)");
    gtk_file_filter_add_pattern(docFilter, "*.pdf");
    gtk_file_filter_add_pattern(docFilter, "*.PDF");
    gtk_file_filter_add_pattern(docFilter, "*.docx");
    gtk_file_filter_add_pattern(docFilter, "*.DOCX");
    gtk_file_filter_add_pattern(docFilter, "*.doc");
    gtk_file_filter_add_pattern(docFilter, "*.DOC");
    gtk_file_filter_add_pattern(docFilter, "*.rtf");
    gtk_file_filter_add_pattern(docFilter, "*.txt");
    gtk_file_filter_add_pattern(docFilter, "*.md");
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dlg), docFilter);

    GtkFileFilter* allFilter = gtk_file_filter_new();
    gtk_file_filter_set_name(allFilter, "جميع الملفات (*.*)");
    gtk_file_filter_add_pattern(allFilter, "*");
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dlg), allFilter);

    if (gtk_dialog_run(GTK_DIALOG(dlg)) == GTK_RESPONSE_ACCEPT) {
        char* filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dlg));
        if (filename) {
            std::string path(filename);
            g_free(filename);
            createNewTab(app, path, true);
        }
    }
    gtk_widget_destroy(dlg);
}

gboolean onWindowKeyPress(GtkWidget*, GdkEventKey* event, gpointer userData) {
    auto* app = static_cast<BrowserApp*>(userData);
    bool ctrl = (event->state & GDK_CONTROL_MASK) != 0;
    if (ctrl && (event->keyval == GDK_KEY_t || event->keyval == GDK_KEY_T)) {
        createNewTab(app, "lite://home", true);
        return TRUE;
    }
    if (ctrl && (event->keyval == GDK_KEY_w || event->keyval == GDK_KEY_W)) {
        if (auto* t = app->currentTab()) onCloseTabClicked(nullptr, t);
        return TRUE;
    }
    if (ctrl && (event->keyval == GDK_KEY_o || event->keyval == GDK_KEY_O)) {
        promptOpenDocument(app);
        return TRUE;
    }
    if (ctrl && (event->keyval == GDK_KEY_l || event->keyval == GDK_KEY_L)) {
        gtk_widget_grab_focus(app->urlEntry);
        gtk_editable_select_region(GTK_EDITABLE(app->urlEntry), 0, -1);
        return TRUE;
    }
    if (event->keyval == GDK_KEY_F5) {
        if (auto* t = app->currentTab()) navigateTab(t, t->logicalUrl);
        return TRUE;
    }
    return FALSE;
}

}  // namespace

int main(int argc, char* argv[]) {
    if (argc >= 2 && std::strcmp(argv[1], "--headless-check") == 0) {
        bool wkOk = g_wk.load();
        litebrowser::StorageManager st;
        auto home = litebrowser::renderInternalPage("lite://home", st, 0);
        litebrowser::fs::cleanupTempSessionDir();
        std::printf("LiteBrowser Linux Check: WebKit2GTK=%s, HomeHtmlBytes=%zu\n",
                    wkOk ? "LOADED" : "MISSING", home.html.size());
        return wkOk ? 0 : 1;
    }

    gtk_init(&argc, &argv);

    if (!g_wk.load()) {
        GtkWidget* errDlg = gtk_message_dialog_new(
            nullptr, GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR, GTK_BUTTONS_OK,
            "لم يتم العثور على مكتبة المحرك libwebkit2gtk-4.1 أو 4.0 على النظام.\n"
            "يرجى تثبيتها بالأمر: sudo apt install libwebkit2gtk-4.1-0");
        gtk_dialog_run(GTK_DIALOG(errDlg));
        gtk_widget_destroy(errDlg);
        return 1;
    }

    BrowserApp app;
    g_signal_connect(G_OBJECT(g_wk.web_context_get_default()), "download-started",
                     G_CALLBACK(onContextDownloadStarted), &app);

    app.window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(app.window), "متصفح النور الخفيف - LiteBrowser");
    gtk_window_set_default_size(GTK_WINDOW(app.window), 1180, 760);
    g_signal_connect(G_OBJECT(app.window), "destroy", G_CALLBACK(gtk_main_quit), nullptr);
    g_signal_connect(G_OBJECT(app.window), "key-press-event", G_CALLBACK(onWindowKeyPress), &app);

    GtkWidget* rootVBox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_add(GTK_CONTAINER(app.window), rootVBox);

    // Top Navigation Bar
    GtkWidget* navBar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_widget_set_margin_start(navBar, 6);
    gtk_widget_set_margin_end(navBar, 6);
    gtk_widget_set_margin_top(navBar, 6);
    gtk_widget_set_margin_bottom(navBar, 6);

    app.btnBack = gtk_button_new_with_label("◀");
    app.btnForward = gtk_button_new_with_label("▶");
    GtkWidget* btnReload = gtk_button_new_with_label("⟳");
    GtkWidget* btnHome = gtk_button_new_with_label("🏠");
    app.urlEntry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(app.urlEntry), "ابحث في الإنترنت أو أدخل رابط موقع أو مسار ملف PDF/Word...");

    app.btnBookmark = gtk_button_new_with_label("☆");
    GtkWidget* btnOpenDoc = gtk_button_new_with_label("📂 PDF/Word");
    GtkWidget* btnGames = gtk_button_new_with_label("🎮 ألعاب");
    GtkWidget* btnDownloads = gtk_button_new_with_label("⬇️ التحميلات");
    auto initSettings = app.storage.getSettings();
    app.btnLiteMode = gtk_button_new_with_label(initSettings.liteMode ? "⚡ خفيف: مفعّل" : "⚡ خفيف");
    GtkWidget* btnNewTab = gtk_button_new_with_label("+ تبويب");

    gtk_box_pack_start(GTK_BOX(navBar), app.btnBack, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(navBar), app.btnForward, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(navBar), btnReload, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(navBar), btnHome, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(navBar), app.urlEntry, TRUE, TRUE, 4);
    gtk_box_pack_start(GTK_BOX(navBar), app.btnBookmark, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(navBar), btnOpenDoc, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(navBar), btnGames, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(navBar), btnDownloads, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(navBar), app.btnLiteMode, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(navBar), btnNewTab, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(rootVBox), navBar, FALSE, FALSE, 0);

    // Tab Notebook
    app.notebook = gtk_notebook_new();
    gtk_notebook_set_scrollable(GTK_NOTEBOOK(app.notebook), TRUE);
    gtk_box_pack_start(GTK_BOX(rootVBox), app.notebook, TRUE, TRUE, 0);

    // Status Bar
    GtkWidget* statusBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_widget_set_margin_start(statusBox, 8);
    gtk_widget_set_margin_end(statusBox, 8);
    gtk_widget_set_margin_top(statusBox, 3);
    gtk_widget_set_margin_bottom(statusBox, 3);
    app.statusLabel = gtk_label_new("جاهز");
    gtk_box_pack_start(GTK_BOX(statusBox), app.statusLabel, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(rootVBox), statusBox, FALSE, FALSE, 0);

    // Wire toolbar signals
    g_signal_connect(G_OBJECT(app.btnBack), "clicked", G_CALLBACK(+[](GtkButton*, gpointer d) {
        auto* a = static_cast<BrowserApp*>(d);
        if (auto* t = a->currentTab()) g_wk.web_view_go_back(t->webView);
    }), &app);

    g_signal_connect(G_OBJECT(app.btnForward), "clicked", G_CALLBACK(+[](GtkButton*, gpointer d) {
        auto* a = static_cast<BrowserApp*>(d);
        if (auto* t = a->currentTab()) g_wk.web_view_go_forward(t->webView);
    }), &app);

    g_signal_connect(G_OBJECT(btnReload), "clicked", G_CALLBACK(+[](GtkButton*, gpointer d) {
        auto* a = static_cast<BrowserApp*>(d);
        if (auto* t = a->currentTab()) {
            if (litebrowser::fs::startsWithIgnoreCase(t->logicalUrl, "lite://") ||
                litebrowser::isSupportedDocumentFile(t->logicalUrl)) {
                navigateTab(t, t->logicalUrl);
            } else {
                g_wk.web_view_reload(t->webView);
            }
        }
    }), &app);

    g_signal_connect(G_OBJECT(btnHome), "clicked", G_CALLBACK(+[](GtkButton*, gpointer d) {
        auto* a = static_cast<BrowserApp*>(d);
        if (auto* t = a->currentTab()) navigateTab(t, "lite://home");
    }), &app);

    g_signal_connect(G_OBJECT(app.urlEntry), "activate", G_CALLBACK(+[](GtkEntry* entry, gpointer d) {
        auto* a = static_cast<BrowserApp*>(d);
        const char* txt = gtk_entry_get_text(entry);
        if (auto* t = a->currentTab()) navigateTab(t, txt ? txt : "");
    }), &app);

    g_signal_connect(G_OBJECT(app.btnBookmark), "clicked", G_CALLBACK(+[](GtkButton*, gpointer d) {
        auto* a = static_cast<BrowserApp*>(d);
        if (auto* t = a->currentTab()) {
            const char* title = g_wk.web_view_get_title ? g_wk.web_view_get_title(t->webView) : nullptr;
            a->storage.toggleBookmark((title && *title) ? title : t->logicalUrl, t->logicalUrl);
            updateToolbarForTab(a, t);
        }
    }), &app);

    g_signal_connect(G_OBJECT(btnOpenDoc), "clicked", G_CALLBACK(+[](GtkButton*, gpointer d) {
        promptOpenDocument(static_cast<BrowserApp*>(d));
    }), &app);

    g_signal_connect(G_OBJECT(btnGames), "clicked", G_CALLBACK(+[](GtkButton*, gpointer d) {
        auto* a = static_cast<BrowserApp*>(d);
        if (auto* t = a->currentTab()) navigateTab(t, "lite://games");
    }), &app);

    g_signal_connect(G_OBJECT(btnDownloads), "clicked", G_CALLBACK(+[](GtkButton*, gpointer d) {
        auto* a = static_cast<BrowserApp*>(d);
        if (auto* t = a->currentTab()) navigateTab(t, "lite://downloads");
    }), &app);

    g_signal_connect(G_OBJECT(app.btnLiteMode), "clicked", G_CALLBACK(+[](GtkButton*, gpointer d) {
        auto* a = static_cast<BrowserApp*>(d);
        if (auto* t = a->currentTab()) handleLiteAction(t, "lite-action://toggle-lite");
    }), &app);

    g_signal_connect(G_OBJECT(btnNewTab), "clicked", G_CALLBACK(+[](GtkButton*, gpointer d) {
        createNewTab(static_cast<BrowserApp*>(d), "lite://home", true);
    }), &app);

    g_signal_connect(G_OBJECT(app.notebook), "switch-page", G_CALLBACK(+[](GtkNotebook*, GtkWidget* page, guint, gpointer d) {
        auto* a = static_cast<BrowserApp*>(d);
        for (auto& t : a->tabs) {
            if (t->container == page) {
                updateToolbarForTab(a, t.get());
                break;
            }
        }
    }), &app);

    std::string startTarget = (argc >= 2) ? argv[1] : app.storage.getSettings().homePage;
    createNewTab(&app, startTarget, true);

    gtk_widget_show_all(app.window);
    gtk_main();

    litebrowser::fs::cleanupTempSessionDir();
    return 0;
}

