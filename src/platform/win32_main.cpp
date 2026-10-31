#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0501
#endif
#ifndef WINVER
#define WINVER 0x0501
#endif
#ifndef _WIN32_IE
#define _WIN32_IE 0x0600
#endif

#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>
#include <exdisp.h>
#include <exdispid.h>
#include <mshtmhst.h>
#include <ole2.h>
#include <shellapi.h>
#include <shlobj.h>
#include <wininet.h>

#include "../core/document_viewer.hpp"
#include "../core/fs_utils.hpp"
#include "../core/internal_pages.hpp"
#include "../core/storage.hpp"
#include "../core/url_utils.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

// Windows XP (NT 5.1) compatibility shim for GCC 15 static libgcc gthr-win32 reference to GetThreadId.
// On Vista/7/8/10/11 calls kernel32!GetThreadId; on Windows XP queries ntdll!NtQueryInformationThread.
#if defined(__i386__)
extern "C" {
struct XpThreadBasicInfo {
    LONG ExitStatus;
    PVOID TebBaseAddress;
    HANDLE UniqueProcessId;
    HANDLE UniqueThreadId;
    ULONG_PTR AffinityMask;
    LONG BasePriority;
    LONG DiffProcessPriority;
};

static DWORD WINAPI XpSafeGetThreadId(HANDLE hThread) {
    using GetThreadIdFn = DWORD(WINAPI*)(HANDLE);
    static GetThreadIdFn pGetThreadId = reinterpret_cast<GetThreadIdFn>(
        GetProcAddress(GetModuleHandleA("kernel32.dll"), "GetThreadId"));
    if (pGetThreadId) {
        return pGetThreadId(hThread);
    }
    using NtQueryInfoThreadFn = LONG(WINAPI*)(HANDLE, int, PVOID, ULONG, PULONG);
    static NtQueryInfoThreadFn pNtQuery = reinterpret_cast<NtQueryInfoThreadFn>(
        GetProcAddress(GetModuleHandleA("ntdll.dll"), "NtQueryInformationThread"));
    if (pNtQuery) {
        XpThreadBasicInfo tbi{};
        if (pNtQuery(hThread, 0, &tbi, sizeof(tbi), nullptr) == 0) {
            return static_cast<DWORD>(reinterpret_cast< ULONG_PTR >(tbi.UniqueThreadId));
        }
    }
    return GetCurrentThreadId();
}

DWORD(WINAPI* pImpGetThreadId)(HANDLE) asm("__imp__GetThreadId@4") = &XpSafeGetThreadId;

static VOID WINAPI XpSafeInitializeConditionVariable(PVOID cv) {
    using Fn = VOID(WINAPI*)(PVOID);
    static Fn p = reinterpret_cast<Fn>(GetProcAddress(GetModuleHandleA("kernel32.dll"), "InitializeConditionVariable"));
    if (p) { p(cv); return; }
    if (cv) *reinterpret_cast<HANDLE*>(cv) = CreateEventW(nullptr, FALSE, FALSE, nullptr);
}

static BOOL WINAPI XpSafeSleepConditionVariableCS(PVOID cv, PCRITICAL_SECTION cs, DWORD ms) {
    using Fn = BOOL(WINAPI*)(PVOID, PCRITICAL_SECTION, DWORD);
    static Fn p = reinterpret_cast<Fn>(GetProcAddress(GetModuleHandleA("kernel32.dll"), "SleepConditionVariableCS"));
    if (p) return p(cv, cs, ms);
    HANDLE ev = cv ? *reinterpret_cast<HANDLE*>(cv) : nullptr;
    if (!ev) { ev = CreateEventW(nullptr, FALSE, FALSE, nullptr); if (cv) *reinterpret_cast<HANDLE*>(cv) = ev; }
    LeaveCriticalSection(cs);
    DWORD r = WaitForSingleObject(ev, ms);
    EnterCriticalSection(cs);
    return (r == WAIT_OBJECT_0);
}

static VOID WINAPI XpSafeWakeConditionVariable(PVOID cv) {
    using Fn = VOID(WINAPI*)(PVOID);
    static Fn p = reinterpret_cast<Fn>(GetProcAddress(GetModuleHandleA("kernel32.dll"), "WakeConditionVariable"));
    if (p) { p(cv); return; }
    if (cv && *reinterpret_cast<HANDLE*>(cv)) SetEvent(*reinterpret_cast<HANDLE*>(cv));
}

static VOID WINAPI XpSafeWakeAllConditionVariable(PVOID cv) {
    using Fn = VOID(WINAPI*)(PVOID);
    static Fn p = reinterpret_cast<Fn>(GetProcAddress(GetModuleHandleA("kernel32.dll"), "WakeAllConditionVariable"));
    if (p) { p(cv); return; }
    if (cv && *reinterpret_cast<HANDLE*>(cv)) SetEvent(*reinterpret_cast<HANDLE*>(cv));
}

VOID(WINAPI* pImpInitCV)(PVOID) asm("__imp__InitializeConditionVariable@4") = &XpSafeInitializeConditionVariable;
BOOL(WINAPI* pImpSleepCV)(PVOID, PCRITICAL_SECTION, DWORD) asm("__imp__SleepConditionVariableCS@12") = &XpSafeSleepConditionVariableCS;
VOID(WINAPI* pImpWakeCV)(PVOID) asm("__imp__WakeConditionVariable@4") = &XpSafeWakeConditionVariable;
VOID(WINAPI* pImpWakeAllCV)(PVOID) asm("__imp__WakeAllConditionVariable@4") = &XpSafeWakeAllConditionVariable;
}
#endif

namespace {

constexpr UINT WM_DOWNLOAD_STATUS = WM_APP + 10;

enum ControlIds {
    ID_BTN_BACK = 1001,
    ID_BTN_FWD,
    ID_BTN_RELOAD,
    ID_BTN_HOME,
    ID_EDIT_URL,
    ID_BTN_GO,
    ID_BTN_STAR,
    ID_BTN_OPENDOC,
    ID_BTN_GAMES,
    ID_BTN_DOWNLOADS,
    ID_BTN_LITE,
    ID_BTN_NEWTAB,
    ID_BTN_CLOSETAB,
    ID_TABCTRL
};

// Optional Miniblink (node.dll / mb.dll) dynamic C API for Chromium rendering on Windows XP/7/8/10/11
struct MiniblinkApi {
    HMODULE dll = nullptr;
    using wkeWebView = void*;

