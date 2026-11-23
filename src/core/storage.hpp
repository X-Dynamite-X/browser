#pragma once

#include <cstdint>
#include <string>
#include <vector>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <mutex>
#endif

namespace litebrowser {

class PlatformMutex {
public:
    PlatformMutex() {
#ifdef _WIN32
        InitializeCriticalSection(&cs_);
#endif
    }
    ~PlatformMutex() {
#ifdef _WIN32
        DeleteCriticalSection(&cs_);
#endif
    }
    PlatformMutex(const PlatformMutex&) = delete;
    PlatformMutex& operator=(const PlatformMutex&) = delete;

    void lock() {
#ifdef _WIN32
        EnterCriticalSection(&cs_);
#else
        mtx_.lock();
#endif
    }
    void unlock() {
#ifdef _WIN32
        LeaveCriticalSection(&cs_);
#else
        mtx_.unlock();
#endif
    }

private:
#ifdef _WIN32
    CRITICAL_SECTION cs_;
#else
    std::mutex mtx_;
#endif
};

class ScopedLock {
public:
    explicit ScopedLock(PlatformMutex& m) : m_(m) { m_.lock(); }
    ~ScopedLock() { m_.unlock(); }
    ScopedLock(const ScopedLock&) = delete;
    ScopedLock& operator=(const ScopedLock&) = delete;
private:
    PlatformMutex& m_;
};

struct BookmarkEntry {
    std::string title;
    std::string url;
};

struct HistoryEntry {
    std::string title;
    std::string url;
    std::string timestamp;
};

enum class DownloadStatus {
    InProgress = 0,
    Paused = 1,
    Completed = 2,
    Failed = 3,
    Cancelled = 4
};

struct DownloadRecord {
    int id = 0;
    std::string fileName;
    std::string url;
    std::string savePath;
    uint64_t receivedBytes = 0;
    uint64_t totalBytes = 0;
    uint64_t speedBytesPerSec = 0;
    bool openInReaderWhenDone = false;
    DownloadStatus status = DownloadStatus::InProgress;
    std::string statusNote;
};

struct BrowserSettings {
    bool liteMode = false;                 // الوضع الخفيف للأجهزة الضعيفة و Windows XP
    bool adBlockEnabled = true;            // مانع الإعلانات والنوافذ المنبثقة
    bool showBookmarksBar = true;          // إظهار شريط المفضلة العلوي
    bool restoreSessionOnStartup = false;  // استعادة التبويبات المفتوحة عند التشغيل
    std::string searchEngine = "google";   // google | ddg | ddg-lite | bing | youtube
    std::string homePage = "lite://home";
    std::string theme = "dark";            // "dark" | "light"
    std::string language = "ar";           // "ar" | "en"

    bool isDarkMode() const { return theme != "light"; }
    bool isEnglish() const { return language == "en"; }
};

class StorageManager {
public:
    explicit StorageManager(const std::string& customDir = "");

    std::string getBaseDir() const { return baseDir_; }

    BrowserSettings getSettings();
    void updateSettings(const BrowserSettings& s);

    std::vector<BookmarkEntry> getBookmarks();
    bool isBookmarked(const std::string& url);
    void toggleBookmark(const std::string& title, const std::string& url);
    void removeBookmarkByIndex(size_t idx);

    std::vector<HistoryEntry> getHistory();
    void addHistory(const std::string& title, const std::string& url);
    void clearHistory();

    int addDownload(const std::string& fileName, const std::string& url,
                    const std::string& savePath, bool openInReaderWhenDone = false);
    void updateDownloadProgress(int id, uint64_t receivedBytes, uint64_t totalBytes, uint64_t speedBytesPerSec = 0);
    void setDownloadStatus(int id, DownloadStatus status, const std::string& note = "");
    void finishDownload(int id, DownloadStatus status, const std::string& note = "");
    void clearCompletedDownloads();
    std::vector<DownloadRecord> getDownloads();
    DownloadRecord getDownloadById(int id);
    bool hasActiveDownloads();

    // Session restore & recently closed tabs (Ctrl+Shift+T)
    void saveSessionTabs(const std::vector<std::string>& urls);
    std::vector<std::string> loadSessionTabs();
    void pushClosedTab(const std::string& url);
    std::string popClosedTab();

    // AdBlock statistics
    uint64_t incrementBlockedAds();
    uint64_t getBlockedAdsCount();

private:
    void loadUnlocked();
    void saveSettingsUnlocked();
    void saveBookmarksUnlocked();
    void saveHistoryUnlocked();
    void saveDownloadsUnlocked();

    PlatformMutex mtx_;
    std::string baseDir_;
    BrowserSettings settings_;
    std::vector<BookmarkEntry> bookmarks_;
    std::vector<HistoryEntry> history_;
    std::vector<DownloadRecord> downloads_;
    std::vector<std::string> closedTabs_;
    int nextDownloadId_ = 1;
    uint64_t blockedAdsCount_ = 0;
};

}  // namespace litebrowser
