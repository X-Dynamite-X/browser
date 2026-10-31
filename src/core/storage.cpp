#include "storage.hpp"

#include "fs_utils.hpp"

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
            else if (k == "search_engine" && !v.empty()) settings_.searchEngine = v;
            else if (k == "home_page" && !v.empty()) settings_.homePage = v;
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
            {"يوتيوب فيديو", "https://www.youtube.com"},
            {"الألعاب المدمجة", "lite://games"},
            {"مشغل الفيديو", "lite://video"},
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
            size_t t1 = line.find('\t');
            if (t1 == std::string::npos) continue;
            size_t t2 = line.find('\t', t1 + 1);
            if (t2 == std::string::npos) continue;
            HistoryEntry h;
            h.timestamp = line.substr(0, t1);
            h.title = line.substr(t1 + 1, t2 - t1 - 1);
            h.url = line.substr(t2 + 1);
            if (!h.url.empty()) history_.push_back(h);
        }
    }
}

void StorageManager::saveSettingsUnlocked() {
    std::string out;
    out += std::string("lite_mode=") + (settings_.liteMode ? "1" : "0") + "\n";
    out += std::string("adblock=") + (settings_.adBlockEnabled ? "1" : "0") + "\n";
    out += "search_engine=" + sanitizeField(settings_.searchEngine) + "\n";
    out += "home_page=" + sanitizeField(settings_.homePage) + "\n";
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
    size_t start = (history_.size() > 250) ? (history_.size() - 250) : 0;
    for (size_t i = start; i < history_.size(); ++i) {
        out += sanitizeField(history_[i].timestamp) + "\t" +
               sanitizeField(history_[i].title) + "\t" +
               sanitizeField(history_[i].url) + "\n";
    }
    fs::writeFileText(fs::joinPath(baseDir_, "history.tsv"), out);
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

int StorageManager::addDownload(const std::string& fileName, const std::string& url, const std::string& savePath) {
    ScopedLock lk(mtx_);
    DownloadRecord rec;
    rec.id = nextDownloadId_++;
    rec.fileName = fileName.empty() ? "downloaded_file" : fileName;
    rec.url = url;
    rec.savePath = savePath;
    rec.status = DownloadStatus::InProgress;
    downloads_.push_back(rec);
    return rec.id;
}

void StorageManager::updateDownloadProgress(int id, uint64_t receivedBytes, uint64_t totalBytes) {
    ScopedLock lk(mtx_);
    for (auto& d : downloads_) {
        if (d.id == id) {
            d.receivedBytes = receivedBytes;
            if (totalBytes > 0) d.totalBytes = totalBytes;
            break;
        }
    }
}

void StorageManager::finishDownload(int id, DownloadStatus status, const std::string& note) {
    ScopedLock lk(mtx_);
    for (auto& d : downloads_) {
        if (d.id == id) {
            d.status = status;
            d.statusNote = note;
            if (status == DownloadStatus::Completed && d.receivedBytes == 0 && fs::fileExists(d.savePath)) {
                d.receivedBytes = fs::fileSize(d.savePath);
                if (d.totalBytes == 0) d.totalBytes = d.receivedBytes;
            }
            break;
        }
    }
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

}  // namespace litebrowser