    void (*wkeInitialize)(void) = nullptr;
    wkeWebView (*wkeCreateWebWindow)(int type, HWND parent, int x, int y, int width, int height) = nullptr;
    void (*wkeDestroyWebWindow)(wkeWebView webWindow) = nullptr;
    HWND (*wkeGetWindowHandle)(wkeWebView webWindow) = nullptr;
    void (*wkeShowWindow)(wkeWebView webWindow, bool showFlag) = nullptr;
    void (*wkeMoveWindow)(wkeWebView webWindow, int x, int y, int width, int height) = nullptr;
    void (*wkeLoadURLW)(wkeWebView webView, const wchar_t* url) = nullptr;
    bool (*wkeCanGoBack)(wkeWebView webView) = nullptr;
    bool (*wkeGoBack)(wkeWebView webView) = nullptr;
    bool (*wkeCanGoForward)(wkeWebView webView) = nullptr;
    bool (*wkeGoForward)(wkeWebView webView) = nullptr;
    void (*wkeReload)(wkeWebView webView) = nullptr;

    bool tryLoad() {
        const wchar_t* names[] = {L"node.dll", L"miniblink.dll", L"mb.dll"};
        for (const wchar_t* n : names) {
            dll = LoadLibraryW(n);
            if (dll) break;
        }
        if (!dll) return false;
#define BIND_WKE(fn) fn = reinterpret_cast<decltype(fn)>(GetProcAddress(dll, #fn))
        BIND_WKE(wkeInitialize);
        BIND_WKE(wkeCreateWebWindow);
        BIND_WKE(wkeDestroyWebWindow);
        BIND_WKE(wkeGetWindowHandle);
        BIND_WKE(wkeShowWindow);
        BIND_WKE(wkeMoveWindow);
        BIND_WKE(wkeLoadURLW);
        BIND_WKE(wkeCanGoBack);
        BIND_WKE(wkeGoBack);
        BIND_WKE(wkeCanGoForward);
        BIND_WKE(wkeGoForward);
        BIND_WKE(wkeReload);
#undef BIND_WKE
        if (wkeInitialize && wkeCreateWebWindow && wkeLoadURLW && wkeMoveWindow && wkeShowWindow) {
            wkeInitialize();
            return true;
        }
        FreeLibrary(dll);
        dll = nullptr;
        return false;
    }
};

MiniblinkApi g_mb;

void enableModernBrowserEmulation() {
    wchar_t exePath[MAX_PATH] = {0};
    if (!GetModuleFileNameW(nullptr, exePath, MAX_PATH)) return;
    const wchar_t* exeName = wcsrchr(exePath, L'\\');
    exeName = exeName ? (exeName + 1) : exePath;

    HKEY hKey = nullptr;
    if (RegCreateKeyExW(
            HKEY_CURRENT_USER,
            L"Software\\Microsoft\\Internet Explorer\\Main\\FeatureControl\\FEATURE_BROWSER_EMULATION",
            0, nullptr, 0, KEY_SET_VALUE, nullptr, &hKey, nullptr) == ERROR_SUCCESS) {
        DWORD mode = 11001;  // Highest HTML5 Edge/IE11 standards mode available on the system
        RegSetValueExW(hKey, exeName, 0, REG_DWORD, reinterpret_cast<const BYTE*>(&mode), sizeof(mode));
        RegCloseKey(hKey);
    }
    if (RegCreateKeyExW(
            HKEY_CURRENT_USER,
            L"Software\\Microsoft\\Internet Explorer\\Main\\FeatureControl\\FEATURE_GPU_RENDERING",
            0, nullptr, 0, KEY_SET_VALUE, nullptr, &hKey, nullptr) == ERROR_SUCCESS) {
        DWORD gpu = 1;
        RegSetValueExW(hKey, exeName, 0, REG_DWORD, reinterpret_cast<const BYTE*>(&gpu), sizeof(gpu));
        RegCloseKey(hKey);
    }
}

struct WinBrowserApp;
struct WinTab;

// Lightweight OLE ActiveX Host for IWebBrowser2 + DWebBrowserEvents2 (zero external DLLs; works XP..Win11)
class OleBrowserSite final : public IOleClientSite,
                             public IOleInPlaceSite,
                             public IOleInPlaceFrame,
                             public IDispatch {
public:
    WinTab* ownerTab = nullptr;
    HWND hostHwnd = nullptr;
    IOleObject* oleObj = nullptr;
    IWebBrowser2* webBrowser = nullptr;
    IConnectionPoint* connPt = nullptr;
    DWORD adviseCookie = 0;

    explicit OleBrowserSite(WinTab* tab, HWND hwnd) : ownerTab(tab), hostHwnd(hwnd) {}

    ~OleBrowserSite() { shutdown(); }

    bool create() {
        HRESULT hr = CoCreateInstance(CLSID_WebBrowser, nullptr, CLSCTX_INPROC_SERVER | CLSCTX_INPROC_HANDLER,
                                      IID_IOleObject, reinterpret_cast<void**>(&oleObj));
        if (FAILED(hr) || !oleObj) return false;

        oleObj->SetClientSite(this);
        OleSetContainedObject(oleObj, TRUE);

        RECT rc{0, 0, 800, 500};
        GetClientRect(hostHwnd, &rc);
        hr = oleObj->DoVerb(OLEIVERB_INPLACEACTIVATE, nullptr, this, -1, hostHwnd, &rc);
        if (FAILED(hr)) return false;

        hr = oleObj->QueryInterface(IID_IWebBrowser2, reinterpret_cast<void**>(&webBrowser));
        if (FAILED(hr) || !webBrowser) return false;

        webBrowser->put_Silent(VARIANT_TRUE);

        IConnectionPointContainer* cpc = nullptr;
        if (SUCCEEDED(webBrowser->QueryInterface(IID_IConnectionPointContainer, reinterpret_cast<void**>(&cpc))) && cpc) {
            if (SUCCEEDED(cpc->FindConnectionPoint(DIID_DWebBrowserEvents2, &connPt)) && connPt) {
                connPt->Advise(static_cast<IDispatch*>(this), &adviseCookie);
            }
            cpc->Release();
        }
        resize(rc);
        return true;
    }

    void resize(const RECT& rc) {
        if (!webBrowser) return;
        IOleInPlaceObject* ipo = nullptr;
        if (SUCCEEDED(webBrowser->QueryInterface(IID_IOleInPlaceObject, reinterpret_cast<void**>(&ipo))) && ipo) {
            ipo->SetObjectRects(&rc, &rc);
            ipo->Release();
        }
    }

    void navigate(const std::wstring& url) {
        if (!webBrowser) return;
        VARIANT vUrl;
        VariantInit(&vUrl);
        vUrl.vt = VT_BSTR;
        vUrl.bstrVal = SysAllocString(url.c_str());
        VARIANT vEmpty;
        VariantInit(&vEmpty);
        webBrowser->Navigate2(&vUrl, &vEmpty, &vEmpty, &vEmpty, &vEmpty);
        VariantClear(&vUrl);
    }

    void shutdown() {
        if (connPt && adviseCookie) {
            connPt->Unadvise(adviseCookie);
            adviseCookie = 0;
        }
        if (connPt) { connPt->Release(); connPt = nullptr; }
        if (webBrowser) {
            webBrowser->Stop();
            webBrowser->Quit();
            webBrowser->Release();
            webBrowser = nullptr;
        }
        if (oleObj) {
            oleObj->Close(OLECLOSE_NOSAVE);
            oleObj->SetClientSite(nullptr);
            oleObj->Release();
            oleObj = nullptr;
        }
    }

    // IUnknown
    STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override {
        if (!ppv) return E_POINTER;
        if (riid == IID_IUnknown || riid == IID_IOleClientSite) {
            *ppv = static_cast<IOleClientSite*>(this);
        } else if (riid == IID_IOleWindow || riid == IID_IOleInPlaceSite) {
            *ppv = static_cast<IOleInPlaceSite*>(this);
        } else if (riid == IID_IOleInPlaceUIWindow || riid == IID_IOleInPlaceFrame) {
            *ppv = static_cast<IOleInPlaceFrame*>(this);
        } else if (riid == IID_IDispatch || riid == DIID_DWebBrowserEvents2) {
            *ppv = static_cast<IDispatch*>(this);
        } else {
            *ppv = nullptr;
            return E_NOINTERFACE;
        }
        return S_OK;
    }
    STDMETHODIMP_(ULONG) AddRef() override { return 2; }
    STDMETHODIMP_(ULONG) Release() override { return 1; }

    // IOleClientSite
    STDMETHODIMP SaveObject() override { return E_NOTIMPL; }
    STDMETHODIMP GetMoniker(DWORD, DWORD, IMoniker**) override { return E_NOTIMPL; }
    STDMETHODIMP GetContainer(IOleContainer** pp) override { if (pp) *pp = nullptr; return E_NOINTERFACE; }
    STDMETHODIMP ShowObject() override { return S_OK; }
    STDMETHODIMP OnShowWindow(BOOL) override { return S_OK; }
    STDMETHODIMP RequestNewObjectLayout() override { return E_NOTIMPL; }

    // IOleWindow
    STDMETHODIMP GetWindow(HWND* phwnd) override {
        if (!phwnd) return E_POINTER;
        *phwnd = hostHwnd;
        return S_OK;
    }
    STDMETHODIMP ContextSensitiveHelp(BOOL) override { return E_NOTIMPL; }

    // IOleInPlaceSite
    STDMETHODIMP CanInPlaceActivate() override { return S_OK; }
    STDMETHODIMP OnInPlaceActivate() override { return S_OK; }
    STDMETHODIMP OnUIActivate() override { return S_OK; }
    STDMETHODIMP GetWindowContext(IOleInPlaceFrame** ppFrame, IOleInPlaceUIWindow** ppDoc,
                                  LPRECT lprcPosRect, LPRECT lprcClipRect, LPOLEINPLACEFRAMEINFO lpFrameInfo) override {
        if (ppFrame) *ppFrame = static_cast<IOleInPlaceFrame*>(this);
        if (ppDoc) *ppDoc = nullptr;
        RECT rc{0, 0, 800, 500};
        GetClientRect(hostHwnd, &rc);
        if (lprcPosRect) *lprcPosRect = rc;
        if (lprcClipRect) *lprcClipRect = rc;
        if (lpFrameInfo) {
            lpFrameInfo->fMDIApp = FALSE;
            lpFrameInfo->hwndFrame = hostHwnd;
            lpFrameInfo->haccel = nullptr;
            lpFrameInfo->cAccelEntries = 0;
        }
        return S_OK;
    }
    STDMETHODIMP Scroll(SIZE) override { return E_NOTIMPL; }
    STDMETHODIMP OnUIDeactivate(BOOL) override { return S_OK; }
    STDMETHODIMP OnInPlaceDeactivate() override { return S_OK; }
    STDMETHODIMP DiscardUndoState() override { return E_NOTIMPL; }
    STDMETHODIMP DeactivateAndUndo() override { return E_NOTIMPL; }
    STDMETHODIMP OnPosRectChange(LPCRECT lprcPosRect) override {
        if (lprcPosRect) resize(*lprcPosRect);
        return S_OK;
    }

    // IOleInPlaceUIWindow / IOleInPlaceFrame
    STDMETHODIMP GetBorder(LPRECT) override { return INPLACE_E_NOTOOLSPACE; }
    STDMETHODIMP RequestBorderSpace(LPCBORDERWIDTHS) override { return INPLACE_E_NOTOOLSPACE; }
    STDMETHODIMP SetBorderSpace(LPCBORDERWIDTHS) override { return S_OK; }
    STDMETHODIMP SetActiveObject(IOleInPlaceActiveObject*, LPCOLESTR) override { return S_OK; }
    STDMETHODIMP InsertMenus(HMENU, LPOLEMENUGROUPWIDTHS) override { return E_NOTIMPL; }
    STDMETHODIMP SetMenu(HMENU, HOLEMENU, HWND) override { return S_OK; }
    STDMETHODIMP RemoveMenus(HMENU) override { return E_NOTIMPL; }
    STDMETHODIMP SetStatusText(LPCOLESTR) override { return S_OK; }
    STDMETHODIMP EnableModeless(BOOL) override { return S_OK; }
    STDMETHODIMP TranslateAccelerator(LPMSG, WORD) override { return E_NOTIMPL; }

    // IDispatch (DWebBrowserEvents2)
    STDMETHODIMP GetTypeInfoCount(UINT* pctinfo) override { if (pctinfo) *pctinfo = 0; return S_OK; }
    STDMETHODIMP GetTypeInfo(UINT, LCID, ITypeInfo**) override { return E_NOTIMPL; }
    STDMETHODIMP GetIDsOfNames(REFIID, LPOLESTR*, UINT, LCID, DISPID*) override { return E_NOTIMPL; }
    STDMETHODIMP Invoke(DISPID dispIdMember, REFIID, LCID, WORD, DISPPARAMS* pDispParams,
                        VARIANT*, EXCEPINFO*, UINT*) override;
};

struct WinTab {
    WinBrowserApp* app = nullptr;
    int tabId = 0;
    HWND hostPanel = nullptr;
    MiniblinkApi::wkeWebView mbView = nullptr;
    std::unique_ptr<OleBrowserSite> oleSite;
    std::string logicalUrl = "lite://home";
    std::string activeTempUrl;
    std::string title = "الرئيسية";
};

struct WinBrowserApp {
    HINSTANCE hInst = nullptr;
    HWND mainWnd = nullptr;
    HWND btnBack = nullptr, btnFwd = nullptr, btnReload = nullptr, btnHome = nullptr;
    HWND editUrl = nullptr, btnGo = nullptr, btnStar = nullptr;
    HWND btnOpenDoc = nullptr, btnGames = nullptr, btnDownloads = nullptr, btnLite = nullptr;
    HWND btnNewTab = nullptr, btnCloseTab = nullptr;
    HWND tabCtrl = nullptr;
    HWND statusWnd = nullptr;
    HFONT uiFont = nullptr;
    WNDPROC origEditProc = nullptr;

