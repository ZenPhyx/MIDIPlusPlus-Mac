#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX        // prevent Windows headers from defining min/max macros
#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>
#include <shellapi.h>
#include <shlobj.h>
#include <string>
#include <vector>
#include <algorithm>
#include <memory>
#include <fstream>
#include <cctype>
#include <cmath>

#include "MIDIPlayer.hpp"
#include "RobloxKeyMapper.hpp"
#include "InputInjector.hpp"
#include "RtMidi.h"

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "comdlg32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "winmm.lib")

// ─── Colors (BGR for Win32) ───────────────────────────────────────────────────

static COLORREF clrWin()   { return 0xE3EFF5; }  // cream
static COLORREF clrBar()   { return 0xD5E5ED; }  // slightly darker cream
static COLORREF clrText()  { return 0x100E1C; }  // dark ink
static COLORREF clrDim()   { return 0x887060; }  // muted
static COLORREF clrInput() { return 0xF8FEFE; }  // near-white

// ─── IDs ─────────────────────────────────────────────────────────────────────

enum {
    ID_BROWSE   = 101,
    ID_SEARCH, ID_LIBRARY,
    ID_ELAPSED, ID_TOTAL, ID_PROGRESS,
    ID_RESTART, ID_REWIND, ID_PLAYPAUSE, ID_FWD, ID_STOP,
    ID_SPEED_SLD, ID_SPEED_LBL,
    ID_DEVICE_CMB, ID_REFRESH, ID_LIVE,
    ID_STATUS,
    ID_TIMER_PROGRESS = 200,
    ID_TIMER_MIDI     = 201,
};

// ─── Globals ──────────────────────────────────────────────────────────────────

static HWND    g_hwnd;
static HBRUSH  g_brWin, g_brBar, g_brInput;
static HFONT   g_fontTitle, g_fontBold, g_fontNorm, g_fontSm;

static std::unique_ptr<MIDIPlayer>  g_player;
static std::unique_ptr<RtMidiIn>    g_midiIn;
static std::vector<unsigned char>   g_midiMsg;

struct LibEntry { std::wstring path, name, duration; };
static std::vector<LibEntry>  g_library;
static std::vector<LibEntry*> g_filtered;
static int g_selected = -1;

// ─── String helpers ───────────────────────────────────────────────────────────

static std::wstring toWide(const std::string& s) {
    if (s.empty()) return {};
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    std::wstring w(n, 0);
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, w.data(), n);
    if (!w.empty() && w.back() == 0) w.pop_back();
    return w;
}

static std::string toUtf8(const std::wstring& w) {
    if (w.empty()) return {};
    int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string s(n, 0);
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, s.data(), n, nullptr, nullptr);
    if (!s.empty() && s.back() == 0) s.pop_back();
    return s;
}

static std::wstring fmtDur(double sec) {
    int t = (int)sec;
    wchar_t buf[16];
    swprintf_s(buf, L"%d:%02d", t / 60, t % 60);
    return buf;
}

// Extract filename without extension from a full path
static std::wstring stemOf(const std::wstring& path) {
    size_t slash = path.find_last_of(L"\\/");
    std::wstring name = (slash == std::wstring::npos) ? path : path.substr(slash + 1);
    size_t dot = name.rfind(L'.');
    if (dot != std::wstring::npos) name = name.substr(0, dot);
    return name;
}

// ─── Library persistence ──────────────────────────────────────────────────────

static std::wstring libFilePath() {
    wchar_t buf[MAX_PATH] = {};
    SHGetFolderPathW(nullptr, CSIDL_APPDATA, nullptr, 0, buf);
    std::wstring dir = std::wstring(buf) + L"\\Snuffiano";
    CreateDirectoryW(dir.c_str(), nullptr);
    return dir + L"\\library.txt";
}

static void saveLibrary() {
    std::wofstream f(libFilePath());
    for (auto& e : g_library) f << e.path << L"\n";
}

static void loadLibrary() {
    std::wifstream f(libFilePath());
    std::wstring line;
    while (std::getline(f, line)) {
        if (line.empty()) continue;
        double dur = MIDIPlayer::fileDuration(toUtf8(line));
        g_library.push_back({ line, stemOf(line), dur > 0 ? fmtDur(dur) : L"--:--" });
    }
}

