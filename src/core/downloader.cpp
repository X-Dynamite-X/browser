#include "downloader.hpp"
#include "fs_utils.hpp"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <wininet.h>
#else
#include <dlfcn.h>
#include <unistd.h>
#endif

namespace litebrowser {

DownloadTask::DownloadTask(int id, const std::string& url, const std::string& savePath, bool openInReader)
    : id_(id), url_(url), savePath_(savePath), openInReader_(openInReader) {}

DownloadTask::~DownloadTask() {
    cancel();
}

void DownloadTask::pause() {
    paused_.store(true);
}

void DownloadTask::resume() {
    paused_.store(false);
}

void DownloadTask::cancel() {
    cancelled_.store(true);
}

void DownloadTask::run(DownloadProgressCallback progressCb) {
    running_.store(true);
    paused_.store(false);
    cancelled_.store(false);

    uint64_t existingBytes = 0;
    if (fs::fileExists(savePath_)) {
        existingBytes = fs::fileSize(savePath_);
    }

    uint64_t received = existingBytes;
    uint64_t total = existingBytes;
    uint64_t speed = 0;

    auto notify = [&](DownloadStatus st, const std::string& note = "") {
        if (progressCb) {
            DownloadProgressInfo info;
            info.id = id_;
            info.receivedBytes = received;
            info.totalBytes = total;
            info.speedBytesPerSec = speed;
            info.status = st;
            info.errorNote = note;
            progressCb(info);
        }
    };

    notify(DownloadStatus::InProgress);

#ifdef _WIN32
    HINTERNET hInet = InternetOpenW(L"LiteBrowser/1.0", INTERNET_OPEN_TYPE_PRECONFIG, nullptr, nullptr, 0);
    if (!hInet) {
        notify(DownloadStatus::Failed, "WinInet init failed");
        running_.store(false);
        return;
    }

    std::wstring wUrl = fs::utf8ToWide(url_);
    std::wstring headers;
    if (existingBytes > 0) {
        wchar_t hbuf[128];
        _snwprintf(hbuf, sizeof(hbuf)/sizeof(wchar_t), L"Range: bytes=%I64u-\r\n", existingBytes);
        headers = hbuf;
    }

    HINTERNET hUrl = InternetOpenUrlW(
        hInet, wUrl.c_str(), headers.empty() ? nullptr : headers.c_str(),
        headers.empty() ? 0 : static_cast<DWORD>(headers.size()),
        INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE | INTERNET_FLAG_IGNORE_CERT_CN_INVALID | INTERNET_FLAG_IGNORE_CERT_DATE_INVALID,
        0);

    if (!hUrl) {
        InternetCloseHandle(hInet);
        notify(DownloadStatus::Failed, "Failed to connect to URL");
        running_.store(false);
        return;
    }

    wchar_t codeBuf[32] = {0};
    DWORD codeBufSz = sizeof(codeBuf);
    DWORD statusCode = 200;
    if (HttpQueryInfoW(hUrl, HTTP_QUERY_STATUS_CODE, codeBuf, &codeBufSz, nullptr)) {
        statusCode = static_cast<DWORD>(_wtoi(codeBuf));
    }

    bool appendMode = (statusCode == 206 && existingBytes > 0);
    if (!appendMode) {
        existingBytes = 0;
        received = 0;
    }

    wchar_t lenBuf[64] = {0};
    DWORD lenBufSz = sizeof(lenBuf);
    if (HttpQueryInfoW(hUrl, HTTP_QUERY_CONTENT_LENGTH, lenBuf, &lenBufSz, nullptr)) {
        uint64_t streamLen = static_cast<uint64_t>(_wtoi64(lenBuf));
        total = existingBytes + streamLen;
    }

    std::wstring wPath = fs::utf8ToWide(savePath_);
    FILE* fp = _wfopen(wPath.c_str(), appendMode ? L"ab" : L"wb");
    if (!fp) {
        InternetCloseHandle(hUrl);
        InternetCloseHandle(hInet);
        notify(DownloadStatus::Failed, "Cannot write to save path");
        running_.store(false);
        return;
    }

    uint8_t buf[32768];
    DWORD bytesRead = 0;
    auto lastTick = std::chrono::steady_clock::now();
    uint64_t bytesSinceLastTick = 0;

    while (true) {
        if (cancelled_.load()) {
            std::fclose(fp);
            InternetCloseHandle(hUrl);
            InternetCloseHandle(hInet);
            notify(DownloadStatus::Cancelled, "Cancelled by user");
            running_.store(false);
            return;
        }

        if (paused_.load()) {
            std::fclose(fp);
            InternetCloseHandle(hUrl);
            InternetCloseHandle(hInet);
            notify(DownloadStatus::Paused, "Paused");
            running_.store(false);
            return;
        }

        if (!InternetReadFile(hUrl, buf, sizeof(buf), &bytesRead) || bytesRead == 0) {
            break;
        }

        if (std::fwrite(buf, 1, bytesRead, fp) != bytesRead) {
            std::fclose(fp);
            InternetCloseHandle(hUrl);
            InternetCloseHandle(hInet);
            notify(DownloadStatus::Failed, "Disk write error");
            running_.store(false);
            return;
        }

        received += bytesRead;
        bytesSinceLastTick += bytesRead;

        auto now = std::chrono::steady_clock::now();
        auto diffMs = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastTick).count();
        if (diffMs >= 500) {
            speed = (bytesSinceLastTick * 1000) / diffMs;
            bytesSinceLastTick = 0;
            lastTick = now;
            notify(DownloadStatus::InProgress);
        }
    }

