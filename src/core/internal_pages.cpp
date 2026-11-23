#include "internal_pages.hpp"

#include "fs_utils.hpp"
#include "i18n.hpp"

#include <cstdio>

namespace litebrowser {

namespace {

std::string commonPageHead(const std::string& title, const std::string& activeUri, StorageManager& storage) {
    auto s = storage.getSettings();
    std::string lang = s.language.empty() ? "ar" : s.language;
    std::string theme = s.theme.empty() ? "dark" : s.theme;
    bool en = (lang == "en");
    bool dark = (theme == "dark");
    std::string dir = en ? "ltr" : "rtl";

    std::string html;
    html.reserve(12288);
    html += "<!DOCTYPE html>\n<!-- saved from url=(0014)about:internet -->\n<html lang=\"" + lang + "\" dir=\"" + dir + "\">\n<head>\n"
           "<meta charset=\"UTF-8\"/>\n"
           "<meta http-equiv=\"X-UA-Compatible\" content=\"IE=edge\"/>\n"
           "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\"/>\n"
           "<title>" + fs::htmlEscape(title) + "</title>\n"
           "<style>\n"
           ":root {\n"
           "  --font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', 'Noto Sans Arabic', Tahoma, Arial, sans-serif;\n"
           "  --radius-sm: 6px;\n"
           "  --radius-md: 10px;\n"
           "  --radius-lg: 16px;\n"
           "  --radius-pill: 9999px;\n"
           "  --trans-fast: 0.15s ease;\n"
           "}\n"
           "body.theme-light, [data-theme=\"light\"] {\n"
           "  --bg-page: #f8fafc;\n"
           "  --bg-surface: #ffffff;\n"
           "  --bg-surface-elevated: #ffffff;\n"
           "  --bg-surface-alt: #f1f5f9;\n"
           "  --bg-nav: #0f172a;\n"
           "  --text-primary: #0f172a;\n"
           "  --text-secondary: #475569;\n"
           "  --text-muted: #64748b;\n"
           "  --brand-primary: #0284c7;\n"
           "  --brand-hover: #0369a1;\n"
           "  --brand-light: #e0f2fe;\n"
           "  --accent-emerald: #059669;\n"
           "  --accent-amber: #d97706;\n"
           "  --accent-rose: #e11d48;\n"
           "  --accent-purple: #7c3aed;\n"
           "  --accent-cyan: #0891b2;\n"
           "  --border-subtle: #e2e8f0;\n"
           "  --border-strong: #cbd5e1;\n"
           "  --shadow-sm: 0 1px 3px rgba(15,23,42,0.05);\n"
           "  --shadow-md: 0 4px 16px -2px rgba(15,23,42,0.06), 0 2px 6px -1px rgba(15,23,42,0.04);\n"
           "  --shadow-lg: 0 10px 25px -3px rgba(15,23,42,0.08), 0 4px 10px -2px rgba(15,23,42,0.04);\n"
           "  --hero-bar: linear-gradient(90deg, #0284c7, #38bdf8, #10b981);\n"
           "}\n"
           "body.theme-dark, [data-theme=\"dark\"] {\n"
           "  --bg-page: #090d16;\n"
           "  --bg-surface: #111827;\n"
           "  --bg-surface-elevated: #1e293b;\n"
           "  --bg-surface-alt: #1a2234;\n"
           "  --bg-nav: #070a10;\n"
           "  --text-primary: #f8fafc;\n"
           "  --text-secondary: #cbd5e1;\n"
           "  --text-muted: #94a3b8;\n"
           "  --brand-primary: #38bdf8;\n"
           "  --brand-hover: #7dd3fc;\n"
           "  --brand-light: rgba(56, 189, 248, 0.15);\n"
           "  --accent-emerald: #10b981;\n"
           "  --accent-amber: #f59e0b;\n"
           "  --accent-rose: #f43f5e;\n"
           "  --accent-purple: #a855f7;\n"
           "  --accent-cyan: #06b6d4;\n"
           "  --border-subtle: #1e293b;\n"
           "  --border-strong: #334155;\n"
           "  --shadow-sm: 0 1px 3px rgba(0,0,0,0.3);\n"
           "  --shadow-md: 0 4px 16px -2px rgba(0,0,0,0.4), 0 2px 6px -1px rgba(0,0,0,0.25);\n"
           "  --shadow-lg: 0 10px 25px -4px rgba(0,0,0,0.6), 0 4px 10px -2px rgba(0,0,0,0.4);\n"
           "  --hero-bar: linear-gradient(90deg, #38bdf8, #818cf8, #34d399);\n"
           "}\n"
           "* { box-sizing: border-box; margin: 0; padding: 0; }\n"
           "body { font-family: var(--font-family, Tahoma, Arial, sans-serif); background: var(--bg-page); color: var(--text-primary); line-height: 1.65; min-height: 100vh; transition: background 0.2s, color 0.2s; }\n"
           "/* Navigation Bar */\n"
           ".top-nav { background: var(--bg-nav); color: #ffffff; padding: 10px 20px; border-bottom: 1px solid var(--border-subtle); box-shadow: 0 2px 10px rgba(0,0,0,0.2); *zoom: 1; }\n"
           ".top-nav:after { content: ''; display: table; clear: both; }\n"
           ".nav-inner { max-width: 1120px; margin: 0 auto; display: flex; flex-wrap: wrap; align-items: center; justify-content: space-between; gap: 10px; }\n"
           ".brand { font-size: 17px; font-weight: 800; color: #38bdf8; text-decoration: none; display: flex; align-items: center; gap: 8px; }\n"
           ".brand:hover { color: #7dd3fc; }\n"
           ".brand-badge { background: #0284c7; color: #fff; font-size: 11px; padding: 2px 7px; border-radius: 9999px; font-weight: bold; }\n"
           ".nav-links { display: flex; flex-wrap: wrap; align-items: center; gap: 4px; }\n"
           ".nav-links a { color: #cbd5e1; text-decoration: none; font-size: 13px; font-weight: 500; padding: 5px 11px; border-radius: 9999px; transition: all 0.15s ease; display: inline-block; *display: inline; *zoom: 1; }\n"
           ".nav-links a:hover { background: rgba(255,255,255,0.12); color: #ffffff; transform: translateY(-1px); }\n"
           ".nav-links a.active { background: #0284c7; color: #ffffff; font-weight: bold; }\n"
           ".nav-controls { display: flex; align-items: center; gap: 6px; }\n"
           ".nav-btn-pill { background: rgba(255,255,255,0.08); border: 1px solid rgba(255,255,255,0.15); color: #f8fafc; font-size: 12px; font-weight: 600; padding: 4px 10px; border-radius: 9999px; text-decoration: none; cursor: pointer; display: inline-flex; align-items: center; gap: 4px; transition: all 0.15s ease; }\n"
           ".nav-btn-pill:hover { background: rgba(255,255,255,0.18); color: #ffffff; transform: translateY(-1px); }\n"
           "/* Container Wrapper */\n"
           ".wrap { max-width: 1060px; margin: 24px auto; padding: 0 16px 48px 16px; }\n"
           "/* Hero Card */\n"
           ".hero { background: var(--bg-surface); border-radius: 16px; padding: 32px 28px; text-align: center; box-shadow: var(--shadow-md); margin-bottom: 24px; border: 1px solid var(--border-subtle); position: relative; overflow: hidden; }\n"
           ".hero:before { content: ''; position: absolute; top: 0; left: 0; right: 0; height: 4px; background: var(--hero-bar); }\n"
           ".hero h1 { margin-bottom: 8px; font-size: 26px; font-weight: 800; color: var(--text-primary); }\n"
           ".hero p { margin-bottom: 22px; color: var(--text-secondary); font-size: 14px; max-width: 720px; margin-inline: auto; }\n"
           "/* Status Pill Strip */\n"
           ".status-strip { background: var(--bg-surface); border: 1px solid var(--border-subtle); border-radius: 12px; padding: 12px 18px; margin-bottom: 20px; box-shadow: var(--shadow-sm); display: flex; flex-wrap: wrap; justify-content: space-between; align-items: center; gap: 10px; font-size: 13px; *zoom: 1; }\n"
           ".status-strip:after { content: ''; display: table; clear: both; }\n"
           ".status-info { display: flex; align-items: center; gap: 10px; flex-wrap: wrap; }\n"
           ".status-pill { display: inline-flex; align-items: center; gap: 6px; padding: 4px 10px; border-radius: 9999px; font-size: 12px; font-weight: 600; background: #e0f2fe; color: #0369a1; }\n"
           ".status-pill.green { background: #d1fae5; color: #065f46; }\n"
           ".status-pill.amber { background: #fef3c7; color: #92400e; }\n"
           ".status-actions { display: flex; gap: 6px; }\n"
           "/* Omnibox Search Bar */\n"
           ".search-box-wrap { max-width: 720px; margin: 0 auto 16px auto; }\n"
           ".search-tabs { display: flex; justify-content: center; gap: 6px; margin-bottom: 10px; flex-wrap: wrap; }\n"
           ".search-tab { background: var(--bg-surface-alt); border: 1px solid var(--border-strong); border-radius: 9999px; padding: 5px 13px; font-size: 12px; font-weight: 600; cursor: pointer; color: var(--text-secondary); transition: all 0.15s ease; }\n"
           ".search-tab:hover { background: var(--border-subtle); color: var(--text-primary); }\n"
           ".search-tab.active { background: #0284c7; color: #ffffff; border-color: #0284c7; }\n"
           ".search-row { display: flex; gap: 8px; flex-wrap: wrap; }\n"
           ".search-inp { flex: 1; min-width: 240px; padding: 12px 18px; font-size: 15px; border: 2px solid var(--border-strong); border-radius: 9999px; font-family: inherit; outline: none; transition: border-color 0.15s, box-shadow 0.15s; background: var(--bg-surface); color: var(--text-primary); display: inline-block; *display: inline; *zoom: 1; vertical-align: middle; }\n"
           ".search-inp:focus { border-color: #0284c7; box-shadow: 0 0 0 3px rgba(2, 132, 199, 0.2); }\n"
           "/* Buttons */\n"
           ".btn { background: #0284c7; color: #ffffff; border: none; border-radius: 9999px; padding: 11px 22px; font-size: 14px; font-weight: 700; cursor: pointer; text-decoration: none; display: inline-flex; align-items: center; justify-content: center; gap: 6px; font-family: inherit; transition: all 0.15s ease; *display: inline; *zoom: 1; vertical-align: middle; }\n"
           ".btn:hover { background: #0369a1; transform: translateY(-1px); box-shadow: 0 4px 12px rgba(2,132,199,0.25); }\n"
           ".btn-sec { background: var(--bg-surface-alt); color: var(--text-primary); border: 1px solid var(--border-strong); }\n"
           ".btn-sec:hover { background: var(--border-subtle); box-shadow: var(--shadow-sm); }\n"
           ".btn-green { background: #059669; }\n"
           ".btn-green:hover { background: #047857; box-shadow: 0 4px 12px rgba(5,150,105,0.25); }\n"
           ".btn-amber { background: #d97706; }\n"
           ".btn-amber:hover { background: #b45309; box-shadow: 0 4px 12px rgba(217,119,6,0.25); }\n"
           ".btn-rose { background: #e11d48; }\n"
           ".btn-rose:hover { background: #be123c; box-shadow: 0 4px 12px rgba(225,29,72,0.25); }\n"
           ".btn-sm { padding: 6px 12px; font-size: 12px; border-radius: 6px; }\n"
           "/* Quick Links Chips */\n"
           ".quick-links { display: flex; flex-wrap: wrap; justify-content: center; gap: 8px; margin-top: 14px; }\n"
           ".quick-links a { display: inline-flex; align-items: center; gap: 5px; padding: 6px 14px; background: var(--bg-surface-alt); color: var(--text-primary); border-radius: 9999px; text-decoration: none; font-size: 12px; font-weight: 600; border: 1px solid var(--border-subtle); transition: all 0.15s ease; *display: inline; *zoom: 1; }\n"
           ".quick-links a:hover { background: var(--border-subtle); border-color: #38bdf8; color: #0284c7; transform: translateY(-1px); }\n"
           "/* Feature Grid & Cards */\n"
           ".grid { display: flex; flex-wrap: wrap; gap: 16px; margin-top: 20px; *zoom: 1; }\n"
           ".grid:after { content: ''; display: table; clear: both; }\n"
           ".card { background: var(--bg-surface); border: 1px solid var(--border-subtle); border-radius: 14px; padding: 22px; width: calc(33.333% - 11px); min-width: 280px; flex-grow: 1; box-shadow: var(--shadow-sm); display: inline-block; *display: inline; *zoom: 1; vertical-align: top; margin: 6px 0.3%; box-sizing: border-box; transition: transform 0.18s ease, box-shadow 0.18s ease; position: relative; overflow: hidden; }\n"
           ".card:hover { transform: translateY(-3px); box-shadow: var(--shadow-md); border-color: var(--border-strong); }\n"
           ".card-accent { position: absolute; top: 0; left: 0; right: 0; height: 3px; }\n"
           ".card-accent.blue { background: #0284c7; }\n"
           ".card-accent.emerald { background: #059669; }\n"
           ".card-accent.purple { background: #7c3aed; }\n"
           ".card-accent.amber { background: #d97706; }\n"
           ".card-accent.cyan { background: #0891b2; }\n"
           ".card-accent.rose { background: #e11d48; }\n"
           ".card-header { display: flex; align-items: center; gap: 10px; margin-bottom: 12px; }\n"
           ".card-icon { width: 38px; height: 38px; border-radius: 10px; display: flex; align-items: center; justify-content: center; font-size: 18px; font-weight: bold; background: var(--bg-surface-alt); }\n"
           ".card h3 { font-size: 16px; font-weight: 700; color: var(--text-primary); margin: 0; }\n"
           ".card p { font-size: 13px; color: var(--text-secondary); margin-bottom: 16px; min-height: 40px; }\n"
           "/* Tables & Data Lists */\n"
           ".tbl-wrap { background: var(--bg-surface); border-radius: 14px; border: 1px solid var(--border-subtle); box-shadow: var(--shadow-sm); overflow: hidden; margin-top: 14px; }\n"
           "table.list-tbl { width: 100%; border-collapse: collapse; text-align: inherit; font-size: 13px; }\n"
           "table.list-tbl th { background: var(--bg-surface-alt); color: var(--text-secondary); font-weight: 700; padding: 12px 16px; border-bottom: 2px solid var(--border-subtle); font-size: 12px; text-transform: uppercase; }\n"
           "table.list-tbl td { padding: 12px 16px; border-bottom: 1px solid var(--border-subtle); color: var(--text-primary); }\n"
           "table.list-tbl tr:hover td { background: var(--bg-surface-alt); }\n"
           ".filter-bar { display: flex; gap: 10px; margin-bottom: 14px; align-items: center; }\n"
           ".filter-inp { padding: 9px 14px; border-radius: 8px; border: 1px solid var(--border-strong); font-size: 13px; font-family: inherit; outline: none; flex: 1; max-width: 360px; background: var(--bg-surface); color: var(--text-primary); }\n"
           ".filter-inp:focus { border-color: #0284c7; }\n"
           "/* Progress Bar */\n"
           ".prog-bg { width: 140px; height: 10px; background: var(--bg-surface-alt); border-radius: 9999px; overflow: hidden; display: inline-block; vertical-align: middle; margin-inline: 8px; border: 1px solid var(--border-subtle); }\n"
           ".prog-fill { height: 100%; background: linear-gradient(90deg, #0284c7, #38bdf8); border-radius: 9999px; transition: width 0.3s ease; }\n"
           "/* Responsive Fallbacks */\n"
           "@media (max-width: 768px) {\n"
           "  .card { width: 100%; min-width: 100%; }\n"
           "  .hero { padding: 24px 16px; }\n"
           "  .hero h1 { font-size: 22px; }\n"
           "}\n"
           "</style>\n</head>\n<body class=\"theme-" + theme + "\" data-theme=\"" + theme + "\">\n"
           "<div class=\"top-nav\">\n"
           "  <div class=\"nav-inner\">\n"
           "    <a class=\"brand\" href=\"lite://home\"><span>⚡ " + fs::htmlEscape(i18n::get("app_name", lang)) + "</span> <span class=\"brand-badge\">LiteBrowser</span></a>\n"
           "    <div class=\"nav-links\">\n"
           "      <a class=\"" + (activeUri == "lite://home" ? "active" : "") + "\" href=\"lite://home\">" + fs::htmlEscape(i18n::get("nav_home", lang)) + "</a>\n"
           "      <a class=\"" + (activeUri == "lite://video" ? "active" : "") + "\" href=\"lite://video\">" + fs::htmlEscape(i18n::get("nav_video", lang)) + "</a>\n"
           "      <a class=\"" + (activeUri == "lite://games" ? "active" : "") + "\" href=\"lite://games\">" + fs::htmlEscape(i18n::get("nav_games", lang)) + "</a>\n"
           "      <a href=\"lite-action://open-file\">" + fs::htmlEscape(i18n::get("nav_open_doc", lang)) + "</a>\n"
           "      <a class=\"" + (activeUri == "lite://downloads" ? "active" : "") + "\" href=\"lite://downloads\">" + fs::htmlEscape(i18n::get("nav_downloads", lang)) + "</a>\n"
           "      <a class=\"" + (activeUri == "lite://bookmarks" ? "active" : "") + "\" href=\"lite://bookmarks\">" + fs::htmlEscape(i18n::get("nav_bookmarks", lang)) + "</a>\n"
           "      <a class=\"" + (activeUri == "lite://history" ? "active" : "") + "\" href=\"lite://history\">" + fs::htmlEscape(i18n::get("nav_history", lang)) + "</a>\n"
           "      <a class=\"" + (activeUri == "lite://settings" ? "active" : "") + "\" href=\"lite://settings\">" + fs::htmlEscape(i18n::get("nav_settings", lang)) + "</a>\n"
           "    </div>\n"
           "    <div class=\"nav-controls\">\n"
           "      <a class=\"nav-btn-pill\" href=\"lite-action://toggle-theme\" title=\"" +
           (dark ? (en ? "Switch to Light Mode" : "التبديل إلى الوضع المضيء") : (en ? "Switch to Dark Mode" : "التبديل إلى الوضع المظلم")) + "\">" +
           (dark ? fs::htmlEscape(i18n::get("theme_light", lang)) : fs::htmlEscape(i18n::get("theme_dark", lang))) + "</a>\n"
           "      <a class=\"nav-btn-pill\" href=\"lite-action://toggle-lang\" title=\"Switch Language / تغيير اللغة\">" +
           fs::htmlEscape(i18n::get("lang_toggle_label", lang)) + "</a>\n"
           "    </div>\n"
           "  </div>\n"
           "</div>\n";
    return html;
}

std::string buildHomeHtml(StorageManager& storage) {
    BrowserSettings s = storage.getSettings();
    std::string lang = s.language.empty() ? "ar" : s.language;
    bool en = (lang == "en");
    auto bookmarks = storage.getBookmarks();

    std::string html = commonPageHead(i18n::get("home_hero_title", lang), "lite://home", storage);
    html += "<div class=\"wrap\">\n";

    // Performance Strip
    html += "<div class=\"status-strip\">\n"
            "  <div class=\"status-info\">\n"
            "    <span class=\"status-pill " + std::string(s.liteMode ? "amber" : "green") + "\">⚡ " +
            fs::htmlEscape(i18n::get(s.liteMode ? "home_status_lite_active" : "home_status_lite_inactive", lang)) + "</span>\n"
            "    <span class=\"status-pill " + std::string(s.adBlockEnabled ? "green" : "amber") + "\">🛡️ " +
            fs::htmlEscape(i18n::get(s.adBlockEnabled ? "home_status_secure" : "set_adblock_label", lang)) + "</span>\n"
            "  </div>\n"
            "  <div class=\"status-actions\">\n"
            "    <a class=\"btn btn-sm btn-sec\" href=\"lite-action://toggle-lite\">" +
            fs::htmlEscape(i18n::get("home_toggle_mode", lang)) + "</a>\n"
            "    <a class=\"btn btn-sm btn-sec\" href=\"lite-action://toggle-theme\">" +
            (s.isDarkMode() ? fs::htmlEscape(i18n::get("theme_light", lang)) : fs::htmlEscape(i18n::get("theme_dark", lang))) + "</a>\n"
            "  </div>\n"
            "</div>\n";

    // Hero with Multi-Engine Omnibox Search
    html += "<div class=\"hero\">\n"
            "  <h1>" + fs::htmlEscape(i18n::get("home_hero_title", lang)) + "</h1>\n"
            "  <p>" + fs::htmlEscape(i18n::get("home_hero_desc", lang)) + "</p>\n"
            "  <div class=\"search-box-wrap\">\n"
            "    <div class=\"search-tabs\">\n"
            "      <button type=\"button\" class=\"search-tab active\" onclick=\"selectEngine('google', this)\">🌐 Google</button>\n"
            "      <button type=\"button\" class=\"search-tab\" onclick=\"selectEngine('ddg', this)\">🦆 DuckDuckGo</button>\n"
            "      <button type=\"button\" class=\"search-tab\" onclick=\"selectEngine('ddg-lite', this)\">⚡ DDG Lite</button>\n"
            "      <button type=\"button\" class=\"search-tab\" onclick=\"selectEngine('youtube', this)\">🎬 YouTube</button>\n"
            "      <button type=\"button\" class=\"search-tab\" onclick=\"selectEngine('wiki', this)\">📖 Wikipedia</button>\n"
            "      <button type=\"button\" class=\"search-tab\" onclick=\"selectEngine('bing', this)\">🔎 Bing</button>\n"
            "    </div>\n"
            "    <form onsubmit=\"return executeSearch();\" class=\"search-row\">\n"
            "      <input id=\"mainQ\" class=\"search-inp\" type=\"text\" placeholder=\"" +
            fs::htmlEscape(i18n::get("search_placeholder", lang)) + "\" autofocus/>\n"
            "      <button type=\"submit\" class=\"btn\">🔍 " + fs::htmlEscape(i18n::get("home_search_btn", lang)) + "</button>\n"
            "    </form>\n"
            "  </div>\n"
            "  <div class=\"quick-links\">\n";
    for (const auto& bm : bookmarks) {
        html += "    <a href=\"" + fs::htmlEscape(bm.url) + "\">★ " + fs::htmlEscape(bm.title) + "</a>\n";
    }
    html += "  </div>\n</div>\n";

    // Feature Cards Grid
    html += "<div class=\"grid\">\n"
            "  <div class=\"card\">\n"
            "    <div class=\"card-accent emerald\"></div>\n"
            "    <div class=\"card-header\">\n"
            "      <div class=\"card-icon\" style=\"color:#10b981;\">📄</div>\n"
            "      <h3>" + fs::htmlEscape(i18n::get("home_card_docs_title", lang)) + "</h3>\n"
            "    </div>\n"
            "    <p>" + fs::htmlEscape(i18n::get("home_card_docs_desc", lang)) + "</p>\n"
            "    <a class=\"btn btn-sm btn-green\" href=\"lite-action://open-file\">" +
            fs::htmlEscape(i18n::get("nav_open_doc", lang)) + "</a>\n"
            "  </div>\n"
            "  <div class=\"card\">\n"
            "    <div class=\"card-accent rose\"></div>\n"
            "    <div class=\"card-header\">\n"
            "      <div class=\"card-icon\" style=\"color:#f43f5e;\">🎬</div>\n"
            "      <h3>" + fs::htmlEscape(i18n::get("home_card_video_title", lang)) + "</h3>\n"
            "    </div>\n"
            "    <p>" + fs::htmlEscape(i18n::get("home_card_video_desc", lang)) + "</p>\n"
            "    <div class=\"quick-links\" style=\"justify-content:flex-start;margin-top:0;\">\n"
            "      <a href=\"https://www.youtube.com\">YouTube</a>\n"
            "      <a href=\"lite://video\">" + fs::htmlEscape(i18n::get("nav_video", lang)) + "</a>\n"
            "      <a href=\"https://archive.org/details/movies\">" + fs::htmlEscape(i18n::get("video_quick_archive", lang)) + "</a>\n"
            "    </div>\n"
            "  </div>\n"
            "  <div class=\"card\">\n"
            "    <div class=\"card-accent purple\"></div>\n"
            "    <div class=\"card-header\">\n"
            "      <div class=\"card-icon\" style=\"color:#a855f7;\">🎮</div>\n"
            "      <h3>" + fs::htmlEscape(i18n::get("home_card_games_title", lang)) + "</h3>\n"
            "    </div>\n"
            "    <p>" + fs::htmlEscape(i18n::get("home_card_games_desc", lang)) + "</p>\n"
            "    <a class=\"btn btn-sm btn-sec\" href=\"lite://games\">" +
            fs::htmlEscape(i18n::get("nav_games", lang)) + "</a>\n"
            "  </div>\n"
            "  <div class=\"card\">\n"
            "    <div class=\"card-accent blue\"></div>\n"
            "    <div class=\"card-header\">\n"
            "      <div class=\"card-icon\" style=\"color:#38bdf8;\">⬇️</div>\n"
            "      <h3>" + fs::htmlEscape(i18n::get("home_card_downloads_title", lang)) + "</h3>\n"
            "    </div>\n"
            "    <p>" + fs::htmlEscape(i18n::get("home_card_downloads_desc", lang)) + "</p>\n"
            "    <a class=\"btn btn-sm btn-sec\" href=\"lite://downloads\">" +
            fs::htmlEscape(i18n::get("nav_downloads", lang)) + "</a>\n"
            "  </div>\n"
            "  <div class=\"card\">\n"
            "    <div class=\"card-accent cyan\"></div>\n"
            "    <div class=\"card-header\">\n"
            "      <div class=\"card-icon\" style=\"color:#06b6d4;\">★</div>\n"
            "      <h3>" + fs::htmlEscape(i18n::get("home_card_bookmarks_title", lang)) + "</h3>\n"
            "    </div>\n"
            "    <p>" + fs::htmlEscape(i18n::get("home_card_bookmarks_desc", lang)) + "</p>\n"
            "    <a class=\"btn btn-sm btn-sec\" href=\"lite://bookmarks\">" +
            fs::htmlEscape(i18n::get("nav_bookmarks", lang)) + "</a>\n"
            "  </div>\n"
            "  <div class=\"card\">\n"
            "    <div class=\"card-accent amber\"></div>\n"
            "    <div class=\"card-header\">\n"
            "      <div class=\"card-icon\" style=\"color:#f59e0b;\">⚙️</div>\n"
            "      <h3>" + fs::htmlEscape(i18n::get("home_card_settings_title", lang)) + "</h3>\n"
            "    </div>\n"
            "    <p>" + fs::htmlEscape(i18n::get("home_card_settings_desc", lang)) + "</p>\n"
            "    <a class=\"btn btn-sm btn-amber\" href=\"lite://settings\">" +
            fs::htmlEscape(i18n::get("nav_settings", lang)) + "</a>\n"
            "  </div>\n"
            "</div>\n"
            "</div>\n"
            "<script>\n"
            "var curEng = '" + s.searchEngine + "';\n"
            "function selectEngine(eng, btn) {\n"
            "  curEng = eng;\n"
            "  var tabs = document.getElementsByClassName('search-tab');\n"
            "  for (var i = 0; i < tabs.length; i++) tabs[i].className = 'search-tab';\n"
            "  btn.className = 'search-tab active';\n"
            "}\n"
            "function executeSearch() {\n"
            "  var q = document.getElementById('mainQ').value;\n"
            "  if (!q) return false;\n"
            "  var target = '';\n"
            "  if (curEng === 'ddg') target = 'https://duckduckgo.com/?q=' + encodeURIComponent(q);\n"
            "  else if (curEng === 'ddg-lite') target = 'https://html.duckduckgo.com/html/?q=' + encodeURIComponent(q);\n"
            "  else if (curEng === 'youtube') target = 'https://www.youtube.com/results?search_query=' + encodeURIComponent(q);\n"
            "  else if (curEng === 'wiki') target = 'https://" + std::string(en ? "en" : "ar") + ".wikipedia.org/w/index.php?search=' + encodeURIComponent(q);\n"
            "  else if (curEng === 'bing') target = 'https://www.bing.com/search?q=' + encodeURIComponent(q);\n"
            "  else target = 'https://www.google.com/search?q=' + encodeURIComponent(q);\n"
            "  location.href = target;\n"
            "  return false;\n"
            "}\n"
            "</script>\n</body>\n</html>\n";
    return html;
}

std::string buildGamesHtml(StorageManager& storage) {
    BrowserSettings s = storage.getSettings();
    std::string lang = s.language.empty() ? "ar" : s.language;

    std::string html = commonPageHead(i18n::get("games_hero_title", lang), "lite://games", storage);
    html += "<div class=\"wrap\">\n"
            "<div class=\"hero\" style=\"padding:20px;\">\n"
            "  <h1>" + fs::htmlEscape(i18n::get("games_hero_title", lang)) + "</h1>\n"
            "  <p>" + fs::htmlEscape(i18n::get("games_hero_desc", lang)) + "</p>\n"
            "  <div class=\"search-tabs\">\n"
            "    <button id=\"tabSnake\" class=\"search-tab active\" onclick=\"startGame('snake')\">" + fs::htmlEscape(i18n::get("games_snake", lang)) + "</button>\n"
            "    <button id=\"tabBricks\" class=\"search-tab\" onclick=\"startGame('bricks')\">" + fs::htmlEscape(i18n::get("games_bricks", lang)) + "</button>\n"
            "    <button id=\"tab2048\" class=\"search-tab\" onclick=\"startGame('2048')\">" + fs::htmlEscape(i18n::get("games_2048", lang)) + "</button>\n"
            "  </div>\n"
            "</div>\n"
            "<div style=\"background:var(--bg-surface);border-radius:16px;padding:20px;border:1px solid var(--border-subtle);box-shadow:var(--shadow-md);text-align:center;\">\n"
            "  <div style=\"display:flex;justify-content:center;gap:24px;margin-bottom:12px;font-weight:bold;font-size:15px;\">\n"
            "    <span style=\"color:#38bdf8;\">" + fs::htmlEscape(i18n::get("games_score", lang)) + " <span id=\"scoreVal\">0</span></span>\n"
            "    <span style=\"color:#f59e0b;\">" + fs::htmlEscape(i18n::get("games_high", lang)) + " <span id=\"highVal\">0</span></span>\n"
            "    <button class=\"btn btn-sm btn-sec\" onclick=\"restartCurGame()\">" + fs::htmlEscape(i18n::get("games_restart", lang)) + "</button>\n"
            "  </div>\n"
            "  <div style=\"display:inline-block;background:#030712;padding:6px;border-radius:12px;border:2px solid var(--border-strong);box-shadow:inset 0 2px 10px rgba(0,0,0,0.8);\">\n"
            "    <canvas id=\"gameCanvas\" width=\"480\" height=\"360\" style=\"display:block;margin:0 auto;background:#090d16;border-radius:8px;\"></canvas>\n"
            "  </div>\n"
            "  <div style=\"margin-top:14px;color:var(--text-secondary);font-size:13px;\">\n"
            "    " + fs::htmlEscape(i18n::get("games_controls_hint", lang)) + "\n"
            "  </div>\n"
            "  <div style=\"margin-top:10px;display:flex;justify-content:center;gap:8px;\">\n"
            "    <button class=\"btn btn-sm btn-sec\" onclick=\"sendDir(0,-1)\">▲</button>\n"
            "    <button class=\"btn btn-sm btn-sec\" onclick=\"sendDir(-1,0)\">◀</button>\n"
            "    <button class=\"btn btn-sm btn-sec\" onclick=\"sendDir(0,1)\">▼</button>\n"
            "    <button class=\"btn btn-sm btn-sec\" onclick=\"sendDir(1,0)\">▶</button>\n"
            "  </div>\n"
            "</div>\n"
            "</div>\n"
            "<script>\n"
            "var cv = document.getElementById('gameCanvas');\n"
            "var ctx = cv.getContext('2d');\n"
            "var mode = 'snake';\n"
            "var score = 0;\n"
            "var highScore = localStorage.getItem('lite_highscore_' + mode) || 0;\n"
            "var timer = null;\n"
            "var snake = [{x:10,y:10}];\n"
            "var food = {x:15,y:15};\n"
            "var sdx = 1, sdy = 0;\n"
            "var ballX = 240, ballY = 300, bdx = 4, bdy = -4;\n"
            "var padX = 200, padW = 90;\n"
            "var bricks = [];\n"
            "var grid2048 = [];\n"
            "function setTab(name) {\n"
            "  document.getElementById('tabSnake').className = 'search-tab' + (name==='snake'?' active':'');\n"
            "  document.getElementById('tabBricks').className = 'search-tab' + (name==='bricks'?' active':'');\n"
            "  document.getElementById('tab2048').className = 'search-tab' + (name==='2048'?' active':'');\n"
            "}\n"
            "function restartCurGame() { startGame(mode); }\n"
            "function startGame(m) {\n"
            "  mode = m;\n"
            "  setTab(m);\n"
            "  if (timer) clearInterval(timer);\n"
            "  score = 0;\n"
            "  highScore = localStorage.getItem('lite_highscore_' + mode) || 0;\n"
            "  document.getElementById('scoreVal').innerHTML = score;\n"
            "  document.getElementById('highVal').innerHTML = highScore;\n"
            "  if (mode === 'snake') {\n"
            "    snake = [{x:12,y:10},{x:11,y:10},{x:10,y:10}];\n"
            "    sdx = 1; sdy = 0;\n"
            "    food = {x:Math.floor(Math.random()*23)+1, y:Math.floor(Math.random()*17)+1};\n"
            "    timer = setInterval(loopSnake, 90);\n"
            "  } else if (mode === 'bricks') {\n"
            "    ballX = 240; ballY = 280; bdx = 3; bdy = -3;\n"
            "    padX = 200;\n"
            "    bricks = [];\n"
            "    for (var r=0; r<4; r++) {\n"
            "      for (var c=0; c<8; c++) {\n"
            "        bricks.push({x: c*58 + 10, y: r*24 + 30, w: 52, h: 18, alive: 1});\n"
            "      }\n"
            "    }\n"
            "    timer = setInterval(loopBricks, 20);\n"
            "  } else {\n"
            "    init2048();\n"
            "    draw2048();\n"
            "  }\n"
            "}\n"
            "function loopSnake() {\n"
            "  var head = {x: snake[0].x + sdx, y: snake[0].y + sdy};\n"
            "  if (head.x < 0 || head.x >= 24 || head.y < 0 || head.y >= 18) { gameOver(); return; }\n"
            "  for (var i=0; i<snake.length; i++) {\n"
            "    if (snake[i].x === head.x && snake[i].y === head.y) { gameOver(); return; }\n"
            "  }\n"
            "  snake.unshift(head);\n"
            "  if (head.x === food.x && head.y === food.y) {\n"
            "    score += 10;\n"
            "    food = {x:Math.floor(Math.random()*23)+1, y:Math.floor(Math.random()*17)+1};\n"
            "    updateScore(score);\n"
            "  } else {\n"
            "    snake.pop();\n"
            "  }\n"
            "  ctx.fillStyle = '#090d16'; ctx.fillRect(0,0,480,360);\n"
            "  ctx.fillStyle = '#f43f5e'; ctx.fillRect(food.x*20, food.y*20, 18, 18);\n"
            "  ctx.fillStyle = '#10b981';\n"
            "  for (var j=0; j<snake.length; j++) {\n"
            "    ctx.fillRect(snake[j].x*20, snake[j].y*20, 18, 18);\n"
            "  }\n"
            "}\n"
            "function loopBricks() {\n"
            "  ballX += bdx; ballY += bdy;\n"
            "  if (ballX <= 6 || ballX >= 474) bdx = -bdx;\n"
            "  if (ballY <= 6) bdy = -bdy;\n"
            "  if (ballY >= 335 && ballX >= padX && ballX <= padX + padW) {\n"
            "    bdy = -Math.abs(bdy);\n"
            "    bdx = ((ballX - (padX + padW/2)) / (padW/2)) * 4;\n"
            "  }\n"
            "  if (ballY > 360) { gameOver(); return; }\n"
            "  var allDead = true;\n"
            "  for (var i=0; i<bricks.length; i++) {\n"
            "    var b = bricks[i];\n"
            "    if (b.alive) {\n"
            "      allDead = false;\n"
            "      if (ballX >= b.x && ballX <= b.x + b.w && ballY >= b.y && ballY <= b.y + b.h) {\n"
            "        b.alive = 0; bdy = -bdy; score += 20; updateScore(score);\n"
            "      }\n"
            "    }\n"
            "  }\n"
            "  if (allDead) { alert('مبروك! فزت!'); startGame('bricks'); return; }\n"
            "  ctx.fillStyle = '#090d16'; ctx.fillRect(0,0,480,360);\n"
            "  ctx.fillStyle = '#38bdf8'; ctx.fillRect(padX, 340, padW, 12);\n"
            "  ctx.beginPath(); ctx.arc(ballX, ballY, 6, 0, Math.PI*2); ctx.fillStyle = '#facc15'; ctx.fill();\n"
            "  for (var j=0; j<bricks.length; j++) {\n"
            "    if (bricks[j].alive) {\n"
            "      ctx.fillStyle = j % 2 === 0 ? '#0284c7' : '#ec4899';\n"
            "      ctx.fillRect(bricks[j].x, bricks[j].y, bricks[j].w, bricks[j].h);\n"
            "    }\n"
            "  }\n"
            "}\n"
            "function init2048() {\n"
            "  grid2048 = [ [0,0,0,0], [0,0,0,0], [0,0,0,0], [0,0,0,0] ];\n"
            "  addRandom2048(); addRandom2048();\n"
            "}\n"
            "function addRandom2048() {\n"
            "  var empty = [];\n"
            "  for (var r=0; r<4; r++) {\n"
            "    for (var c=0; c<4; c++) {\n"
            "      if (grid2048[r][c] === 0) empty.push({r:r, c:c});\n"
            "    }\n"
            "  }\n"
            "  if (empty.length > 0) {\n"
            "    var spot = empty[Math.floor(Math.random() * empty.length)];\n"
            "    grid2048[spot.r][spot.c] = Math.random() < 0.9 ? 2 : 4;\n"
            "  }\n"
            "}\n"
            "function draw2048() {\n"
            "  ctx.fillStyle = '#1e293b'; ctx.fillRect(0,0,480,360);\n"
            "  var colors = { 0:'#334155', 2:'#e2e8f0', 4:'#fef08a', 8:'#fdba74', 16:'#f87171', 32:'#fb7185', 64:'#38bdf8', 128:'#a78bfa', 256:'#fbbf24', 512:'#34d399', 1024:'#f472b6', 2048:'#e11d48' };\n"
            "  for (var r=0; r<4; r++) {\n"
            "    for (var c=0; c<4; c++) {\n"
            "      var v = grid2048[r][c];\n"
            "      var x = 80 + c * 80;\n"
            "      var y = 20 + r * 80;\n"
            "      ctx.fillStyle = colors[v] || '#ffffff';\n"
            "      ctx.fillRect(x, y, 74, 74);\n"
            "      if (v > 0) {\n"
            "        ctx.fillStyle = v <= 4 ? '#0f172a' : '#ffffff';\n"
            "        ctx.font = 'bold 24px Tahoma, sans-serif';\n"
            "        ctx.textAlign = 'center';\n"
            "        ctx.fillText(v, x + 37, y + 46);\n"
            "      }\n"
            "    }\n"
            "  }\n"
            "}\n"
            "function move2048(dx, dy) {\n"
            "  var moved = false;\n"
            "  if (dx === -1) { for (var r=0; r<4; r++) moved = slide2048(r, 0, 1) || moved; }\n"
            "  else if (dx === 1) { for (var r=0; r<4; r++) moved = slide2048(r, 3, -1) || moved; }\n"
            "  else if (dy === -1) { for (var c=0; c<4; c++) moved = slideCol2048(c, 0, 1) || moved; }\n"
            "  else if (dy === 1) { for (var c=0; c<4; c++) moved = slideCol2048(c, 3, -1) || moved; }\n"
            "  if (moved) {\n"
            "    addRandom2048();\n"
            "    draw2048();\n"
            "    updateScore(score);\n"
            "  }\n"
            "}\n"
            "function slide2048(r, start, step) {\n"
            "  var row = [];\n"
            "  for (var i=0; i<4; i++) { var v = grid2048[r][start + i*step]; if (v) row.push(v); }\n"
            "  for (var i=0; i<row.length-1; i++) {\n"
            "    if (row[i] === row[i+1]) { row[i] *= 2; score += row[i]; row.splice(i+1, 1); }\n"
            "  }\n"
            "  while (row.length < 4) row.push(0);\n"
            "  var changed = false;\n"
            "  for (var i=0; i<4; i++) {\n"
            "    if (grid2048[r][start + i*step] !== row[i]) changed = true;\n"
            "    grid2048[r][start + i*step] = row[i];\n"
            "  }\n"
            "  return changed;\n"
            "}\n"
            "function slideCol2048(c, start, step) {\n"
            "  var col = [];\n"
            "  for (var i=0; i<4; i++) { var v = grid2048[start + i*step][c]; if (v) col.push(v); }\n"
            "  for (var i=0; i<col.length-1; i++) {\n"
            "    if (col[i] === col[i+1]) { col[i] *= 2; score += col[i]; col.splice(i+1, 1); }\n"
            "  }\n"
            "  while (col.length < 4) col.push(0);\n"
            "  var changed = false;\n"
            "  for (var i=0; i<4; i++) {\n"
            "    if (grid2048[start + i*step][c] !== col[i]) changed = true;\n"
            "    grid2048[start + i*step][c] = col[i];\n"
            "  }\n"
            "  return changed;\n"
            "}\n"
            "function updateScore(s) {\n"
            "  document.getElementById('scoreVal').innerHTML = s;\n"
            "  if (s > highScore) {\n"
            "    highScore = s;\n"
            "    localStorage.setItem('lite_highscore_' + mode, highScore);\n"
            "    document.getElementById('highVal').innerHTML = highScore;\n"
            "  }\n"
            "}\n"
            "function gameOver() {\n"
            "  if (timer) clearInterval(timer);\n"
            "  alert('انتهت اللعبة! نقاطك: ' + score);\n"
            "  startGame(mode);\n"
            "}\n"
            "function sendDir(dx, dy) {\n"
            "  if (mode === 'snake') {\n"
            "    if (dx !== -sdx || dy !== -sdy) { sdx = dx; sdy = dy; }\n"
            "  } else if (mode === 'bricks') {\n"
            "    padX = Math.max(0, Math.min(390, padX + dx * 32));\n"
            "  } else {\n"
            "    move2048(dx, dy);\n"
            "  }\n"
            "}\n"
            "document.onkeydown = function(e) {\n"
            "  e = e || window.event; var k = e.keyCode;\n"
            "  if (k === 37 || k === 65) { sendDir(-1,0); return false; }\n"
            "  if (k === 38 || k === 87) { sendDir(0,-1); return false; }\n"
            "  if (k === 39 || k === 68) { sendDir(1,0); return false; }\n"
            "  if (k === 40 || k === 83) { sendDir(0,1); return false; }\n"
            "};\n"
            "startGame('snake');\n"
            "</script>\n</body>\n</html>\n";
    return html;
}

std::string buildVideoHtml(StorageManager& storage) {
    BrowserSettings s = storage.getSettings();
    std::string lang = s.language.empty() ? "ar" : s.language;

    std::string html = commonPageHead(i18n::get("video_hero_title", lang), "lite://video", storage);
    html += "<div class=\"wrap\">\n"
            "<div class=\"hero\" style=\"padding:24px;\">\n"
            "  <h1>" + fs::htmlEscape(i18n::get("video_hero_title", lang)) + "</h1>\n"
            "  <p>" + fs::htmlEscape(i18n::get("video_hero_desc", lang)) + "</p>\n"
            "  <form onsubmit=\"return searchYt();\" class=\"search-row\" style=\"max-width:620px;margin:0 auto 14px auto;\">\n"
            "    <input id=\"ytQ\" class=\"search-inp\" type=\"text\" placeholder=\"" +
            fs::htmlEscape(i18n::get("video_search_placeholder", lang)) + "\"/>\n"
            "    <button type=\"submit\" class=\"btn btn-rose\">" + fs::htmlEscape(i18n::get("video_search_btn", lang)) + "</button>\n"
            "  </form>\n"
            "  <div class=\"quick-links\">\n"
            "    <a href=\"https://www.youtube.com\">" + fs::htmlEscape(i18n::get("video_quick_yt", lang)) + "</a>\n"
            "    <a href=\"https://m.youtube.com\">" + fs::htmlEscape(i18n::get("video_quick_yt_m", lang)) + "</a>\n"
            "    <a href=\"https://archive.org/details/movies\">" + fs::htmlEscape(i18n::get("video_quick_archive", lang)) + "</a>\n"
            "  </div>\n"
            "</div>\n"
            "<div style=\"background:var(--bg-surface);border-radius:16px;padding:24px;border:1px solid var(--border-subtle);box-shadow:var(--shadow-md);\">\n"
            "  <h3 style=\"margin-top:0;font-size:17px;font-weight:700;\">" + fs::htmlEscape(i18n::get("video_player_title", lang)) + "</h3>\n"
            "  <p style=\"color:var(--text-secondary);font-size:13px;margin-bottom:14px;\">" +
            fs::htmlEscape(i18n::get("video_player_desc", lang)) + "</p>\n"
            "  <div class=\"search-row\" style=\"max-width:100%;margin-bottom:16px;\">\n"
            "    <input id=\"vUrl\" class=\"search-inp\" type=\"text\" placeholder=\"" +
            fs::htmlEscape(i18n::get("video_url_placeholder", lang)) + "\"/>\n"
            "    <button class=\"btn\" onclick=\"playVideo()\">" + fs::htmlEscape(i18n::get("video_play_btn", lang)) + "</button>\n"
            "    <button class=\"btn btn-sec\" onclick=\"dlVideo()\">" + fs::htmlEscape(i18n::get("video_download_btn", lang)) + "</button>\n"
            "  </div>\n"
            "  <div style=\"background:#030712;border-radius:12px;padding:12px;border:1px solid var(--border-strong);text-align:center;\">\n"
            "    <video id=\"vPlayer\" controls style=\"max-width:100%;height:420px;border-radius:8px;outline:none;\">\n"
            "      <p style=\"color:#fff;\">HTML5 Video is not supported on this view.</p>\n"
            "    </video>\n"
            "  </div>\n"
            "</div>\n"
            "</div>\n"
            "<script>\n"
            "function searchYt() {\n"
            "  var q = document.getElementById('ytQ').value;\n"
            "  if (!q) return false;\n"
            "  location.href = 'https://www.youtube.com/results?search_query=' + encodeURIComponent(q);\n"
            "  return false;\n"
            "}\n"
            "function playVideo() {\n"
            "  var u = document.getElementById('vUrl').value;\n"
            "  if (!u) return;\n"
            "  var p = document.getElementById('vPlayer');\n"
            "  p.src = u;\n"
            "  p.play();\n"
            "}\n"
            "function dlVideo() {\n"
            "  var u = document.getElementById('vUrl').value;\n"
            "  if (!u) return;\n"
            "  location.href = 'lite-action://download?url=' + encodeURIComponent(u);\n"
            "}\n"
            "</script>\n</body>\n</html>\n";
    return html;
}

std::string buildDownloadsHtml(StorageManager& storage) {
    BrowserSettings s = storage.getSettings();
    std::string lang = s.language.empty() ? "ar" : s.language;
    auto downloads = storage.getDownloads();

    std::string html = commonPageHead(i18n::get("dl_hero_title", lang), "lite://downloads", storage);
    html += "<div class=\"wrap\">\n"
            "<div class=\"hero\" style=\"padding:24px;\">\n"
            "  <h1>" + fs::htmlEscape(i18n::get("dl_hero_title", lang)) + "</h1>\n"
            "  <p>" + fs::htmlEscape(i18n::get("dl_hero_desc", lang)) + "</p>\n"
            "  <form onsubmit=\"return startDirectDl();\" class=\"search-row\" style=\"max-width:680px;margin:0 auto 12px auto;\">\n"
            "    <input id=\"dlUrlInput\" class=\"search-inp\" type=\"text\" placeholder=\"" +
            fs::htmlEscape(i18n::get("dl_new_placeholder", lang)) + "\"/>\n"
            "    <button type=\"submit\" class=\"btn\">⬇️ " + fs::htmlEscape(i18n::get("dl_start_btn", lang)) + "</button>\n"
            "  </form>\n"
            "</div>\n"
            "<div class=\"filter-bar\">\n"
            "  <input id=\"dlFilter\" class=\"filter-inp\" type=\"text\" placeholder=\"" +
            fs::htmlEscape(i18n::get("dl_filter_placeholder", lang)) + "\" onkeyup=\"filterDownloads()\"/>\n"
            "  <a class=\"btn btn-sm btn-sec\" href=\"lite-action://open-downloads-dir\">" +
            fs::htmlEscape(i18n::get("dl_open_folder", lang)) + "</a>\n"
            "  <a class=\"btn btn-sm btn-sec\" href=\"lite-action://clear-completed-downloads\">" +
            fs::htmlEscape(i18n::get("dl_clear_completed", lang)) + "</a>\n"
            "</div>\n"
            "<div class=\"tbl-wrap\">\n"
            "<table class=\"list-tbl\" id=\"dlTable\">\n"
            "  <thead><tr>\n"
            "    <th>#</th><th>" + fs::htmlEscape(i18n::get("dl_col_file", lang)) + "</th>\n"
            "    <th>" + fs::htmlEscape(i18n::get("dl_col_size", lang)) + "</th>\n"
            "    <th>" + fs::htmlEscape(i18n::get("dl_col_progress", lang)) + "</th>\n"
            "    <th>" + fs::htmlEscape(i18n::get("dl_col_status", lang)) + "</th>\n"
            "    <th>" + fs::htmlEscape(i18n::get("dl_col_actions", lang)) + "</th>\n"
            "  </tr></thead>\n"
            "  <tbody>\n";

    if (downloads.empty()) {
        html += "    <tr><td colspan=\"6\" style=\"text-align:center;padding:24px;color:var(--text-secondary);\">" +
                fs::htmlEscape(i18n::get("dl_empty", lang)) + "</td></tr>\n";
    } else {
        for (const auto& d : downloads) {
            std::string stText, stColor;
            if (d.status == DownloadStatus::Completed) { stText = i18n::get("dl_status_done", lang); stColor = "#10b981"; }
            else if (d.status == DownloadStatus::InProgress) { stText = i18n::get("dl_status_active", lang); stColor = "#0284c7"; }
            else if (d.status == DownloadStatus::Paused) { stText = i18n::get("dl_status_paused", lang); stColor = "#f59e0b"; }
            else { stText = i18n::get("dl_status_failed", lang); stColor = "#f43f5e"; }

            int pct = (d.totalBytes > 0) ? static_cast<int>((d.receivedBytes * 100) / d.totalBytes) : 0;
            std::string sizeStr = fs::formatBytes(d.receivedBytes);
            if (d.totalBytes > 0) sizeStr += " / " + fs::formatBytes(d.totalBytes);

            html += "    <tr class=\"dl-row\">\n"
                    "      <td>" + std::to_string(d.id) + "</td>\n"
                    "      <td class=\"dl-name\" style=\"font-weight:bold;\">" + fs::htmlEscape(d.fileName) + "</td>\n"
                    "      <td>" + sizeStr + "</td>\n"
                    "      <td>\n"
                    "        <div class=\"prog-bg\"><div class=\"prog-fill\" style=\"width:" + std::to_string(pct) + "%;\"></div></div>\n"
                    "        <span>" + std::to_string(pct) + "%</span>\n"
                    "      </td>\n"
                    "      <td><span style=\"color:" + stColor + ";font-weight:bold;\">" + stText + "</span></td>\n"
                    "      <td>\n";
            if (d.status == DownloadStatus::Completed) {
                html += "        <a class=\"btn btn-sm btn-green\" href=\"lite-action://open-downloaded?id=" + std::to_string(d.id) + "\">" + fs::htmlEscape(i18n::get("dl_open", lang)) + "</a>\n";
            } else if (d.status == DownloadStatus::InProgress) {
                html += "        <a class=\"btn btn-sm btn-amber\" href=\"lite-action://pause-download?id=" + std::to_string(d.id) + "\">" + fs::htmlEscape(i18n::get("dl_pause", lang)) + "</a>\n";
                html += "        <a class=\"btn btn-sm btn-rose\" href=\"lite-action://cancel-download?id=" + std::to_string(d.id) + "\">" + fs::htmlEscape(i18n::get("dl_cancel", lang)) + "</a>\n";
            } else if (d.status == DownloadStatus::Paused) {
                html += "        <a class=\"btn btn-sm btn-sec\" href=\"lite-action://resume-download?id=" + std::to_string(d.id) + "\">" + fs::htmlEscape(i18n::get("dl_resume", lang)) + "</a>\n";
                html += "        <a class=\"btn btn-sm btn-rose\" href=\"lite-action://cancel-download?id=" + std::to_string(d.id) + "\">" + fs::htmlEscape(i18n::get("dl_cancel", lang)) + "</a>\n";
            }
            html += "      </td>\n    </tr>\n";
        }
    }

    html += "  </tbody>\n</table>\n</div>\n</div>\n"
            "<script>\n"
            "function startDirectDl() {\n"
            "  var u = document.getElementById('dlUrlInput').value;\n"
            "  if (!u) return false;\n"
            "  location.href = 'lite-action://download?url=' + encodeURIComponent(u);\n"
            "  return false;\n"
            "}\n"
            "function filterDownloads() {\n"
            "  var q = document.getElementById('dlFilter').value.toLowerCase();\n"
            "  var rows = document.getElementsByClassName('dl-row');\n"
            "  for (var i = 0; i < rows.length; i++) {\n"
            "    var name = rows[i].getElementsByClassName('dl-name')[0].innerText.toLowerCase();\n"
            "    rows[i].style.display = (name.indexOf(q) !== -1) ? '' : 'none';\n"
            "  }\n"
            "}\n"
            "</script>\n</body>\n</html>\n";
    return html;
}

std::string buildBookmarksHtml(StorageManager& storage) {
    BrowserSettings s = storage.getSettings();
    std::string lang = s.language.empty() ? "ar" : s.language;
    auto bookmarks = storage.getBookmarks();

    std::string html = commonPageHead(i18n::get("bm_hero_title", lang), "lite://bookmarks", storage);
    html += "<div class=\"wrap\">\n"
            "<div class=\"hero\" style=\"padding:24px;\">\n"
            "  <h1>" + fs::htmlEscape(i18n::get("bm_hero_title", lang)) + "</h1>\n"
            "  <p>" + fs::htmlEscape(i18n::get("bm_hero_desc", lang)) + "</p>\n"
            "</div>\n"
            "<div class=\"filter-bar\">\n"
            "  <input id=\"bmFilter\" class=\"filter-inp\" type=\"text\" placeholder=\"" +
            fs::htmlEscape(i18n::get("bm_filter_placeholder", lang)) + "\" onkeyup=\"filterBookmarks()\"/>\n"
            "</div>\n"
            "<div class=\"tbl-wrap\">\n"
            "<table class=\"list-tbl\" id=\"bmTable\">\n"
            "  <thead><tr><th>★</th><th>" + fs::htmlEscape(i18n::get("dl_col_file", lang)) + "</th><th>URL</th><th>" +
            fs::htmlEscape(i18n::get("dl_col_actions", lang)) + "</th></tr></thead>\n"
            "  <tbody>\n";

    if (bookmarks.empty()) {
        html += "    <tr><td colspan=\"4\" style=\"text-align:center;padding:24px;color:var(--text-secondary);\">" +
                fs::htmlEscape(i18n::get("bm_empty", lang)) + "</td></tr>\n";
    } else {
        for (size_t i = 0; i < bookmarks.size(); ++i) {
            const auto& b = bookmarks[i];
            html += "    <tr class=\"bm-row\">\n"
                    "      <td style=\"color:#f59e0b;\">★</td>\n"
                    "      <td class=\"bm-title\" style=\"font-weight:bold;\"><a style=\"color:inherit;text-decoration:none;\" href=\"" + fs::htmlEscape(b.url) + "\">" + fs::htmlEscape(b.title) + "</a></td>\n"
                    "      <td style=\"color:var(--text-secondary);font-family:monospace;font-size:12px;\">" + fs::htmlEscape(b.url) + "</td>\n"
                    "      <td>\n"
                    "        <button class=\"btn btn-sm btn-sec\" onclick=\"copyText('" + fs::htmlEscape(b.url) + "')\">" + fs::htmlEscape(i18n::get("bm_copy", lang)) + "</button>\n"
                    "        <a class=\"btn btn-sm btn-rose\" href=\"lite-action://remove-bookmark?idx=" + std::to_string(i) + "\">" + fs::htmlEscape(i18n::get("bm_delete", lang)) + "</a>\n"
                    "      </td>\n    </tr>\n";
        }
    }

    html += "  </tbody>\n</table>\n</div>\n</div>\n"
            "<script>\n"
            "function filterBookmarks() {\n"
            "  var q = document.getElementById('bmFilter').value.toLowerCase();\n"
            "  var rows = document.getElementsByClassName('bm-row');\n"
            "  for (var i = 0; i < rows.length; i++) {\n"
            "    var t = rows[i].getElementsByClassName('bm-title')[0].innerText.toLowerCase();\n"
            "    rows[i].style.display = (t.indexOf(q) !== -1) ? '' : 'none';\n"
            "  }\n"
            "}\n"
            "function copyText(txt) {\n"
            "  if (navigator.clipboard) { navigator.clipboard.writeText(txt); alert('تم نسخ الرابط!'); }\n"
            "  else { prompt('انسخ الرابط:', txt); }\n"
            "}\n"
            "</script>\n</body>\n</html>\n";
    return html;
}

std::string buildHistoryHtml(StorageManager& storage) {
    BrowserSettings s = storage.getSettings();
    std::string lang = s.language.empty() ? "ar" : s.language;
    auto history = storage.getHistory();

    std::string html = commonPageHead(i18n::get("hist_hero_title", lang), "lite://history", storage);
    html += "<div class=\"wrap\">\n"
            "<div class=\"hero\" style=\"padding:24px;\">\n"
            "  <h1>" + fs::htmlEscape(i18n::get("hist_hero_title", lang)) + "</h1>\n"
            "  <p>" + fs::htmlEscape(i18n::get("hist_hero_desc", lang)) + "</p>\n"
            "</div>\n"
            "<div class=\"filter-bar\">\n"
            "  <input id=\"hFilter\" class=\"filter-inp\" type=\"text\" placeholder=\"" +
            fs::htmlEscape(i18n::get("hist_filter_placeholder", lang)) + "\" onkeyup=\"filterHistory()\"/>\n"
            "  <a class=\"btn btn-sm btn-rose\" href=\"lite-action://clear-history\">" +
            fs::htmlEscape(i18n::get("hist_clear_all", lang)) + "</a>\n"
            "</div>\n"
            "<div class=\"tbl-wrap\">\n"
            "<table class=\"list-tbl\" id=\"hTable\">\n"
            "  <thead><tr><th>🕒</th><th>" + fs::htmlEscape(i18n::get("dl_col_file", lang)) + "</th><th>URL</th></tr></thead>\n"
            "  <tbody>\n";

    if (history.empty()) {
        html += "    <tr><td colspan=\"3\" style=\"text-align:center;padding:24px;color:var(--text-secondary);\">" +
                fs::htmlEscape(i18n::get("hist_empty", lang)) + "</td></tr>\n";
    } else {
        for (auto it = history.rbegin(); it != history.rend(); ++it) {
            html += "    <tr class=\"h-row\">\n"
                    "      <td style=\"color:var(--text-muted);font-size:11px;\">" + fs::htmlEscape(it->timestamp) + "</td>\n"
                    "      <td class=\"h-title\" style=\"font-weight:bold;\"><a style=\"color:inherit;text-decoration:none;\" href=\"" + fs::htmlEscape(it->url) + "\">" + fs::htmlEscape(it->title) + "</a></td>\n"
                    "      <td style=\"color:var(--text-secondary);font-family:monospace;font-size:12px;\">" + fs::htmlEscape(it->url) + "</td>\n"
                    "    </tr>\n";
        }
    }

    html += "  </tbody>\n</table>\n</div>\n</div>\n"
            "<script>\n"
            "function filterHistory() {\n"
            "  var q = document.getElementById('hFilter').value.toLowerCase();\n"
            "  var rows = document.getElementsByClassName('h-row');\n"
            "  for (var i = 0; i < rows.length; i++) {\n"
            "    var t = rows[i].getElementsByClassName('h-title')[0].innerText.toLowerCase();\n"
            "    rows[i].style.display = (t.indexOf(q) !== -1) ? '' : 'none';\n"
            "  }\n"
            "}\n"
            "</script>\n</body>\n</html>\n";
    return html;
}

std::string buildSettingsHtml(StorageManager& storage) {
    BrowserSettings s = storage.getSettings();
    std::string lang = s.language.empty() ? "ar" : s.language;
    bool en = (lang == "en");

    std::string html = commonPageHead(i18n::get("set_hero_title", lang), "lite://settings", storage);
    html += "<div class=\"wrap\">\n"
            "<div class=\"hero\" style=\"padding:24px;\">\n"
            "  <h1>" + fs::htmlEscape(i18n::get("set_hero_title", lang)) + "</h1>\n"
            "  <p>" + fs::htmlEscape(i18n::get("set_hero_desc", lang)) + "</p>\n"
            "</div>\n";

    // Appearance & Language Group
    html += "<div class=\"tbl-wrap\" style=\"padding:24px;margin-bottom:20px;\">\n"
            "  <h3 style=\"margin-bottom:14px;font-size:16px;\">" + fs::htmlEscape(i18n::get("set_group_appearance", lang)) + "</h3>\n"
            "  <div style=\"margin-bottom:16px;\">\n"
            "    <label style=\"display:block;margin-bottom:6px;font-weight:bold;font-size:13px;\">" +
            fs::htmlEscape(i18n::get("set_theme_label", lang)) + "</label>\n"
            "    <select class=\"filter-inp\" style=\"max-width:320px;\" onchange=\"saveTheme(this.value)\">\n"
            "      <option value=\"dark\"" + std::string(s.isDarkMode() ? " selected" : "") + ">" +
            fs::htmlEscape(i18n::get("set_theme_dark_opt", lang)) + "</option>\n"
            "      <option value=\"light\"" + std::string(!s.isDarkMode() ? " selected" : "") + ">" +
            fs::htmlEscape(i18n::get("set_theme_light_opt", lang)) + "</option>\n"
            "    </select>\n"
            "  </div>\n"
            "  <div>\n"
            "    <label style=\"display:block;margin-bottom:6px;font-weight:bold;font-size:13px;\">" +
            fs::htmlEscape(i18n::get("set_lang_label", lang)) + "</label>\n"
            "    <select class=\"filter-inp\" style=\"max-width:320px;\" onchange=\"saveLang(this.value)\">\n"
            "      <option value=\"ar\"" + std::string(lang == "ar" ? " selected" : "") + ">" +
            fs::htmlEscape(i18n::get("set_lang_ar_opt", lang)) + "</option>\n"
            "      <option value=\"en\"" + std::string(lang == "en" ? " selected" : "") + ">" +
            fs::htmlEscape(i18n::get("set_lang_en_opt", lang)) + "</option>\n"
            "    </select>\n"
            "  </div>\n"
            "</div>\n";

    // Search Engine Group
    html += "<div class=\"tbl-wrap\" style=\"padding:24px;margin-bottom:20px;\">\n"
            "  <h3 style=\"margin-bottom:14px;font-size:16px;\">" + fs::htmlEscape(i18n::get("set_group_search", lang)) + "</h3>\n"
            "  <div style=\"margin-bottom:16px;\">\n"
            "    <label style=\"display:block;margin-bottom:6px;font-weight:bold;font-size:13px;\">" +
            fs::htmlEscape(i18n::get("set_search_label", lang)) + "</label>\n"
            "    <select class=\"filter-inp\" style=\"max-width:320px;\" onchange=\"saveSearch(this.value)\">\n"
            "      <option value=\"google\"" + std::string(s.searchEngine == "google" ? " selected" : "") + ">Google</option>\n"
            "      <option value=\"ddg\"" + std::string(s.searchEngine == "ddg" ? " selected" : "") + ">DuckDuckGo</option>\n"
            "      <option value=\"ddg-lite\"" + std::string(s.searchEngine == "ddg-lite" ? " selected" : "") + ">DDG Lite (HTML Only)</option>\n"
            "      <option value=\"bing\"" + std::string(s.searchEngine == "bing" ? " selected" : "") + ">Bing</option>\n"
            "      <option value=\"youtube\"" + std::string(s.searchEngine == "youtube" ? " selected" : "") + ">YouTube</option>\n"
            "    </select>\n"
            "  </div>\n"
            "  <div>\n"
            "    <label style=\"display:block;margin-bottom:6px;font-weight:bold;font-size:13px;\">" +
            fs::htmlEscape(i18n::get("set_home_label", lang)) + "</label>\n"
            "    <div style=\"display:flex;gap:8px;max-width:540px;\">\n"
            "      <input id=\"homeInp\" class=\"filter-inp\" type=\"text\" value=\"" + fs::htmlEscape(s.homePage) + "\"/>\n"
            "      <button class=\"btn btn-sm\" onclick=\"saveHomePage()\">" + fs::htmlEscape(i18n::get("set_home_save_btn", lang)) + "</button>\n"
            "    </div>\n"
            "  </div>\n"
            "</div>\n";

    // Performance & Resource Saving Group
    html += "<div class=\"tbl-wrap\" style=\"padding:24px;margin-bottom:20px;\">\n"
            "  <h3 style=\"margin-bottom:14px;font-size:16px;\">" + fs::htmlEscape(i18n::get("set_group_performance", lang)) + "</h3>\n"
            "  <div style=\"margin-bottom:18px;\">\n"
            "    <label style=\"font-weight:bold;display:block;margin-bottom:4px;\">" +
            fs::htmlEscape(i18n::get("set_lite_mode_label", lang)) + " " +
            std::string(s.liteMode ? "<span style=\"color:#10b981;\">ON</span>" : "<span style=\"color:var(--text-muted);\">OFF</span>") + "</label>\n"
            "    <p style=\"font-size:13px;color:var(--text-secondary);margin-bottom:8px;\">" +
            fs::htmlEscape(i18n::get("set_lite_mode_desc", lang)) + "</p>\n"
            "    <a class=\"btn btn-sm btn-sec\" href=\"lite-action://toggle-lite\">" +
            fs::htmlEscape(i18n::get("home_toggle_mode", lang)) + "</a>\n"
            "  </div>\n"
            "  <div style=\"margin-bottom:18px;\">\n"
            "    <label style=\"font-weight:bold;display:block;margin-bottom:4px;\">" +
            fs::htmlEscape(i18n::get("set_adblock_label", lang)) + " " +
            std::string(s.adBlockEnabled ? "<span style=\"color:#10b981;\">ON</span>" : "<span style=\"color:var(--text-muted);\">OFF</span>") + "</label>\n"
            "    <p style=\"font-size:13px;color:var(--text-secondary);margin-bottom:8px;\">" +
            fs::htmlEscape(i18n::get("set_adblock_desc", lang)) + "</p>\n"
            "    <a class=\"btn btn-sm btn-sec\" href=\"lite-action://toggle-adblock\">" +
            std::string(s.adBlockEnabled ? (en ? "Disable AdBlock" : "إيقاف مانع الإعلانات") : (en ? "Enable AdBlock" : "تفعيل مانع الإعلانات")) + "</a>\n"
            "  </div>\n"
            "  <div>\n"
            "    <label style=\"font-weight:bold;display:block;margin-bottom:4px;\">" +
            fs::htmlEscape(i18n::get("set_restore_label", lang)) + "</label>\n"
            "    <p style=\"font-size:13px;color:var(--text-secondary);margin-bottom:8px;\">" +
            fs::htmlEscape(i18n::get("set_restore_desc", lang)) + "</p>\n"
            "    <a class=\"btn btn-sm btn-sec\" href=\"lite-action://set-setting?key=restore-session&val=" +
            std::string(s.restoreSessionOnStartup ? "0" : "1") + "\">" +
            std::string(s.restoreSessionOnStartup ? (en ? "Turn OFF" : "تعطيل") : (en ? "Turn ON" : "تفعيل")) + "</a>\n"
            "  </div>\n"
            "</div>\n";

    // Privacy & Cache Group
    html += "<div class=\"tbl-wrap\" style=\"padding:24px;margin-bottom:20px;\">\n"
            "  <h3 style=\"margin-bottom:14px;font-size:16px;\">" + fs::htmlEscape(i18n::get("set_group_privacy", lang)) + "</h3>\n"
            "  <div style=\"display:flex;gap:10px;flex-wrap:wrap;\">\n"
            "    <a class=\"btn btn-sm btn-sec\" href=\"lite-action://clear-cache\">" + fs::htmlEscape(i18n::get("set_clear_cache", lang)) + "</a>\n"
            "    <a class=\"btn btn-sm btn-sec\" href=\"lite-action://clear-history\">" + fs::htmlEscape(i18n::get("set_clear_hist", lang)) + "</a>\n"
            "    <a class=\"btn btn-sm btn-sec\" href=\"lite-action://clear-completed-downloads\">" + fs::htmlEscape(i18n::get("set_clear_dl", lang)) + "</a>\n"
            "  </div>\n"
            "</div>\n";

    // Windows XP Tip
    html += "<div class=\"tbl-wrap\" style=\"padding:24px;background:var(--bg-surface-alt);border-inline-start:4px solid #38bdf8;\">\n"
            "  <h3 style=\"margin-top:0;font-size:15px;color:#0284c7;\">💡 " + fs::htmlEscape(i18n::get("set_xp_tip_title", lang)) + "</h3>\n"
            "  <p style=\"font-size:13px;color:var(--text-secondary);margin:0;\">" +
            fs::htmlEscape(i18n::get("set_xp_tip_desc", lang)) + "</p>\n"
            "</div>\n";

    html += "</div>\n"
            "<script>\n"
            "function saveTheme(val) {\n"
            "  location.href = 'lite-action://set-setting?key=theme&val=' + encodeURIComponent(val);\n"
            "}\n"
            "function saveLang(val) {\n"
            "  location.href = 'lite-action://set-setting?key=language&val=' + encodeURIComponent(val);\n"
            "}\n"
            "function saveSearch(val) {\n"
            "  location.href = 'lite-action://set-setting?key=search-engine&val=' + encodeURIComponent(val);\n"
            "}\n"
            "function saveHomePage() {\n"
            "  var val = document.getElementById('homeInp').value;\n"
            "  location.href = 'lite-action://set-setting?key=homepage&val=' + encodeURIComponent(val);\n"
            "}\n"
            "</script>\n</body>\n</html>\n";
    return html;
}

}  // namespace

InternalPageOutput renderInternalPage(const std::string& liteUri, StorageManager& storage, int slotId) {
    InternalPageOutput out;
    BrowserSettings s = storage.getSettings();
    std::string lang = s.language.empty() ? "ar" : s.language;

    std::string u = fs::toLowerAscii(fs::trim(liteUri));
    if (u == "lite://games") {
        out.title = i18n::get("nav_games", lang);
        out.html = buildGamesHtml(storage);
    } else if (u == "lite://video") {
        out.title = i18n::get("nav_video", lang);
        out.html = buildVideoHtml(storage);
    } else if (u == "lite://downloads") {
        out.title = i18n::get("nav_downloads", lang);
        out.html = buildDownloadsHtml(storage);
    } else if (u == "lite://bookmarks") {
        out.title = i18n::get("nav_bookmarks", lang);
        out.html = buildBookmarksHtml(storage);
    } else if (u == "lite://history") {
        out.title = i18n::get("nav_history", lang);
        out.html = buildHistoryHtml(storage);
    } else if (u == "lite://settings") {
        out.title = i18n::get("nav_settings", lang);
        out.html = buildSettingsHtml(storage);
    } else {
        out.title = i18n::get("nav_home", lang);
        out.html = buildHomeHtml(storage);
    }

    char nameBuf[64];
    std::snprintf(nameBuf, sizeof(nameBuf), "internal_page_%d.html", slotId);
    out.tempHtmlPath = fs::joinPath(fs::getTempSessionDir(), nameBuf);
    fs::writeFileText(out.tempHtmlPath, out.html);
    out.tempFileUrl = fs::pathToFileUrl(out.tempHtmlPath);
    return out;
}

}  // namespace litebrowser
