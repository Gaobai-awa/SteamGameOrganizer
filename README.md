# SteamLauncher

A third-party, open-source Steam library manager and game launcher for Windows. Built with C++17, Dear ImGui, and DirectX 11.

Unlike the official Steam client — which is closed-source, Electron-based, slow to start, and crammed with social/store features you might not want — **SteamLauncher** focuses on one thing: launching and organizing **your** games quickly, with **your** own categories, without running the Steam client.

## Features

### Library Management
- **Full library scan** from `appinfo.vdf` and `libraryfolders.vdf` — discovers every game, tool, and DLC you own across all Steam library folders
- **Chinese name support** — pulls localized names from `appinfo.vdf` so you see 游戏名 not English names
- **Per-game metadata** — appid, install path, playtime, last-played, installed/uninstalled status
- **Multiple scan modes** — `libraryfolders + appinfo` (recommended), `appinfo only`, or third-party (e.g. OpenSteamTool) for detecting unowned games

### Categories
- **User-defined categories with sub-categories** — e.g. "单机 → RPG", "多人 → 联机"
- **Drag-free batch management** — select multiple games, assign/remove categories in one action
- **Per-game quick-tag popup** — right-click a game, choose 1+ categories from a submenu
- **Category manager dialog** — create, rename, delete, and re-parent categories

### Game Info & Launching
- **Smart launcher** — handles Steam URI, steam:// protocol, and direct exe launch
- **Playtime tracking** — read + write your own playtime logs (independent of Steam)
- **Launch counter** — sessions per game with timestamps
- **Cover/icon display** — pulls library icons from Steam's cache, renders via ImGui

### Searching & Filtering
- **Real-time text search** — filters as you type, matches Chinese or English names
- **Category filter** — click a category on the left panel to see only its games
- **Hidden games mode** — hide games without removing them; restore from a dedicated dialog
- **Sort by name / appid / playtime / install status**

### Steam Process Tools
- **Steam process watcher** — detects when Steam starts/stops
- **DLL injector** — for advanced users who want to hook the Steam client (e.g. for `steam_hook.dll`)
- **Auto-launch support** — integrates with Steam's protocol handlers

### UI
- **Dear ImGui + DirectX 11** rendering — instant startup, ~80MB RAM, GPU-friendly
- **Dark hacker theme** — custom colors, rounded corners, no bloat
- **Chinese font support** — auto-loads `msyh.ttc` / `simhei.ttf` with full Chinese glyph range
- **High-DPI aware** — font scales with system DPI

## Why Not Just Use Steam's Own Client?

| Feature | Steam Client | SteamLauncher |
|---|---|---|
| Open source | ❌ Closed (Electron) | ✅ MIT, C++17 |
| Startup time | 5-15 seconds | < 1 second |
| RAM usage | 400MB-1GB | ~80MB |
| User-defined sub-categories | ❌ Flat only | ✅ Unlimited nesting |
| Batch category assignment | ❌ | ✅ Select 50+ games at once |
| Launch without Steam running | ❌ | ✅ Reads appinfo.vdf directly |
| Play without online check | ❌ (unless `-offline`) | ✅ Optional DLL hook for `steam_hook.dll` |
| Inspectable data | ❌ Encrypted/closed | ✅ Plain JSON in `data/` |
| Per-game notes/remarks | ❌ | ✅ Custom remark per game |
| Free from store / chat / workshop | ❌ Always-on | ✅ Library only |
| Cross-platform portability | ❌ Windows-only (officially) | ✅ Code is portable; only Win32 launch path is Windows-specific |

## Architecture

```
src/
├── main.cpp              # entry point
├── win_main.cpp          # WinMain + window class
├── gui_app.{h,cpp}       # ImGui + DX11 + main UI
├── steam_scanner.{h,cpp} # appinfo.vdf / libraryfolders.vdf / localconfig.vdf reader
├── binary_vdf.{h,cpp}    # Steam's binary VDF parser
├── vdf_parser.{h,cpp}    # text VDF parser
├── category_manager.{h,cpp} # category CRUD + save/load
├── playtime_tracker.{h,cpp} # playtime sessions
├── icon_loader.{h,cpp}   # game icon extraction from Steam cache
├── game_launcher.{h,cpp} # process launching
├── dll_injector.{h,cpp}  # Windows DLL injection
├── steam_watcher.{h,cpp} # Steam process monitor
├── achievement_manager.{h,cpp}
├── console_ui.{h,cpp}    # (legacy) alternate console UI
├── app_settings.h        # JSON-based config
├── game_data.h           # GameInfo / Category structs
├── imgui/                # Dear ImGui 1.91.0 + DX11/Win32 backends
└── dll/                  # steam_hook.dll source
```

## Data Storage

SteamLauncher stores all user data in `<exe_dir>/data/`:

- `data/settings.json` — Steam path, scan method, hidden appids, theme
- `data/categories.json` — your category tree + game assignments
- `data/playtime.json` — session log + totals
- `data/remarks.json` — per-game notes

Everything is plain JSON — you can edit it by hand if you want.

## Build

### Prerequisites (Windows)
- Visual Studio 2022 (MSVC v14.5+)
- Windows 10/11 SDK
- Dear ImGui 1.91.0 (already vendored in `src/imgui/`)

### Compile
```bat
build_msvc.bat
```

The output is `bin/SteamLauncher.exe` (~2MB) + `bin/steam_hook.dll` (~125KB).

### Debug mode
```bat
SteamLauncher.exe -debug
```
Writes `E:\temp\steam_launcher_debug.log` with diagnostic output.

## Steam Path Detection

SteamLauncher auto-detects your Steam installation from:
1. `HKCU\Software\Valve\Steam\SteamPath` (registry)
2. `C:\Program Files (x86)\Steam\`
3. `C:\Program Files\Steam\`
4. Any drive root with a `Steam` subfolder containing `steam.exe`

If detection fails, set it manually in **Settings → Steam path**.

## License

MIT. See `LICENSE`.

## Credits

- [Dear ImGui](https://github.com/ocornut/imgui) by Omar Cornut — MIT
- [ValveKeyValue](https://github.com/ValveResourceFormat/ValveKeyValue) reference — for the VDF format
- [JackalClient](https://github.com/JackalClient/JackalClient) — UI/UX inspiration

## Disclaimer

This project is not affiliated with Valve Corporation or Steam. "Steam" and the Steam logo are trademarks of Valve Corporation. This is an unofficial, community tool for personal use.