    std::fclose(fp);
    InternetCloseHandle(hUrl);
    InternetCloseHandle(hInet);

    if (total == 0 || received >= total) {
        notify(DownloadStatus::Completed);
    } else {
        notify(DownloadStatus::Failed, "Connection closed before complete");
    }
#else
    // Dynamic libcurl or system downloader for Linux
    typedef void CURL;
    typedef int CURLcode;
    typedef void* (*curl_easy_init_fn)(void);
    typedef CURLcode (*curl_easy_setopt_fn)(CURL*, int, ...);
    typedef CURLcode (*curl_easy_perform_fn)(CURL*);
    typedef void (*curl_easy_cleanup_fn)(CURL*);

    void* hCurl = dlopen("libcurl.so.4", RTLD_LAZY | RTLD_LOCAL);
    if (!hCurl) hCurl = dlopen("libcurl.so.3", RTLD_LAZY | RTLD_LOCAL);

    if (hCurl) {
        auto c_init = (curl_easy_init_fn)dlsym(hCurl, "curl_easy_init");
        auto c_setopt = (curl_easy_setopt_fn)dlsym(hCurl, "curl_easy_setopt");
        auto c_perform = (curl_easy_perform_fn)dlsym(hCurl, "curl_easy_perform");
        auto c_cleanup = (curl_easy_cleanup_fn)dlsym(hCurl, "curl_easy_cleanup");

        if (c_init && c_setopt && c_perform && c_cleanup) {
            CURL* curl = c_init();
            FILE* fp = std::fopen(savePath_.c_str(), (existingBytes > 0) ? "ab" : "wb");
            if (curl && fp) {
                // CURLOPT_URL = 10002, CURLOPT_WRITEDATA = 10001, CURLOPT_RESUME_FROM_LARGE = 30116
                c_setopt(curl, 10002, url_.c_str());
                c_setopt(curl, 10001, fp);
                c_setopt(curl, 52, 1L); // CURLOPT_FOLLOWLOCATION
                c_setopt(curl, 64, 0L); // CURLOPT_SSL_VERIFYPEER = 0
                if (existingBytes > 0) {
                    c_setopt(curl, 30116, (long long)existingBytes);
                }

                struct XferData {
                    DownloadTask* task;
                    uint64_t* rcv;
                    uint64_t* tot;
                    uint64_t* spd;
                    std::chrono::steady_clock::time_point lastTick;
                    uint64_t bytesSince;
                    DownloadProgressCallback cb;
                };

                XferData xd{this, &received, &total, &speed, std::chrono::steady_clock::now(), 0, progressCb};

                auto xferCb = +[](void* clientp, double dltotal, double dlnow, double, double) -> int {
                    auto* d = static_cast<XferData*>(clientp);
                    if (d->task->isCancelled()) return 1;
                    if (d->task->isPaused()) return 1;
                    if (dltotal > 0) *d->tot = (uint64_t)dltotal;
                    *d->rcv = (uint64_t)dlnow;
                    auto now = std::chrono::steady_clock::now();
                    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - d->lastTick).count();
                    if (ms >= 500) {
                        d->lastTick = now;
                        if (d->cb) {
                            DownloadProgressInfo inf;
                            inf.id = d->task->getId();
                            inf.receivedBytes = *d->rcv;
                            inf.totalBytes = *d->tot;
                            inf.speedBytesPerSec = (uint64_t)dlnow * 1000 / (ms > 0 ? ms : 1);
                            inf.status = DownloadStatus::InProgress;
                            d->cb(inf);
                        }
                    }
                    return 0;
                };

                c_setopt(curl, 20019, (void*)xferCb); // CURLOPT_PROGRESSFUNCTION
                c_setopt(curl, 43, 0L);              // CURLOPT_NOPROGRESS = 0
                c_setopt(curl, 10005, &xd);          // CURLOPT_PROGRESSDATA

                CURLcode res = c_perform(curl);
                std::fclose(fp);
                c_cleanup(curl);
                dlclose(hCurl);

                if (cancelled_.load()) {
                    notify(DownloadStatus::Cancelled);
                } else if (paused_.load()) {
                    notify(DownloadStatus::Paused);
                } else if (res == 0) {
                    received = fs::fileSize(savePath_);
                    total = received;
                    notify(DownloadStatus::Completed);
                } else {
                    notify(DownloadStatus::Failed, "cURL download error");
                }
                running_.store(false);
                return;
            }
            if (fp) std::fclose(fp);
            if (curl) c_cleanup(curl);
        }
        dlclose(hCurl);
    }

    // Fallback: commandline curl
    std::string cmd = "curl -k -L -s -C - -o \"" + savePath_ + "\" \"" + url_ + "\"";
    int r = std::system(cmd.c_str());
    if (r == 0 && fs::fileExists(savePath_)) {
        received = fs::fileSize(savePath_);
        total = received;
        notify(DownloadStatus::Completed);
    } else {
        notify(DownloadStatus::Failed, "Failed to download");
    }
