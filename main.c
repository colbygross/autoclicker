#define _WIN32_WINNT 0x0501
#define _WIN32_IE 0x0501
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>
#include <stdio.h>
#include <stdlib.h>
#include <mmsystem.h>

#define ID_HOTKEY_F6 1001

#define IDC_GRP_TIMING         2001
#define IDC_EDIT_CYCLE_HRS     2002
#define IDC_EDIT_CYCLE_MINS    2003
#define IDC_EDIT_CYCLE_SECS    2004
#define IDC_EDIT_CYCLE_MS      2005
#define IDC_EDIT_HOLD_SECS     2006
#define IDC_EDIT_HOLD_MS       2007

#define IDC_GRP_BURST          2010
#define IDC_CHK_BURST_ENABLE   2011
#define IDC_EDIT_BURST_CLICKS  2012
#define IDC_EDIT_BURST_HRS     2013
#define IDC_EDIT_BURST_MINS    2014
#define IDC_EDIT_BURST_SECS    2015
#define IDC_EDIT_BURST_MS      2016

#define IDC_GRP_OPTIONS        2020
#define IDC_RADIO_LEFT         2021
#define IDC_RADIO_RIGHT        2022
#define IDC_RADIO_SINGLE       2023
#define IDC_RADIO_DOUBLE       2024

#define IDC_GRP_REPEAT         2030
#define IDC_RADIO_REPEAT_INF   2031
#define IDC_RADIO_REPEAT_CNT   2032
#define IDC_EDIT_REPEAT_CNT    2033

#define IDC_BTN_STARTSTOP      2040
#define IDC_STATIC_STATUS      2041

#define WM_CLICKER_FINISHED    (WM_USER + 100)
#define WM_CLICKER_UPDATE      (WM_USER + 101)

typedef struct {
    DWORD cycleMs;
    DWORD holdMs;
    BOOL isRightClick;
    BOOL isDoubleClick;
    BOOL repeatInfinite;
    DWORD repeatCount;
    // Burst mode settings
    BOOL burstEnabled;
    DWORD burstClicks;
    DWORD burstDelayMs;
} ClickerConfig;

static HINSTANCE g_hInstance = NULL;
static HWND g_hMainWnd = NULL;
static HFONT g_hFont = NULL;

static HWND g_hEditCycleHrs, g_hEditCycleMins, g_hEditCycleSecs, g_hEditCycleMs;
static HWND g_hEditHoldSecs, g_hEditHoldMs;

static HWND g_hChkBurstEnable, g_hEditBurstClicks;
static HWND g_hEditBurstHrs, g_hEditBurstMins, g_hEditBurstSecs, g_hEditBurstMs;

static HWND g_hRadioLeft, g_hRadioRight;
static HWND g_hRadioSingle, g_hRadioDouble;
static HWND g_hRadioRepeatInf, g_hRadioRepeatCnt, g_hEditRepeatCnt;
static HWND g_hBtnStartStop, g_hStaticStatus;

static volatile BOOL g_isRunning = FALSE;
static HANDLE g_hClickThread = NULL;
static volatile DWORD g_clicksDone = 0;
static ClickerConfig g_config;

// High precision sleep helper
static void AccurateSleep(DWORD ms) {
    if (ms == 0) return;
    if (ms <= 15) {
        Sleep(ms);
        return;
    }
    DWORD elapsed = 0;
    while (g_isRunning && elapsed < ms) {
        DWORD chunk = (ms - elapsed > 15) ? 15 : (ms - elapsed);
        Sleep(chunk);
        elapsed += chunk;
    }
}

// Emergency release of mouse button
static void ReleaseMouseButton(BOOL isRight) {
    INPUT input = {0};
    input.type = INPUT_MOUSE;
    input.mi.dwFlags = isRight ? MOUSEEVENTF_RIGHTUP : MOUSEEVENTF_LEFTUP;
    SendInput(1, &input, sizeof(INPUT));
}

