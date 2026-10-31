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
#include <mshtml.h>
#include <mshtmhst.h>
#include <mshtmcid.h>
#include <ole2.h>
#include <shellapi.h>
#include <shlobj.h>
#include <wininet.h>

#include "../core/document_viewer.hpp"
#include "../core/downloader.hpp"
#include "../core/fs_utils.hpp"
#include "../core/internal_pages.hpp"
#include "../core/storage.hpp"
#include "../core/url_utils.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <thread>
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
            return static_cast<DWORD>(reinterpret_cast<ULONG_PTR>(tbi.UniqueThreadId));
        }
    }
    return GetCurrentThreadId();
}

DWORD(WINAPI* pImpGetThreadId)(HANDLE) asm("__imp__GetThreadId@4") = &XpSafeGetThreadId;

struct XpCvWaiterNode {
    HANDLE event;
    XpCvWaiterNode* next;
    XpCvWaiterNode* prev;
};

struct XpCvImpl {
    CRITICAL_SECTION cs;
    XpCvWaiterNode* head;
    XpCvWaiterNode* tail;
};

static XpCvImpl* GetOrCreateXpCv(PVOID* cv) {
    if (!*cv) {
        auto* newCv = new XpCvImpl();
        InitializeCriticalSection(&newCv->cs);
        newCv->head = nullptr;
        newCv->tail = nullptr;
        if (InterlockedCompareExchangePointer(cv, newCv, nullptr) != nullptr) {
            DeleteCriticalSection(&newCv->cs);
            delete newCv;
        }
    }
    return reinterpret_cast<XpCvImpl*>(*cv);
}

static VOID WINAPI XpSafeInitializeConditionVariable(PVOID cv) {
    using Fn = VOID(WINAPI*)(PVOID);
    static Fn p = reinterpret_cast<Fn>(GetProcAddress(GetModuleHandleA("kernel32.dll"), "InitializeConditionVariable"));
    if (p) { p(cv); return; }
    if (cv) *reinterpret_cast<PVOID*>(cv) = nullptr;
}

static BOOL WINAPI XpSafeSleepConditionVariableCS(PVOID cv, PCRITICAL_SECTION cs, DWORD ms) {
    using Fn = BOOL(WINAPI*)(PVOID, PCRITICAL_SECTION, DWORD);
    static Fn p = reinterpret_cast<Fn>(GetProcAddress(GetModuleHandleA("kernel32.dll"), "SleepConditionVariableCS"));
    if (p) return p(cv, cs, ms);
    if (!cv) return FALSE;

    XpCvImpl* impl = GetOrCreateXpCv(reinterpret_cast<PVOID*>(cv));
    if (!impl) return FALSE;

    HANDLE hEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    if (!hEvent) return FALSE;

    XpCvWaiterNode node{ hEvent, nullptr, nullptr };

    EnterCriticalSection(&impl->cs);
    node.prev = impl->tail;
    node.next = nullptr;
    if (impl->tail) impl->tail->next = &node;
    else impl->head = &node;
    impl->tail = &node;
    LeaveCriticalSection(&impl->cs);

    LeaveCriticalSection(cs);
    DWORD r = WaitForSingleObject(hEvent, ms);

    EnterCriticalSection(&impl->cs);
    if (node.next || node.prev || impl->head == &node) {
        if (node.prev) node.prev->next = node.next;
        else impl->head = node.next;
        if (node.next) node.next->prev = node.prev;
        else impl->tail = node.prev;
    }
    LeaveCriticalSection(&impl->cs);

    CloseHandle(hEvent);
    EnterCriticalSection(cs);
    return (r == WAIT_OBJECT_0);
}

static VOID WINAPI XpSafeWakeConditionVariable(PVOID cv) {
    using Fn = VOID(WINAPI*)(PVOID);
    static Fn p = reinterpret_cast<Fn>(GetProcAddress(GetModuleHandleA("kernel32.dll"), "WakeConditionVariable"));
    if (p) { p(cv); return; }
    if (!cv || !*reinterpret_cast<PVOID*>(cv)) return;

    XpCvImpl* impl = reinterpret_cast<XpCvImpl*>(*reinterpret_cast<PVOID*>(cv));
    HANDLE toSignal = nullptr;

    EnterCriticalSection(&impl->cs);
    if (impl->head) {
        XpCvWaiterNode* first = impl->head;
        toSignal = first->event;
        impl->head = first->next;
        if (impl->head) impl->head->prev = nullptr;
        else impl->tail = nullptr;
        first->next = nullptr;
        first->prev = nullptr;
    }
    LeaveCriticalSection(&impl->cs);

    if (toSignal) SetEvent(toSignal);
}

static VOID WINAPI XpSafeWakeAllConditionVariable(PVOID cv) {
    using Fn = VOID(WINAPI*)(PVOID);
    static Fn p = reinterpret_cast<Fn>(GetProcAddress(GetModuleHandleA("kernel32.dll"), "WakeAllConditionVariable"));
    if (p) { p(cv); return; }
    if (!cv || !*reinterpret_cast<PVOID*>(cv)) return;

    XpCvImpl* impl = reinterpret_cast<XpCvImpl*>(*reinterpret_cast<PVOID*>(cv));
    HANDLE eventsToSignal[64];
    int count = 0;

    EnterCriticalSection(&impl->cs);
    XpCvWaiterNode* curr = impl->head;
    while (curr && count < 64) {
        eventsToSignal[count++] = curr->event;
        XpCvWaiterNode* next = curr->next;
        curr->next = nullptr;
        curr->prev = nullptr;
        curr = next;
    }
    impl->head = curr;
    if (impl->head) impl->head->prev = nullptr;
    else impl->tail = nullptr;
    LeaveCriticalSection(&impl->cs);

    for (int i = 0; i < count; ++i) {
        SetEvent(eventsToSignal[i]);
    }
}

VOID(WINAPI* pImpInitCV)(PVOID) asm("__imp__InitializeConditionVariable@4") = &XpSafeInitializeConditionVariable;
BOOL(WINAPI* pImpSleepCV)(PVOID, PCRITICAL_SECTION, DWORD) asm("__imp__SleepConditionVariableCS@12") = &XpSafeSleepConditionVariableCS;
VOID(WINAPI* pImpWakeCV)(PVOID) asm("__imp__WakeConditionVariable@4") = &XpSafeWakeConditionVariable;
VOID(WINAPI* pImpWakeAllCV)(PVOID) asm("__imp__WakeAllConditionVariable@4") = &XpSafeWakeAllConditionVariable;
}
#endif