static void addFile(const std::wstring& path) {
    for (auto& e : g_library)
        if (e.path == path) return;
    double dur = MIDIPlayer::fileDuration(toUtf8(path));
    g_library.insert(g_library.begin(), { path, stemOf(path), dur > 0 ? fmtDur(dur) : L"--:--" });
    saveLibrary();
}

// ─── UI helpers ───────────────────────────────────────────────────────────────

static void setStatus(const std::wstring& msg) {
    SetDlgItemTextW(g_hwnd, ID_STATUS, msg.c_str());
}
static void setStatus(const std::string& msg) { setStatus(toWide(msg)); }

static void updateBrushes() {
    if (g_brWin)   { DeleteObject(g_brWin);   g_brWin   = nullptr; }
    if (g_brBar)   { DeleteObject(g_brBar);   g_brBar   = nullptr; }
    if (g_brInput) { DeleteObject(g_brInput); g_brInput = nullptr; }
    g_brWin   = CreateSolidBrush(clrWin());
    g_brBar   = CreateSolidBrush(clrBar());
    g_brInput = CreateSolidBrush(clrInput());
}

static void populateDevices() {
    HWND cb = GetDlgItem(g_hwnd, ID_DEVICE_CMB);
    SendMessage(cb, CB_RESETCONTENT, 0, 0);
    try {
        RtMidiIn midi;
        unsigned int n = midi.getPortCount();
        if (n == 0) {
            SendMessage(cb, CB_ADDSTRING, 0, (LPARAM)L"No MIDI devices found");
        } else {
            for (unsigned int i = 0; i < n; ++i)
                SendMessage(cb, CB_ADDSTRING, 0, (LPARAM)toWide(midi.getPortName(i)).c_str());
        }
    } catch (...) {
        SendMessage(cb, CB_ADDSTRING, 0, (LPARAM)L"Error listing devices");
    }
    SendMessage(cb, CB_SETCURSEL, 0, 0);
}

static void filterLibrary(const std::wstring& q) {
    g_filtered.clear();
    std::wstring ql = q;
    std::transform(ql.begin(), ql.end(), ql.begin(), [](wchar_t c){ return (wchar_t)towlower(c); });
    for (auto& e : g_library) {
        std::wstring nl = e.name;
        std::transform(nl.begin(), nl.end(), nl.begin(), [](wchar_t c){ return (wchar_t)towlower(c); });
        if (ql.empty() || nl.find(ql) != std::wstring::npos)
            g_filtered.push_back(&e);
    }
    HWND lb = GetDlgItem(g_hwnd, ID_LIBRARY);
    SendMessage(lb, LB_RESETCONTENT, 0, 0);
    for (auto* e : g_filtered)
        SendMessage(lb, LB_ADDSTRING, 0, (LPARAM)e->name.c_str());

    wchar_t buf[64];
    swprintf_s(buf, L"%zu song%s", g_filtered.size(), g_filtered.size() == 1 ? L"" : L"s");
    SetDlgItemTextW(g_hwnd, ID_STATUS, buf);
}

// ─── Playback ─────────────────────────────────────────────────────────────────

static void startPlaying() {
    if (g_selected < 0 || g_selected >= (int)g_filtered.size()) {
        setStatus(L"Select a song from the library first.");
        return;
    }
    std::string path = toUtf8(g_filtered[g_selected]->path);
    double dur = MIDIPlayer::fileDuration(path);
    SetDlgItemTextW(g_hwnd, ID_TOTAL, fmtDur(dur).c_str());
    SendMessage(GetDlgItem(g_hwnd, ID_PROGRESS), TBM_SETPOS, TRUE, 0);

    g_player->playFile(
        path,
        [](char key, bool press) { press ? pressKey(key) : releaseKey(key); },
        [](const std::string& msg) {
            // Marshal status to main thread
            auto* s = new std::wstring(toWide(msg));
            PostMessage(g_hwnd, WM_USER + 1, 0, (LPARAM)s);
        },
        []() { PostMessage(g_hwnd, WM_USER + 2, 0, 0); }
    );

    SetDlgItemTextW(g_hwnd, ID_PLAYPAUSE, L"⏸"); // ⏸
    SetTimer(g_hwnd, ID_TIMER_PROGRESS, 100, nullptr);
}