// Perform a single or double click with customizable hold (press) duration
static void DoMouseClick(BOOL isRight, BOOL isDouble, DWORD holdMs) {
    INPUT input = {0};
    input.type = INPUT_MOUSE;
    DWORD downFlag = isRight ? MOUSEEVENTF_RIGHTDOWN : MOUSEEVENTF_LEFTDOWN;
    DWORD upFlag   = isRight ? MOUSEEVENTF_RIGHTUP   : MOUSEEVENTF_LEFTUP;

    DWORD actualHold = (holdMs == 0) ? 1 : holdMs;

    if (!isDouble) {
        // Press Down
        input.mi.dwFlags = downFlag;
        SendInput(1, &input, sizeof(INPUT));

        // Hold for specified press duration
        AccurateSleep(actualHold);

        // Release Up
        input.mi.dwFlags = upFlag;
        SendInput(1, &input, sizeof(INPUT));
    } else {
        // Double click: First click
        input.mi.dwFlags = downFlag;
        SendInput(1, &input, sizeof(INPUT));

        AccurateSleep(actualHold);

        input.mi.dwFlags = upFlag;
        SendInput(1, &input, sizeof(INPUT));

        AccurateSleep(20);

        // Second click
        input.mi.dwFlags = downFlag;
        SendInput(1, &input, sizeof(INPUT));

        AccurateSleep(actualHold);

        input.mi.dwFlags = upFlag;
        SendInput(1, &input, sizeof(INPUT));
    }
}

// Background clicking thread
static DWORD WINAPI ClickerThreadProc(LPVOID lpParam) {
    timeBeginPeriod(1);

    g_clicksDone = 0;
    DWORD burstClickCounter = 0;

    while (g_isRunning) {
        if (!g_config.repeatInfinite && g_clicksDone >= g_config.repeatCount) {
            break;
        }

        DWORD startTick = GetTickCount();

        // Perform click
        DoMouseClick(g_config.isRightClick, g_config.isDoubleClick, g_config.holdMs);
        g_clicksDone++;
        burstClickCounter++;

        PostMessage(g_hMainWnd, WM_CLICKER_UPDATE, (WPARAM)g_clicksDone, 0);

        if (!g_isRunning) break;
        if (!g_config.repeatInfinite && g_clicksDone >= g_config.repeatCount) {
            break;
        }

        // Burst check: If burst count reached, apply burst pause
        if (g_config.burstEnabled && burstClickCounter >= g_config.burstClicks) {
            burstClickCounter = 0; // reset for next burst
            if (g_config.burstDelayMs > 0) {
                AccurateSleep(g_config.burstDelayMs);
            }
        } else {
            // Normal intra-burst interval (cycle time)
            DWORD elapsed = GetTickCount() - startTick;
            if (g_config.cycleMs > elapsed) {
                AccurateSleep(g_config.cycleMs - elapsed);
            }
        }
    }

    timeEndPeriod(1);
    PostMessage(g_hMainWnd, WM_CLICKER_FINISHED, 0, 0);
    return 0;
}

static void UpdateStatusUI() {
    char buf[128];
    if (g_isRunning) {
        SetWindowTextA(g_hBtnStartStop, "STOP (F6)");
        const char *modeTag = g_config.burstEnabled ? " (Burst Mode)" : "";
        if (g_config.repeatInfinite) {
            snprintf(buf, sizeof(buf), "Status: Running%s... | Total Clicks: %lu", modeTag, g_clicksDone);
        } else {
            snprintf(buf, sizeof(buf), "Status: Running%s... | Clicks: %lu / %lu", modeTag, g_clicksDone, g_config.repeatCount);
        }
    } else {
        SetWindowTextA(g_hBtnStartStop, "START (F6)");
        snprintf(buf, sizeof(buf), "Status: Stopped | Total Clicks: %lu", g_clicksDone);
    }
    SetWindowTextA(g_hStaticStatus, buf);
}

static void StopClicker() {
    if (!g_isRunning) return;
    g_isRunning = FALSE;
    ReleaseMouseButton(g_config.isRightClick);

    if (g_hClickThread != NULL) {
        WaitForSingleObject(g_hClickThread, 200);
        CloseHandle(g_hClickThread);
        g_hClickThread = NULL;
    }
    UpdateStatusUI();
}

