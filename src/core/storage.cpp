#include "storage.hpp"

#include "fs_utils.hpp"

#include <cstdlib>
#include <ctime>
#include <sstream>

namespace litebrowser {

namespace {

std::string sanitizeField(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        if (c == '\t' || c == '\r' || c == '\n') out.push_back(' ');
        else out.push_back(c);
    }
    return out;
}

std::string nowTimestamp() {
    std::time_t t = std::time(nullptr);
    struct std::tm* lt = std::localtime(&t);
    if (!lt) return "";
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M", lt);
    return buf;
}

std::vector<std::string> splitTabs(const std::string& line) {
    std::vector<std::string> parts;
    size_t start = 0;
    while (true) {
        size_t pos = line.find('\t', start);
        if (pos == std::string::npos) {
            parts.push_back(line.substr(start));
            break;
        }
        parts.push_back(line.substr(start, pos - start));
        start = pos + 1;
    }
    return parts;
}

}  // namespace

StorageManager::StorageManager(const std::string& customDir) {
    baseDir_ = customDir.empty() ? fs::getConfigDir() : customDir;
    fs::ensureDir(baseDir_);
    ScopedLock lk(mtx_);
    loadUnlocked();
}

void StorageManager::loadUnlocked() {
    // Load settings
    std::string sText;
    if (fs::readFileText(fs::joinPath(baseDir_, "settings.conf"), sText)) {
        std::istringstream iss(sText);
        std::string line;
        while (std::getline(iss, line)) {
            line = fs::trim(line);
            size_t eq = line.find('=');
            if (eq == std::string::npos) continue;
            std::string k = fs::trim(line.substr(0, eq));
            std::string v = fs::trim(line.substr(eq + 1));
            if (k == "lite_mode") settings_.liteMode = (v == "1" || v == "true");
            else if (k == "adblock") settings_.adBlockEnabled = (v == "1" || v == "true");
            else if (k == "bookmarks_bar") settings_.showBookmarksBar = (v == "1" || v == "true");
            else if (k == "restore_session") settings_.restoreSessionOnStartup = (v == "1" || v == "true");
            else if (k == "search_engine" && !v.empty()) settings_.searchEngine = v;
            else if (k == "home_page" && !v.empty()) settings_.homePage = v;
            else if (k == "theme" && !v.empty()) settings_.theme = v;
            else if (k == "language" && !v.empty()) settings_.language = v;
            else if (k == "blocked_ads") blockedAdsCount_ = std::strtoull(v.c_str(), nullptr, 10);
        }
    }

    // Load bookmarks
    bookmarks_.clear();
    std::string bText;
    std::string bPath = fs::joinPath(baseDir_, "bookmarks.tsv");
    if (fs::fileExists(bPath) && fs::readFileText(bPath, bText)) {
        std::istringstream iss(bText);
        std::string line;
        while (std::getline(iss, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            size_t tab = line.find('\t');
            if (tab == std::string::npos) continue;
            BookmarkEntry b;
            b.title = line.substr(0, tab);
            b.url = line.substr(tab + 1);
            if (!b.url.empty()) bookmarks_.push_back(b);
        }
    } else {
        bookmarks_ = {
            {"الرئيسية", "lite://home"},
            {"محرك جوجل", "https://www.google.com"},
            {"بحث خفيف XP", "https://html.duckduckgo.com/html/"},
            {"يوتيوب", "https://www.youtube.com"},
            {"الألعاب", "lite://games"},
            {"التحميلات", "lite://downloads"},
            {"ويكيبيديا", "https://ar.wikipedia.org"}
        };
        saveBookmarksUnlocked();
    }

    // Load history
    history_.clear();
    std::string hText;
    if (fs::readFileText(fs::joinPath(baseDir_, "history.tsv"), hText)) {
        std::istringstream iss(hText);
        std::string line;
        while (std::getline(iss, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            auto p = splitTabs(line);
            if (p.size() < 3) continue;
            HistoryEntry h{p[1], p[2], p[0]};
            if (!h.url.empty()) history_.push_back(h);
        }
    }

    // Load persistent downloads
    downloads_.clear();
    nextDownloadId_ = 1;
    std::string dText;
    if (fs::readFileText(fs::joinPath(baseDir_, "downloads.tsv"), dText)) {
        std::istringstream iss(dText);
        std::string line;
        while (std::getline(iss, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            auto p = splitTabs(line);
            if (p.size() < 7) continue;
            DownloadRecord rec;
            rec.id = std::atoi(p[0].c_str());
            rec.fileName = p[1];
            rec.url = p[2];
            rec.savePath = p[3];
            rec.receivedBytes = std::strtoull(p[4].c_str(), nullptr, 10);
            rec.totalBytes = std::strtoull(p[5].c_str(), nullptr, 10);
            int stCode = std::atoi(p[6].c_str());
            if (stCode == static_cast<int>(DownloadStatus::InProgress)) {
                // Previous session interrupted -> mark Paused so user can resume!
                rec.status = DownloadStatus::Paused;
            } else if (stCode >= 0 && stCode <= 4) {
                rec.status = static_cast<DownloadStatus>(stCode);
            } else {
                rec.status = DownloadStatus::Failed;
            }
            if (p.size() >= 8) rec.statusNote = p[7];
            if (rec.id >= nextDownloadId_) nextDownloadId_ = rec.id + 1;
            downloads_.push_back(rec);
        }
    }
}

void StorageManager::saveSettingsUnlocked() {
    std::string out;
    out += std::string("lite_mode=") + (settings_.liteMode ? "1" : "0") + "\n";
    out += std::string("adblock=") + (settings_.adBlockEnabled ? "1" : "0") + "\n";
    out += std::string("bookmarks_bar=") + (settings_.showBookmarksBar ? "1" : "0") + "\n";
    out += std::string("restore_session=") + (settings_.restoreSessionOnStartup ? "1" : "0") + "\n";
    out += "search_engine=" + sanitizeField(settings_.searchEngine) + "\n";
    out += "home_page=" + sanitizeField(settings_.homePage) + "\n";
    out += "theme=" + sanitizeField(settings_.theme) + "\n";
    out += "language=" + sanitizeField(settings_.language) + "\n";
    out += "blocked_ads=" + std::to_string(blockedAdsCount_) + "\n";
    fs::writeFileText(fs::joinPath(baseDir_, "settings.conf"), out);
}

void StorageManager::saveBookmarksUnlocked() {
    std::string out;
    for (const auto& b : bookmarks_) {
        out += sanitizeField(b.title) + "\t" + sanitizeField(b.url) + "\n";
    }
    fs::writeFileText(fs::joinPath(baseDir_, "bookmarks.tsv"), out);
}

void StorageManager::saveHistoryUnlocked() {
    std::string out;
    size_t start = (history_.size() > 300) ? (history_.size() - 300) : 0;
    for (size_t i = start; i < history_.size(); ++i) {
        out += sanitizeField(history_[i].timestamp) + "\t" +
               sanitizeField(history_[i].title) + "\t" +
               sanitizeField(history_[i].url) + "\n";
    }
    fs::writeFileText(fs::joinPath(baseDir_, "history.tsv"), out);
}

void StorageManager::saveDownloadsUnlocked() {
    std::string out;
    size_t start = (downloads_.size() > 150) ? (downloads_.size() - 150) : 0;
    for (size_t i = start; i < downloads_.size(); ++i) {
        const auto& d = downloads_[i];
        out += std::to_string(d.id) + "\t" +
               sanitizeField(d.fileName) + "\t" +
               sanitizeField(d.url) + "\t" +
               sanitizeField(d.savePath) + "\t" +
               std::to_string(d.receivedBytes) + "\t" +
               std::to_string(d.totalBytes) + "\t" +
               std::to_string(static_cast<int>(d.status)) + "\t" +
               sanitizeField(d.statusNote) + "\n";
    }
    fs::writeFileText(fs::joinPath(baseDir_, "downloads.tsv"), out);
}

BrowserSettings StorageManager::getSettings() {
    ScopedLock lk(mtx_);
    return settings_;
}

void StorageManager::updateSettings(const BrowserSettings& s) {
    ScopedLock lk(mtx_);
    settings_ = s;
    saveSettingsUnlocked();
}

std::vector<BookmarkEntry> StorageManager::getBookmarks() {
    ScopedLock lk(mtx_);
    return bookmarks_;
}

bool StorageManager::isBookmarked(const std::string& url) {
    ScopedLock lk(mtx_);
    for (const auto& b : bookmarks_) {
        if (b.url == url) return true;
    }
    return false;
}

void StorageManager::toggleBookmark(const std::string& title, const std::string& url) {
    if (url.empty()) return;
    ScopedLock lk(mtx_);
    for (auto it = bookmarks_.begin(); it != bookmarks_.end(); ++it) {
        if (it->url == url) {
            bookmarks_.erase(it);
            saveBookmarksUnlocked();
            return;
        }
    }
    BookmarkEntry b;
    b.title = title.empty() ? url : title;
    b.url = url;
    bookmarks_.push_back(b);
    saveBookmarksUnlocked();
}

void StorageManager::removeBookmarkByIndex(size_t idx) {
    ScopedLock lk(mtx_);
    if (idx < bookmarks_.size()) {
        bookmarks_.erase(bookmarks_.begin() + static_cast<std::ptrdiff_t>(idx));
        saveBookmarksUnlocked();
    }
}

std::vector<HistoryEntry> StorageManager::getHistory() {
    ScopedLock lk(mtx_);
    return history_;
}

void StorageManager::addHistory(const std::string& title, const std::string& url) {
    if (url.empty() || fs::startsWithIgnoreCase(url, "lite://") || fs::startsWithIgnoreCase(url, "about:")) {
        return;
    }
    ScopedLock lk(mtx_);
    if (!history_.empty() && history_.back().url == url) {
        if (!title.empty()) history_.back().title = title;
        return;
    }
    HistoryEntry h;
    h.timestamp = nowTimestamp();
    h.title = title.empty() ? url : title;
    h.url = url;
    history_.push_back(h);
    saveHistoryUnlocked();
}

void StorageManager::clearHistory() {
    ScopedLock lk(mtx_);
    history_.clear();
    saveHistoryUnlocked();
}

int StorageManager::addDownload(const std::string& fileName, const std::string& url,
                                const std::string& savePath, bool openInReaderWhenDone) {
    ScopedLock lk(mtx_);
    DownloadRecord rec;
    rec.id = nextDownloadId_++;
    rec.fileName = fileName.empty() ? "downloaded_file" : fileName;
    rec.url = url;
    rec.savePath = savePath;
    rec.openInReaderWhenDone = openInReaderWhenDone;
    rec.status = DownloadStatus::InProgress;
    downloads_.push_back(rec);
    saveDownloadsUnlocked();
    return rec.id;
}

void StorageManager::updateDownloadProgress(int id, uint64_t receivedBytes, uint64_t totalBytes, uint64_t speedBytesPerSec) {
    ScopedLock lk(mtx_);
    for (auto& d : downloads_) {
        if (d.id == id) {
            d.receivedBytes = receivedBytes;
            if (totalBytes > 0) d.totalBytes = totalBytes;
            d.speedBytesPerSec = speedBytesPerSec;
            break;
        }
    }
}

void StorageManager::setDownloadStatus(int id, DownloadStatus status, const std::string& note) {
    ScopedLock lk(mtx_);
    for (auto& d : downloads_) {
        if (d.id == id) {
            d.status = status;
            d.speedBytesPerSec = 0;
            if (!note.empty()) d.statusNote = note;
            break;
        }
    }
    saveDownloadsUnlocked();
}

void StorageManager::finishDownload(int id, DownloadStatus status, const std::string& note) {
    ScopedLock lk(mtx_);
    for (auto& d : downloads_) {
        if (d.id == id) {
            d.status = status;
            d.speedBytesPerSec = 0;
            d.statusNote = note;
            if (status == DownloadStatus::Completed && fs::fileExists(d.savePath)) {
                uint64_t actual = fs::fileSize(d.savePath);
                if (actual > d.receivedBytes) d.receivedBytes = actual;
                if (d.totalBytes == 0) d.totalBytes = d.receivedBytes;
            }
            break;
        }
    }
    saveDownloadsUnlocked();
}

void StorageManager::clearCompletedDownloads() {
    ScopedLock lk(mtx_);
    std::vector<DownloadRecord> active;
    for (const auto& d : downloads_) {
        if (d.status == DownloadStatus::InProgress || d.status == DownloadStatus::Paused) {
            active.push_back(d);
        }
    }
    downloads_ = std::move(active);
    saveDownloadsUnlocked();
}

std::vector<DownloadRecord> StorageManager::getDownloads() {
    ScopedLock lk(mtx_);
    return downloads_;
}

DownloadRecord StorageManager::getDownloadById(int id) {
    ScopedLock lk(mtx_);
    for (const auto& d : downloads_) {
        if (d.id == id) return d;
    }
    return DownloadRecord{};
}

bool StorageManager::hasActiveDownloads() {
    ScopedLock lk(mtx_);
    for (const auto& d : downloads_) {
        if (d.status == DownloadStatus::InProgress) return true;
    }
    return false;
}

void StorageManager::saveSessionTabs(const std::vector<std::string>& urls) {
    ScopedLock lk(mtx_);
    std::string out;
    for (const auto& u : urls) {
        if (!u.empty()) out += sanitizeField(u) + "\n";
    }
    fs::writeFileText(fs::joinPath(baseDir_, "session.tsv"), out);
}

std::vector<std::string> StorageManager::loadSessionTabs() {
    ScopedLock lk(mtx_);
    std::vector<std::string> urls;
    std::string text;
    if (fs::readFileText(fs::joinPath(baseDir_, "session.tsv"), text)) {
        std::istringstream iss(text);
        std::string line;
        while (std::getline(iss, line)) {
            line = fs::trim(line);
            if (!line.empty()) urls.push_back(line);
        }
    }
    return urls;
}

void StorageManager::pushClosedTab(const std::string& url) {
    if (url.empty() || fs::toLowerAscii(url) == "lite://home") return;
    ScopedLock lk(mtx_);
    closedTabs_.push_back(url);
    if (closedTabs_.size() > 30) closedTabs_.erase(closedTabs_.begin());
}

std::string StorageManager::popClosedTab() {
    ScopedLock lk(mtx_);
    if (closedTabs_.empty()) return "";
    std::string u = closedTabs_.back();
    closedTabs_.pop_back();
    return u;
}

uint64_t StorageManager::incrementBlockedAds() {
    ScopedLock lk(mtx_);
    return ++blockedAdsCount_;
}

uint64_t StorageManager::getBlockedAdsCount() {
    ScopedLock lk(mtx_);
    return blockedAdsCount_;
}

}  // namespace litebrowser
