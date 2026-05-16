#define WIN32_LEAN_AND_MEAN
#define UNICODE
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
#include <sstream>
#include <cmath>

#include "MIDIPlayer.hpp"
#include "RobloxKeyMapper.hpp"
#include "InputInjector.hpp"
#include "RtMidi.h"

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "comdlg32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "winmm.lib")

// ─── Colors ───────────────────────────────────────────────────────────────────

static bool g_dark = false;

static COLORREF clrWin()    { return g_dark ? 0x100E1C : 0xE3EFF5; }
static COLORREF clrBar()    { return g_dark ? 0x08090E : 0xD5E5ED; }
static COLORREF clrText()   { return g_dark ? 0xDCE8F0 : 0x100E1C; }
static COLORREF clrDim()    { return g_dark ? 0x667080 : 0x887060; }
static COLORREF clrInput()  { return g_dark ? 0x1A1F28 : 0xF0FAFF; }
static COLORREF clrRed()    { return 0x2A35C8; } // BGR for #C8352A
static COLORREF clrGold()   { return 0x27A8D4; } // BGR for #D4A827

// ─── Control IDs ──────────────────────────────────────────────────────────────

enum {
    ID_BROWSE=101, ID_SEARCH, ID_LIBRARY,
    ID_ELAPSED, ID_TOTAL, ID_PROGRESS,
    ID_RESTART, ID_REWIND, ID_PLAYPAUSE, ID_FWD, ID_STOP,
    ID_SPEED_SLD, ID_SPEED_LBL,
    ID_DEVICE_CMB, ID_REFRESH, ID_LIVE,
    ID_STATUS, ID_TIMER=200
};

// ─── Globals ──────────────────────────────────────────────────────────────────

static HWND g_hwnd;
static HBRUSH g_brWin, g_brBar, g_brInput;
static HFONT g_fontTitle, g_fontBold, g_fontNorm, g_fontSm;

static std::unique_ptr<MIDIPlayer>  g_player;
static std::unique_ptr<RtMidiIn>    g_midiIn;
static std::vector<unsigned char>   g_midiMsg;
static bool                         g_playing = false;

struct LibEntry { std::wstring path, name, duration; };
static std::vector<LibEntry> g_library;
static std::vector<LibEntry*> g_filtered;
static int g_selected = -1;

// ─── Library persistence ──────────────────────────────────────────────────────

static std::wstring libPath() {
    wchar_t buf[MAX_PATH];
    SHGetFolderPathW(nullptr, CSIDL_APPDATA, nullptr, 0, buf);
    std::wstring dir = std::wstring(buf) + L"\\Snuffiano";
    CreateDirectoryW(dir.c_str(), nullptr);
    return dir + L"\\library.txt";
}

static void saveLibrary() {
    std::wofstream f(libPath());
    for (auto& e : g_library) f << e.path << L"\n";
}

static std::wstring toWide(const std::string& s) {
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    std::wstring w(n, 0); MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, w.data(), n);
    return w;
}
static std::string toUtf8(const std::wstring& w) {
    int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string s(n, 0); WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, s.data(), n, nullptr, nullptr);
    return s;
}

static std::wstring fmtDur(double s) {
    int t = (int)s;
    wchar_t buf[16]; swprintf_s(buf, L"%d:%02d", t/60, t%60);
    return buf;
}

static void loadLibrary() {
    std::wifstream f(libPath());
    std::wstring line;
    while (std::getline(f, line)) {
        if (line.empty()) continue;
        double dur = MIDIPlayer::fileDuration(toUtf8(line));
        wchar_t* fn = PathFindFileNameW(line.c_str());
        std::wstring name = fn;
        size_t dot = name.rfind(L'.'); if (dot != std::wstring::npos) name = name.substr(0, dot);
        g_library.push_back({line, name, dur > 0 ? fmtDur(dur) : L"--:--"});
    }
}

static void addFile(const std::wstring& path) {
    for (auto& e : g_library) if (e.path == path) return;
    double dur = MIDIPlayer::fileDuration(toUtf8(path));
    wchar_t* fn = PathFindFileNameW(path.c_str());
    std::wstring name = fn;
    size_t dot = name.rfind(L'.'); if (dot != std::wstring::npos) name = name.substr(0, dot);
    g_library.insert(g_library.begin(), {path, name, dur > 0 ? fmtDur(dur) : L"--:--"});
    saveLibrary();
}