static void UpdateBurstControlsState() {
    BOOL enabled = (SendMessage(g_hChkBurstEnable, BM_GETCHECK, 0, 0) == BST_CHECKED);
    EnableWindow(g_hEditBurstClicks, enabled);
    EnableWindow(g_hEditBurstHrs, enabled);
    EnableWindow(g_hEditBurstMins, enabled);
    EnableWindow(g_hEditBurstSecs, enabled);
    EnableWindow(g_hEditBurstMs, enabled);
}

static void StartClicker() {
    if (g_isRunning) return;

    // Read Total Cycle Time (time between clicks in a burst)
    char buf[32];
    GetWindowTextA(g_hEditCycleHrs, buf, sizeof(buf));
    DWORD cycleHrs = (DWORD)strtoul(buf, NULL, 10);

    GetWindowTextA(g_hEditCycleMins, buf, sizeof(buf));
    DWORD cycleMins = (DWORD)strtoul(buf, NULL, 10);

    GetWindowTextA(g_hEditCycleSecs, buf, sizeof(buf));
    DWORD cycleSecs = (DWORD)strtoul(buf, NULL, 10);

    GetWindowTextA(g_hEditCycleMs, buf, sizeof(buf));
    DWORD cycleMs = (DWORD)strtoul(buf, NULL, 10);

    DWORD totalCycleMs = (cycleHrs * 3600000) + (cycleMins * 60000) + (cycleSecs * 1000) + cycleMs;
    if (totalCycleMs < 1) {
        MessageBoxA(g_hMainWnd, "Click interval/cycle time must be at least 1 ms!", "Invalid Timing", MB_ICONWARNING | MB_OK);
        return;
    }
    g_config.cycleMs = totalCycleMs;

    // Read Click Hold Duration (Press time)
    GetWindowTextA(g_hEditHoldSecs, buf, sizeof(buf));
    DWORD holdSecs = (DWORD)strtoul(buf, NULL, 10);

    GetWindowTextA(g_hEditHoldMs, buf, sizeof(buf));
    DWORD holdMs = (DWORD)strtoul(buf, NULL, 10);

    DWORD totalHoldMs = (holdSecs * 1000) + holdMs;
    if (totalHoldMs >= totalCycleMs) {
        MessageBoxA(g_hMainWnd, 
            "Click press/hold duration must be less than the click cycle time!\n\n"
            "Example:\n"
            "Click Cycle Time: 100 ms\n"
            "Click Press Time: 20 ms", 
            "Invalid Timing Configuration", MB_ICONWARNING | MB_OK);
        return;
    }
    g_config.holdMs = totalHoldMs;

    // Read Burst Mode settings
    g_config.burstEnabled = (SendMessage(g_hChkBurstEnable, BM_GETCHECK, 0, 0) == BST_CHECKED);
    if (g_config.burstEnabled) {
        GetWindowTextA(g_hEditBurstClicks, buf, sizeof(buf));
        g_config.burstClicks = (DWORD)strtoul(buf, NULL, 10);
        if (g_config.burstClicks < 1) {
            MessageBoxA(g_hMainWnd, "Clicks per burst must be at least 1!", "Invalid Burst Configuration", MB_ICONWARNING | MB_OK);
            return;
        }

        GetWindowTextA(g_hEditBurstHrs, buf, sizeof(buf));
        DWORD bHrs = (DWORD)strtoul(buf, NULL, 10);

        GetWindowTextA(g_hEditBurstMins, buf, sizeof(buf));
        DWORD bMins = (DWORD)strtoul(buf, NULL, 10);

        GetWindowTextA(g_hEditBurstSecs, buf, sizeof(buf));
        DWORD bSecs = (DWORD)strtoul(buf, NULL, 10);

        GetWindowTextA(g_hEditBurstMs, buf, sizeof(buf));
        DWORD bMs = (DWORD)strtoul(buf, NULL, 10);

        g_config.burstDelayMs = (bHrs * 3600000) + (bMins * 60000) + (bSecs * 1000) + bMs;
    } else {
        g_config.burstClicks = 1;
        g_config.burstDelayMs = 0;
    }

    // Mouse button
    g_config.isRightClick = (SendMessage(g_hRadioRight, BM_GETCHECK, 0, 0) == BST_CHECKED);
    g_config.isDoubleClick = (SendMessage(g_hRadioDouble, BM_GETCHECK, 0, 0) == BST_CHECKED);

    // Repeat count
    if (SendMessage(g_hRadioRepeatInf, BM_GETCHECK, 0, 0) == BST_CHECKED) {
        g_config.repeatInfinite = TRUE;
        g_config.repeatCount = 0;
    } else {
        g_config.repeatInfinite = FALSE;
        GetWindowTextA(g_hEditRepeatCnt, buf, sizeof(buf));
        g_config.repeatCount = (DWORD)strtoul(buf, NULL, 10);
        if (g_config.repeatCount == 0) {
            MessageBoxA(g_hMainWnd, "Repeat count must be greater than 0!", "Invalid Repeat Count", MB_ICONWARNING | MB_OK);
            return;
        }
    }

    g_isRunning = TRUE;
    g_clicksDone = 0;
    UpdateStatusUI();

    g_hClickThread = CreateThread(NULL, 0, ClickerThreadProc, NULL, 0, NULL);
    if (!g_hClickThread) {
        g_isRunning = FALSE;
        UpdateStatusUI();
        MessageBoxA(g_hMainWnd, "Failed to start clicker thread.", "Error", MB_ICONERROR | MB_OK);
    }
}

