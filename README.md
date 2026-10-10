# Lightweight C AutoClicker

A native, ultra-lightweight AutoClicker written in pure C for Windows with a clean GUI and zero external dependencies.

![C](https://img.shields.io/badge/C-A8B9CC?logo=c&logoColor=black)
![Win32 API](https://img.shields.io/badge/Win32_API-0078D6?logo=windows&logoColor=white)
![Size](https://img.shields.io/badge/binary-~60_KB-brightgreen)

**Download:** grab [`autoclicker.exe`](autoclicker.exe) and run it. No installer, no runtime, no dependencies.

## Features

- **Burst Mode (Group Clicks with Delay)**:
  - Click a specified number of times (e.g., 3 clicks), pause for a configurable duration (e.g., 1 second), and repeat!
  - Fully customizable: Clicks per burst, and delay in Hours, Minutes, Seconds, and Milliseconds.
- **Click Cycle Time**:
  - Sets the interval/cadence between individual clicks within a burst.
- **Click Press Time (Hold Duration)**:
  - Controls how long the mouse button is physically held down before being released (Seconds, Milliseconds).
- **Emergency Mouse Safety**:
  - Automatically releases the mouse button if stopped while held down.
- **Mouse Buttons**: Left Click or Right Click.
- **Click Types**: Single Click or Double Click.
- **Click Amounts**:
  - Repeat indefinitely until stopped.
  - Repeat a fixed number of total clicks (e.g., 50, 100, 500).
- **Global Hotkey (`F6`)**: Press `F6` anywhere on your computer to Start or Stop the clicker without needing to focus the window.
- **Always on Top**: Stays above other windows for easy monitoring.
- **Ultra Lightweight**: Standalone executable (~60.5 KB) using ~2 MB RAM.

## How It Works

- **~600 lines of C** against the raw Win32 API, with no frameworks or libraries beyond what ships with Windows.
- Clicks are generated with `SendInput` on a **dedicated worker thread**, so the GUI stays responsive while clicking.
- `timeBeginPeriod` raises the system timer resolution for accurate millisecond-level click timing.
- `RegisterHotKey` provides the global `F6` start/stop toggle, even when the window is not focused.
- Built with `-O2 -s` and an embedded manifest and resource file to keep the binary tiny.

## Compiling from Source

Using MinGW GCC:
```cmd
build.bat
```

## Running

Launch `autoclicker.exe` from File Explorer or command prompt:
```cmd
autoclicker.exe
```