namespace {

constexpr UINT WM_DOWNLOAD_STATUS = WM_APP + 10;
constexpr UINT WM_DOC_LOADED = WM_APP + 42;

struct AsyncDocResult {
    int tabId = 0;
    std::string path;
    litebrowser::DocumentViewOutput doc;
};

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
    ID_BTN_SETTINGS,
    ID_BTN_LITE,
    ID_BTN_NEWTAB,
    ID_BTN_CLOSETAB,
    ID_TABCTRL,

    // Find Bar Controls
    ID_FIND_EDIT,
    ID_FIND_PREV,
    ID_FIND_NEXT,
    ID_FIND_CLOSE,

    // Accelerators
    ID_ACCEL_NEWTAB = 2001,
    ID_ACCEL_CLOSETAB,
    ID_ACCEL_REOPENTAB,
    ID_ACCEL_PRIVATETAB,
    ID_ACCEL_OPENDOC,
    ID_ACCEL_FOCUSURL,
    ID_ACCEL_FIND,
    ID_ACCEL_TOGGLE_BOOKMARKS,
    ID_ACCEL_RELOAD,
    ID_ACCEL_FULLSCREEN,
    ID_ACCEL_ESCAPE,
    ID_ACCEL_ZOOM_IN,
    ID_ACCEL_ZOOM_OUT,
    ID_ACCEL_ZOOM_RESET,

    // Dynamic Bookmarks Base
    ID_BM_BASE = 3000
};

// Optional Miniblink (node.dll / mb.dll) dynamic C API for Chromium rendering on Windows XP/7/8/10/11
struct MiniblinkApi {
    HMODULE dll = nullptr;
    using wkeWebView = void*;
    using wkeString = void*;

    typedef bool (*wkeNavigationCallback)(wkeWebView webView, void* param, int navigationType, const wkeString url);
    typedef void (*wkeTitleChangedCallback)(wkeWebView webView, void* param, const wkeString title);
    typedef void (*wkeURLChangedCallback)(wkeWebView webView, void* param, const wkeString url);
    typedef wkeWebView (*wkeCreateViewCallback)(wkeWebView webView, void* param, int navigationType, const wkeString url, const void* windowFeatures);
    typedef bool (*wkeDownloadCallback)(wkeWebView webView, void* param, const char* url);

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

    const wchar_t* (*wkeGetStringW)(wkeString str) = nullptr;
    const char* (*wkeGetString)(wkeString str) = nullptr;
    void (*wkeSetZoomFactor)(wkeWebView webView, float factor) = nullptr;
    float (*wkeGetZoomFactor)(wkeWebView webView) = nullptr;

    void (*wkeOnNavigation)(wkeWebView webView, wkeNavigationCallback callback, void* callbackParam) = nullptr;
    void (*wkeOnTitleChanged)(wkeWebView webView, wkeTitleChangedCallback callback, void* callbackParam) = nullptr;
    void (*wkeOnURLChanged)(wkeWebView webView, wkeURLChangedCallback callback, void* callbackParam) = nullptr;
    void (*wkeOnCreateView)(wkeWebView webView, wkeCreateViewCallback callback, void* callbackParam) = nullptr;
    void (*wkeOnDownload)(wkeWebView webView, wkeDownloadCallback callback, void* callbackParam) = nullptr;
    bool (*wkeFindText)(wkeWebView webView, const wchar_t* text, bool forward, bool matchCase) = nullptr;

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
        BIND_WKE(wkeGetStringW);
        BIND_WKE(wkeGetString);
        BIND_WKE(wkeSetZoomFactor);
        BIND_WKE(wkeGetZoomFactor);
        BIND_WKE(wkeOnNavigation);
        BIND_WKE(wkeOnTitleChanged);
        BIND_WKE(wkeOnURLChanged);
        BIND_WKE(wkeOnCreateView);
        BIND_WKE(wkeOnDownload);
        BIND_WKE(wkeFindText);
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

// Releases unused physical RAM pages to Windows XP/7 OS
void trimProcessWorkingSet() {
    SetProcessWorkingSetSize(GetCurrentProcess(), static_cast<SIZE_T>(-1), static_cast<SIZE_T>(-1));
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
    bool isPrivate = false;
    float zoomFactor = 1.0f;
};

struct WinBrowserApp {
    HINSTANCE hInst = nullptr;
    HWND mainWnd = nullptr;
    HWND btnBack = nullptr, btnFwd = nullptr, btnReload = nullptr, btnHome = nullptr;
    HWND editUrl = nullptr, btnGo = nullptr, btnStar = nullptr;
    HWND btnOpenDoc = nullptr, btnGames = nullptr, btnDownloads = nullptr, btnSettings = nullptr, btnLite = nullptr;
    HWND btnNewTab = nullptr, btnCloseTab = nullptr;
    HWND tabCtrl = nullptr;
    HWND statusWnd = nullptr;
    HFONT uiFont = nullptr;
    WNDPROC origEditProc = nullptr;

    // Bookmarks Bar
    HWND bookmarksBarWnd = nullptr;
    bool bookmarksBarVisible = true;
    std::vector<HWND> bookmarkButtons;

