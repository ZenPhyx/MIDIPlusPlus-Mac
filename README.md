<div align="center">
  <img src="assets/icon.png" width="120" alt="Snuffiano icon">
  <h1>Snuffiano</h1>
  <p>
    Snuffiano is a MIDI piano app inspired by my character Snuffy, a mix of Snoopy and Luffy.<br>
    Built to combine music, creativity, and a fun hand-drawn style.
  </p>
</div>

---

<!-- Replace the image below with a screenshot of the app -->
![App Screenshot](https://placehold.co/860x480?text=App+Screenshot)

---

## What it does

Snuffiano lets you play MIDI files and live MIDI keyboards through **Roblox Virtual Piano** automatically — it reads the notes and injects the matching keystrokes so the in-game piano plays them for you.

- **MIDI file playback** — drop a `.mid` file, hit play, and Snuffiano handles everything
- **Live keyboard mode** — plug in any MIDI keyboard and play in real time
- **Auto-transpose** — automatically shifts songs to fit the Roblox piano's range
- **Playback controls** — play/pause, seek, skip ±10s, speed from 0.25× to 2×
- **Song library** — browse, search, and double-click to play your saved songs
- **Always on top** — the app floats above Roblox so you can control it mid-song without breaking focus

---

## Screenshots

<!-- Add your screenshots below — drag images into GitHub's editor or replace the src links -->

| macOS | Windows |
|-------|---------|
| ![macOS screenshot](https://placehold.co/400x500?text=macOS) | ![Windows screenshot](https://placehold.co/400x500?text=Windows) |

---

## Download

Head to the [**Releases**](../../releases) page to grab the latest build.

| Platform | Status |
|----------|--------|
| macOS | ✅ Available |
| Windows | ✅ Available |

---

## Build from source

**macOS**
```bash
mkdir build && cd build
cmake ..
make
```

**Windows** (requires CMake 3.20+ and Visual Studio)
```bash
mkdir build && cd build
cmake ..
cmake --build . --config Release
```

The Windows build downloads the WebView2 SDK automatically via CMake.

---

## Requirements

- **macOS** — Accessibility permission required  
  `System Settings → Privacy & Security → Accessibility → add the app`
- **Windows** — Microsoft Edge / WebView2 Runtime (pre-installed on Windows 11, free download for Windows 10)
- **Roblox Virtual Piano** — the in-game piano the app plays through

---

<div align="center">
  <sub>Created by ZenPhyx &nbsp;·&nbsp; Inspired by Snuffy 🎵</sub>
</div>