// ─── Control factory helpers ──────────────────────────────────────────────────

static HWND mkBtn(HWND p, const wchar_t* t, int x, int y, int w, int h, int id) {
    return CreateWindowW(L"BUTTON", t,
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        x, y, w, h, p, (HMENU)(INT_PTR)id, nullptr, nullptr);
}

static HWND mkStatic(HWND p, const wchar_t* t, int x, int y, int w, int h, int id, DWORD xStyle = 0) {
    return CreateWindowW(L"STATIC", t,
        WS_CHILD | WS_VISIBLE | SS_CENTER | xStyle,
        x, y, w, h, p, (HMENU)(INT_PTR)id, nullptr, nullptr);
}

static HWND mkTrack(HWND p, int x, int y, int w, int h, int id, int mn, int mx, int val) {
    HWND tb = CreateWindowW(TRACKBAR_CLASSW, L"",
        WS_CHILD | WS_VISIBLE | TBS_HORZ | TBS_NOTICKS,
        x, y, w, h, p, (HMENU)(INT_PTR)id, nullptr, nullptr);
    SendMessage(tb, TBM_SETRANGE, TRUE, MAKELPARAM(mn, mx));
    SendMessage(tb, TBM_SETPOS,   TRUE, val);
    return tb;
}

static void setFont(HWND h, HFONT f) { SendMessage(h, WM_SETFONT, (WPARAM)f, TRUE); }

