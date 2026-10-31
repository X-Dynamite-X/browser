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
    InProgress,
    Completed,
    Failed,
    Cancelled
};

struct DownloadRecord {
    int id = 0;
    std::string fileName;
    std::string url;
    std::string savePath;
    uint64_t receivedBytes = 0;
    uint64_t totalBytes = 0;
    DownloadStatus status = DownloadStatus::InProgress;
    std::string statusNote;
};

struct BrowserSettings {
    bool liteMode = false;         // الوضع الخفيف للأجهزة الضعيفة والأنظمة القديمة
    bool adBlockEnabled = true;    // مانع الإعلانات والمتتبعات المدمج
    std::string searchEngine = "google";  // google | ddg | ddg-lite | youtube
    std::string homePage = "lite://home";
};

class StorageManager {
public:
    explicit StorageManager(const std::string& customDir = "");

    BrowserSettings getSettings();
    void updateSettings(const BrowserSettings& s);

    std::vector<BookmarkEntry> getBookmarks();
    bool isBookmarked(const std::string& url);
    void toggleBookmark(const std::string& title, const std::string& url);
    void removeBookmarkByIndex(size_t idx);

    std::vector<HistoryEntry> getHistory();
    void addHistory(const std::string& title, const std::string& url);
    void clearHistory();

    int addDownload(const std::string& fileName, const std::string& url, const std::string& savePath);
    void updateDownloadProgress(int id, uint64_t receivedBytes, uint64_t totalBytes);
    void finishDownload(int id, DownloadStatus status, const std::string& note = "");
    std::vector<DownloadRecord> getDownloads();
    DownloadRecord getDownloadById(int id);

private:
    void loadUnlocked();
    void saveSettingsUnlocked();
    void saveBookmarksUnlocked();
    void saveHistoryUnlocked();

    PlatformMutex mtx_;
    std::string baseDir_;
    BrowserSettings settings_;
    std::vector<BookmarkEntry> bookmarks_;
    std::vector<HistoryEntry> history_;
    std::vector<DownloadRecord> downloads_;
    int nextDownloadId_ = 1;
};

}  // namespace litebrowser