static void ToggleClicker() {
    if (g_isRunning) {
        StopClicker();
    } else {
        StartClicker();
    }
}

static void ApplyFontToChildren(HWND hwnd, HFONT hFont) {
    HWND hChild = GetWindow(hwnd, GW_CHILD);
    while (hChild) {
        SendMessage(hChild, WM_SETFONT, (WPARAM)hFont, TRUE);
        hChild = GetWindow(hChild, GW_HWNDNEXT);
    }
}

static LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_CREATE: {
        // Group 1: Timing Settings (Cycle time + Click hold duration)
        CreateWindowA("BUTTON", "Click Timing",
            WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
            12, 10, 415, 125, hWnd, (HMENU)IDC_GRP_TIMING, g_hInstance, NULL);

        // Cycle time label and edits
        CreateWindowA("STATIC", "Click Cycle Time (interval between clicks):",
            WS_CHILD | WS_VISIBLE, 22, 28, 380, 16, hWnd, NULL, g_hInstance, NULL);

        CreateWindowA("STATIC", "Hours:", WS_CHILD | WS_VISIBLE, 22, 46, 45, 16, hWnd, NULL, g_hInstance, NULL);
        g_hEditCycleHrs = CreateWindowA("EDIT", "0", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_NUMBER | ES_RIGHT,
            22, 64, 55, 22, hWnd, (HMENU)IDC_EDIT_CYCLE_HRS, g_hInstance, NULL);

        CreateWindowA("STATIC", "Mins:", WS_CHILD | WS_VISIBLE, 92, 46, 45, 16, hWnd, NULL, g_hInstance, NULL);
        g_hEditCycleMins = CreateWindowA("EDIT", "0", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_NUMBER | ES_RIGHT,
            92, 64, 55, 22, hWnd, (HMENU)IDC_EDIT_CYCLE_MINS, g_hInstance, NULL);

        CreateWindowA("STATIC", "Secs:", WS_CHILD | WS_VISIBLE, 162, 46, 45, 16, hWnd, NULL, g_hInstance, NULL);
        g_hEditCycleSecs = CreateWindowA("EDIT", "0", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_NUMBER | ES_RIGHT,
            162, 64, 55, 22, hWnd, (HMENU)IDC_EDIT_CYCLE_SECS, g_hInstance, NULL);

        CreateWindowA("STATIC", "Millis (ms):", WS_CHILD | WS_VISIBLE, 232, 46, 75, 16, hWnd, NULL, g_hInstance, NULL);
        g_hEditCycleMs = CreateWindowA("EDIT", "100", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_NUMBER | ES_RIGHT,
            232, 64, 75, 22, hWnd, (HMENU)IDC_EDIT_CYCLE_MS, g_hInstance, NULL);

        // Click hold duration label and edits
        CreateWindowA("STATIC", "Click Press Time (hold duration):",
            WS_CHILD | WS_VISIBLE, 22, 94, 210, 18, hWnd, NULL, g_hInstance, NULL);

        CreateWindowA("STATIC", "Secs:", WS_CHILD | WS_VISIBLE, 240, 94, 38, 18, hWnd, NULL, g_hInstance, NULL);
        g_hEditHoldSecs = CreateWindowA("EDIT", "0", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_NUMBER | ES_RIGHT,
            278, 92, 45, 22, hWnd, (HMENU)IDC_EDIT_HOLD_SECS, g_hInstance, NULL);

        CreateWindowA("STATIC", "ms:", WS_CHILD | WS_VISIBLE, 332, 94, 25, 18, hWnd, NULL, g_hInstance, NULL);
        g_hEditHoldMs = CreateWindowA("EDIT", "20", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_NUMBER | ES_RIGHT,
            358, 92, 55, 22, hWnd, (HMENU)IDC_EDIT_HOLD_MS, g_hInstance, NULL);

        // Group 2: Burst / Batch Mode (Group clicks with delay between groups)
        CreateWindowA("BUTTON", "Burst Mode (Group Clicks with Delay)",
            WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
            12, 145, 415, 125, hWnd, (HMENU)IDC_GRP_BURST, g_hInstance, NULL);

        g_hChkBurstEnable = CreateWindowA("BUTTON", "Enable Burst Mode (e.g. click 3 times, pause 1s, repeat)",
            WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
            22, 168, 395, 20, hWnd, (HMENU)IDC_CHK_BURST_ENABLE, g_hInstance, NULL);

        CreateWindowA("STATIC", "Clicks per burst:", WS_CHILD | WS_VISIBLE, 22, 196, 115, 18, hWnd, NULL, g_hInstance, NULL);
        g_hEditBurstClicks = CreateWindowA("EDIT", "3", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_NUMBER | ES_RIGHT,
            140, 194, 55, 22, hWnd, (HMENU)IDC_EDIT_BURST_CLICKS, g_hInstance, NULL);

        CreateWindowA("STATIC", "Delay between bursts:", WS_CHILD | WS_VISIBLE, 22, 224, 135, 18, hWnd, NULL, g_hInstance, NULL);

        CreateWindowA("STATIC", "H:", WS_CHILD | WS_VISIBLE, 160, 224, 18, 18, hWnd, NULL, g_hInstance, NULL);
        g_hEditBurstHrs = CreateWindowA("EDIT", "0", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_NUMBER | ES_RIGHT,
            180, 222, 42, 22, hWnd, (HMENU)IDC_EDIT_BURST_HRS, g_hInstance, NULL);

        CreateWindowA("STATIC", "M:", WS_CHILD | WS_VISIBLE, 228, 224, 18, 18, hWnd, NULL, g_hInstance, NULL);
        g_hEditBurstMins = CreateWindowA("EDIT", "0", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_NUMBER | ES_RIGHT,
            248, 222, 42, 22, hWnd, (HMENU)IDC_EDIT_BURST_MINS, g_hInstance, NULL);

        CreateWindowA("STATIC", "S:", WS_CHILD | WS_VISIBLE, 296, 224, 18, 18, hWnd, NULL, g_hInstance, NULL);
        g_hEditBurstSecs = CreateWindowA("EDIT", "1", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_NUMBER | ES_RIGHT,
            316, 222, 42, 22, hWnd, (HMENU)IDC_EDIT_BURST_SECS, g_hInstance, NULL);

        CreateWindowA("STATIC", "ms:", WS_CHILD | WS_VISIBLE, 362, 224, 22, 18, hWnd, NULL, g_hInstance, NULL);
        g_hEditBurstMs = CreateWindowA("EDIT", "0", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_NUMBER | ES_RIGHT,
            384, 222, 35, 22, hWnd, (HMENU)IDC_EDIT_BURST_MS, g_hInstance, NULL);

        // Group 3: Click Options
        CreateWindowA("BUTTON", "Click Options",
            WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
            12, 280, 415, 70, hWnd, (HMENU)IDC_GRP_OPTIONS, g_hInstance, NULL);

        CreateWindowA("STATIC", "Mouse Button:", WS_CHILD | WS_VISIBLE, 22, 301, 90, 18, hWnd, NULL, g_hInstance, NULL);
        g_hRadioLeft = CreateWindowA("BUTTON", "Left Click",
            WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON | WS_GROUP,
            120, 299, 90, 20, hWnd, (HMENU)IDC_RADIO_LEFT, g_hInstance, NULL);
        g_hRadioRight = CreateWindowA("BUTTON", "Right Click",
            WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON,
            220, 299, 95, 20, hWnd, (HMENU)IDC_RADIO_RIGHT, g_hInstance, NULL);

        CreateWindowA("STATIC", "Click Type:", WS_CHILD | WS_VISIBLE, 22, 323, 90, 18, hWnd, NULL, g_hInstance, NULL);
        g_hRadioSingle = CreateWindowA("BUTTON", "Single",
            WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON | WS_GROUP,
            120, 321, 70, 20, hWnd, (HMENU)IDC_RADIO_SINGLE, g_hInstance, NULL);
        g_hRadioDouble = CreateWindowA("BUTTON", "Double",
            WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON,
            220, 321, 70, 20, hWnd, (HMENU)IDC_RADIO_DOUBLE, g_hInstance, NULL);

        // Group 4: Click Repeat
        CreateWindowA("BUTTON", "Click Repeat",
            WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
            12, 360, 415, 75, hWnd, (HMENU)IDC_GRP_REPEAT, g_hInstance, NULL);

        g_hRadioRepeatInf = CreateWindowA("BUTTON", "Repeat until stopped",
            WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON | WS_GROUP,
            22, 381, 160, 20, hWnd, (HMENU)IDC_RADIO_REPEAT_INF, g_hInstance, NULL);

        g_hRadioRepeatCnt = CreateWindowA("BUTTON", "Repeat",
            WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON,
            22, 404, 70, 20, hWnd, (HMENU)IDC_RADIO_REPEAT_CNT, g_hInstance, NULL);

        g_hEditRepeatCnt = CreateWindowA("EDIT", "100",
            WS_CHILD | WS_VISIBLE | WS_BORDER | ES_NUMBER | ES_RIGHT,
            96, 403, 65, 22, hWnd, (HMENU)IDC_EDIT_REPEAT_CNT, g_hInstance, NULL);

        CreateWindowA("STATIC", "times", WS_CHILD | WS_VISIBLE, 168, 406, 45, 18, hWnd, NULL, g_hInstance, NULL);

        // Start/Stop Button
        g_hBtnStartStop = CreateWindowA("BUTTON", "START (F6)",
            WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
            12, 445, 415, 42, hWnd, (HMENU)IDC_BTN_STARTSTOP, g_hInstance, NULL);

        // Status bar / text
        g_hStaticStatus = CreateWindowA("STATIC", "Status: Ready | Press F6 or Click Start",
            WS_CHILD | WS_VISIBLE | SS_CENTER,
            12, 495, 415, 20, hWnd, (HMENU)IDC_STATIC_STATUS, g_hInstance, NULL);

        // Defaults
        SendMessage(g_hRadioLeft, BM_SETCHECK, BST_CHECKED, 0);
        SendMessage(g_hRadioSingle, BM_SETCHECK, BST_CHECKED, 0);
        SendMessage(g_hRadioRepeatInf, BM_SETCHECK, BST_CHECKED, 0);
        EnableWindow(g_hEditRepeatCnt, FALSE);

        // Burst controls disabled by default until checked
        UpdateBurstControlsState();

        // Register Global Hotkey (F6)
        if (!RegisterHotKey(hWnd, ID_HOTKEY_F6, 0, VK_F6)) {
            MessageBoxA(hWnd, "Warning: Could not register global hotkey F6 (it might be used by another app).",
                        "Hotkey Warning", MB_ICONWARNING | MB_OK);
        }

        // Apply clean Segoe UI font
        NONCLIENTMETRICS ncm;
        ncm.cbSize = sizeof(NONCLIENTMETRICS);
        SystemParametersInfo(SPI_GETICONTITLELOGFONT, sizeof(LOGFONT), &ncm.lfMessageFont, 0);
        g_hFont = CreateFontIndirect(&ncm.lfMessageFont);
        if (!g_hFont) {
            g_hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
        }
        ApplyFontToChildren(hWnd, g_hFont);
        return 0;
    }

    case WM_COMMAND: {
        WORD id = LOWORD(wParam);
        if (id == IDC_BTN_STARTSTOP) {
            ToggleClicker();
        } else if (id == IDC_RADIO_REPEAT_INF || id == IDC_RADIO_REPEAT_CNT) {
            BOOL isRepeatCnt = (SendMessage(g_hRadioRepeatCnt, BM_GETCHECK, 0, 0) == BST_CHECKED);
            EnableWindow(g_hEditRepeatCnt, isRepeatCnt);
            if (isRepeatCnt) {
                SetFocus(g_hEditRepeatCnt);
            }
        } else if (id == IDC_CHK_BURST_ENABLE) {
            UpdateBurstControlsState();
        }
        return 0;
    }

    case WM_HOTKEY: {
        if (wParam == ID_HOTKEY_F6) {
            ToggleClicker();
        }
        return 0;
    }

    case WM_CLICKER_UPDATE: {
        UpdateStatusUI();
        return 0;
    }

    case WM_CLICKER_FINISHED: {
        StopClicker();
        return 0;
    }

    case WM_DESTROY: {
        UnregisterHotKey(hWnd, ID_HOTKEY_F6);
        StopClicker();
        if (g_hFont) DeleteObject(g_hFont);
        PostQuitMessage(0);
        return 0;
    }
    }
    return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    g_hInstance = hInstance;

    INITCOMMONCONTROLSEX icex;
    icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icex.dwICC = ICC_STANDARD_CLASSES;
    InitCommonControlsEx(&icex);

    WNDCLASSEXA wc = {0};
    wc.cbSize = sizeof(WNDCLASSEXA);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = "UltraLightAutoClickerClass";
    wc.hIconSm = LoadIcon(NULL, IDI_APPLICATION);

    if (!RegisterClassExA(&wc)) {
        MessageBoxA(NULL, "Window Registration Failed!", "Error", MB_ICONEXCLAMATION | MB_OK);
        return 0;
    }

    // Compute window size from desired client area (440 x 525)
    RECT rc = {0, 0, 440, 525};
    DWORD dwStyle = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
    DWORD dwExStyle = WS_EX_TOPMOST;
    AdjustWindowRectEx(&rc, dwStyle, FALSE, dwExStyle);

    int w = rc.right - rc.left;
    int h = rc.bottom - rc.top;
    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);
    int posX = (screenW - w) / 2;
    int posY = (screenH - h) / 2;

    g_hMainWnd = CreateWindowExA(
        dwExStyle,
        wc.lpszClassName,
        "AutoClicker (Lightweight)",
        dwStyle | WS_VISIBLE,
        posX, posY, w, h,
        NULL, NULL, hInstance, NULL
    );

    if (!g_hMainWnd) {
        MessageBoxA(NULL, "Window Creation Failed!", "Error", MB_ICONEXCLAMATION | MB_OK);
        return 0;
    }

    ShowWindow(g_hMainWnd, nCmdShow);
    UpdateWindow(g_hMainWnd);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        // Allow TAB navigation between inputs
        if (!IsDialogMessage(g_hMainWnd, &msg)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }
    return (int)msg.wParam;
}
