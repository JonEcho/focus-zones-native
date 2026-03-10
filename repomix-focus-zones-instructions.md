# Focus Zones Native — Onboarding Context

This package contains the complete production code for **focus-zones-native**, a Windows system utility that dynamically resizes windows within FancyZones columns when focus changes. It is a C rewrite of the original Python `focus-zones` project.

## Architecture Overview

- **Language**: C11 (compiled with MSVC via CMake)
- **Platform**: Windows-only (Win32 API, DWM, Shell)
- **Dependencies**: cJSON (vendored in `vendor/cJSON/`)
- **Build**: CMake with Visual Studio 16 2019 generator

## Core Concepts

### Columns
The screen is divided into columns matching FancyZones zone boundaries. Each column has `x_min`/`x_max` pixel bounds and a `dynamic_resize` flag. Center column is static; left and right are dynamic.

### Focus-Driven Resize
When a window gains focus in a dynamic column, the focused window expands vertically (based on `focus_ratio`) while sibling windows in the same column shrink. Single-occupant columns auto-expand their window to fill.

### Window Swap
Drag-and-drop between columns triggers a swap: the dropped window replaces the closest overlapping window, which moves to the source position. Includes FancyZones snap polling.

## File Organization

### Core Logic (`src/`)
- `main.c` — Entry point, WinEvent hooks (`on_focus`, `on_move_size`), message loop, config-driven column initialization
- `config.c`/`.h` — JSON config loading via cJSON: columns, ratios, ignored executables, hotkey
- `layout.c`/`.h` — Monitor detection (`EnumDisplayMonitors`), column definitions, auto-generation fallback
- `window.c`/`.h` — Window queries (`is_app`, `get_exe`, `get_rect`), column enumeration via `EnumWindows`
- `resize.c`/`.h` — Column resize math: height allocation based on focus ratio with min-size clamping
- `swap.c`/`.h` — Drag tracking and cross-column window swap with FancyZones snap polling
- `hotkey.c`/`.h` — Hotkey parsing (`ctrl+alt+z`) and Win32 RegisterHotKey
- `tray.c`/`.h` — System tray icon (custom pixel art), context menu, change-hotkey capture via keyboard hook
- `overlay.c`/`.h` — Column boundary overlay (reserved for future visual feedback)

### Configuration
- `config.json` — Column boundaries, focus ratio, debounce, ignored executables, toggle hotkey. Column `x_min`/`x_max` values must match FancyZones zone boundaries exactly.

## Key Design Decisions

1. **Config-driven columns**: Column pixel boundaries are read from `config.json` (matching the Python version). Falls back to auto-generation based on monitor resolution if no columns configured.
2. **Y bounds from work area**: Column top/bottom are always derived dynamically from `GetMonitorInfoW` work area, never hardcoded.
3. **x_center membership**: A window belongs to a column if its horizontal center falls within the column bounds. No width tolerance filtering.
4. **No dialog filter in focus callback**: Unlike early native versions, the focus handler does NOT filter `WS_POPUP` windows — matching Python behavior.
5. **Close-on-click delay**: A configurable delay (`title_bar_click_delay_ms`) before resize allows close-button clicks to register. If the window is gone after the delay, remaining windows re-layout.