#endif
    running_.store(false);
}

DownloaderPool& DownloaderPool::instance() {
    static DownloaderPool pool;
    return pool;
}

DownloaderPool::~DownloaderPool() {
    ScopedLock lk(mtx_);
    for (auto& t : tasks_) {
        t->cancel();
    }
}

int DownloaderPool::startDownload(StorageManager& storage, const std::string& url,
                                  const std::string& customSavePath,
                                  bool openInReaderWhenDone,
                                  DownloadProgressCallback cb) {
    std::string safeName = fs::getFileName(url);
    if (safeName.empty() || safeName.find('.') == std::string::npos) {
        safeName = "downloaded_file.bin";
    }

    std::string savePath = customSavePath;
    if (savePath.empty()) {
        std::string dir = fs::getDownloadsDir();
        savePath = fs::joinPath(dir, safeName);
        if (fs::fileExists(savePath)) {
            std::string ext = fs::getExtensionLower(safeName);
            std::string stem = ext.empty() ? safeName : safeName.substr(0, safeName.size() - ext.size());
            for (int i = 1; i < 1000; ++i) {
                std::string cand = fs::joinPath(dir, stem + "(" + std::to_string(i) + ")" + ext);
                if (!fs::fileExists(cand)) {
                    savePath = cand;
                    break;
                }
            }
        }
    }

    int id = storage.addDownload(fs::getFileName(savePath), url, savePath, openInReaderWhenDone);
    auto task = std::make_shared<DownloadTask>(id, url, savePath, openInReaderWhenDone);

    {
        ScopedLock lk(mtx_);
        tasks_.push_back(task);
    }

    std::thread([task, &storage, cb]() {
        task->run([&storage, cb](const DownloadProgressInfo& info) {
            storage.updateDownloadProgress(info.id, info.receivedBytes, info.totalBytes, info.speedBytesPerSec);
            if (info.status != DownloadStatus::InProgress) {
                storage.finishDownload(info.id, info.status, info.errorNote);
            }
            if (cb) cb(info);
        });
    }).detach();

    return id;
}