// ─── Window procedure ─────────────────────────────────────────────────────────

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {

    case WM_CREATE:
        DragAcceptFiles(hwnd, TRUE);
        break;

    case WM_TIMER:
        if (wp == ID_TIMER_PROGRESS && g_player->isRunning()) {
            double pos = g_player->getPosition();
            double dur = g_player->getDuration();
            if (dur > 0) {
                SendMessage(GetDlgItem(hwnd, ID_PROGRESS), TBM_SETPOS, TRUE, (LPARAM)(int)(pos / dur * 1000));
                SetDlgItemTextW(hwnd, ID_ELAPSED, fmtDur(pos).c_str());
            }
        }
        if (wp == ID_TIMER_MIDI && g_midiIn) {
            g_midiIn->getMessage(&g_midiMsg);
            if (g_midiMsg.size() >= 3) {
                uint8_t type = g_midiMsg[0] & 0xF0;
                uint8_t note = g_midiMsg[1];
                uint8_t vel  = g_midiMsg[2];
                auto key = RobloxKeyMapper::map((int)note);
                if (key) {
                    if (type == 0x90 && vel > 0) pressKey(*key);
                    else if (type == 0x80 || (type == 0x90 && vel == 0)) releaseKey(*key);
                }
            }
        }
        break;

    case WM_USER + 1: {   // status string from player thread
        auto* s = reinterpret_cast<std::wstring*>(lp);
        setStatus(*s);
        delete s;
        break;
    }

    case WM_USER + 2:     // playback finished
        KillTimer(hwnd, ID_TIMER_PROGRESS);
        SendMessage(GetDlgItem(hwnd, ID_PROGRESS), TBM_SETPOS, TRUE, 0);
        SetDlgItemTextW(hwnd, ID_ELAPSED, L"0:00");
        SetDlgItemTextW(hwnd, ID_PLAYPAUSE, L"▶"); // ▶
        break;

    case WM_DROPFILES: {
        HDROP hDrop = (HDROP)wp;
        wchar_t path[MAX_PATH] = {};
        if (DragQueryFileW(hDrop, 0, path, MAX_PATH)) {
            addFile(path);
            SetDlgItemTextW(hwnd, ID_SEARCH, L"");
            filterLibrary(L"");
        }
        DragFinish(hDrop);
        break;
    }

    case WM_HSCROLL: {
        HWND ctrl = (HWND)lp;
        if (ctrl == GetDlgItem(hwnd, ID_PROGRESS)) {
            double dur = g_player->getDuration();
            if (dur > 0) {
                int pos = (int)SendMessage(ctrl, TBM_GETPOS, 0, 0);
                double t = pos / 1000.0 * dur;
                g_player->seek(t);
                SetDlgItemTextW(hwnd, ID_ELAPSED, fmtDur(t).c_str());
            }
        }
        if (ctrl == GetDlgItem(hwnd, ID_SPEED_SLD)) {
            int pos = (int)SendMessage(ctrl, TBM_GETPOS, 0, 0);
            double sp = 0.25 + pos / 100.0 * 1.75;
            g_player->setSpeed(sp);
            wchar_t buf[16];
            swprintf_s(buf, L"%.2f×", sp); // ×
            SetDlgItemTextW(hwnd, ID_SPEED_LBL, buf);
        }
        break;
    }

    case WM_COMMAND:
        switch (LOWORD(wp)) {

        case ID_BROWSE: {
            wchar_t path[MAX_PATH] = {};
            OPENFILENAMEW ofn = {};
            ofn.lStructSize = sizeof(ofn);
            ofn.hwndOwner   = hwnd;
            ofn.lpstrFilter = L"MIDI Files\0*.mid;*.midi\0All Files\0*.*\0";
            ofn.lpstrFile   = path;
            ofn.nMaxFile    = MAX_PATH;
            ofn.Flags       = OFN_FILEMUSTEXIST;
            if (GetOpenFileNameW(&ofn)) {
                addFile(path);
                SetDlgItemTextW(hwnd, ID_SEARCH, L"");
                filterLibrary(L"");
            }
            break;
        }

        case ID_SEARCH:
            if (HIWORD(wp) == EN_CHANGE) {
                wchar_t buf[256] = {};
                GetDlgItemTextW(hwnd, ID_SEARCH, buf, 256);
                filterLibrary(buf);
            }
            break;

        case ID_LIBRARY:
            if (HIWORD(wp) == LBN_SELCHANGE)
                g_selected = (int)SendMessage(GetDlgItem(hwnd, ID_LIBRARY), LB_GETCURSEL, 0, 0);
            if (HIWORD(wp) == LBN_DBLCLK) {
                g_player->stop();
                KillTimer(hwnd, ID_TIMER_PROGRESS);
                startPlaying();
            }
            break;

        case ID_PLAYPAUSE:
            if (g_player->isRunning()) {
                if (g_player->isPaused()) {
                    g_player->resume();
                    SetDlgItemTextW(hwnd, ID_PLAYPAUSE, L"⏸");
                } else {
                    g_player->pause();
                    SetDlgItemTextW(hwnd, ID_PLAYPAUSE, L"▶");
                }
            } else {
                startPlaying();
            }
            break;

        case ID_STOP:
            KillTimer(hwnd, ID_TIMER_PROGRESS);
            KillTimer(hwnd, ID_TIMER_MIDI);
            g_player->stop();
            if (g_midiIn) { g_midiIn->closePort(); g_midiIn.reset(); }
            resetModifiers();
            SendMessage(GetDlgItem(hwnd, ID_PROGRESS), TBM_SETPOS, TRUE, 0);
            SetDlgItemTextW(hwnd, ID_ELAPSED,   L"0:00");
            SetDlgItemTextW(hwnd, ID_PLAYPAUSE, L"▶");
            setStatus(L"Stopped.");
            break;

        case ID_RESTART:
            if (g_player->isRunning()) g_player->seek(0.0);
            else startPlaying();
            break;

        case ID_REWIND:
            g_player->seek(std::max(0.0, g_player->getPosition() - 10.0));
            break;

        case ID_FWD: {
            double dur = g_player->getDuration();
            double pos = g_player->getPosition();
            g_player->seek(std::min(pos + 10.0, dur > 0 ? dur - 0.1 : pos + 10.0));
            break;
        }

        case ID_REFRESH:
            populateDevices();
            break;

        case ID_LIVE: {
            int idx = (int)SendMessage(GetDlgItem(hwnd, ID_DEVICE_CMB), CB_GETCURSEL, 0, 0);
            if (idx < 0) { setStatus(L"No MIDI device selected."); break; }
            try {
                g_midiIn = std::make_unique<RtMidiIn>();
                g_midiIn->openPort((unsigned int)idx);
                g_midiIn->ignoreTypes(true, true, true);
            } catch (const std::exception& e) {
                setStatus(std::string("Error: ") + e.what());
                g_midiIn.reset();
                break;
            }
            setStatus(L"Live — play your keyboard!");
            SetTimer(hwnd, ID_TIMER_MIDI, 8, nullptr);
            break;
        }

        } // switch LOWORD(wp)
        break;

    // ── Custom coloring ───────────────────────────────────────────────────────
    case WM_CTLCOLORSTATIC: {
        HDC dc = (HDC)wp;
        int id = GetDlgCtrlID((HWND)lp);
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, id == ID_SPEED_LBL ? RGB(0x27, 0xA8, 0xD4)
                       : id == ID_STATUS     ? RGB(0x88, 0x70, 0x60)
                                             : clrText());
        return (LRESULT)g_brWin;
    }

    case WM_CTLCOLOREDIT:
        SetTextColor((HDC)wp, clrText());
        SetBkColor((HDC)wp, clrInput());
        return (LRESULT)g_brInput;

    case WM_CTLCOLORLISTBOX:
        SetTextColor((HDC)wp, clrText());
        SetBkColor((HDC)wp, clrInput());
        return (LRESULT)g_brInput;

    case WM_CTLCOLORBTN:
        return (LRESULT)g_brWin;

    case WM_ERASEBKGND: {
        HDC dc = (HDC)wp;
        RECT rc;
        GetClientRect(hwnd, &rc);
        FillRect(dc, &rc, g_brWin);
        RECT bar = { 0, 0, rc.right, 52 };
        FillRect(dc, &bar, g_brBar);
        // separator line
        HPEN pen = CreatePen(PS_SOLID, 1, RGB(0xCC, 0xCC, 0xCC));
        HPEN old = (HPEN)SelectObject(dc, pen);
        MoveToEx(dc, 20, 52, nullptr);
        LineTo(dc, rc.right - 20, 52);
        SelectObject(dc, old);
        DeleteObject(pen);
        return 1;
    }

    case WM_DESTROY:
        KillTimer(hwnd, ID_TIMER_PROGRESS);
        KillTimer(hwnd, ID_TIMER_MIDI);
        g_player->stop();
        PostQuitMessage(0);
        break;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

