#pragma once

#include "storage.hpp"

#include <atomic>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace litebrowser {

struct DownloadProgressInfo {
    int id = 0;
    uint64_t receivedBytes = 0;
    uint64_t totalBytes = 0;
    uint64_t speedBytesPerSec = 0;
    DownloadStatus status = DownloadStatus::InProgress;
    std::string errorNote;
};

using DownloadProgressCallback = std::function<void(const DownloadProgressInfo&)>;

class DownloadTask {
public:
    DownloadTask(int id, const std::string& url, const std::string& savePath, bool openInReader = false);
    ~DownloadTask();

    int getId() const { return id_; }
    std::string getUrl() const { return url_; }
    std::string getSavePath() const { return savePath_; }
    bool isOpenInReader() const { return openInReader_; }

    void pause();
    void resume();
    void cancel();

    bool isPaused() const { return paused_.load(); }
    bool isCancelled() const { return cancelled_.load(); }

    // Execute download synchronously in background thread
    void run(DownloadProgressCallback progressCb);

private:
    int id_ = 0;
    std::string url_;
    std::string savePath_;
    bool openInReader_ = false;

    std::atomic<bool> paused_{false};
    std::atomic<bool> cancelled_{false};
    std::atomic<bool> running_{false};
};

class DownloaderPool {
public:
    static DownloaderPool& instance();

    int startDownload(StorageManager& storage, const std::string& url,
                      const std::string& customSavePath = "",
                      bool openInReaderWhenDone = false,
                      DownloadProgressCallback cb = nullptr);

    bool pauseDownload(int id, StorageManager& storage);
    bool resumeDownload(int id, StorageManager& storage, DownloadProgressCallback cb = nullptr);
    bool cancelDownload(int id, StorageManager& storage);

    bool isTaskActive(int id);

private:
    DownloaderPool() = default;
    ~DownloaderPool();
    DownloaderPool(const DownloaderPool&) = delete;
    DownloaderPool& operator=(const DownloaderPool&) = delete;

    PlatformMutex mtx_;
    std::vector<std::shared_ptr<DownloadTask>> tasks_;
};

}  // namespace litebrowser