    // In-Page Find Bar
    HWND findBarWnd = nullptr;
    HWND editFind = nullptr;
    HWND btnFindPrev = nullptr;
    HWND btnFindNext = nullptr;
    HWND btnFindClose = nullptr;
    bool findBarVisible = false;

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

void navigateWinTab(WinTab* tab, const std::string& rawInput);
WinTab* createWinTab(const std::string& initialUrl, bool activate = true, bool isPrivate = false);
void promptWinOpenDoc();
void syncWinToolbar();
void layoutWinChildren();
void refreshWinBookmarksBar();
void showWinFindBar(bool show);
void doWinFind(bool forward);
void startWinDownload(const std::string& rawUrl, bool openInReaderWhenDone = false);

void startWinDownload(const std::string& rawUrl, bool openInReaderWhenDone) {
    if (!g_app || rawUrl.empty()) return;
    std::string url = rawUrl;
    if (!litebrowser::fs::startsWithIgnoreCase(url, "http://") &&
        !litebrowser::fs::startsWithIgnoreCase(url, "https://")) {
        url = "https://" + url;
    }
    std::string fname = litebrowser::fs::getFileName(url);
    if (fname.empty() || fname.find('.') == std::string::npos) fname = "downloaded_file.bin";
    std::string savePath = makeUniqueWinDownloadPath(fname);

    int id = litebrowser::DownloaderPool::instance().startDownload(
        g_app->storage, url, savePath, openInReaderWhenDone, [](const litebrowser::DownloadProgressInfo& p) {
            if (p.status == litebrowser::DownloadStatus::Completed && g_app && g_app->mainWnd) {
                PostMessageW(g_app->mainWnd, WM_DOWNLOAD_STATUS, 1, p.id);
            } else if (p.status == litebrowser::DownloadStatus::Failed && g_app && g_app->mainWnd) {
                PostMessageW(g_app->mainWnd, WM_DOWNLOAD_STATUS, 0, p.id);
            }
        });

    if (id > 0) {
        setWinStatus("بدأ التحميل في الخلفية: " + savePath);
    }
}

void applyWinTabZoom(WinTab* tab, float factor) {
    if (!tab) return;
    tab->zoomFactor = (factor < 0.3f) ? 0.3f : ((factor > 3.0f) ? 3.0f : factor);
    if (tab->mbView && g_mb.wkeSetZoomFactor) {
        g_mb.wkeSetZoomFactor(tab->mbView, tab->zoomFactor);
    } else if (tab->oleSite && tab->oleSite->webBrowser) {
        static const GUID CGID_MSHTML_LOCAL = {0xDE4BA900, 0x59CA, 0x11CF, {0x95, 0x92, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00}};
        IOleCommandTarget* pCmdTarget = nullptr;
        if (SUCCEEDED(tab->oleSite->webBrowser->QueryInterface(IID_IOleCommandTarget, reinterpret_cast<void**>(&pCmdTarget)))) {
            VARIANT vIn;
            VariantInit(&vIn);
            vIn.vt = VT_I4;
            vIn.lVal = static_cast<LONG>(tab->zoomFactor * 100.0f);
            pCmdTarget->Exec(&CGID_MSHTML_LOCAL, 63 /* OLECMDID_OPTICAL_ZOOM */, OLECMDEXECOPT_DONTPROMPTUSER, &vIn, nullptr);
            pCmdTarget->Release();
        }
    }
    int pct = static_cast<int>(tab->zoomFactor * 100.0f);
    setWinStatus("نسبة التكبير: " + std::to_string(pct) + "%");
}

void updateTabHeaderText(int index, const std::string& utf8Title) {
    if (!g_app || !g_app->tabCtrl || index < 0) return;
    std::string t = utf8Title.empty() ? "تبويب" : utf8Title;
    if (t.size() > 28) t = t.substr(0, 25) + "...";
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
        if (s.liteMode) trimProcessWorkingSet();
        setWinStatus(s.liteMode ? "تم تفعيل الوضع الخفيف لتوفير الذاكرة" : "تم التبديل للوضع الكامل");
        navigateWinTab(tab, tab->logicalUrl);
    } else if (litebrowser::fs::startsWithIgnoreCase(rest, "toggle-adblock")) {
        auto s = g_app->storage.getSettings();
        s.adBlockEnabled = !s.adBlockEnabled;
        g_app->storage.updateSettings(s);
        setWinStatus(s.adBlockEnabled ? "تم تفعيل مانع الإعلانات" : "تم إيقاف مانع الإعلانات");
        navigateWinTab(tab, tab->logicalUrl);
    } else if (litebrowser::fs::startsWithIgnoreCase(rest, "download?url=")) {
        std::string dlUrl = litebrowser::fs::urlDecode(rest.substr(std::strlen("download?url=")));
        startWinDownload(dlUrl);
        navigateWinTab(tab, "lite://downloads");
    } else if (litebrowser::fs::startsWithIgnoreCase(rest, "pause-download?id=")) {
        int id = std::atoi(rest.c_str() + std::strlen("pause-download?id="));
        litebrowser::DownloaderPool::instance().pauseDownload(id, g_app->storage);
        setWinStatus("تم إيقاف التحميل مؤقتاً");
        navigateWinTab(tab, "lite://downloads");
    } else if (litebrowser::fs::startsWithIgnoreCase(rest, "resume-download?id=")) {
        int id = std::atoi(rest.c_str() + std::strlen("resume-download?id="));
        litebrowser::DownloaderPool::instance().resumeDownload(id, g_app->storage);
        setWinStatus("تم استئناف التحميل");
        navigateWinTab(tab, "lite://downloads");
    } else if (litebrowser::fs::startsWithIgnoreCase(rest, "cancel-download?id=")) {
        int id = std::atoi(rest.c_str() + std::strlen("cancel-download?id="));
        litebrowser::DownloaderPool::instance().cancelDownload(id, g_app->storage);
        setWinStatus("تم إلغاء التحميل");
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
        refreshWinBookmarksBar();
        navigateWinTab(tab, "lite://bookmarks");
    } else if (litebrowser::fs::startsWithIgnoreCase(rest, "clear-history")) {
        g_app->storage.clearHistory();
        navigateWinTab(tab, "lite://history");
    } else if (litebrowser::fs::startsWithIgnoreCase(rest, "clear-cache")) {
        litebrowser::fs::cleanupTempSessionDir();
        trimProcessWorkingSet();
        setWinStatus("تم تفريغ الملفات المؤقتة بنجاح");
        navigateWinTab(tab, "lite://settings");
    } else if (litebrowser::fs::startsWithIgnoreCase(rest, "set-setting?")) {
        std::string q = rest.substr(std::strlen("set-setting?"));
        std::string key, val;
        size_t amp = q.find('&');
        if (q.find("key=") == 0 && amp != std::string::npos && q.find("val=", amp + 1) != std::string::npos) {
            key = q.substr(4, amp - 4);
            val = q.substr(amp + 5);
        }
        key = litebrowser::fs::urlDecode(key);
        val = litebrowser::fs::urlDecode(val);

        auto s = g_app->storage.getSettings();
        if (key == "search-engine") s.searchEngine = val;
        else if (key == "homepage") s.homePage = val;
        else if (key == "restore-session") s.restoreSessionOnStartup = (val == "1" || val == "true");
        else if (key == "adblock") s.adBlockEnabled = (val == "1" || val == "true");
        else if (key == "show-bookmarks") {
            s.showBookmarksBar = (val == "1" || val == "true");
            g_app->bookmarksBarVisible = s.showBookmarksBar;
            layoutWinChildren();
        } else if (key == "lite-mode") {
            s.liteMode = (val == "1" || val == "true");
            SetWindowTextW(g_app->btnLite, s.liteMode ? L"خفيف: نشط" : L"وضع خفيف");
            if (s.liteMode) trimProcessWorkingSet();
        }

        g_app->storage.updateSettings(s);
        setWinStatus("تم تحديث الإعدادات بنجاح");
        navigateWinTab(tab, "lite://settings");
    }
}

STDMETHODIMP OleBrowserSite::Invoke(DISPID dispIdMember, REFIID, LCID, WORD, DISPPARAMS* pDispParams,
                                    VARIANT*, EXCEPINFO*, UINT*) {
    if (!ownerTab || !g_app || !pDispParams) return S_OK;

    if (dispIdMember == DISPID_BEFORENAVIGATE2 && pDispParams->cArgs >= 7) {
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
            if (litebrowser::isRemoteDocumentUrl(url)) {
                if (pCancel) *pCancel = VARIANT_TRUE;
                startWinDownload(url, true);
                navigateWinTab(ownerTab, "lite://downloads");
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
    } else if ((dispIdMember == 251 /* DISPID_NEWWINDOW2 */ || dispIdMember == 273 /* DISPID_NEWWINDOW3 */) && pDispParams->cArgs >= 1) {
        VARIANT* vCancel = &pDispParams->rgvarg[0];
        if (g_app->storage.getSettings().adBlockEnabled) {
            if (vCancel->vt == (VT_BOOL | VT_BYREF) && vCancel->pboolVal) {
                *vCancel->pboolVal = VARIANT_TRUE; // Block popup window
            }
        }
    } else if (dispIdMember == DISPID_TITLECHANGE && pDispParams->cArgs >= 1) {
        VARIANT* vTitle = &pDispParams->rgvarg[0];
        if (vTitle->vt == VT_BSTR && vTitle->bstrVal) {
            ownerTab->title = litebrowser::fs::wideToUtf8(vTitle->bstrVal);
            std::string dispTitle = ownerTab->isPrivate ? ("[سري] " + ownerTab->title) : ownerTab->title;
            for (size_t i = 0; i < g_app->tabs.size(); ++i) {
                if (g_app->tabs[i].get() == ownerTab) {
                    updateTabHeaderText(static_cast<int>(i), dispTitle);
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
                if (!ownerTab->isPrivate && !litebrowser::fs::startsWithIgnoreCase(u, "lite://")) {
                    g_app->storage.addHistory(ownerTab->title, u);
                }
            }
            if (ownerTab == g_app->currentTab()) {
                syncWinToolbar();
                setWinStatus("جاهز");
            }
        }
    }
    return S_OK;
}

// Miniblink navigation callback
bool mbNavigationCallback(MiniblinkApi::wkeWebView webView, void* param, int, MiniblinkApi::wkeString urlStr) {
    auto* tab = static_cast<WinTab*>(param);
    if (!tab || !g_app || !urlStr) return true;
    const wchar_t* urlW = g_mb.wkeGetStringW ? g_mb.wkeGetStringW(urlStr) : nullptr;
    if (!urlW) return true;
    std::string url = litebrowser::fs::wideToUtf8(urlW);

    if (litebrowser::fs::startsWithIgnoreCase(url, "lite-action://")) {
        handleWinLiteAction(tab, url);
        return false;
    }
    if (litebrowser::fs::startsWithIgnoreCase(url, "lite://")) {
        navigateWinTab(tab, url);
        return false;
    }
    if (g_app->storage.getSettings().adBlockEnabled && litebrowser::shouldBlockAdOrTracker(url)) {
        return false;
    }
    if (litebrowser::isRemoteDocumentUrl(url)) {
        startWinDownload(url, true);
        navigateWinTab(tab, "lite://downloads");
        return false;
    }
    if (litebrowser::isDirectDownloadExtension(url) &&
        (litebrowser::fs::startsWithIgnoreCase(url, "http://") || litebrowser::fs::startsWithIgnoreCase(url, "https://"))) {
        startWinDownload(url);
        navigateWinTab(tab, "lite://downloads");
        return false;
    }
    return true;
}

void mbTitleChangedCallback(MiniblinkApi::wkeWebView, void* param, MiniblinkApi::wkeString titleStr) {
    auto* tab = static_cast<WinTab*>(param);
    if (!tab || !g_app || !titleStr) return;
    const wchar_t* titleW = g_mb.wkeGetStringW ? g_mb.wkeGetStringW(titleStr) : nullptr;
    if (!titleW) return;
    tab->title = litebrowser::fs::wideToUtf8(titleW);
    std::string disp = tab->isPrivate ? ("[سري] " + tab->title) : tab->title;
    for (size_t i = 0; i < g_app->tabs.size(); ++i) {
        if (g_app->tabs[i].get() == tab) {
            updateTabHeaderText(static_cast<int>(i), disp);
            break;
        }
    }
}

void mbURLChangedCallback(MiniblinkApi::wkeWebView, void* param, MiniblinkApi::wkeString urlStr) {
    auto* tab = static_cast<WinTab*>(param);
    if (!tab || !g_app || !urlStr) return;
    const wchar_t* urlW = g_mb.wkeGetStringW ? g_mb.wkeGetStringW(urlStr) : nullptr;
    if (!urlW) return;
    std::string u = litebrowser::fs::wideToUtf8(urlW);
    if (u.find("litebrowser_session_") == std::string::npos) {
        tab->logicalUrl = u;
        if (!tab->isPrivate && !litebrowser::fs::startsWithIgnoreCase(u, "lite://")) {
            g_app->storage.addHistory(tab->title, u);
        }
    }
    if (tab == g_app->currentTab()) {
        syncWinToolbar();
        setWinStatus("جاهز");
    }
}

MiniblinkApi::wkeWebView mbCreateViewCallback(MiniblinkApi::wkeWebView, void*, int, MiniblinkApi::wkeString urlStr, const void*) {
    if (!g_app || !urlStr) return nullptr;
    const wchar_t* urlW = g_mb.wkeGetStringW ? g_mb.wkeGetStringW(urlStr) : nullptr;
    std::string url = (urlW && *urlW) ? litebrowser::fs::wideToUtf8(urlW) : "lite://home";
    WinTab* t = createWinTab(url, true);
    return t ? t->mbView : nullptr;
}

bool mbDownloadCallback(MiniblinkApi::wkeWebView, void*, const char* url) {
    if (!url || !g_app) return false;
    startWinDownload(url);
    if (g_app->currentTab()) {
        navigateWinTab(g_app->currentTab(), "lite://downloads");
    }
    return false;
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
    if (litebrowser::isRemoteDocumentUrl(rawInput)) {
        startWinDownload(rawInput, true);
        setWinStatus("جاري تنزيل المستند لفتحه في القارئ المدمج...");
        navigateWinTab(tab, "lite://downloads");
        return;
    }

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
        std::string disp = tab->isPrivate ? ("[سري] " + page.title) : page.title;
        updateTabHeaderText(tabIdx, disp);
        loadUrlIntoTabEngine(tab, page.tempFileUrl);
    } else if (resolved.type == litebrowser::OmniboxActionType::OpenLocalDocument) {
        tab->logicalUrl = resolved.targetUrlOrPath;
        tab->title = "جاري قراءة " + litebrowser::fs::getFileName(resolved.targetUrlOrPath) + "...";
        std::string disp = tab->isPrivate ? ("[سري] " + tab->title) : tab->title;
        updateTabHeaderText(tabIdx, disp);
        setWinStatus("جاري معالجة المستند وقراءته في الخلفية...");

        int targetTabId = tab->tabId;
        std::string path = resolved.targetUrlOrPath;
        std::thread([targetTabId, path]() {
            auto doc = litebrowser::renderDocumentToHtml(path, targetTabId);
            auto* res = new AsyncDocResult{ targetTabId, path, std::move(doc) };
            if (g_app && g_app->mainWnd) {
                PostMessageW(g_app->mainWnd, WM_DOC_LOADED, 0, reinterpret_cast<LPARAM>(res));
            } else {
                delete res;
            }
        }).detach();
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
    if (g_app->storage.getSettings().liteMode) {
        trimProcessWorkingSet();
    }
}

WinTab* createWinTab(const std::string& initialUrl, bool activate, bool isPrivate) {
    auto tab = std::make_unique<WinTab>();
    tab->app = g_app;
    tab->tabId = g_app->nextTabId++;
    tab->isPrivate = isPrivate;

    tab->hostPanel = CreateWindowExW(
        0, L"STATIC", L"", WS_CHILD | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
        0, 74, 900, 560, g_app->mainWnd, nullptr, g_app->hInst, nullptr);

    if (g_mb.dll && g_mb.wkeCreateWebWindow) {
        tab->mbView = g_mb.wkeCreateWebWindow(2, tab->hostPanel, 0, 0, 900, 560);
        if (tab->mbView) {
            g_mb.wkeShowWindow(tab->mbView, true);
            if (g_mb.wkeOnNavigation) g_mb.wkeOnNavigation(tab->mbView, mbNavigationCallback, tab.get());
            if (g_mb.wkeOnTitleChanged) g_mb.wkeOnTitleChanged(tab->mbView, mbTitleChangedCallback, tab.get());
            if (g_mb.wkeOnURLChanged) g_mb.wkeOnURLChanged(tab->mbView, mbURLChangedCallback, tab.get());
            if (g_mb.wkeOnCreateView) g_mb.wkeOnCreateView(tab->mbView, mbCreateViewCallback, tab.get());
            if (g_mb.wkeOnDownload) g_mb.wkeOnDownload(tab->mbView, mbDownloadCallback, tab.get());
        }
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
    int idx = g_app->activeIndex;
    if (idx < 0 || idx >= static_cast<int>(g_app->tabs.size())) return;

    WinTab* cur = g_app->tabs[static_cast<size_t>(idx)].get();
    if (!cur->isPrivate && !cur->logicalUrl.empty() && cur->logicalUrl != "lite://home") {
        g_app->storage.pushClosedTab(cur->logicalUrl);
    }

    if (g_app->tabs.size() == 1) {
        navigateWinTab(cur, "lite://home");
        return;
    }

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

void refreshWinBookmarksBar() {
    if (!g_app) return;

    // Destroy existing bookmark buttons
    for (HWND h : g_app->bookmarkButtons) {
        DestroyWindow(h);
    }
    g_app->bookmarkButtons.clear();

    if (!g_app->bookmarksBarVisible) {
        ShowWindow(g_app->bookmarksBarWnd, SW_HIDE);
        return;
    }

    ShowWindow(g_app->bookmarksBarWnd, SW_SHOW);
    auto bms = g_app->storage.getBookmarks();
    size_t count = std::min(bms.size(), static_cast<size_t>(10));

    int bx = 4;
    for (size_t i = 0; i < count; ++i) {
        std::string shortTitle = bms[i].title.empty() ? bms[i].url : bms[i].title;
        if (shortTitle.size() > 18) shortTitle = shortTitle.substr(0, 15) + "...";
        std::wstring wTitle = litebrowser::fs::utf8ToWide(shortTitle);

        HWND btn = CreateWindowExW(
            0, L"BUTTON", wTitle.c_str(), WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            bx, 2, 100, 22, g_app->bookmarksBarWnd,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_BM_BASE + i)), g_app->hInst, nullptr);
        SendMessageW(btn, WM_SETFONT, reinterpret_cast<WPARAM>(g_app->uiFont), TRUE);
        g_app->bookmarkButtons.push_back(btn);
        bx += 104;
    }
}

void showWinFindBar(bool show) {
    if (!g_app || !g_app->findBarWnd) return;
    g_app->findBarVisible = show;
    ShowWindow(g_app->findBarWnd, show ? SW_SHOW : SW_HIDE);
    if (show) {
        SetFocus(g_app->editFind);
        SendMessageW(g_app->editFind, EM_SETSEL, 0, -1);
    }
    layoutWinChildren();
}

void doWinFind(bool forward) {
    if (!g_app) return;
    WinTab* cur = g_app->currentTab();
    if (!cur) return;

    int len = GetWindowTextLengthW(g_app->editFind);
    if (len <= 0) return;
    std::wstring text(static_cast<size_t>(len + 1), L'\0');
    GetWindowTextW(g_app->editFind, &text[0], len + 1);
    text.resize(static_cast<size_t>(len));

    if (cur->mbView && g_mb.wkeFindText) {
        g_mb.wkeFindText(cur->mbView, text.c_str(), forward, false);
        return;
    }

    if (cur->oleSite && cur->oleSite->webBrowser) {
        IDispatch* docDisp = nullptr;
        if (SUCCEEDED(cur->oleSite->webBrowser->get_Document(&docDisp)) && docDisp) {
            IHTMLDocument2* doc2 = nullptr;
            if (SUCCEEDED(docDisp->QueryInterface(IID_IHTMLDocument2, reinterpret_cast<void**>(&doc2))) && doc2) {
                IHTMLElement* body = nullptr;
                if (SUCCEEDED(doc2->get_body(&body)) && body) {
                    IHTMLBodyElement* bodyEl = nullptr;
                    if (SUCCEEDED(body->QueryInterface(IID_IHTMLBodyElement, reinterpret_cast<void**>(&bodyEl))) && bodyEl) {
                        IHTMLTxtRange* rng = nullptr;
                        if (SUCCEEDED(bodyEl->createTextRange(&rng)) && rng) {
                            BSTR bFind = SysAllocString(text.c_str());
                            VARIANT_BOOL found = VARIANT_FALSE;
                            long count = forward ? 1000000 : -1000000;
                            rng->findText(bFind, count, 0, &found);
                            if (found == VARIANT_TRUE) {
                                rng->select();
                                rng->scrollIntoView(VARIANT_TRUE);
                                setWinStatus("تم العثور على النتيجة في الصفحة");
                            } else {
                                setWinStatus("لم يتم العثور على نتائج إضافية");
                            }
                            SysFreeString(bFind);
                            rng->Release();
                        }
                        bodyEl->Release();
                    }
                    body->Release();
                }
                doc2->Release();
            }
            docDisp->Release();
        }
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

    int rightButtonsWidth = 40 + 34 + 86 + 62 + 74 + 70 + 80 + 62 + 34 + 30;
    int urlW = std::max(120, w - x - rightButtonsWidth);
    MoveWindow(g_app->editUrl, x, topY + 2, urlW, btnH - 4, TRUE); x += urlW + 4;
    MoveWindow(g_app->btnGo, x, topY, 38, btnH, TRUE); x += 41;
    MoveWindow(g_app->btnStar, x, topY, 32, btnH, TRUE); x += 35;
    MoveWindow(g_app->btnOpenDoc, x, topY, 84, btnH, TRUE); x += 87;
    MoveWindow(g_app->btnGames, x, topY, 60, btnH, TRUE); x += 63;
    MoveWindow(g_app->btnDownloads, x, topY, 72, btnH, TRUE); x += 75;
    MoveWindow(g_app->btnSettings, x, topY, 68, btnH, TRUE); x += 71;
    MoveWindow(g_app->btnLite, x, topY, 78, btnH, TRUE); x += 81;
    MoveWindow(g_app->btnNewTab, x, topY, 58, btnH, TRUE); x += 61;
    MoveWindow(g_app->btnCloseTab, x, topY, 30, btnH, TRUE);

    int curY = 38;
    if (g_app->bookmarksBarVisible) {
        MoveWindow(g_app->bookmarksBarWnd, 4, curY, w - 8, 26, TRUE);
        curY += 28;
    }

    const int tabBarH = 28;
    MoveWindow(g_app->tabCtrl, 4, curY, w - 8, tabBarH, TRUE);
    curY += tabBarH + 2;

    if (g_app->findBarVisible) {
        MoveWindow(g_app->findBarWnd, 4, curY, w - 8, 28, TRUE);
        MoveWindow(g_app->editFind, 130, 2, std::min(300, w - 320), 22, TRUE);
        MoveWindow(g_app->btnFindPrev, std::min(435, w - 185), 1, 55, 24, TRUE);
        MoveWindow(g_app->btnFindNext, std::min(495, w - 125), 1, 55, 24, TRUE);
        MoveWindow(g_app->btnFindClose, std::min(555, w - 65), 1, 30, 24, TRUE);
        curY += 30;
    }

    const int statusH = 22;
    MoveWindow(g_app->statusWnd, 0, h - statusH, w, statusH, TRUE);

    const int viewH = std::max(40, h - curY - statusH);
    if (WinTab* cur = g_app->currentTab()) {
        MoveWindow(cur->hostPanel, 0, curY, w, viewH, TRUE);
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
                if (rec.openInReaderWhenDone) {
                    createWinTab(rec.savePath, true);
                }
            } else {
                setWinStatus("انتهت عملية التحميل بتعذّر الوصول إلى الملف.");
            }
            if (WinTab* t = g_app->currentTab()) {
                if (t->logicalUrl == "lite://downloads") navigateWinTab(t, "lite://downloads");
            }
            return 0;
        }

        case WM_DOC_LOADED: {
            auto* res = reinterpret_cast<AsyncDocResult*>(lp);
            if (res) {
                if (g_app) {
                    for (size_t i = 0; i < g_app->tabs.size(); ++i) {
                        if (g_app->tabs[i]->tabId == res->tabId) {
                            WinTab* t = g_app->tabs[i].get();
                            t->activeTempUrl = res->doc.tempFileUrl;
                            t->title = res->doc.tabTitle;
                            std::string disp = t->isPrivate ? ("[سري] " + t->title) : t->title;
                            updateTabHeaderText(static_cast<int>(i), disp);
                            loadUrlIntoTabEngine(t, res->doc.tempFileUrl);
                            if (t == g_app->currentTab()) {
                                syncWinToolbar();
                                setWinStatus("تم فتح المستند بنجاح: " + res->doc.tabTitle);
                            }
                            break;
                        }
                    }
                }
                delete res;
            }
            return 0;
        }

        case WM_COMMAND: {
            int id = LOWORD(wp);
            WinTab* cur = g_app ? g_app->currentTab() : nullptr;

            // Handle bookmarks bar clicks (ID_BM_BASE .. ID_BM_BASE + 99)
            if (id >= ID_BM_BASE && id < ID_BM_BASE + 100) {
                size_t bmIdx = static_cast<size_t>(id - ID_BM_BASE);
                auto bms = g_app->storage.getBookmarks();
                if (bmIdx < bms.size() && cur) {
                    navigateWinTab(cur, bms[bmIdx].url);
                }
                return 0;
            }

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
                case ID_ACCEL_RELOAD:
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
                        refreshWinBookmarksBar();
                    }
                    break;
                case ID_BTN_OPENDOC:
                case ID_ACCEL_OPENDOC:
                    promptWinOpenDoc();
                    break;
                case ID_BTN_GAMES:
                    if (cur) navigateWinTab(cur, "lite://games");
                    break;
                case ID_BTN_DOWNLOADS:
                    if (cur) navigateWinTab(cur, "lite://downloads");
                    break;
                case ID_BTN_SETTINGS:
                    if (cur) navigateWinTab(cur, "lite://settings");
                    break;
                case ID_BTN_LITE:
                    if (cur) handleWinLiteAction(cur, "lite-action://toggle-lite");
                    break;
                case ID_BTN_NEWTAB:
                case ID_ACCEL_NEWTAB:
                    createWinTab("lite://home", true);
                    break;
                case ID_BTN_CLOSETAB:
                case ID_ACCEL_CLOSETAB:
                    closeCurrentWinTab();
                    break;
                case ID_ACCEL_REOPENTAB: {
                    std::string closed = g_app->storage.popClosedTab();
                    if (!closed.empty()) {
                        createWinTab(closed, true);
                        setWinStatus("تمت استعادة التبويب المغلق: " + closed);
                    }
                    break;
                }
                case ID_ACCEL_PRIVATETAB:
                    createWinTab("lite://home", true, true);
                    setWinStatus("تم فتح تبويب تصفح سري لا يحفظ في السجل");
                    break;
                case ID_ACCEL_FOCUSURL:
                    SetFocus(g_app->editUrl);
                    SendMessageW(g_app->editUrl, EM_SETSEL, 0, -1);
                    break;
                case ID_ACCEL_TOGGLE_BOOKMARKS:
                    g_app->bookmarksBarVisible = !g_app->bookmarksBarVisible;
                    refreshWinBookmarksBar();
                    layoutWinChildren();
                    break;
                case ID_ACCEL_FIND:
                    showWinFindBar(true);
                    break;
                case ID_ACCEL_ESCAPE:
                    if (g_app->findBarVisible) showWinFindBar(false);
                    break;
                case ID_FIND_NEXT:
                    doWinFind(true);
                    break;
                case ID_FIND_PREV:
                    doWinFind(false);
                    break;
                case ID_FIND_CLOSE:
                    showWinFindBar(false);
                    break;
                case ID_ACCEL_ZOOM_IN:
                    if (cur) applyWinTabZoom(cur, cur->zoomFactor + 0.1f);
                    break;
                case ID_ACCEL_ZOOM_OUT:
                    if (cur) applyWinTabZoom(cur, cur->zoomFactor - 0.1f);
                    break;
                case ID_ACCEL_ZOOM_RESET:
                    if (cur) applyWinTabZoom(cur, 1.0f);
                    break;
                case ID_ACCEL_FULLSCREEN: {
                    static bool isFull = false;
                    static RECT prevRc;
                    if (!isFull) {
                        GetWindowRect(hwnd, &prevRc);
                        SetWindowLongW(hwnd, GWL_STYLE, WS_POPUP | WS_VISIBLE);
                        SetWindowPos(hwnd, HWND_TOP, 0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN), SWP_FRAMECHANGED);
                        isFull = true;
                    } else {
                        SetWindowLongW(hwnd, GWL_STYLE, WS_OVERLAPPEDWINDOW | WS_VISIBLE);
                        SetWindowPos(hwnd, HWND_NOTOPMOST, prevRc.left, prevRc.top, prevRc.right - prevRc.left, prevRc.bottom - prevRc.top, SWP_FRAMECHANGED);
                        isFull = false;
                    }
                    layoutWinChildren();
                    break;
                }
            }
            return 0;
        }

