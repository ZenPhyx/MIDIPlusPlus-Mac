#ifndef UNICODE
#define UNICODE
#endif
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <wrl.h>
#include <wrl/event.h>
#include <WebView2.h>
#include <commdlg.h>
#include <shellapi.h>
#include <shlobj.h>
#include <string>
#include <vector>
#include <algorithm>
#include <memory>
#include <fstream>
#include <cmath>

#include "MIDIPlayer.hpp"
#include "RobloxKeyMapper.hpp"
#include "InputInjector.hpp"
#include "NtUserInput.h"
#include "RtMidi.h"

#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "comdlg32.lib")
#pragma comment(lib, "version.lib")

using namespace Microsoft::WRL;

// ─── Embedded HTML UI ────────────────────────────────────────────────────────
// The complete Snuffiano UI — same design as the macOS version.
static const char HTML_UI[] = R"HTML(<!DOCTYPE html>
<html lang="en"><head><meta charset="UTF-8"><style>
*{margin:0;padding:0;box-sizing:border-box}
:root{--bg:#F5EFE3;--bar:#EDE5D5;--pri:#1C1410;--sec:rgba(28,20,16,.5);--dim:rgba(28,20,16,.28);--sep:rgba(0,0,0,.09);--inp:rgba(255,255,255,.6);--lib:rgba(255,255,255,.45);--btn:rgba(255,255,255,.6);--act:rgba(200,53,42,.11);--red:#C8352A;--gold:#D4A827}
.dark{--bg:#1C1610;--bar:#140E08;--pri:#F0E8DC;--sec:rgba(240,232,220,.45);--dim:rgba(240,232,220,.22);--sep:rgba(255,255,255,.08);--inp:rgba(255,255,255,.07);--lib:rgba(255,255,255,.04);--btn:rgba(255,255,255,.08);--act:rgba(200,53,42,.22)}
body{font-family:"Segoe UI",system-ui,sans-serif;background:var(--bg);width:460px;overflow-x:hidden;transition:background .25s}
.bar{background:var(--bar);border-bottom:1px solid var(--sep);padding:4px 20px 6px;display:flex;align-items:center;gap:8px;position:relative;transition:background .25s,border-color .25s}
.bar-icon{width:40px;height:40px;border-radius:9px;border:2px solid var(--red);object-fit:cover;flex-shrink:0}
.bar-title{font-size:18px;font-weight:700;color:var(--pri);letter-spacing:-.3px;transition:color .25s}
.bar-credit{position:absolute;right:50px;font-size:10px;color:var(--dim);transition:color .25s}
.dark-btn{position:absolute;right:20px;width:26px;height:20px;background:rgba(0,0,0,.07);border:1px solid var(--sep);border-radius:5px;cursor:pointer;display:flex;align-items:center;justify-content:center;font-size:12px;user-select:none}
.dark .dark-btn{background:rgba(255,255,255,.08)}
.c{padding:8px 20px 14px}
.sep{height:1px;background:var(--sep);margin-bottom:8px;transition:background .25s}
.drop{height:70px;background:var(--inp);border:1.5px dashed var(--sep);border-radius:10px;display:flex;align-items:center;justify-content:center;gap:12px;margin-bottom:6px;cursor:pointer;transition:background .15s}
.drop:hover{filter:brightness(1.05)}
.drop-main{font-size:13px;font-weight:600;color:var(--pri);transition:color .25s}
.drop-sub{font-size:11px;color:var(--sec);margin-top:2px;transition:color .25s}
.lib-hdr{display:flex;align-items:center;justify-content:space-between;margin-bottom:5px}
.lbl-sm{font-size:10.5px;font-weight:600;text-transform:uppercase;letter-spacing:.5px;color:var(--sec);transition:color .25s}
.lib-cnt{font-size:10px;color:var(--dim);transition:color .25s}
.search{width:100%;height:26px;background:var(--inp);border:1px solid var(--sep);border-radius:7px;padding:0 10px 0 26px;font-size:12.5px;color:var(--pri);outline:none;margin-bottom:5px;background-image:url("data:image/svg+xml,%3Csvg xmlns='http://www.w3.org/2000/svg' width='12' height='12' viewBox='0 0 24 24' fill='none' stroke='%23999' stroke-width='2.2' stroke-linecap='round' stroke-linejoin='round'%3E%3Ccircle cx='11' cy='11' r='8'/%3E%3Cline x1='21' y1='21' x2='16.65' y2='16.65'/%3E%3C/svg%3E");background-repeat:no-repeat;background-position:7px center;transition:background-color .25s,border-color .25s,color .25s}
.search:focus{border-color:rgba(200,53,42,.45);box-shadow:0 0 0 2px rgba(200,53,42,.1)}
.lib{background:var(--lib);border:1px solid var(--sep);border-radius:10px;overflow:hidden;margin-bottom:10px;max-height:130px;overflow-y:auto;transition:background .25s,border-color .25s}
.lib::-webkit-scrollbar{width:4px}.lib::-webkit-scrollbar-thumb{background:rgba(0,0,0,.15);border-radius:2px}
.dark .lib::-webkit-scrollbar-thumb{background:rgba(255,255,255,.12)}
.li{display:flex;align-items:center;height:30px;padding:0 10px;gap:8px;cursor:pointer;border-bottom:1px solid var(--sep);transition:background .1s}
.li:last-child{border-bottom:none}.li:hover{background:rgba(200,53,42,.06)}.li.sel{background:var(--act)}
.li-n{font-size:10px;color:var(--dim);width:16px;text-align:right;flex-shrink:0}
.li-ic{font-size:13px;flex-shrink:0}
.li-nm{flex:1;font-size:12.5px;font-weight:500;color:var(--pri);white-space:nowrap;overflow:hidden;text-overflow:ellipsis}
.li-d{font-size:10.5px;color:var(--sec);flex-shrink:0;font-variant-numeric:tabular-nums}
.li-x{font-size:11px;color:var(--dim);flex-shrink:0;cursor:pointer;padding:2px 4px;border-radius:3px}
.li-x:hover{color:var(--red);background:rgba(200,53,42,.08)}
.lib-empty{padding:16px;text-align:center;font-size:12px;color:var(--sec)}
.pt{display:flex;justify-content:space-between;font-size:11px;color:var(--sec);margin-bottom:4px;font-variant-numeric:tabular-nums;transition:color .25s}
input[type=range]{-webkit-appearance:none;appearance:none;width:100%;height:4px;border-radius:2px;outline:none;cursor:pointer}
.pb::-webkit-slider-thumb{-webkit-appearance:none;width:14px;height:14px;border-radius:50%;background:var(--red);box-shadow:0 1px 4px rgba(200,53,42,.4);cursor:pointer}
.sb::-webkit-slider-thumb{-webkit-appearance:none;width:14px;height:14px;border-radius:50%;background:var(--gold);box-shadow:0 1px 4px rgba(212,168,39,.4);cursor:pointer}
.pw{margin-bottom:12px}
.trans{display:flex;align-items:center;justify-content:center;gap:8px;margin-bottom:8px}
.tb{border:none;border-radius:8px;cursor:pointer;display:flex;align-items:center;justify-content:center;font-weight:600;background:var(--btn);border:1px solid var(--sep);color:var(--pri);transition:filter .12s,background .25s,border-color .25s,color .25s}
.tb:hover{filter:brightness(.9)}.tb:active{transform:scale(.95)}
.tb-sm{width:44px;height:36px;font-size:12px;flex-direction:column;gap:1px}
.tb-sm span{font-size:8.5px;color:var(--sec);font-weight:500}
.tb-play{width:60px;height:46px;font-size:20px;font-weight:700;background:var(--red);color:#fff;border:none;border-radius:10px;box-shadow:0 2px 8px rgba(200,53,42,.4)}
.tb-side{width:44px;height:36px;font-size:15px}
.tb-stop{color:var(--red)}
.sr{display:flex;align-items:center;gap:10px;margin-bottom:10px}
.sr-lbl{font-size:11.5px;font-weight:600;color:var(--sec);white-space:nowrap;transition:color .25s}
.sr-val{font-size:11.5px;font-weight:700;color:var(--gold);white-space:nowrap;min-width:34px;text-align:right}
.sep-sm{height:1px;background:var(--sep);margin:8px 0;transition:background .25s}
.or{text-align:center;font-size:11px;color:var(--dim);margin-bottom:6px;transition:color .25s}
.dr{display:flex;align-items:center;gap:8px;margin-bottom:8px}
.dr-lbl{font-size:12.5px;color:var(--sec);white-space:nowrap;width:104px;flex-shrink:0;transition:color .25s}
.dr-sel{flex:1;height:26px;background:var(--inp);border:1px solid var(--sep);border-radius:6px;color:var(--pri);font-size:12.5px;padding:0 8px;appearance:none;transition:background .25s,border-color .25s,color .25s}
.ref{width:34px;height:26px;background:var(--btn);border:1px solid var(--sep);border-radius:6px;color:var(--pri);font-size:14px;cursor:pointer;display:flex;align-items:center;justify-content:center;transition:background .25s}
.live{width:100%;height:38px;background:var(--gold);border:none;border-radius:8px;font-size:13px;font-weight:700;color:#1C1410;cursor:pointer;box-shadow:0 2px 8px rgba(212,168,39,.38);margin-bottom:8px;transition:filter .12s}
.live:hover{filter:brightness(.93)}
.status{text-align:center;font-size:11.5px;color:var(--sec);margin-bottom:5px;transition:color .25s}
.a11y{text-align:center;font-size:9px;color:var(--dim);line-height:1.4;transition:color .25s}
</style></head>
<body><div id="win">
<div class="bar">
  <img class="bar-icon" id="icon" src="" alt="" onerror="this.style.display='none'">
  <span class="bar-title">Snuffiano</span>
  <span class="bar-credit">Created by ZenPhyx</span>
  <div class="dark-btn" id="dtb" onclick="toggleDark()">&#x1F319;</div>
</div>
<div class="c">
  <div class="sep"></div>
  <div class="drop" onclick="send({a:'browse'})">
    <span style="font-size:22px">&#x1F3B5;</span>
    <div><div class="drop-main">Drop a MIDI file here</div><div class="drop-sub">or click to browse &amp; save to library</div></div>
  </div>
  <div class="lib-hdr"><span class="lbl-sm">Library</span><span class="lib-cnt" id="cnt">0 songs</span></div>
  <input class="search" type="text" placeholder="Search songs&#x2026;" id="srch" oninput="send({a:'search',q:this.value})">
  <div class="lib" id="lib"></div>
  <div class="pw">
    <div class="pt"><span id="el">0:00</span><span id="tot">0:00</span></div>
    <input type="range" class="pb" id="pb" value="0" min="0" max="1000" oninput="updPb(this);send({a:'seek',v:this.value/1000})">
  </div>
  <div class="trans">
    <button class="tb tb-side" onclick="send({a:'restart'})">&#x21A9;</button>
    <button class="tb tb-sm" onclick="send({a:'rewind'})">&#x27E8;&#x27E8;<span>&minus;10s</span></button>
    <button class="tb tb-play" id="ppb" onclick="send({a:'play'})">&#x25B6;</button>
    <button class="tb tb-sm" onclick="send({a:'forward'})">&#x27E9;&#x27E9;<span>+10s</span></button>
    <button class="tb tb-side tb-stop" onclick="send({a:'stop'})">&#x25A0;</button>
  </div>
  <div class="sr">
    <span class="sr-lbl">Speed</span>
    <input type="range" class="sb" id="spd" value="43" min="0" max="100" style="flex:1" oninput="onSpd(this.value)">
    <span class="sr-val" id="sv">1.00&times;</span>
  </div>
  <div class="sep-sm"></div>
  <div class="or">&mdash;&nbsp; or use a MIDI keyboard &nbsp;&mdash;</div>
  <div class="dr">
    <span class="dr-lbl">MIDI Keyboard:</span>
    <select class="dr-sel" id="devs"><option>No MIDI devices found</option></select>
    <div class="ref" onclick="send({a:'refresh'})">&#x21BA;</div>
  </div>
  <button class="live" onclick="send({a:'live',i:document.getElementById('devs').selectedIndex})">&#x1F3B9;&nbsp; Live Mode</button>
  <div class="status" id="st">Drop a MIDI file or pick one from your library</div>
  <div class="a11y">Accessibility required &middot; Run as administrator if keys don&apos;t inject</div>
</div></div>
<script>
let dark=false,sel=-1,gdur=0;
function send(o){if(window.chrome&&window.chrome.webview)window.chrome.webview.postMessage(JSON.stringify(o));}
function toggleDark(){dark=!dark;document.getElementById('win').className=dark?'dark':'';document.getElementById('dtb').innerHTML=dark?'&#x2600;&#xFE0F;':'&#x1F319;';document.body.style.background=dark?'#1C1610':'#F5EFE3';updSliders();}
function tc(){return dark?'rgba(255,255,255,.12)':'rgba(0,0,0,.12)';}
function updPb(r){r.style.background=`linear-gradient(to right,#C8352A ${r.value/10}%,${tc()} ${r.value/10}%)`;}
function updSpd(r){r.style.background=`linear-gradient(to right,#D4A827 ${r.value}%,${tc()} ${r.value}%)`;}
function updSliders(){updPb(document.getElementById('pb'));updSpd(document.getElementById('spd'));}
function onSpd(v){const sp=0.25+v/100*1.75;document.getElementById('sv').textContent=sp.toFixed(2)+'×';updSpd(document.getElementById('spd'));send({a:'speed',v:sp});}
function fmt(s){const t=Math.floor(s);return Math.floor(t/60)+':'+(''+(t%60)).padStart(2,'0');}
function esc(s){return s.replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;').replace(/"/g,'&quot;');}
function selRow(i){document.querySelectorAll('.li').forEach((e,j)=>e.className='li'+(j===i?' sel':''));sel=i;send({a:'select',i:i});}
function playRow(i){selRow(i);send({a:'play_index',i:i});}
window.snuf={
  progress(pos,dur){gdur=dur;const f=dur>0?pos/dur:0;const pb=document.getElementById('pb');pb.value=Math.round(f*1000);updPb(pb);document.getElementById('el').textContent=fmt(pos);document.getElementById('tot').textContent=fmt(dur);},
  status(m){document.getElementById('st').textContent=m;},
  setLibrary(items){sel=-1;const lib=document.getElementById('lib');const cnt=document.getElementById('cnt');cnt.textContent=items.length+(items.length===1?' song':' songs');if(!items.length){lib.innerHTML='<div class="lib-empty">Library is empty &mdash; browse a MIDI file to add it</div>';return;}lib.innerHTML=items.map((it,i)=>`<div class="li" onclick="selRow(${i})" ondblclick="playRow(${i})"><span class="li-n">${i+1}</span><span class="li-ic">&#x1F3B5;</span><span class="li-nm">${esc(it.name)}</span><span class="li-d">${it.dur}</span><span class="li-x" onclick="event.stopPropagation();send({a:'delete',i:${i}})">&#x2715;</span></div>`).join('');},
  setDevices(d){document.getElementById('devs').innerHTML=d.map(x=>`<option>${esc(x)}</option>`).join('');},
  setPlaying(p){document.getElementById('ppb').innerHTML=p?'&#x23F8;':'&#x25B6;';},
};
if(window.chrome&&window.chrome.webview){window.chrome.webview.addEventListener('message',e=>{try{const d=JSON.parse(e.data);if(d.fn&&window.snuf[d.fn])window.snuf[d.fn](...(d.args||[]));} catch(ex){}});}
updSliders();send({a:'init'});
</script></body></html>)HTML";

// ─── Globals ──────────────────────────────────────────────────────────────────

static HWND g_hwnd;
static ComPtr<ICoreWebView2Controller> g_wvc;
static ComPtr<ICoreWebView2>           g_wv;

static std::unique_ptr<MIDIPlayer>  g_player;
static std::unique_ptr<RtMidiIn>    g_midiIn;
static std::vector<unsigned char>   g_midiMsg;
static std::vector<std::string>     g_deviceNames;

struct LibEntry { std::wstring path, name, duration; };
static std::vector<LibEntry>  g_library;
static std::vector<LibEntry*> g_filtered;
static int g_selected = -1;

static const UINT_PTR TID_PROGRESS = 1;
static const UINT_PTR TID_MIDI     = 2;

// ─── String helpers ───────────────────────────────────────────────────────────

static std::wstring toWide(const std::string& s) {
    if (s.empty()) return {};
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    std::wstring w(n, 0);
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, w.data(), n);
    if (!w.empty() && w.back()==0) w.pop_back();
    return w;
}
static std::string toUtf8(const std::wstring& w) {
    if (w.empty()) return {};
    int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string s(n, 0);
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, s.data(), n, nullptr, nullptr);
    if (!s.empty() && s.back()==0) s.pop_back();
    return s;
}
static std::wstring fmtDur(double sec) {
    int t = (int)sec; wchar_t b[16];
    swprintf_s(b, L"%d:%02d", t/60, t%60); return b;
}
static std::wstring stemOf(const std::wstring& path) {
    size_t sl = path.find_last_of(L"\\/");
    std::wstring n = sl==std::wstring::npos ? path : path.substr(sl+1);
    size_t dot = n.rfind(L'.'); if (dot!=std::wstring::npos) n=n.substr(0,dot);
    return n;
}

// ─── Library persistence ──────────────────────────────────────────────────────

static std::wstring libFile() {
    wchar_t buf[MAX_PATH]={};
    SHGetFolderPathW(nullptr, CSIDL_APPDATA, nullptr, 0, buf);
    std::wstring d = std::wstring(buf)+L"\\Snuffiano";
    CreateDirectoryW(d.c_str(), nullptr);
    return d+L"\\library.txt";
}
static void saveLib() {
    std::wofstream f(libFile());
    for (auto& e : g_library) f << e.path << L"\n";
}
static void loadLib() {
    std::wifstream f(libFile()); std::wstring line;
    while (std::getline(f, line)) {
        if (line.empty()) continue;
        double dur = MIDIPlayer::fileDuration(toUtf8(line));
        g_library.push_back({line, stemOf(line), dur>0?fmtDur(dur):L"--:--"});
    }
}
static void addFile(const std::wstring& path) {
    for (auto& e : g_library) if (e.path==path) return;
    double dur = MIDIPlayer::fileDuration(toUtf8(path));
    g_library.insert(g_library.begin(), {path, stemOf(path), dur>0?fmtDur(dur):L"--:--"});
    saveLib();
}

// ─── JS execution ─────────────────────────────────────────────────────────────

static std::wstring jsEsc(const std::wstring& s) {
    std::wstring r; r.reserve(s.size());
    for (wchar_t c : s) {
        if      (c==L'\'') r+=L"\\'";
        else if (c==L'\\') r+=L"\\\\";
        else if (c==L'\n') r+=L"\\n";
        else                r+=c;
    }
    return r;
}

static void jsExec(const std::wstring& script) {
    if (g_wv) g_wv->ExecuteScript(script.c_str(), nullptr);
}
static void jsCall(const wchar_t* fn, const std::wstring& args) {
    jsExec(std::wstring(L"window.snuf.") + fn + L"(" + args + L")");
}

static void pushLibrary(const std::wstring& q) {
    std::wstring ql = q;
    std::transform(ql.begin(), ql.end(), ql.begin(), [](wchar_t c){return (wchar_t)towlower(c);});
    g_filtered.clear();
    for (auto& e : g_library) {
        std::wstring nl = e.name;
        std::transform(nl.begin(), nl.end(), nl.begin(), [](wchar_t c){return (wchar_t)towlower(c);});
        if (ql.empty() || nl.find(ql)!=std::wstring::npos) g_filtered.push_back(&e);
    }
    std::wstring arr = L"[";
    for (auto* e : g_filtered)
        arr += L"{name:'" + jsEsc(e->name) + L"',dur:'" + jsEsc(e->duration) + L"'},";
    arr += L"]";
    jsCall(L"setLibrary", arr);
}

static void pushDevices() {
    g_deviceNames.clear();
    std::wstring arr = L"[";
    try {
        RtMidiIn midi; unsigned int n = midi.getPortCount();
        if (!n) { arr += L"'No MIDI devices found'"; g_deviceNames.push_back(""); }
        else for (unsigned int i=0; i<n; ++i) {
            auto nm = midi.getPortName(i);
            g_deviceNames.push_back(nm);
            arr += L"'" + jsEsc(toWide(nm)) + L"',";
        }
    } catch (...) { arr += L"'Error listing devices'"; }
    arr += L"]";
    jsCall(L"setDevices", arr);
}

// ─── Playback ─────────────────────────────────────────────────────────────────

static void startPlaying() {
    if (g_selected<0 || g_selected>=(int)g_filtered.size()) {
        jsCall(L"status", L"'Select a song from the library first.'"); return;
    }
    KillTimer(g_hwnd, TID_PROGRESS);
    std::string path = toUtf8(g_filtered[g_selected]->path);

    g_player->playFile(path,
        [](char key, bool press){ press ? pressKey(key) : releaseKey(key); },
        [](const std::string& msg){
            auto* s = new std::wstring(toWide(msg));
            PostMessage(g_hwnd, WM_USER+1, 0, (LPARAM)s);
        },
        [](){ PostMessage(g_hwnd, WM_USER+2, 0, 0); }
    );
    jsCall(L"setPlaying", L"true");
    SetTimer(g_hwnd, TID_PROGRESS, 100, nullptr);
}

// ─── JS message handler ───────────────────────────────────────────────────────

static void onMsg(const std::wstring& m) {
    auto has = [&](const wchar_t* k){ return m.find(k)!=std::wstring::npos; };
    auto numv = [&](const wchar_t* k) -> double {
        auto p = m.find(k); if (p==std::wstring::npos) return 0;
        p = m.find(L':', p+wcslen(k)); if (p==std::wstring::npos) return 0;
        return _wtof(m.c_str()+p+1);
    };

    if (has(L"\"init\""))    { pushLibrary(L""); pushDevices(); return; }
    if (has(L"\"refresh\"")) { pushDevices(); return; }

    if (has(L"\"browse\"")) {
        wchar_t path[MAX_PATH]={};
        OPENFILENAMEW ofn={};
        ofn.lStructSize=sizeof(ofn); ofn.hwndOwner=g_hwnd;
        ofn.lpstrFilter=L"MIDI Files\0*.mid;*.midi\0All Files\0*.*\0";
        ofn.lpstrFile=path; ofn.nMaxFile=MAX_PATH; ofn.Flags=OFN_FILEMUSTEXIST;
        if (GetOpenFileNameW(&ofn)) {
            addFile(path);
            jsExec(L"document.getElementById('srch').value=''");
            pushLibrary(L"");
        }
        return;
    }

    if (has(L"\"search\"")) {
        auto p = m.find(L"\"q\""); if (p==std::wstring::npos) return;
        p = m.find(L'"', p+4); if (p==std::wstring::npos) return; p++;
        auto e = m.find(L'"', p); if (e==std::wstring::npos) return;
        pushLibrary(m.substr(p, e-p));
        return;
    }

    if (has(L"\"select\"")) { g_selected = (int)numv(L"\"i\""); return; }

    if (has(L"\"play_index\"")) {
        g_selected = (int)numv(L"\"i\"");
        g_player->stop();
        startPlaying();
        return;
    }

    if (has(L"\"delete\"")) {
        int i = (int)numv(L"\"i\"");
        if (i>=0 && i<(int)g_filtered.size()) {
            auto path = g_filtered[i]->path;
            g_library.erase(std::remove_if(g_library.begin(), g_library.end(),
                [&](const LibEntry& e){ return e.path==path; }), g_library.end());
            saveLib();
            pushLibrary(L"");
        }
        return;
    }

    if (has(L"\"play\"")) {
        if (g_player->isRunning()) {
            if (g_player->isPaused()) { g_player->resume(); jsCall(L"setPlaying",L"true"); }
            else                      { g_player->pause();  jsCall(L"setPlaying",L"false"); }
        } else startPlaying();
        return;
    }

    if (has(L"\"stop\"")) {
        KillTimer(g_hwnd, TID_PROGRESS); KillTimer(g_hwnd, TID_MIDI);
        g_player->stop();
        if (g_midiIn) { g_midiIn->closePort(); g_midiIn.reset(); }
        resetModifiers();
        jsCall(L"setPlaying", L"false");
        jsCall(L"progress",   L"0,0");
        jsCall(L"status",     L"'Stopped.'");
        return;
    }

    if (has(L"\"restart\"")) {
        if (g_player->isRunning()) g_player->seek(0.0); else startPlaying(); return;
    }
    if (has(L"\"rewind\""))  { g_player->seek(std::max(0.0, g_player->getPosition()-10.0)); return; }
    if (has(L"\"forward\"")) {
        double dur=g_player->getDuration(), pos=g_player->getPosition();
        g_player->seek(std::min(pos+10.0, dur>0?dur-0.1:pos+10.0)); return;
    }
    if (has(L"\"seek\""))  { double dur=g_player->getDuration(); if(dur>0) g_player->seek(numv(L"\"v\"")*dur); return; }
    if (has(L"\"speed\"")) { g_player->setSpeed(numv(L"\"v\"")); return; }

    if (has(L"\"live\"")) {
        int idx = (int)numv(L"\"i\"");
        if (idx<0 || idx>=(int)g_deviceNames.size()) { jsCall(L"status",L"'No MIDI device selected.'"); return; }
        try {
            g_midiIn = std::make_unique<RtMidiIn>();
            g_midiIn->openPort((unsigned int)idx);
            g_midiIn->ignoreTypes(true,true,true);
        } catch (const std::exception& e) {
            jsCall(L"status", L"'Error: " + jsEsc(toWide(e.what())) + L"'");
            g_midiIn.reset(); return;
        }
        jsCall(L"status", L"'Live — play your keyboard!'");
        SetTimer(g_hwnd, TID_MIDI, 8, nullptr);
        return;
    }
}

// ─── WndProc ──────────────────────────────────────────────────────────────────

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_MOUSEACTIVATE:
        // While playing, don't steal focus from Roblox when the user clicks
        // our controls — clicks still register (MA_NOACTIVATE passes the mouse
        // message through), but Roblox stays as the foreground window so key
        // injection keeps going to it.
        if (g_player && g_player->isRunning()) return MA_NOACTIVATE;
        break;

    case WM_SIZE:
        if (g_wvc) { RECT r; GetClientRect(hwnd,&r); g_wvc->put_Bounds(r); }
        break;

    case WM_TIMER:
        if (wp==TID_PROGRESS && g_player->isRunning()) {
            double pos=g_player->getPosition(), dur=g_player->getDuration();
            wchar_t s[128]; swprintf_s(s, L"window.snuf.progress(%f,%f)", pos, dur);
            jsExec(s);
        }
        if (wp==TID_MIDI && g_midiIn) {
            g_midiIn->getMessage(&g_midiMsg);
            if (g_midiMsg.size()>=3) {
                uint8_t type=g_midiMsg[0]&0xF0, note=g_midiMsg[1], vel=g_midiMsg[2];
                auto key = RobloxKeyMapper::map((int)note);
                if (key) {
                    if (type==0x90&&vel>0) livePress(*key);
                    else if (type==0x80||(type==0x90&&vel==0)) liveRelease(*key);
                }
            }
        }
        break;

    case WM_USER+1: { auto* s=(std::wstring*)lp; jsCall(L"status",L"'"+jsEsc(*s)+L"'"); delete s; break; }
    case WM_USER+2:
        KillTimer(hwnd,TID_PROGRESS);
        jsCall(L"setPlaying",L"false");
        jsCall(L"progress",L"0,0");
        jsCall(L"status",L"'Done!'");
        break;

    case WM_DESTROY:
        KillTimer(hwnd,TID_PROGRESS); KillTimer(hwnd,TID_MIDI);
        g_player->stop(); PostQuitMessage(0); break;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

// ─── WinMain ──────────────────────────────────────────────────────────────────

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int) {
    // WebView2 uses COM — must be initialized before CreateCoreWebView2EnvironmentWithOptions
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

    // Set up direct NT syscall for input injection (bypasses API hooks, same as MIDI++)
    InitializeNtUserSendInputCall();

    g_player = std::make_unique<MIDIPlayer>();
    loadLib();

    WNDCLASSW wc={};
    wc.lpfnWndProc=WndProc; wc.hInstance=hInst;
    wc.lpszClassName=L"SnuffianoWnd";
    wc.hCursor=LoadCursor(nullptr,IDC_ARROW);
    wc.hbrBackground=(HBRUSH)(COLOR_WINDOW+1);
    RegisterClassW(&wc);

    g_hwnd = CreateWindowExW(WS_EX_TOPMOST, L"SnuffianoWnd", L"Snuffiano",
        WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX,
        CW_USEDEFAULT,CW_USEDEFAULT,462,648,
        nullptr,nullptr,hInst,nullptr);

    // Convert embedded HTML (UTF-8) → wide string for WebView2
    int n = MultiByteToWideChar(CP_UTF8,0,HTML_UI,-1,nullptr,0);
    std::wstring html(n,0);
    MultiByteToWideChar(CP_UTF8,0,HTML_UI,-1,html.data(),n);
    if (!html.empty()&&html.back()==0) html.pop_back();

    HRESULT wv2hr = CreateCoreWebView2EnvironmentWithOptions(nullptr,nullptr,nullptr,
        Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
        [html](HRESULT hr, ICoreWebView2Environment* env) -> HRESULT {
            if (FAILED(hr) || !env) {
                MessageBoxW(g_hwnd,
                    L"WebView2 runtime not found.\n\n"
                    L"Please install Microsoft Edge (Chromium) or the WebView2 Runtime:\n"
                    L"https://developer.microsoft.com/microsoft-edge/webview2/",
                    L"Snuffiano — Missing Component", MB_ICONERROR|MB_OK);
                PostQuitMessage(1);
                return hr;
            }
            env->CreateCoreWebView2Controller(g_hwnd,
                Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
                [html](HRESULT, ICoreWebView2Controller* ctrl) -> HRESULT {
                    g_wvc = ctrl;
                    ctrl->get_CoreWebView2(&g_wv);
                    RECT r; GetClientRect(g_hwnd,&r); ctrl->put_Bounds(r);

                    ComPtr<ICoreWebView2Settings> cfg;
                    g_wv->get_Settings(&cfg);
                    cfg->put_AreDefaultContextMenusEnabled(FALSE);
                    cfg->put_AreDevToolsEnabled(FALSE);

                    EventRegistrationToken tok;
                    g_wv->add_WebMessageReceived(
                        Callback<ICoreWebView2WebMessageReceivedEventHandler>(
                        [](ICoreWebView2*, ICoreWebView2WebMessageReceivedEventArgs* args) -> HRESULT {
                            LPWSTR msg=nullptr;
                            args->TryGetWebMessageAsString(&msg);
                            if (msg) { onMsg(msg); CoTaskMemFree(msg); }
                            return S_OK;
                        }).Get(), &tok);

                    g_wv->NavigateToString(html.c_str());
                    return S_OK;
                }).Get());
            return S_OK;
        }).Get());

    if (FAILED(wv2hr)) {
        MessageBoxW(nullptr,
            L"Failed to start WebView2.\n\nMake sure Microsoft Edge is installed.",
            L"Snuffiano", MB_ICONERROR|MB_OK);
        return 1;
    }

    ShowWindow(g_hwnd, SW_SHOWNORMAL);
    UpdateWindow(g_hwnd);

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    CoUninitialize();
    return 0;
}