    litebrowser::StorageManager storage;
    std::vector<std::unique_ptr<WinTab>> tabs;
    int activeIndex = -1;
    int nextTabId = 1;

    WinTab* currentTab() {
        if (activeIndex >= 0 && activeIndex < static_cast<int>(tabs.size())) {
            return tabs[static_cast<size_t>(activeIndex)].get();
        }
        return nullptr;
    }
};

WinBrowserApp* g_app = nullptr;

void setWinStatus(const std::string& utf8Msg) {
    if (!g_app || !g_app->statusWnd) return;
    std::wstring w = litebrowser::fs::utf8ToWide(utf8Msg);
    SetWindowTextW(g_app->statusWnd, w.c_str());
}

std::string makeUniqueWinDownloadPath(const std::string& rawName) {
    std::string dir = litebrowser::fs::getDownloadsDir();
    std::string safeName = rawName.empty() ? "download.bin" : litebrowser::fs::getFileName(rawName);
    if (safeName.empty()) safeName = "download.bin";
    std::string p = litebrowser::fs::joinPath(dir, safeName);
    if (!litebrowser::fs::fileExists(p)) return p;
    std::string ext = litebrowser::fs::getExtensionLower(safeName);
    std::string stem = ext.empty() ? safeName : safeName.substr(0, safeName.size() - ext.size());
    for (int i = 1; i < 1000; ++i) {
        std::string cand = litebrowser::fs::joinPath(dir, stem + "(" + std::to_string(i) + ")" + ext);
        if (!litebrowser::fs::fileExists(cand)) return cand;
    }
    return p;
}

struct WinInetDownloadTask {
    std::string url;
    std::string savePath;
    int recordId = 0;
};

DWORD WINAPI winInetDownloadThread(LPVOID param) {
    std::unique_ptr<WinInetDownloadTask> task(static_cast<WinInetDownloadTask*>(param));
    HINTERNET hInet = InternetOpenW(L"LiteBrowser/1.0", INTERNET_OPEN_TYPE_PRECONFIG, nullptr, nullptr, 0);
    if (!hInet) {
        if (g_app) g_app->storage.finishDownload(task->recordId, litebrowser::DownloadStatus::Failed, "WinInet init error");
        return 1;
    }

    std::wstring wUrl = litebrowser::fs::utf8ToWide(task->url);
    HINTERNET hUrl = InternetOpenUrlW(
        hInet, wUrl.c_str(), nullptr, 0,
        INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE | INTERNET_FLAG_IGNORE_CERT_CN_INVALID, 0);
    if (!hUrl) {
        InternetCloseHandle(hInet);
        if (g_app) g_app->storage.finishDownload(task->recordId, litebrowser::DownloadStatus::Failed, "Connection failed");
        if (g_app && g_app->mainWnd) PostMessageW(g_app->mainWnd, WM_DOWNLOAD_STATUS, 0, 0);
        return 1;
    }

    uint64_t totalBytes = 0;
    wchar_t lenBuf[64] = {0};
    DWORD lenBufSz = sizeof(lenBuf);
    if (HttpQueryInfoW(hUrl, HTTP_QUERY_CONTENT_LENGTH, lenBuf, &lenBufSz, nullptr)) {
        totalBytes = static_cast<uint64_t>(_wtoi64(lenBuf));
    }

    std::wstring wSave = litebrowser::fs::utf8ToWide(task->savePath);
    FILE* fp = _wfopen(wSave.c_str(), L"wb");
    if (!fp) {
        InternetCloseHandle(hUrl);
        InternetCloseHandle(hInet);
        if (g_app) g_app->storage.finishDownload(task->recordId, litebrowser::DownloadStatus::Failed, "Cannot write file");
        return 1;
    }

    uint8_t buf[32768];
    DWORD bytesRead = 0;
    uint64_t received = 0;
    bool ok = true;
    while (InternetReadFile(hUrl, buf, sizeof(buf), &bytesRead) && bytesRead > 0) {
        if (std::fwrite(buf, 1, bytesRead, fp) != bytesRead) {
            ok = false;
            break;
        }
        received += bytesRead;
        if (g_app) g_app->storage.updateDownloadProgress(task->recordId, received, totalBytes);
    }

    std::fclose(fp);
    InternetCloseHandle(hUrl);
    InternetCloseHandle(hInet);

    if (g_app) {
        g_app->storage.finishDownload(
            task->recordId, ok ? litebrowser::DownloadStatus::Completed : litebrowser::DownloadStatus::Failed);
        if (g_app->mainWnd) PostMessageW(g_app->mainWnd, WM_DOWNLOAD_STATUS, ok ? 1 : 0, task->recordId);
    }
    return 0;
}

void startWinDownload(const std::string& rawUrl) {
    if (!g_app || rawUrl.empty()) return;
    std::string url = rawUrl;
    if (!litebrowser::fs::startsWithIgnoreCase(url, "http://") &&
        !litebrowser::fs::startsWithIgnoreCase(url, "https://")) {
        url = "https://" + url;
    }
    std::string fname = litebrowser::fs::getFileName(url);
    if (fname.empty() || fname.find('.') == std::string::npos) fname = "downloaded_program.bin";
    std::string savePath = makeUniqueWinDownloadPath(fname);
    int id = g_app->storage.addDownload(litebrowser::fs::getFileName(savePath), url, savePath);

    auto* task = new WinInetDownloadTask{url, savePath, id};
    DWORD tid = 0;
    HANDLE hThread = CreateThread(nullptr, 0, &winInetDownloadThread, task, 0, &tid);
    if (hThread) CloseHandle(hThread);
    setWinStatus("بدأ التحميل في الخلفية إلى: " + savePath);
}

void navigateWinTab(WinTab* tab, const std::string& rawInput);
WinTab* createWinTab(const std::string& initialUrl, bool activate = true);
void promptWinOpenDoc();
void syncWinToolbar();
void layoutWinChildren();

void updateTabHeaderText(int index, const std::string& utf8Title) {
    if (!g_app || !g_app->tabCtrl || index < 0) return;
    std::string t = utf8Title.empty() ? "تبويب" : utf8Title;
    if (t.size() > 30) t = t.substr(0, 27) + "...";
    std::wstring wt = litebrowser::fs::utf8ToWide(t);
    TCITEMW item{};
    item.mask = TCIF_TEXT;
    item.pszText = const_cast<LPWSTR>(wt.c_str());
    TabCtrl_SetItem(g_app->tabCtrl, index, &item);
}

void handleWinLiteAction(WinTab* tab, const std::string& actionUri) {
    if (!g_app || !tab) return;
    std::string rest = actionUri.substr(std::strlen("lite-action://"));
    if (litebrowser::fs::startsWithIgnoreCase(rest, "open-file")) {
        promptWinOpenDoc();
    } else if (litebrowser::fs::startsWithIgnoreCase(rest, "toggle-lite")) {
        auto s = g_app->storage.getSettings();
        s.liteMode = !s.liteMode;
        g_app->storage.updateSettings(s);
        SetWindowTextW(g_app->btnLite, s.liteMode ? L"خفيف: نشط" : L"وضع خفيف");
        navigateWinTab(tab, tab->logicalUrl);
    } else if (litebrowser::fs::startsWithIgnoreCase(rest, "toggle-adblock")) {
        auto s = g_app->storage.getSettings();
        s.adBlockEnabled = !s.adBlockEnabled;
        g_app->storage.updateSettings(s);
        navigateWinTab(tab, tab->logicalUrl);
    } else if (litebrowser::fs::startsWithIgnoreCase(rest, "download?url=")) {
        std::string dlUrl = litebrowser::fs::urlDecode(rest.substr(std::strlen("download?url=")));
        startWinDownload(dlUrl);
        navigateWinTab(tab, "lite://downloads");
    } else if (litebrowser::fs::startsWithIgnoreCase(rest, "open-downloads-dir")) {
        std::wstring wDir = litebrowser::fs::utf8ToWide(litebrowser::fs::getDownloadsDir());
        ShellExecuteW(g_app->mainWnd, L"open", wDir.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    } else if (litebrowser::fs::startsWithIgnoreCase(rest, "open-downloaded?id=")) {
        int id = std::atoi(rest.c_str() + std::strlen("open-downloaded?id="));
        auto rec = g_app->storage.getDownloadById(id);
        if (rec.id != 0 && !rec.savePath.empty()) {
            if (litebrowser::isSupportedDocumentFile(rec.savePath) && litebrowser::fs::fileExists(rec.savePath)) {
                createWinTab(rec.savePath, true);
            } else {
                std::wstring wp = litebrowser::fs::utf8ToWide(rec.savePath);
                ShellExecuteW(g_app->mainWnd, L"open", wp.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
            }
        }
    } else if (litebrowser::fs::startsWithIgnoreCase(rest, "remove-bookmark?idx=")) {
        size_t idx = static_cast<size_t>(std::strtoul(rest.c_str() + std::strlen("remove-bookmark?idx="), nullptr, 10));
        g_app->storage.removeBookmarkByIndex(idx);
        navigateWinTab(tab, "lite://bookmarks");
    } else if (litebrowser::fs::startsWithIgnoreCase(rest, "clear-history")) {
        g_app->storage.clearHistory();
        navigateWinTab(tab, "lite://history");
    }
}

STDMETHODIMP OleBrowserSite::Invoke(DISPID dispIdMember, REFIID, LCID, WORD, DISPPARAMS* pDispParams,
                                    VARIANT*, EXCEPINFO*, UINT*) {
    if (!ownerTab || !g_app || !pDispParams) return S_OK;

    if (dispIdMember == DISPID_BEFORENAVIGATE2 && pDispParams->cArgs >= 7) {
        // Args in reverse order: [0]=Cancel(VT_BOOL|VT_BYREF), [5]=URL(VT_VARIANT|VT_BYREF -> VT_BSTR)
        VARIANT* vUrl = &pDispParams->rgvarg[5];
        if (vUrl->vt == (VT_VARIANT | VT_BYREF) && vUrl->pvarVal) vUrl = vUrl->pvarVal;
        BSTR bstrUrl = (vUrl->vt == VT_BSTR) ? vUrl->bstrVal :
                       ((vUrl->vt == (VT_BSTR | VT_BYREF) && vUrl->pbstrVal) ? *vUrl->pbstrVal : nullptr);

        VARIANT_BOOL* pCancel = nullptr;
        VARIANT* vCancel = &pDispParams->rgvarg[0];
        if (vCancel->vt == (VT_BOOL | VT_BYREF)) pCancel = vCancel->pboolVal;

        if (bstrUrl) {
            std::string url = litebrowser::fs::wideToUtf8(bstrUrl);
            if (litebrowser::fs::startsWithIgnoreCase(url, "lite-action://")) {
                if (pCancel) *pCancel = VARIANT_TRUE;
                handleWinLiteAction(ownerTab, url);
                return S_OK;
            }
            if (litebrowser::fs::startsWithIgnoreCase(url, "lite://")) {
                if (pCancel) *pCancel = VARIANT_TRUE;
                navigateWinTab(ownerTab, url);
                return S_OK;
            }
            if (g_app->storage.getSettings().adBlockEnabled && litebrowser::shouldBlockAdOrTracker(url)) {
                if (pCancel) *pCancel = VARIANT_TRUE;
                return S_OK;
            }
            if (litebrowser::isDirectDownloadExtension(url) &&
                (litebrowser::fs::startsWithIgnoreCase(url, "http://") || litebrowser::fs::startsWithIgnoreCase(url, "https://"))) {
                if (pCancel) *pCancel = VARIANT_TRUE;
                startWinDownload(url);
                navigateWinTab(ownerTab, "lite://downloads");
                return S_OK;
            }
        }
    } else if (dispIdMember == DISPID_TITLECHANGE && pDispParams->cArgs >= 1) {
        VARIANT* vTitle = &pDispParams->rgvarg[0];
        if (vTitle->vt == VT_BSTR && vTitle->bstrVal) {
            ownerTab->title = litebrowser::fs::wideToUtf8(vTitle->bstrVal);
            for (size_t i = 0; i < g_app->tabs.size(); ++i) {
                if (g_app->tabs[i].get() == ownerTab) {
                    updateTabHeaderText(static_cast<int>(i), ownerTab->title);
                    break;
                }
            }
        }
    } else if (dispIdMember == DISPID_NAVIGATECOMPLETE2 && pDispParams->cArgs >= 1) {
        VARIANT* vUrl = &pDispParams->rgvarg[0];
        if (vUrl->vt == (VT_VARIANT | VT_BYREF) && vUrl->pvarVal) vUrl = vUrl->pvarVal;
        if (vUrl->vt == VT_BSTR && vUrl->bstrVal) {
            std::string u = litebrowser::fs::wideToUtf8(vUrl->bstrVal);
            if (u.find("litebrowser_session_") == std::string::npos) {
                ownerTab->logicalUrl = u;
                g_app->storage.addHistory(ownerTab->title, u);
            }
            if (ownerTab == g_app->currentTab()) {
                syncWinToolbar();
                setWinStatus("جاهز");
            }
        }
    }
    return S_OK;
}

void loadUrlIntoTabEngine(WinTab* tab, const std::string& urlUtf8) {
    std::wstring wUrl = litebrowser::fs::utf8ToWide(urlUtf8);
    if (tab->mbView && g_mb.wkeLoadURLW) {
        g_mb.wkeLoadURLW(tab->mbView, wUrl.c_str());
    } else if (tab->oleSite) {
        tab->oleSite->navigate(wUrl);
    }
}

void navigateWinTab(WinTab* tab, const std::string& rawInput) {
    if (!tab || !g_app) return;
    auto settings = g_app->storage.getSettings();
    auto resolved = litebrowser::resolveOmniboxInput(rawInput, settings.liteMode, settings.searchEngine);

    int tabIdx = -1;
    for (size_t i = 0; i < g_app->tabs.size(); ++i) {
        if (g_app->tabs[i].get() == tab) { tabIdx = static_cast<int>(i); break; }
    }

    if (resolved.type == litebrowser::OmniboxActionType::InternalPage) {
        auto page = litebrowser::renderInternalPage(resolved.targetUrlOrPath, g_app->storage, tab->tabId);
        tab->logicalUrl = resolved.targetUrlOrPath;
        tab->activeTempUrl = page.tempFileUrl;
        tab->title = page.title;
        updateTabHeaderText(tabIdx, page.title);
        loadUrlIntoTabEngine(tab, page.tempFileUrl);
    } else if (resolved.type == litebrowser::OmniboxActionType::OpenLocalDocument) {
        setWinStatus("جاري فتح المستند...");
        auto doc = litebrowser::renderDocumentToHtml(resolved.targetUrlOrPath, tab->tabId);
        tab->logicalUrl = resolved.targetUrlOrPath;
        tab->activeTempUrl = doc.tempFileUrl;
        tab->title = doc.tabTitle;
        updateTabHeaderText(tabIdx, doc.tabTitle);
        loadUrlIntoTabEngine(tab, doc.tempFileUrl);
    } else {
        tab->logicalUrl = resolved.targetUrlOrPath;
        tab->activeTempUrl.clear();
        loadUrlIntoTabEngine(tab, resolved.targetUrlOrPath);
    }

    if (tab == g_app->currentTab()) syncWinToolbar();
}

void switchActiveWinTab(int index) {
    if (!g_app || index < 0 || index >= static_cast<int>(g_app->tabs.size())) return;
    g_app->activeIndex = index;
    TabCtrl_SetCurSel(g_app->tabCtrl, index);
    for (size_t i = 0; i < g_app->tabs.size(); ++i) {
        ShowWindow(g_app->tabs[i]->hostPanel, (static_cast<int>(i) == index) ? SW_SHOW : SW_HIDE);
    }
    layoutWinChildren();
    syncWinToolbar();
}

WinTab* createWinTab(const std::string& initialUrl, bool activate) {
    auto tab = std::make_unique<WinTab>();
    tab->app = g_app;
    tab->tabId = g_app->nextTabId++;

    tab->hostPanel = CreateWindowExW(
        0, L"STATIC", L"", WS_CHILD | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
        0, 74, 900, 560, g_app->mainWnd, nullptr, g_app->hInst, nullptr);

    if (g_mb.dll && g_mb.wkeCreateWebWindow) {
        // WKE_WINDOW_TYPE_CONTROL == 2
        tab->mbView = g_mb.wkeCreateWebWindow(2, tab->hostPanel, 0, 0, 900, 560);
        if (tab->mbView) g_mb.wkeShowWindow(tab->mbView, true);
    } else {
        tab->oleSite = std::make_unique<OleBrowserSite>(tab.get(), tab->hostPanel);
        tab->oleSite->create();
    }

    TCITEMW tci{};
    tci.mask = TCIF_TEXT;
    wchar_t initLabel[] = L"تبويب جديد";
    tci.pszText = initLabel;
    int idx = static_cast<int>(g_app->tabs.size());
    TabCtrl_InsertItem(g_app->tabCtrl, idx, &tci);

    WinTab* rawPtr = tab.get();
    g_app->tabs.push_back(std::move(tab));

    if (activate) switchActiveWinTab(idx);
    navigateWinTab(rawPtr, initialUrl);
    return rawPtr;
}

void closeCurrentWinTab() {
    if (!g_app || g_app->tabs.empty()) return;
    if (g_app->tabs.size() == 1) {
        navigateWinTab(g_app->tabs[0].get(), "lite://home");
        return;
    }
    int idx = g_app->activeIndex;
    auto& t = g_app->tabs[static_cast<size_t>(idx)];
    if (t->mbView && g_mb.wkeDestroyWebWindow) g_mb.wkeDestroyWebWindow(t->mbView);
    if (t->oleSite) t->oleSite->shutdown();
    DestroyWindow(t->hostPanel);

    TabCtrl_DeleteItem(g_app->tabCtrl, idx);
    g_app->tabs.erase(g_app->tabs.begin() + idx);
    int nextIdx = (idx >= static_cast<int>(g_app->tabs.size())) ? (static_cast<int>(g_app->tabs.size()) - 1) : idx;
    switchActiveWinTab(nextIdx);
}

void syncWinToolbar() {
    if (!g_app) return;
    WinTab* t = g_app->currentTab();
    if (!t) return;
    std::wstring wUrl = litebrowser::fs::utf8ToWide(t->logicalUrl);
    SetWindowTextW(g_app->editUrl, wUrl.c_str());
    bool starred = g_app->storage.isBookmarked(t->logicalUrl);
    SetWindowTextW(g_app->btnStar, starred ? L"★" : L"☆");
}

void promptWinOpenDoc() {
    wchar_t fileBuf[MAX_PATH] = {0};
    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = g_app->mainWnd;
    ofn.lpstrFilter = L"مستندات PDF و Word (*.pdf;*.docx;*.doc;*.rtf;*.txt)\0*.pdf;*.docx;*.doc;*.rtf;*.txt;*.md\0جميع الملفات (*.*)\0*.*\0";
    ofn.lpstrFile = fileBuf;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
    if (GetOpenFileNameW(&ofn)) {
        createWinTab(litebrowser::fs::wideToUtf8(fileBuf), true);
    }
}

void layoutWinChildren() {
    if (!g_app || !g_app->mainWnd) return;
    RECT rc;
    GetClientRect(g_app->mainWnd, &rc);
    int w = rc.right - rc.left;
    int h = rc.bottom - rc.top;
    if (w < 100 || h < 100) return;

    const int topY = 6;
    const int btnH = 28;
    int x = 6;

    MoveWindow(g_app->btnBack, x, topY, 32, btnH, TRUE); x += 35;
    MoveWindow(g_app->btnFwd, x, topY, 32, btnH, TRUE); x += 35;
    MoveWindow(g_app->btnReload, x, topY, 32, btnH, TRUE); x += 35;
    MoveWindow(g_app->btnHome, x, topY, 56, btnH, TRUE); x += 60;

    int rightButtonsWidth = 40 + 34 + 86 + 62 + 74 + 80 + 62 + 34 + 30;
    int urlW = std::max(120, w - x - rightButtonsWidth);
    MoveWindow(g_app->editUrl, x, topY + 2, urlW, btnH - 4, TRUE); x += urlW + 4;
    MoveWindow(g_app->btnGo, x, topY, 38, btnH, TRUE); x += 41;
    MoveWindow(g_app->btnStar, x, topY, 32, btnH, TRUE); x += 35;
    MoveWindow(g_app->btnOpenDoc, x, topY, 84, btnH, TRUE); x += 87;
    MoveWindow(g_app->btnGames, x, topY, 60, btnH, TRUE); x += 63;
    MoveWindow(g_app->btnDownloads, x, topY, 72, btnH, TRUE); x += 75;
    MoveWindow(g_app->btnLite, x, topY, 78, btnH, TRUE); x += 81;
    MoveWindow(g_app->btnNewTab, x, topY, 58, btnH, TRUE); x += 61;
    MoveWindow(g_app->btnCloseTab, x, topY, 30, btnH, TRUE);

    const int tabY = 40;
    const int tabBarH = 28;
    MoveWindow(g_app->tabCtrl, 4, tabY, w - 8, tabBarH, TRUE);

    const int statusH = 22;
    MoveWindow(g_app->statusWnd, 0, h - statusH, w, statusH, TRUE);

    const int viewY = tabY + tabBarH + 2;
    const int viewH = std::max(40, h - viewY - statusH);
    if (WinTab* cur = g_app->currentTab()) {
        MoveWindow(cur->hostPanel, 0, viewY, w, viewH, TRUE);
        if (cur->mbView && g_mb.wkeMoveWindow) {
            g_mb.wkeMoveWindow(cur->mbView, 0, 0, w, viewH);
        } else if (cur->oleSite) {
            RECT vrc{0, 0, w, viewH};
            cur->oleSite->resize(vrc);
        }
    }
}

LRESULT CALLBACK urlEditSubclassProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_KEYDOWN && wp == VK_RETURN) {
        SendMessageW(g_app->mainWnd, WM_COMMAND, ID_BTN_GO, 0);
        return 0;
    }
    return CallWindowProcW(g_app->origEditProc, hwnd, msg, wp, lp);
}

LRESULT CALLBACK mainWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_SIZE:
            layoutWinChildren();
            return 0;