        case WM_DESTROY:
            if (g_app) {
                if (g_app->storage.getSettings().restoreSessionOnStartup) {
                    std::vector<std::string> sessionUrls;
                    for (const auto& t : g_app->tabs) {
                        if (!t->isPrivate && !t->logicalUrl.empty() && t->logicalUrl != "lite://home") {
                            sessionUrls.push_back(t->logicalUrl);
                        }
                    }
                    g_app->storage.saveSessionTabs(sessionUrls);
                }
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

void applyResourceProfile(WinBrowserApp& app) {
    MEMORYSTATUSEX memStatus{};
    memStatus.dwLength = sizeof(memStatus);
    GlobalMemoryStatusEx(&memStatus);
    uint64_t totalRamMb = memStatus.ullTotalPhys / (1024 * 1024);

    OSVERSIONINFOEXW osvi{};
    osvi.dwOSVersionInfoSize = sizeof(osvi);
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
    GetVersionExW(reinterpret_cast<OSVERSIONINFOW*>(&osvi));
#pragma GCC diagnostic pop

    bool isXpOr2003 = (osvi.dwMajorVersion == 5);
    auto settings = app.storage.getSettings();

    // If on Windows XP or total RAM < 2GB, enable Lite Mode automatically for optimum speed
    if ((isXpOr2003 || totalRamMb < 2048) && !settings.liteMode) {
        settings.liteMode = true;
        app.storage.updateSettings(settings);
    }
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

    applyResourceProfile(app);

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = mainWndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hIcon = LoadIconW(hInstance, MAKEINTRESOURCEW(101));
    if (!wc.hIcon) wc.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    wc.hIconSm = LoadIconW(hInstance, MAKEINTRESOURCEW(101));
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1);
    wc.lpszClassName = L"LiteBrowserWinClass";
    RegisterClassExW(&wc);

    app.mainWnd = CreateWindowExW(
        0, L"LiteBrowserWinClass", L"متصفح النور الخفيف - LiteBrowser",
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
        CW_USEDEFAULT, CW_USEDEFAULT, 1200, 780,
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
    app.btnSettings = makeBtn(L"الإعدادات", ID_BTN_SETTINGS);
    auto initS = app.storage.getSettings();
    app.btnLite = makeBtn(initS.liteMode ? L"خفيف: نشط" : L"وضع خفيف", ID_BTN_LITE);
    app.btnNewTab = makeBtn(L"+ تبويب", ID_BTN_NEWTAB);
    app.btnCloseTab = makeBtn(L"×", ID_BTN_CLOSETAB);

    // Bookmarks Bar Container
    app.bookmarksBarWnd = CreateWindowExW(
        0, L"STATIC", L"", WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN,
        0, 38, 800, 26, app.mainWnd, nullptr, hInstance, nullptr);

    // Find Bar Container
    app.findBarWnd = CreateWindowExW(
        0, L"STATIC", L"", WS_CHILD | WS_CLIPCHILDREN,
        0, 70, 800, 28, app.mainWnd, nullptr, hInstance, nullptr);

    HWND lblFind = CreateWindowExW(
        0, L"STATIC", L"البحث في الصفحة:", WS_CHILD | WS_VISIBLE,
        6, 5, 120, 20, app.findBarWnd, nullptr, hInstance, nullptr);
    SendMessageW(lblFind, WM_SETFONT, reinterpret_cast<WPARAM>(app.uiFont), TRUE);

    app.editFind = CreateWindowExW(
        WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
        130, 2, 250, 22, app.findBarWnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_FIND_EDIT)), hInstance, nullptr);
    SendMessageW(app.editFind, WM_SETFONT, reinterpret_cast<WPARAM>(app.uiFont), TRUE);