static void filterLibrary(const std::wstring& q) {
    g_filtered.clear();
    std::wstring ql = q;
    std::transform(ql.begin(), ql.end(), ql.begin(), ::towlower);
    for (auto& e : g_library) {
        std::wstring nl = e.name; std::transform(nl.begin(), nl.end(), nl.begin(), ::towlower);
        if (ql.empty() || nl.find(ql) != std::wstring::npos)
            g_filtered.push_back(&e);
    }
    HWND lb = GetDlgItem(g_hwnd, ID_LIBRARY);
    SendMessage(lb, LB_RESETCONTENT, 0, 0);
    for (auto* e : g_filtered) SendMessage(lb, LB_ADDSTRING, 0, (LPARAM)e->name.c_str());
    wchar_t buf[64]; swprintf_s(buf, L"%zu song%s", g_filtered.size(), g_filtered.size()==1?L"":L"s");
    SetDlgItemTextW(g_hwnd, ID_STATUS, buf);
}

// ─── Helpers ──────────────────────────────────────────────────────────────────

static void setStatus(const std::wstring& msg) { SetDlgItemTextW(g_hwnd, ID_STATUS, msg.c_str()); }
static void setStatus(const std::string& msg)  { setStatus(toWide(msg)); }

static void updateBrushes() {
    if (g_brWin)   DeleteObject(g_brWin);
    if (g_brBar)   DeleteObject(g_brBar);
    if (g_brInput) DeleteObject(g_brInput);
    g_brWin   = CreateSolidBrush(clrWin());
    g_brBar   = CreateSolidBrush(clrBar());
    g_brInput = CreateSolidBrush(clrInput());
}

static void populateDevices() {
    HWND cb = GetDlgItem(g_hwnd, ID_DEVICE_CMB);
    SendMessage(cb, CB_RESETCONTENT, 0, 0);
    try {
        RtMidiIn midi; unsigned int n = midi.getPortCount();
        if (!n) SendMessage(cb, CB_ADDSTRING, 0, (LPARAM)L"No MIDI devices found");
        else for (unsigned int i=0; i<n; ++i)
            SendMessage(cb, CB_ADDSTRING, 0, (LPARAM)toWide(midi.getPortName(i)).c_str());
    } catch (...) { SendMessage(cb, CB_ADDSTRING, 0, (LPARAM)L"Error listing devices"); }
    SendMessage(cb, CB_SETCURSEL, 0, 0);
}

// ─── Playback ─────────────────────────────────────────────────────────────────

static void startPlaying() {
    if (g_selected < 0 || g_selected >= (int)g_filtered.size()) {
        setStatus(L"Select a song from the library first."); return;
    }
    std::string path = toUtf8(g_filtered[g_selected]->path);
    double dur = MIDIPlayer::fileDuration(path);
    wchar_t buf[32]; swprintf_s(buf, L"%s", fmtDur(dur).c_str());
    SetDlgItemTextW(g_hwnd, ID_TOTAL, buf);

    HWND pb = GetDlgItem(g_hwnd, ID_PROGRESS);
    SendMessage(pb, TBM_SETPOS, TRUE, 0);

    g_player->playFile(path,
        [](char key, bool press){ press ? pressKey(key) : releaseKey(key); },
        [](const std::string& msg){ PostMessage(g_hwnd, WM_USER+1, 0, (LPARAM)new std::wstring(toWide(msg))); },
        [](){
            PostMessage(g_hwnd, WM_USER+2, 0, 0);
        }
    );
    SetDlgItemTextW(g_hwnd, ID_PLAYPAUSE, L"⏸");
    g_playing = true;
    SetTimer(g_hwnd, ID_TIMER, 100, nullptr);
}