        case WM_NOTIFY: {
            auto* nm = reinterpret_cast<LPNMHDR>(lp);
            if (nm && nm->idFrom == ID_TABCTRL && nm->code == TCN_SELCHANGE) {
                int sel = TabCtrl_GetCurSel(g_app->tabCtrl);
                switchActiveWinTab(sel);
            }
            return 0;
        }

        case WM_DOWNLOAD_STATUS: {
            if (wp == 1) {
                auto rec = g_app->storage.getDownloadById(static_cast<int>(lp));
                setWinStatus("اكتمل التحميل بنجاح: " + rec.fileName);
            } else {
                setWinStatus("انتهت عملية التحميل بتعذّر الوصول إلى الرابط.");
            }
            if (WinTab* t = g_app->currentTab()) {
                if (t->logicalUrl == "lite://downloads") navigateWinTab(t, "lite://downloads");
            }
            return 0;
        }

        case WM_COMMAND: {
            int id = LOWORD(wp);
            WinTab* cur = g_app ? g_app->currentTab() : nullptr;
            switch (id) {
                case ID_BTN_BACK:
                    if (cur) {
                        if (cur->mbView && g_mb.wkeGoBack) g_mb.wkeGoBack(cur->mbView);
                        else if (cur->oleSite && cur->oleSite->webBrowser) cur->oleSite->webBrowser->GoBack();
                    }
                    break;
                case ID_BTN_FWD:
                    if (cur) {
                        if (cur->mbView && g_mb.wkeGoForward) g_mb.wkeGoForward(cur->mbView);
                        else if (cur->oleSite && cur->oleSite->webBrowser) cur->oleSite->webBrowser->GoForward();
                    }
                    break;
                case ID_BTN_RELOAD:
                    if (cur) {
                        if (litebrowser::fs::startsWithIgnoreCase(cur->logicalUrl, "lite://") ||
                            litebrowser::isSupportedDocumentFile(cur->logicalUrl)) {
                            navigateWinTab(cur, cur->logicalUrl);
                        } else if (cur->mbView && g_mb.wkeReload) {
                            g_mb.wkeReload(cur->mbView);
                        } else if (cur->oleSite && cur->oleSite->webBrowser) {
                            cur->oleSite->webBrowser->Refresh();
                        }
                    }
                    break;
                case ID_BTN_HOME:
                    if (cur) navigateWinTab(cur, "lite://home");
                    break;
                case ID_BTN_GO:
                    if (cur) {
                        int len = GetWindowTextLengthW(g_app->editUrl);
                        std::wstring buf(static_cast<size_t>(len + 1), L'\0');
                        GetWindowTextW(g_app->editUrl, &buf[0], len + 1);
                        buf.resize(static_cast<size_t>(len));
                        navigateWinTab(cur, litebrowser::fs::wideToUtf8(buf));
                    }
                    break;
                case ID_BTN_STAR:
                    if (cur) {
                        g_app->storage.toggleBookmark(cur->title, cur->logicalUrl);
                        syncWinToolbar();
                    }
                    break;
                case ID_BTN_OPENDOC:
                    promptWinOpenDoc();
                    break;
                case ID_BTN_GAMES:
                    if (cur) navigateWinTab(cur, "lite://games");
                    break;
                case ID_BTN_DOWNLOADS:
                    if (cur) navigateWinTab(cur, "lite://downloads");
                    break;
                case ID_BTN_LITE:
                    if (cur) handleWinLiteAction(cur, "lite-action://toggle-lite");
                    break;
                case ID_BTN_NEWTAB:
                    createWinTab("lite://home", true);
                    break;
                case ID_BTN_CLOSETAB:
                    closeCurrentWinTab();
                    break;
            }
            return 0;
        }