    app.btnFindPrev = CreateWindowExW(
        0, L"BUTTON", L"السابق", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        385, 1, 55, 24, app.findBarWnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_FIND_PREV)), hInstance, nullptr);
    SendMessageW(app.btnFindPrev, WM_SETFONT, reinterpret_cast<WPARAM>(app.uiFont), TRUE);

    app.btnFindNext = CreateWindowExW(
        0, L"BUTTON", L"التالي", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        445, 1, 55, 24, app.findBarWnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_FIND_NEXT)), hInstance, nullptr);
    SendMessageW(app.btnFindNext, WM_SETFONT, reinterpret_cast<WPARAM>(app.uiFont), TRUE);

    app.btnFindClose = CreateWindowExW(
        0, L"BUTTON", L"×", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        505, 1, 30, 24, app.findBarWnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_FIND_CLOSE)), hInstance, nullptr);
    SendMessageW(app.btnFindClose, WM_SETFONT, reinterpret_cast<WPARAM>(app.uiFont), TRUE);

    app.tabCtrl = CreateWindowExW(
        0, WC_TABCONTROLW, L"", WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS,
        0, 40, 800, 28, app.mainWnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_TABCTRL)), hInstance, nullptr);
    SendMessageW(app.tabCtrl, WM_SETFONT, reinterpret_cast<WPARAM>(app.uiFont), TRUE);

    app.statusWnd = CreateWindowExW(
        0, STATUSCLASSNAMEW, L"جاهز", WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP,
        0, 0, 0, 0, app.mainWnd, nullptr, hInstance, nullptr);
    SendMessageW(app.statusWnd, WM_SETFONT, reinterpret_cast<WPARAM>(app.uiFont), TRUE);

    // Setup Accelerators Table
    ACCEL accels[] = {
        { FCONTROL | FVIRTKEY, 'T', ID_ACCEL_NEWTAB },
        { FCONTROL | FVIRTKEY, 'W', ID_ACCEL_CLOSETAB },
        { FCONTROL | FSHIFT | FVIRTKEY, 'T', ID_ACCEL_REOPENTAB },
        { FCONTROL | FSHIFT | FVIRTKEY, 'N', ID_ACCEL_PRIVATETAB },
        { FCONTROL | FSHIFT | FVIRTKEY, 'B', ID_ACCEL_TOGGLE_BOOKMARKS },
        { FCONTROL | FVIRTKEY, 'O', ID_ACCEL_OPENDOC },
        { FCONTROL | FVIRTKEY, 'L', ID_ACCEL_FOCUSURL },
        { FCONTROL | FVIRTKEY, 'F', ID_ACCEL_FIND },
        { FCONTROL | FVIRTKEY, 'R', ID_ACCEL_RELOAD },
        { FVIRTKEY, VK_F5, ID_ACCEL_RELOAD },
        { FCONTROL | FVIRTKEY, VK_OEM_PLUS, ID_ACCEL_ZOOM_IN },
        { FCONTROL | FVIRTKEY, VK_ADD, ID_ACCEL_ZOOM_IN },
        { FCONTROL | FVIRTKEY, VK_OEM_MINUS, ID_ACCEL_ZOOM_OUT },
        { FCONTROL | FVIRTKEY, VK_SUBTRACT, ID_ACCEL_ZOOM_OUT },
        { FCONTROL | FVIRTKEY, '0', ID_ACCEL_ZOOM_RESET },
        { FCONTROL | FVIRTKEY, VK_NUMPAD0, ID_ACCEL_ZOOM_RESET },
        { FVIRTKEY, VK_F11, ID_ACCEL_FULLSCREEN },
        { FVIRTKEY, VK_ESCAPE, ID_ACCEL_ESCAPE }
    };
    HACCEL hAccel = CreateAcceleratorTableW(accels, sizeof(accels) / sizeof(accels[0]));

    refreshWinBookmarksBar();

    // Check command-line file/URL argument or session restore
    int numArgs = 0;
    LPWSTR* wArgs = CommandLineToArgvW(GetCommandLineW(), &numArgs);

    bool restored = false;
    if (app.storage.getSettings().restoreSessionOnStartup) {
        auto saved = app.storage.loadSessionTabs();
        if (!saved.empty()) {
            for (const auto& u : saved) {
                createWinTab(u, false);
            }
            if (!app.tabs.empty()) {
                switchActiveWinTab(0);
                restored = true;
            }
        }
    }

    if (!restored) {
        std::string initialTarget = app.storage.getSettings().homePage;
        if (wArgs && numArgs >= 2) {
            initialTarget = litebrowser::fs::wideToUtf8(wArgs[1]);
        }
        createWinTab(initialTarget, true);
    }
    if (wArgs) LocalFree(wArgs);

    layoutWinChildren();

    ShowWindow(app.mainWnd, nCmdShow);
    UpdateWindow(app.mainWnd);

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        if (hAccel && TranslateAcceleratorW(app.mainWnd, hAccel, &msg)) {
            continue;
        }

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

    if (hAccel) DestroyAcceleratorTable(hAccel);
    if (app.uiFont) DeleteObject(app.uiFont);
    litebrowser::fs::cleanupTempSessionDir();
    OleUninitialize();
    return static_cast<int>(msg.wParam);
}