// ─── Window procedure ─────────────────────────────────────────────────────────

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {

    case WM_CREATE:
        DragAcceptFiles(hwnd, TRUE);
        break;

    case WM_TIMER:
        if (!g_player->isRunning()) break;
        {
            double pos = g_player->getPosition(), dur = g_player->getDuration();
            if (dur > 0) {
                HWND pb = GetDlgItem(hwnd, ID_PROGRESS);
                SendMessage(pb, TBM_SETPOS, TRUE, (LPARAM)(int)(pos/dur*1000));
                SetDlgItemTextW(hwnd, ID_ELAPSED, fmtDur(pos).c_str());
            }
        }
        break;

    case WM_USER+1: // status from player thread
        {
            auto* s = (std::wstring*)lp;
            setStatus(*s); delete s;
        }
        break;

    case WM_USER+2: // playback done
        KillTimer(hwnd, ID_TIMER);
        SendMessage(GetDlgItem(hwnd, ID_PROGRESS), TBM_SETPOS, TRUE, 0);
        SetDlgItemTextW(hwnd, ID_ELAPSED, L"0:00");
        SetDlgItemTextW(hwnd, ID_PLAYPAUSE, L"▶");
        g_playing = false;
        break;

    case WM_DROPFILES:
        {
            HDROP hDrop = (HDROP)wp;
            wchar_t path[MAX_PATH];
            if (DragQueryFileW(hDrop, 0, path, MAX_PATH)) {
                addFile(path); filterLibrary(L"");
                SetDlgItemTextW(hwnd, ID_SEARCH, L"");
            }
            DragFinish(hDrop);
        }
        break;

    case WM_HSCROLL:
        {
            HWND ctrl = (HWND)lp;
            if (ctrl == GetDlgItem(hwnd, ID_PROGRESS) && g_player->getDuration() > 0) {
                int pos = (int)SendMessage(ctrl, TBM_GETPOS, 0, 0);
                double t = pos / 1000.0 * g_player->getDuration();
                g_player->seek(t);
                SetDlgItemTextW(hwnd, ID_ELAPSED, fmtDur(t).c_str());
            }
            if (ctrl == GetDlgItem(hwnd, ID_SPEED_SLD)) {
                int pos = (int)SendMessage(ctrl, TBM_GETPOS, 0, 0);
                double sp = 0.25 + pos / 100.0 * 1.75;
                g_player->setSpeed(sp);
                wchar_t buf[16]; swprintf_s(buf, L"%.2f×", sp);
                SetDlgItemTextW(hwnd, ID_SPEED_LBL, buf);
            }
        }
        break;

    case WM_COMMAND:
        switch (LOWORD(wp)) {
        case ID_BROWSE:
            {
                wchar_t path[MAX_PATH] = {};
                OPENFILENAMEW ofn = {};
                ofn.lStructSize = sizeof(ofn);
                ofn.hwndOwner   = hwnd;
                ofn.lpstrFilter = L"MIDI Files\0*.mid;*.midi\0All Files\0*.*\0";
                ofn.lpstrFile   = path;
                ofn.nMaxFile    = MAX_PATH;
                ofn.Flags       = OFN_FILEMUSTEXIST;
                if (GetOpenFileNameW(&ofn)) {
                    addFile(path); filterLibrary(L"");
                    SetDlgItemTextW(hwnd, ID_SEARCH, L"");
                }
            }
            break;

        case ID_SEARCH:
            if (HIWORD(wp) == EN_CHANGE) {
                wchar_t buf[256]; GetDlgItemTextW(hwnd, ID_SEARCH, buf, 256);
                filterLibrary(buf);
            }
            break;

        case ID_LIBRARY:
            if (HIWORD(wp) == LBN_SELCHANGE)
                g_selected = (int)SendMessage(GetDlgItem(hwnd, ID_LIBRARY), LB_GETCURSEL, 0, 0);
            if (HIWORD(wp) == LBN_DBLCLK) { g_player->stop(); startPlaying(); }
            break;

        case ID_PLAYPAUSE:
            if (g_player->isRunning()) {
                if (g_player->isPaused()) { g_player->resume(); SetDlgItemTextW(hwnd, ID_PLAYPAUSE, L"⏸"); }
                else                      { g_player->pause();  SetDlgItemTextW(hwnd, ID_PLAYPAUSE, L"▶"); }
            } else startPlaying();
            break;

        case ID_STOP:
            KillTimer(hwnd, ID_TIMER);
            g_player->stop();
            if (g_midiIn) { g_midiIn->closePort(); g_midiIn.reset(); }
            resetModifiers();
            SendMessage(GetDlgItem(hwnd, ID_PROGRESS), TBM_SETPOS, TRUE, 0);
            SetDlgItemTextW(hwnd, ID_ELAPSED, L"0:00");
            SetDlgItemTextW(hwnd, ID_PLAYPAUSE, L"▶");
            g_playing = false;
            setStatus(L"Stopped.");
            break;

        case ID_RESTART:
            if (g_player->isRunning()) g_player->seek(0.0);
            else startPlaying();
            break;

        case ID_REWIND:
            g_player->seek(std::max(0.0, g_player->getPosition() - 10.0));
            break;

        case ID_FWD:
            {
                double dur = g_player->getDuration(), pos = g_player->getPosition();
                g_player->seek(std::min(pos + 10.0, dur > 0 ? dur - 0.1 : pos + 10.0));
            }
            break;

        case ID_REFRESH:
            populateDevices();
            break;

        case ID_LIVE:
            {
                int idx = (int)SendMessage(GetDlgItem(hwnd, ID_DEVICE_CMB), CB_GETCURSEL, 0, 0);
                if (idx < 0) { setStatus(L"No MIDI device selected."); break; }
                try {
                    g_midiIn = std::make_unique<RtMidiIn>();
                    g_midiIn->openPort((unsigned int)idx);
                    g_midiIn->ignoreTypes(true, true, true);
                } catch (const std::exception& e) {
                    setStatus(std::string("Error: ") + e.what()); g_midiIn.reset(); break;
                }
                setStatus(L"Live — play your keyboard!");
                SetTimer(hwnd, ID_TIMER+1, 8, nullptr);
            }
            break;
        }
        break;

    case WM_TIMER+1: // MIDI poll (ID_TIMER+1 = 201)
    case 201:
        if (g_midiIn) {
            g_midiIn->getMessage(&g_midiMsg);
            if (g_midiMsg.size() >= 3) {
                uint8_t type=g_midiMsg[0]&0xF0, note=g_midiMsg[1], vel=g_midiMsg[2];
                auto key = RobloxKeyMapper::map((int)note);
                if (key) {
                    if (type==0x90 && vel>0) pressKey(*key);
                    else if (type==0x80||(type==0x90&&vel==0)) releaseKey(*key);
                }
            }
        }
        break;

    // ── Custom colors ──────────────────────────────────────────────────────
    case WM_CTLCOLORSTATIC:
        {
            HDC dc = (HDC)wp; HWND ctrl = (HWND)lp;
            int id = GetDlgCtrlID(ctrl);
            SetBkMode(dc, TRANSPARENT);
            if (id == ID_SPEED_LBL) SetTextColor(dc, clrGold());
            else SetTextColor(dc, id==ID_STATUS ? clrDim() : clrText());
            return (LRESULT)g_brWin;
        }

    case WM_CTLCOLOREDIT:
        {
            HDC dc = (HDC)wp;
            SetTextColor(dc, clrText());
            SetBkColor(dc, clrInput());
            return (LRESULT)g_brInput;
        }

    case WM_CTLCOLORLISTBOX:
        {
            HDC dc = (HDC)wp;
            SetTextColor(dc, clrText());
            SetBkColor(dc, clrInput());
            return (LRESULT)g_brInput;
        }

    case WM_CTLCOLORBTN:
        return (LRESULT)g_brWin;

    case WM_ERASEBKGND:
        {
            HDC dc = (HDC)wp; RECT r; GetClientRect(hwnd, &r);
            FillRect(dc, &r, g_brWin);
            // Title bar strip
            RECT bar = {0, 0, r.right, 52};
            FillRect(dc, &bar, g_brBar);
            return 1;
        }

    case WM_PAINT:
        {
            PAINTSTRUCT ps; HDC dc = BeginPaint(hwnd, &ps);
            // Separator under title bar
            RECT sep = {20, 53, r: 440, 54};
            HPEN pen = CreatePen(PS_SOLID, 1, g_dark ? 0x222222 : 0xCCCCCC);
            HPEN old = (HPEN)SelectObject(dc, pen);
            MoveToEx(dc, 20, 53, nullptr); LineTo(dc, 440, 53);
            SelectObject(dc, old); DeleteObject(pen);
            EndPaint(hwnd, &ps);
        }
        break;

    case WM_DESTROY:
        KillTimer(hwnd, ID_TIMER); KillTimer(hwnd, 201);
        g_player->stop();
        PostQuitMessage(0);
        break;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

// ─── Create controls ──────────────────────────────────────────────────────────

static HWND mkBtn(HWND p, const wchar_t* t, int x, int y, int w, int h, int id) {
    return CreateWindowW(L"BUTTON", t, WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON, x,y,w,h, p,(HMENU)(INT_PTR)id, nullptr, nullptr);
}
static HWND mkStatic(HWND p, const wchar_t* t, int x, int y, int w, int h, int id, DWORD style=0) {
    return CreateWindowW(L"STATIC", t, WS_CHILD|WS_VISIBLE|SS_CENTER|style, x,y,w,h, p,(HMENU)(INT_PTR)id, nullptr, nullptr);
}
static HWND mkTrack(HWND p, int x, int y, int w, int h, int id, int mn, int mx, int val) {
    HWND tb = CreateWindowW(TRACKBAR_CLASSW, L"", WS_CHILD|WS_VISIBLE|TBS_HORZ|TBS_NOTICKS, x,y,w,h, p,(HMENU)(INT_PTR)id, nullptr, nullptr);
    SendMessage(tb, TBM_SETRANGE, TRUE, MAKELPARAM(mn, mx));
    SendMessage(tb, TBM_SETPOS,   TRUE, val);
    return tb;
}

static void createControls(HWND hwnd) {
    // Fonts
    g_fontTitle = CreateFontW(-18,0,0,0,FW_BOLD,0,0,0,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Segoe UI");
    g_fontBold  = CreateFontW(-13,0,0,0,FW_SEMIBOLD,0,0,0,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Segoe UI");
    g_fontNorm  = CreateFontW(-12,0,0,0,FW_NORMAL,0,0,0,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Segoe UI");
    g_fontSm    = CreateFontW(-10,0,0,0,FW_NORMAL,0,0,0,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Segoe UI");

    auto setFont = [](HWND h, HFONT f){ SendMessage(h,WM_SETFONT,(WPARAM)f,TRUE); };

    // Title bar
    HWND hTitle = mkStatic(hwnd, L"Snuffiano", 66, 14, 180, 26, 0, SS_LEFT);
    setFont(hTitle, g_fontTitle);
    HWND hCredit = mkStatic(hwnd, L"Created by ZenPhyx", 20, 18, 380, 14, 0, SS_RIGHT);
    setFont(hCredit, g_fontSm);

    // Separator + drop
    HWND hBrowse = mkBtn(hwnd, L"📂  Browse MIDI File…", 20, 62, 420, 36, ID_BROWSE);
    setFont(hBrowse, g_fontBold);

    // Library header
    HWND hLibLbl = mkStatic(hwnd, L"LIBRARY", 20, 106, 80, 14, 0, SS_LEFT);
    setFont(hLibLbl, g_fontSm);
    HWND hCount  = mkStatic(hwnd, L"0 songs", 20, 106, 420, 14, ID_STATUS, SS_RIGHT);
    setFont(hCount, g_fontSm);

    // Search
    HWND hSearch = CreateWindowW(L"EDIT", L"", WS_CHILD|WS_VISIBLE|WS_BORDER|ES_AUTOHSCROLL,
        20, 124, 420, 24, hwnd, (HMENU)ID_SEARCH, nullptr, nullptr);
    SendMessage(hSearch, EM_SETCUEBANNER, 0, (LPARAM)L"Search songs…");
    setFont(hSearch, g_fontNorm);

    // Library list
    HWND hLib = CreateWindowW(L"LISTBOX", L"", WS_CHILD|WS_VISIBLE|WS_BORDER|WS_VSCROLL|LBS_NOTIFY,
        20, 152, 420, 130, hwnd, (HMENU)ID_LIBRARY, nullptr, nullptr);
    setFont(hLib, g_fontNorm);

    // Progress
    mkStatic(hwnd, L"0:00", 20, 288, 50, 14, ID_ELAPSED, SS_LEFT);
    setFont(GetDlgItem(hwnd,ID_ELAPSED), g_fontSm);
    mkStatic(hwnd, L"0:00", 370, 288, 70, 14, ID_TOTAL, SS_RIGHT);
    setFont(GetDlgItem(hwnd,ID_TOTAL), g_fontSm);
    mkTrack(hwnd, 20, 304, 420, 22, ID_PROGRESS, 0, 1000, 0);

    // Transport
    int ty = 332;
    mkBtn(hwnd, L"↩",  20,  ty, 52, 34, ID_RESTART);
    mkBtn(hwnd, L"⟨⟨", 76,  ty, 60, 34, ID_REWIND);
    HWND hPlay = mkBtn(hwnd, L"▶", 148, ty-4, 72, 42, ID_PLAYPAUSE);
    setFont(hPlay, g_fontTitle);
    mkBtn(hwnd, L"⟩⟩", 224, ty, 60, 34, ID_FWD);
    mkBtn(hwnd, L"■",  288, ty, 52, 34, ID_STOP);

    for (int id : {ID_RESTART,ID_REWIND,ID_FWD,ID_STOP})
        setFont(GetDlgItem(hwnd,id), g_fontBold);

    // Speed
    HWND hSpLbl = mkStatic(hwnd, L"Speed", 20, 386, 46, 14, 0, SS_LEFT);
    setFont(hSpLbl, g_fontSm);
    mkTrack(hwnd, 70, 382, 310, 22, ID_SPEED_SLD, 0, 100, 43); // 43 ≈ 1.0×
    HWND hSpVal = mkStatic(hwnd, L"1.00×", 384, 386, 56, 14, ID_SPEED_LBL, SS_RIGHT);
    setFont(hSpVal, g_fontSm);

    // Separator + live section
    HWND hOr = mkStatic(hwnd, L"— or use a MIDI keyboard —", 20, 416, 420, 14, 0);
    setFont(hOr, g_fontSm);

    HWND hDevLbl = mkStatic(hwnd, L"MIDI Keyboard:", 20, 438, 104, 18, 0, SS_LEFT);
    setFont(hDevLbl, g_fontNorm);
    HWND hDev = CreateWindowW(L"COMBOBOX", L"", WS_CHILD|WS_VISIBLE|CBS_DROPDOWNLIST|WS_VSCROLL,
        128, 436, 264, 200, hwnd, (HMENU)ID_DEVICE_CMB, nullptr, nullptr);
    setFont(hDev, g_fontNorm);
    HWND hRef = mkBtn(hwnd, L"↺", 396, 436, 44, 24, ID_REFRESH);
    setFont(hRef, g_fontNorm);

    HWND hLive = mkBtn(hwnd, L"🎹  Live Mode", 20, 468, 420, 38, ID_LIVE);
    setFont(hLive, g_fontBold);

    HWND hSt = mkStatic(hwnd, L"Drop a MIDI file or pick one from your library", 20, 514, 420, 16, ID_STATUS);
    setFont(hSt, g_fontNorm);

    HWND hA11y = mkStatic(hwnd, L"Accessibility: run as administrator or grant input injection rights", 20, 534, 420, 14, 0);
    setFont(hA11y, g_fontSm);
}

// ─── WinMain ──────────────────────────────────────────────────────────────────

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int) {
    INITCOMMONCONTROLSEX icc = {sizeof(icc), ICC_BAR_CLASSES};
    InitCommonControlsEx(&icc);

    g_player = std::make_unique<MIDIPlayer>();
    loadLibrary();
    updateBrushes();

    WNDCLASSW wc = {};
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInst;
    wc.lpszClassName = L"SnuffianoWnd";
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW+1);
    wc.hIcon         = LoadIcon(hInst, MAKEINTRESOURCE(1)); // optional app icon resource
    wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);
    RegisterClassW(&wc);

    g_hwnd = CreateWindowExW(0, L"SnuffianoWnd", L"Snuffiano",
        WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, 460, 580,
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
    return 0;
}