bool DownloaderPool::pauseDownload(int id, StorageManager& storage) {
    ScopedLock lk(mtx_);
    for (auto& t : tasks_) {
        if (t->getId() == id) {
            t->pause();
            storage.setDownloadStatus(id, DownloadStatus::Paused, "تم الإيقاف مؤقتاً");
            return true;
        }
    }
    return false;
}

bool DownloaderPool::resumeDownload(int id, StorageManager& storage, DownloadProgressCallback cb) {
    DownloadRecord rec = storage.getDownloadById(id);
    if (rec.id == 0 || rec.url.empty()) return false;

    // If existing task found, resume it; else spawn new resumed task
    {
        ScopedLock lk(mtx_);
        for (auto& t : tasks_) {
            if (t->getId() == id) {
                t->resume();
                storage.setDownloadStatus(id, DownloadStatus::InProgress);
                std::thread([t, &storage, cb]() {
                    t->run([&storage, cb](const DownloadProgressInfo& info) {
                        storage.updateDownloadProgress(info.id, info.receivedBytes, info.totalBytes, info.speedBytesPerSec);
                        if (info.status != DownloadStatus::InProgress) {
                            storage.finishDownload(info.id, info.status, info.errorNote);
                        }
                        if (cb) cb(info);
                    });
                }).detach();
                return true;
            }
        }
    }

    // New task with the same savePath to resume
    auto task = std::make_shared<DownloadTask>(id, rec.url, rec.savePath, rec.openInReaderWhenDone);
    {
        ScopedLock lk(mtx_);
        tasks_.push_back(task);
    }

    storage.setDownloadStatus(id, DownloadStatus::InProgress);
    std::thread([task, &storage, cb]() {
        task->run([&storage, cb](const DownloadProgressInfo& info) {
            storage.updateDownloadProgress(info.id, info.receivedBytes, info.totalBytes, info.speedBytesPerSec);
            if (info.status != DownloadStatus::InProgress) {
                storage.finishDownload(info.id, info.status, info.errorNote);
            }
            if (cb) cb(info);
        });
    }).detach();

    return true;
}

bool DownloaderPool::cancelDownload(int id, StorageManager& storage) {
    ScopedLock lk(mtx_);
    for (auto& t : tasks_) {
        if (t->getId() == id) {
            t->cancel();
            storage.finishDownload(id, DownloadStatus::Cancelled, "تم الإلغاء");
            return true;
        }
    }
    storage.finishDownload(id, DownloadStatus::Cancelled, "تم الإلغاء");
    return true;
}

bool DownloaderPool::isTaskActive(int id) {
    ScopedLock lk(mtx_);
    for (auto& t : tasks_) {
        if (t->getId() == id) return !t->isCancelled() && !t->isPaused();
    }
    return false;
}

}  // namespace litebrowser

