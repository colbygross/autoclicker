# Lightweight C AutoClicker

A native, ultra-lightweight AutoClicker written in pure C for Windows with a clean GUI and zero external dependencies.

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