        case WM_DESTROY:
            if (g_app) {
                for (auto& t : g_app->tabs) {
                    if (t->mbView && g_mb.wkeDestroyWebWindow) g_mb.wkeDestroyWebWindow(t->mbView);
                    if (t->oleSite) t->oleSite->shutdown();
                }
                g_app->tabs.clear();
            }
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

}  // namespace

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow) {
    OleInitialize(nullptr);

    INITCOMMONCONTROLSEX icc{};
    icc.dwSize = sizeof(icc);
    icc.dwICC = ICC_TAB_CLASSES | ICC_BAR_CLASSES | ICC_STANDARD_CLASSES;
    InitCommonControlsEx(&icc);

    enableModernBrowserEmulation();
    g_mb.tryLoad();

    WinBrowserApp app;
    app.hInst = hInstance;
    g_app = &app;

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = mainWndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1);
    wc.lpszClassName = L"LiteBrowserWinClass";
    RegisterClassExW(&wc);

    app.mainWnd = CreateWindowExW(
        0, L"LiteBrowserWinClass", L"متصفح النور الخفيف - LiteBrowser",
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
        CW_USEDEFAULT, CW_USEDEFAULT, 1140, 740,
        nullptr, nullptr, hInstance, nullptr);

    app.uiFont = CreateFontW(
        -14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_SWISS, L"Tahoma");

    auto makeBtn = [&](const wchar_t* text, int id) -> HWND {
        HWND h = CreateWindowExW(0, L"BUTTON", text, WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                 0, 0, 40, 28, app.mainWnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), hInstance, nullptr);
        SendMessageW(h, WM_SETFONT, reinterpret_cast<WPARAM>(app.uiFont), TRUE);
        return h;
    };

    app.btnBack = makeBtn(L"<", ID_BTN_BACK);
    app.btnFwd = makeBtn(L">", ID_BTN_FWD);
    app.btnReload = makeBtn(L"↻", ID_BTN_RELOAD);
    app.btnHome = makeBtn(L"الرئيسية", ID_BTN_HOME);

    app.editUrl = CreateWindowExW(
        WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
        0, 0, 300, 24, app.mainWnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_EDIT_URL)), hInstance, nullptr);
    SendMessageW(app.editUrl, WM_SETFONT, reinterpret_cast<WPARAM>(app.uiFont), TRUE);
    app.origEditProc = reinterpret_cast<WNDPROC>(
        SetWindowLongPtrW(app.editUrl, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(urlEditSubclassProc)));

    app.btnGo = makeBtn(L"اذهب", ID_BTN_GO);
    app.btnStar = makeBtn(L"☆", ID_BTN_STAR);
    app.btnOpenDoc = makeBtn(L"PDF/Word", ID_BTN_OPENDOC);
    app.btnGames = makeBtn(L"ألعاب", ID_BTN_GAMES);
    app.btnDownloads = makeBtn(L"التحميلات", ID_BTN_DOWNLOADS);
    auto initS = app.storage.getSettings();
    app.btnLite = makeBtn(initS.liteMode ? L"خفيف: نشط" : L"وضع خفيف", ID_BTN_LITE);
    app.btnNewTab = makeBtn(L"+ تبويب", ID_BTN_NEWTAB);
    app.btnCloseTab = makeBtn(L"×", ID_BTN_CLOSETAB);

    app.tabCtrl = CreateWindowExW(
        0, WC_TABCONTROLW, L"", WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS,
        0, 40, 800, 28, app.mainWnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_TABCTRL)), hInstance, nullptr);
    SendMessageW(app.tabCtrl, WM_SETFONT, reinterpret_cast<WPARAM>(app.uiFont), TRUE);

    app.statusWnd = CreateWindowExW(
        0, STATUSCLASSNAMEW, L"جاهز", WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP,
        0, 0, 0, 0, app.mainWnd, nullptr, hInstance, nullptr);
    SendMessageW(app.statusWnd, WM_SETFONT, reinterpret_cast<WPARAM>(app.uiFont), TRUE);

    // Check command-line file/URL argument
    int numArgs = 0;
    LPWSTR* wArgs = CommandLineToArgvW(GetCommandLineW(), &numArgs);
    std::string initialTarget = app.storage.getSettings().homePage;
    if (wArgs && numArgs >= 2) {
        initialTarget = litebrowser::fs::wideToUtf8(wArgs[1]);
    }
    if (wArgs) LocalFree(wArgs);

    createWinTab(initialTarget, true);
    layoutWinChildren();

    ShowWindow(app.mainWnd, nCmdShow);
    UpdateWindow(app.mainWnd);

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        if (WinTab* cur = app.currentTab()) {
            if (cur->oleSite && cur->oleSite->webBrowser) {
                IOleInPlaceActiveObject* ipao = nullptr;
                if (SUCCEEDED(cur->oleSite->webBrowser->QueryInterface(
                        IID_IOleInPlaceActiveObject, reinterpret_cast<void**>(&ipao))) && ipao) {
                    if (ipao->TranslateAccelerator(&msg) == S_OK) {
                        ipao->Release();
                        continue;
                    }
                    ipao->Release();
                }
            }
        }
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    if (app.uiFont) DeleteObject(app.uiFont);
    litebrowser::fs::cleanupTempSessionDir();
    OleUninitialize();
    return static_cast<int>(msg.wParam);
}
