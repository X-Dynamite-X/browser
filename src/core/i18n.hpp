#pragma once

#include <string>
#include <unordered_map>

namespace litebrowser {
namespace i18n {

inline const char* get(const std::string& key, const std::string& lang) {
    bool en = (lang == "en");

    // Application & Navigation
    if (key == "app_name") return en ? "LiteBrowser" : "متصفح النور الخفيف";
    if (key == "app_title") return en ? "LiteBrowser - Fast & Lightweight" : "متصفح النور الخفيف - LiteBrowser";
    if (key == "nav_home") return en ? "Home" : "الرئيسية";
    if (key == "nav_video") return en ? "Video" : "الفيديو";
    if (key == "nav_games") return en ? "Games" : "الألعاب";
    if (key == "nav_open_doc") return en ? "Open PDF/Word" : "فتح PDF / Word";
    if (key == "nav_downloads") return en ? "Downloads" : "التحميلات";
    if (key == "nav_bookmarks") return en ? "Bookmarks" : "المفضلة";
    if (key == "nav_history") return en ? "History" : "السجل";
    if (key == "nav_settings") return en ? "Settings" : "الإعدادات";
    if (key == "new_tab") return en ? "+ Tab" : "+ تبويب";
    if (key == "close_tab") return "×";
    if (key == "btn_back") return "◀";
    if (key == "btn_fwd") return "▶";
    if (key == "btn_reload") return "↻";
    if (key == "btn_go") return en ? "Go" : "اذهب";
    if (key == "btn_lite_active") return en ? "⚡ Lite: ON" : "⚡ خفيف: مفعّل";
    if (key == "btn_lite_inactive") return en ? "⚡ Lite Mode" : "⚡ وضع خفيف";
    if (key == "theme_dark") return en ? "🌙 Dark" : "🌙 داكن";
    if (key == "theme_light") return en ? "☀️ Light" : "☀️ مضيء";
    if (key == "lang_toggle_label") return en ? "🌐 العربية" : "🌐 English";
    if (key == "search_placeholder") return en ? "Search the web or enter URL or PDF/Word path..."
                                               : "ابحث في الإنترنت أو أدخل رابط موقع أو مسار ملف PDF/Word...";
    if (key == "find_label") return en ? "Find in page:" : "البحث في الصفحة:";
    if (key == "find_prev") return en ? "▲ Prev" : "▲ السابق";
    if (key == "find_next") return en ? "▼ Next" : "▼ التالي";
    if (key == "status_ready") return en ? "Ready" : "جاهز";
    if (key == "status_loading") return en ? "Loading: " : "جاري التحميل: ";

    // Home Page
    if (key == "home_hero_title") return en ? "LiteBrowser" : "متصفح النور الخفيف";
    if (key == "home_hero_desc") return en
        ? "Ultra-lightweight, high-performance browser designed for all systems from Windows XP to 11 and Linux. Built-in video, HTML5 arcade games, fast downloads, and native PDF/Word document reading."
        : "متصفح عصري وسريع للغاية يعمل بكفاءة تامة على جميع الأنظمة من Windows XP حتى Windows 11 ولينكس، مع دعم تشغيل الألعاب، الفيديو، وتحميل البرامج، وقراءة مستندات PDF و Word دون أي بطء.";
    if (key == "home_search_btn") return en ? "Search" : "بحث سريع";
    if (key == "home_speed_dial") return en ? "Speed Dial" : "الوصول السريع";
    if (key == "home_status_lite_active") return en ? "Lite Mode Active (Maximum RAM & CPU saving)" : "الوضع الخفيف مفعّل (توفير فائق للذاكرة والمعالج)";
    if (key == "home_status_lite_inactive") return en ? "Standard Mode (Full GPU acceleration)" : "الوضع القياسي (تسريع رسومي كامل)";
    if (key == "home_status_secure") return en ? "Connection Secure & Isolated" : "الاتصال آمن ومعزول";
    if (key == "home_toggle_mode") return en ? "Switch Mode" : "تبديل الوضع";
    if (key == "home_card_docs_title") return en ? "Smart Document Reader" : "قارئ المستندات الذكي";
    if (key == "home_card_docs_desc") return en ? "Instant viewing of PDF and Word files (.docx, .doc, .rtf) with no external software." : "قراءة فورية لملفات PDF و Word (.docx, .doc, .rtf) دون الحاجة لتثبيت برامج ثقيلة.";
    if (key == "home_card_games_title") return en ? "HTML5 Arcade Games" : "مركز ألعاب HTML5";
    if (key == "home_card_games_desc") return en ? "Classic offline games (Snake, Bricks, 2048) running smoothly without internet." : "مجموعة ألعاب كلاسيكية خفيفة (الثعبان، تكسير القوالب، 2048) تعمل أوفلاين دون إنترنت.";
    if (key == "home_card_video_title") return en ? "Video & Media Player" : "مشغّل الفيديو والوسائط";
    if (key == "home_card_video_desc") return en ? "Direct stream player and lightweight YouTube search saving CPU and bandwidth." : "تشغيل روابط الفيديو المباشرة وبحث يوتيوب الخفيف دون إرهاق المعالج.";
    if (key == "home_card_downloads_title") return en ? "Advanced Download Manager" : "مدير تحميلات متطور";
    if (key == "home_card_downloads_desc") return en ? "High-speed multi-threaded downloads with resume support and notifications." : "تحميل متسارع للملفات مع دعم الاستئناف التلقائي والإشعارات الفورية.";
    if (key == "home_card_bookmarks_title") return en ? "Bookmarks & History" : "المفضلة والسجل";
    if (key == "home_card_bookmarks_desc") return en ? "Organize favorite sites and local private browsing history with zero tracking." : "حفظ المواقع المفضلة وسجل تصفح خاص ومشفر محلياً دون أي تتبع خارجي.";
    if (key == "home_card_settings_title") return en ? "Control & Privacy" : "التحكم والخصوصية";
    if (key == "home_card_settings_desc") return en ? "Customize search engine, ad blocker, memory saver, themes, and language." : "تخصيص محرك البحث، مانع الإعلانات، إدارة الذاكرة، والمظهر الداكن والفاتح واللغة.";
    if (key == "quick_google") return en ? "Google Search" : "بحث Google";
    if (key == "quick_ddg") return en ? "DuckDuckGo" : "DuckDuckGo الخصوصي";
    if (key == "quick_youtube") return en ? "YouTube" : "يوتيوب";
    if (key == "quick_wikipedia") return en ? "Wikipedia" : "ويكيبيديا الموسوعة";
    if (key == "quick_docs") return en ? "Open Local Document" : "فتح مستند محلي";
    if (key == "quick_games") return en ? "Play Games" : "العب الآن";

    // Video Page
    if (key == "video_hero_title") return en ? "Watch Videos & Direct Media Player" : "مشاهدة الفيديوهات ومشغّل الوسائط المباشر";
    if (key == "video_hero_desc") return en
        ? "Search YouTube or play direct video stream links (.mp4, .webm, .ogg) with minimal CPU and memory load."
        : "ابحث في يوتيوب أو شغّل روابط ملفات الفيديو المباشرة (.mp4, .webm, .ogg) بأقل استهلاك للمعالج والذاكرة.";
    if (key == "video_search_placeholder") return en ? "Type video name to search YouTube..." : "اكتب اسم الفيديو للبحث في يوتيوب...";
    if (key == "video_search_btn") return en ? "🎬 Search YouTube" : "🎬 بحث يوتيوب";
    if (key == "video_quick_yt") return en ? "YouTube Main" : "YouTube الرئيسي";
    if (key == "video_quick_yt_m") return en ? "YouTube Mobile (Fast)" : "YouTube Mobile الخفيف";
    if (key == "video_quick_archive") return en ? "Internet Archive Movies" : "أرشيف الأفلام والوثائقيات";
    if (key == "video_player_title") return en ? "Direct Media Player (Lightweight HTML5)" : "مشغّل الوسائط المباشر (HTML5 Lightweight Player)";
    if (key == "video_player_desc") return en
        ? "Paste a direct video file link to play it in the built-in player without loading the full webpage:"
        : "الصق رابط ملف فيديو مباشر لتشغيله في المشغل المدمج دون الحاجة لتحميل الصفحة كاملة:";
    if (key == "video_url_placeholder") return en ? "Paste direct video link here (e.g. https://example.com/video.mp4)..."
                                                  : "الصق رابط الفيديو المباشر هنا (مثل: https://example.com/video.mp4)...";
    if (key == "video_play_btn") return en ? "▶ Play Video" : "▶ تشغيل الفيديو";
    if (key == "video_download_btn") return en ? "⬇ Download Video" : "⬇ تحميل الفيديو";

    // Games Page
    if (key == "games_hero_title") return en ? "HTML5 Arcade Games Center" : "مركز الألعاب الخفيفة (HTML5 Arcade)";
    if (key == "games_hero_desc") return en
        ? "Interactive, fast games running 100% offline with zero dependencies and maximum FPS on any hardware."
        : "ألعاب كلاسيكية سريعة وممتعة تعمل بدون إنترنت وبأعلى أداء على أي جهاز.";
    if (key == "games_snake") return en ? "🐍 Snake" : "🐍 الثعبان";
    if (key == "games_bricks") return en ? "🧱 Brick Breaker" : "🧱 تكسير القوالب";
    if (key == "games_2048") return en ? "🔢 2048 Puzzle" : "🔢 لغز 2048";
    if (key == "games_score") return en ? "Score:" : "النقاط:";
    if (key == "games_high") return en ? "Best:" : "أعلى رقم:";
    if (key == "games_restart") return en ? "🔄 New Game" : "🔄 بدء لعبة جديدة";
    if (key == "games_controls_hint") return en
        ? "Controls: Use Arrow Keys or WASD on keyboard, or click the directional buttons on screen."
        : "التحكم: استخدم الأسهم في لوحة المفاتيح أو الأزرار التفاعلية على الشاشة للتوجيه.";

    // Downloads Page
    if (key == "dl_hero_title") return en ? "Downloads Manager" : "مدير التحميلات المتطور";
    if (key == "dl_hero_desc") return en
        ? "Track and accelerate file and program downloads with pause, resume, and instant notifications."
        : "متابعة وتنزيل الملفات والبرامج بسرعة عالية مع دعم الاستئناف التلقائي والإشعار الفوري.";
    if (key == "dl_new_link") return en ? "Direct Download Link" : "تحميل من رابط مباشر";
    if (key == "dl_new_placeholder") return en ? "Enter direct file URL..." : "أدخل رابط التحميل المباشر...";
    if (key == "dl_start_btn") return en ? "Start Download" : "بدء التنزيل";
    if (key == "dl_filter_placeholder") return en ? "Filter downloads by name..." : "تصفية التحميلات بالاسم...";
    if (key == "dl_open_folder") return en ? "📂 Open Downloads Folder" : "📂 فتح مجلد التحميلات";
    if (key == "dl_clear_completed") return en ? "Clear Completed" : "مسح المكتمل";
    if (key == "dl_col_file") return en ? "File Name" : "اسم الملف";
    if (key == "dl_col_size") return en ? "Size" : "الحجم";
    if (key == "dl_col_progress") return en ? "Progress" : "التقدم";
    if (key == "dl_col_status") return en ? "Status" : "الحالة";
    if (key == "dl_col_actions") return en ? "Actions" : "إجراءات";
    if (key == "dl_status_done") return en ? "Completed" : "مكتمل";
    if (key == "dl_status_active") return en ? "Downloading" : "جاري التحميل";
    if (key == "dl_status_paused") return en ? "Paused" : "متوقف";
    if (key == "dl_status_failed") return en ? "Failed" : "فشل";
    if (key == "dl_open") return en ? "Open" : "فتح";
    if (key == "dl_pause") return en ? "Pause" : "إيقاف";
    if (key == "dl_resume") return en ? "Resume" : "استئناف";
    if (key == "dl_cancel") return en ? "Cancel" : "إلغاء";
    if (key == "dl_empty") return en ? "No downloads yet." : "لا توجد تحميلات حالياً.";

    // Bookmarks Page
    if (key == "bm_hero_title") return en ? "Bookmarks & Favorites" : "المفضلة وقائمة القراءة";
    if (key == "bm_hero_desc") return en
        ? "Quick access to your saved pages with instant search and one-click URL copy."
        : "الوصول السريع لجميع صفحاتك ومواقعك المحفوظة مع إمكانية البحث والنسخ السريع.";
    if (key == "bm_filter_placeholder") return en ? "Search bookmarks..." : "ابحث في المفضلة...";
    if (key == "bm_copy") return en ? "Copy Link" : "نسخ الرابط";
    if (key == "bm_delete") return en ? "Delete" : "حذف";
    if (key == "bm_empty") return en ? "No bookmarks saved yet." : "لا توجد مواقع في المفضلة حالياً.";

    // History Page
    if (key == "hist_hero_title") return en ? "Private Browsing History" : "سجل التصفح الخاص";
    if (key == "hist_hero_desc") return en
        ? "Local history stored securely on your machine only, preserving total privacy."
        : "سجل محلي مشفر ومخزن على جهازك فقط لحماية خصوصيتك التامة دون أي تتبع خارجي.";
    if (key == "hist_filter_placeholder") return en ? "Search browsing history..." : "ابحث في السجل...";
    if (key == "hist_clear_all") return en ? "Clear All History" : "مسح السجل بالكامل";
    if (key == "hist_empty") return en ? "Browsing history is empty." : "سجل التصفح فارغ.";

    // Settings Page
    if (key == "set_hero_title") return en ? "LiteBrowser Settings" : "إعدادات متصفح النور الخفيف";
    if (key == "set_hero_desc") return en
        ? "Customize appearance, dark/light themes, language, search engine, memory saving, and privacy."
        : "تخصيص المظهر، الوضع المظلم والمضيء، اللغة، محركات البحث، وتوفير الذاكرة والخصوصية.";
    if (key == "set_group_appearance") return en ? "🎨 Appearance & Language" : "🎨 المظهر واللغة";
    if (key == "set_theme_label") return en ? "Browser Theme:" : "مظهر المتصفح:";
    if (key == "set_theme_light_opt") return en ? "☀️ Light Mode (Crisp & Clean)" : "☀️ الوضع المضيء (فاتح ناصع ومريح)";
    if (key == "set_theme_dark_opt") return en ? "🌙 Dark Mode (Midnight Slate)" : "🌙 الوضع المظلم (داكن كحلي فاخر)";
    if (key == "set_lang_label") return en ? "Interface Language:" : "لغة الواجهة:";
    if (key == "set_lang_ar_opt") return en ? "العربية (Arabic - RTL)" : "العربية (Arabic - RTL)";
    if (key == "set_lang_en_opt") return en ? "English (الإنجليزية - LTR)" : "English (LTR)";
    if (key == "set_group_search") return en ? "🔍 Search Engine & Home Page" : "🔍 محرك البحث والصفحة الرئيسية";
    if (key == "set_search_label") return en ? "Default Search Engine:" : "محرك البحث الافتراضي:";
    if (key == "set_home_label") return en ? "Home Page:" : "الصفحة الرئيسية:";
    if (key == "set_home_save_btn") return en ? "Save Home Page" : "حفظ الصفحة الرئيسية";
    if (key == "set_group_performance") return en ? "⚡ Performance & Resource Saving" : "⚡ الأداء وتوفير الموارد";
    if (key == "set_lite_mode_label") return en ? "Lite Mode (Lightweight Performance):" : "وضع التوفير الخفيف (Lite Mode):";
    if (key == "set_lite_mode_desc") return en
        ? "Disables heavy visual effects to maximize speed on low-end machines and Windows XP."
        : "تعطيل المؤثرات الرسومية الثقيلة لتسريع التصفح على الأجهزة الضعيفة و Windows XP.";
    if (key == "set_adblock_label") return en ? "Ad & Popup Blocker:" : "مانع الإعلانات والنوافذ المنبثقة:";
    if (key == "set_adblock_desc") return en
        ? "Blocks intrusive advertisements and tracking scripts to accelerate page loads."
        : "حجب الإعلانات المزعجة وأدوات التتبع لتسريع تحميل المواقع وحفظ استهلاك الإنترنت.";
    if (key == "set_restore_label") return en ? "Restore Tabs on Startup:" : "استعادة الجلسة عند بدء التشغيل:";
    if (key == "set_restore_desc") return en
        ? "Automatically reopen all previously opened tabs when launching the browser."
        : "إعادة فتح جميع التبويبات السابقة تلقائياً عند تشغيل المتصفح.";
    if (key == "set_group_privacy") return en ? "🛡️ Privacy & Temporary Files" : "🛡️ الخصوصية والذاكرة المؤقتة";
    if (key == "set_clear_cache") return en ? "Clear Browser Cache" : "تفريغ ذاكرة التخزين المؤقت (Cache)";
    if (key == "set_clear_hist") return en ? "Clear Browsing History" : "مسح سجل التصفح (History)";
    if (key == "set_clear_dl") return en ? "Clear Downloads List" : "مسح سجل التحميلات (Downloads)";
    if (key == "set_xp_tip_title") return en ? "Guidance for Windows XP Users" : "إرشاد لمستخدمي نظام Windows XP";
    if (key == "set_xp_tip_desc") return en
        ? "To display Arabic and complex scripts crisply on Windows XP, ensure 'Install files for complex script and right-to-left languages' is enabled in Control Panel > Regional Options."
        : "لعرض اللغة العربية والخطوط المعقدة بأفضل وضوح على Windows XP، تأكد من تفعيل خيار 'Install files for complex script and right-to-left languages' من لوحة تحكم الخيارات الإقليمية (Regional Options).";

    // Document Viewer
    if (key == "doc_reader_badge") return en ? "READER" : "القارئ";
    if (key == "doc_search_inp") return en ? "Search in document..." : "بحث في المستند...";
    if (key == "doc_search_btn") return en ? "Find" : "بحث";
    if (key == "doc_rotate_btn") return en ? "↻ Rotate" : "↻ تدوير";
    if (key == "doc_day") return en ? "☀️ Day" : "☀️ نهاري";
    if (key == "doc_sepia") return en ? "📜 Sepia" : "📜 ورقي";
    if (key == "doc_dark") return en ? "🌙 Dark" : "🌙 ليلي";
    if (key == "doc_print") return en ? "🖨️ Print" : "🖨️ طباعة";
    if (key == "doc_found") return en ? "Found" : "تم العثور";
    if (key == "doc_not_found") return en ? "No matches" : "لا نتائج";
    if (key == "doc_in_section") return en ? "Found in section " : "موجود في القسم ";
    if (key == "doc_show_extracted") return en ? "Show extracted text for copy and search" : "عرض النص المستخرج للنسخ والبحث";

    return key.c_str();
}

}  // namespace i18n
}  // namespace litebrowser