// ─── Build controls ───────────────────────────────────────────────────────────

static void createControls(HWND hwnd) {
    g_fontTitle = CreateFontW(-18, 0, 0, 0, FW_BOLD,   0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
    g_fontBold  = CreateFontW(-13, 0, 0, 0, FW_SEMIBOLD,0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
    g_fontNorm  = CreateFontW(-12, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
    g_fontSm    = CreateFontW(-10, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");

    // Title bar area
    HWND hTit = mkStatic(hwnd, L"Snuffiano", 20, 14, 200, 26, 0, SS_LEFT);
    setFont(hTit, g_fontTitle);
    HWND hCred = mkStatic(hwnd, L"Created by ZenPhyx", 20, 18, 420, 14, 0, SS_RIGHT);
    setFont(hCred, g_fontSm);

    // Browse
    HWND hBrowse = mkBtn(hwnd, L"\U0001F4C2  Browse MIDI File…", 20, 60, 420, 36, ID_BROWSE);
    setFont(hBrowse, g_fontBold);

    // Library header
    HWND hLibHdr = mkStatic(hwnd, L"LIBRARY", 20, 104, 80, 14, 0, SS_LEFT);
    setFont(hLibHdr, g_fontSm);

    // Search
    HWND hSearch = CreateWindowW(L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
        20, 122, 420, 24, hwnd, (HMENU)ID_SEARCH, nullptr, nullptr);
    SendMessage(hSearch, EM_SETCUEBANNER, 0, (LPARAM)L"Search songs…");
    setFont(hSearch, g_fontNorm);

    // Library list
    HWND hLib = CreateWindowW(L"LISTBOX", L"",
        WS_CHILD | WS_VISIBLE | WS_BORDER | WS_VSCROLL | LBS_NOTIFY,
        20, 150, 420, 130, hwnd, (HMENU)ID_LIBRARY, nullptr, nullptr);
    setFont(hLib, g_fontNorm);

    // Progress times
    HWND hEl = mkStatic(hwnd, L"0:00", 20, 287, 50, 14, ID_ELAPSED, SS_LEFT);
    setFont(hEl, g_fontSm);
    HWND hTot = mkStatic(hwnd, L"0:00", 390, 287, 50, 14, ID_TOTAL, SS_RIGHT);
    setFont(hTot, g_fontSm);

    // Progress slider
    mkTrack(hwnd, 20, 303, 420, 22, ID_PROGRESS, 0, 1000, 0);

    // Transport
    int ty = 332;
    HWND hRes = mkBtn(hwnd, L"↩", 20,  ty, 52, 34, ID_RESTART);   setFont(hRes, g_fontBold);
    HWND hRew = mkBtn(hwnd, L"⟨⟨", 76, ty, 60, 34, ID_REWIND); setFont(hRew, g_fontBold);
    HWND hPP  = mkBtn(hwnd, L"▶", 148, ty-4, 72, 42, ID_PLAYPAUSE); setFont(hPP,  g_fontTitle);
    HWND hFwd = mkBtn(hwnd, L"⟩⟩", 224, ty, 60, 34, ID_FWD);   setFont(hFwd, g_fontBold);
    HWND hStp = mkBtn(hwnd, L"■", 288, ty, 52, 34, ID_STOP);        setFont(hStp, g_fontBold);
    (void)hRes; (void)hRew; (void)hPP; (void)hFwd; (void)hStp;

    // Speed row
    HWND hSpLbl = mkStatic(hwnd, L"Speed", 20, 385, 46, 14, 0, SS_LEFT);
    setFont(hSpLbl, g_fontSm);
    mkTrack(hwnd, 70, 381, 310, 22, ID_SPEED_SLD, 0, 100, 43);
    HWND hSpVal = mkStatic(hwnd, L"1.00×", 384, 385, 56, 14, ID_SPEED_LBL, SS_RIGHT);
    setFont(hSpVal, g_fontSm);

    // "or" label
    HWND hOr = mkStatic(hwnd, L"— or use a MIDI keyboard —", 20, 415, 420, 14, 0);
    setFont(hOr, g_fontSm);

    // Device row
    HWND hDLbl = mkStatic(hwnd, L"MIDI Keyboard:", 20, 437, 104, 18, 0, SS_LEFT);
    setFont(hDLbl, g_fontNorm);
    HWND hDev = CreateWindowW(L"COMBOBOX", L"",
        WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
        128, 435, 262, 200, hwnd, (HMENU)ID_DEVICE_CMB, nullptr, nullptr);
    setFont(hDev, g_fontNorm);
    HWND hRef = mkBtn(hwnd, L"↺", 394, 435, 46, 24, ID_REFRESH);
    setFont(hRef, g_fontNorm);

    // Live mode
    HWND hLive = mkBtn(hwnd, L"\U0001F3B9  Live Mode", 20, 467, 420, 38, ID_LIVE);
    setFont(hLive, g_fontBold);

    // Status
    HWND hSt = mkStatic(hwnd, L"Drop a MIDI file or pick one from your library", 20, 513, 420, 16, ID_STATUS);
    setFont(hSt, g_fontNorm);

    // A11y note
    HWND hA = mkStatic(hwnd, L"Accessibility: run Snuffiano.exe as administrator if keys don't inject", 20, 533, 420, 14, 0);
    setFont(hA, g_fontSm);
}

// ─── WinMain ──────────────────────────────────────────────────────────────────

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int) {
    INITCOMMONCONTROLSEX icc = { sizeof(icc), ICC_BAR_CLASSES };
    InitCommonControlsEx(&icc);

    g_player = std::make_unique<MIDIPlayer>();
    loadLibrary();
    updateBrushes();

    WNDCLASSW wc    = {};
    wc.lpfnWndProc  = WndProc;
    wc.hInstance    = hInst;
    wc.lpszClassName = L"SnuffianoWnd";
    wc.hCursor      = LoadCursor(nullptr, IDC_ARROW);
    wc.hIcon        = LoadIcon(hInst, MAKEINTRESOURCE(1));
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    RegisterClassW(&wc);

    g_hwnd = CreateWindowExW(0, L"SnuffianoWnd", L"Snuffiano",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, 472, 580,
        nullptr, nullptr, hInst, nullptr);

    createControls(g_hwnd);
    populateDevices();
    filterLibrary(L"");

    ShowWindow(g_hwnd, SW_SHOWNORMAL);
    UpdateWindow(g_hwnd);

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    DeleteObject(g_brWin); DeleteObject(g_brBar); DeleteObject(g_brInput);
    DeleteObject(g_fontTitle); DeleteObject(g_fontBold);
    DeleteObject(g_fontNorm);  DeleteObject(g_fontSm);
    return 0;
}
