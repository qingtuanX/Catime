/**
 * @file pomodoro_stats_window.c
 * @brief Window creation, controls, and message handling of the statistics view.
 */

#include "pomodoro_stats_internal.h"

#include "dialog/dialog_modern.h"
#include "language.h"

#define STATS_CLIENT_WIDTH 940
#define STATS_CLIENT_HEIGHT 560
#define STATS_MIN_CLIENT_WIDTH 760
#define STATS_MIN_CLIENT_HEIGHT 460
#define STATS_REFRESH_TIMER_ID 1

typedef struct {
    UINT id;
    const wchar_t* key;
    BOOL isChip;
} StatsButtonSpec;

static const StatsButtonSpec kStatsButtons[] = {
    {POMODORO_STATS_ID_RANGE_ALL, L"All", TRUE},
    {POMODORO_STATS_ID_RANGE_TODAY, L"Today", TRUE},
    {POMODORO_STATS_ID_RANGE_WEEK, L"Last 7 days", TRUE},
    {POMODORO_STATS_ID_REFRESH, L"Refresh", FALSE},
    {POMODORO_STATS_ID_CLEAR, L"Clear statistics", FALSE},
    {POMODORO_STATS_ID_MANAGE, L"Manage projects", FALSE},
    {POMODORO_STATS_ID_OPEN_FOLDER, L"Open data folder", FALSE},
};

static PomodoroStatsUi g_stats;

static LRESULT CALLBACK StatsWindowProc(HWND hwnd, UINT message,
                                        WPARAM wp, LPARAM lp);

static PomodoroStatsRange StatsRangeForId(UINT id) {
    if (id == POMODORO_STATS_ID_RANGE_TODAY) return POMODORO_STATS_RANGE_TODAY;
    if (id == POMODORO_STATS_ID_RANGE_WEEK) return POMODORO_STATS_RANGE_WEEK;
    return POMODORO_STATS_RANGE_ALL;
}

static void StatsRefresh(void) {
    PomodoroStats_BuildReport(g_stats.range, &g_stats.report);
    if (g_stats.hwnd) {
        InvalidateRect(g_stats.hwnd, NULL, FALSE);
    }
}

static void StatsDestroyFonts(void) {
    if (g_stats.bodyFont) {
        DeleteObject(g_stats.bodyFont);
        g_stats.bodyFont = NULL;
    }
    if (g_stats.titleFont) {
        DeleteObject(g_stats.titleFont);
        g_stats.titleFont = NULL;
    }
}

static void StatsCreateFonts(void) {
    StatsDestroyFonts();
    g_stats.bodyFont = DialogModern_CreateFont(g_stats.dpi, 15, FW_NORMAL);
    g_stats.titleFont = DialogModern_CreateFont(g_stats.dpi, 20, FW_SEMIBOLD);
}

static void StatsCreateButtons(HWND hwnd) {
    size_t index = 0;
    HINSTANCE instance = GetModuleHandleW(NULL);

    for (index = 0; index < sizeof(kStatsButtons) / sizeof(kStatsButtons[0]); index++) {
        CreateWindowExW(0, L"BUTTON", GetLocalizedString(NULL, kStatsButtons[index].key),
                        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
                        0, 0, 0, 0, hwnd, (HMENU)(INT_PTR)kStatsButtons[index].id,
                        instance, NULL);
    }
}

static void StatsCenterOnOwnerMonitor(HWND hwnd, HWND owner, int width, int height) {
    MONITORINFO info = {0};
    HMONITOR monitor = MonitorFromWindow(owner && IsWindow(owner) ? owner : GetForegroundWindow(),
                                         MONITOR_DEFAULTTONEAREST);

    info.cbSize = sizeof(info);
    if (!monitor || !GetMonitorInfoW(monitor, &info)) return;
    SetWindowPos(hwnd, NULL,
                 info.rcWork.left + ((info.rcWork.right - info.rcWork.left) - width) / 2,
                 info.rcWork.top + ((info.rcWork.bottom - info.rcWork.top) - height) / 2,
                 width, height, SWP_NOZORDER | SWP_NOACTIVATE);
}

static void StatsHandleDrawItem(const DRAWITEMSTRUCT* item) {
    size_t index = 0;
    const wchar_t* text = NULL;
    BOOL selected = FALSE;
    RECT rect;

    if (!item) return;
    for (index = 0; index < sizeof(kStatsButtons) / sizeof(kStatsButtons[0]); index++) {
        if (kStatsButtons[index].id == item->CtlID) {
            text = GetLocalizedString(NULL, kStatsButtons[index].key);
            break;
        }
    }
    if (!text) return;
    if (kStatsButtons[index].isChip) {
        selected = StatsRangeForId(item->CtlID) == g_stats.range;
    }
    rect = item->rcItem;
    PomodoroStats_PaintButton(item->hDC, &g_stats, &rect, text, selected, item->itemState);
}

static LRESULT CALLBACK StatsWindowProc(HWND hwnd, UINT message, WPARAM wp, LPARAM lp) {
    switch (message) {
    case WM_CREATE:
        g_stats.hwnd = hwnd;
        g_stats.dpi = DialogModern_GetDpi(hwnd);
        g_stats.range = POMODORO_STATS_RANGE_ALL;
        StatsCreateFonts();
        StatsCreateButtons(hwnd);
        StatsRefresh();
        return 0;

    case WM_SIZE:
        PomodoroStats_UiLayout(&g_stats);
        InvalidateRect(hwnd, NULL, FALSE);
        return 0;

    case WM_GETMINMAXINFO: {
        MINMAXINFO* limits = (MINMAXINFO*)lp;
        RECT frame = {0, 0, STATS_MIN_CLIENT_WIDTH, STATS_MIN_CLIENT_HEIGHT};
        if (!limits) break;
        frame.right = DialogModern_Scale(g_stats.dpi, STATS_MIN_CLIENT_WIDTH);
        frame.bottom = DialogModern_Scale(g_stats.dpi, STATS_MIN_CLIENT_HEIGHT);
        AdjustWindowRectEx(&frame, WS_OVERLAPPEDWINDOW, FALSE, 0);
        limits->ptMinTrackSize.x = frame.right - frame.left;
        limits->ptMinTrackSize.y = frame.bottom - frame.top;
        return 0;
    }

    case WM_DPICHANGED: {
        RECT* suggested = (RECT*)lp;
        g_stats.dpi = DialogModern_GetDpi(hwnd);
        StatsCreateFonts();
        if (suggested) {
            SetWindowPos(hwnd, NULL, suggested->left, suggested->top,
                         suggested->right - suggested->left,
                         suggested->bottom - suggested->top,
                         SWP_NOZORDER | SWP_NOACTIVATE);
        }
        PomodoroStats_UiLayout(&g_stats);
        InvalidateRect(hwnd, NULL, TRUE);
        return 0;
    }

    case WM_SHOWWINDOW:
        if (wp) {
            SetTimer(hwnd, STATS_REFRESH_TIMER_ID, POMODORO_STATS_REFRESH_MS, NULL);
            StatsRefresh();
        } else {
            KillTimer(hwnd, STATS_REFRESH_TIMER_ID);
        }
        return 0;

    case WM_TIMER:
        if (wp == STATS_REFRESH_TIMER_ID) {
            StatsRefresh();
            return 0;
        }
        break;

    case WM_ERASEBKGND:
        return 1;

    case WM_DRAWITEM:
        StatsHandleDrawItem((const DRAWITEMSTRUCT*)lp);
        return TRUE;

    case WM_COMMAND:
        switch (LOWORD(wp)) {
        case POMODORO_STATS_ID_RANGE_ALL:
        case POMODORO_STATS_ID_RANGE_TODAY:
        case POMODORO_STATS_ID_RANGE_WEEK: {
            PomodoroStatsRange range = StatsRangeForId(LOWORD(wp));
            if (range != g_stats.range) {
                g_stats.range = range;
                StatsRefresh();
                RedrawWindow(hwnd, NULL, NULL, RDW_INVALIDATE | RDW_ALLCHILDREN);
            }
            return 0;
        }
        case POMODORO_STATS_ID_REFRESH:
            StatsRefresh();
            return 0;
        case POMODORO_STATS_ID_CLEAR:
            PomodoroStats_ShowPurgeDialog(hwnd, g_stats.range);
            StatsRefresh();
            return 0;
        case POMODORO_STATS_ID_MANAGE:
            PomodoroStats_ShowProjectsDialog(hwnd);
            StatsRefresh();
            return 0;
        case POMODORO_STATS_ID_OPEN_FOLDER:
            PomodoroStats_UiOpenDataFolder();
            return 0;
        default:
            break;
        }
        break;

    case WM_PAINT:
        PomodoroStats_PaintWindow(hwnd, &g_stats);
        return 0;

    case WM_DESTROY:
        KillTimer(hwnd, STATS_REFRESH_TIMER_ID);
        StatsDestroyFonts();
        g_stats.hwnd = NULL;
        return 0;

    default:
        break;
    }
    return DefWindowProcW(hwnd, message, wp, lp);
}

void PomodoroStats_ShowWindow(HWND owner) {
    DWORD style = WS_OVERLAPPEDWINDOW;
    HINSTANCE instance = GetModuleHandleW(NULL);
    WNDCLASSEXW existing = {0};
    WNDCLASSEXW windowClass = {0};
    HWND hwnd = NULL;
    UINT dpi = DialogModern_GetDpi(owner && IsWindow(owner) ? owner : NULL);
    RECT frame = {0, 0, DialogModern_Scale(dpi, STATS_CLIENT_WIDTH),
                  DialogModern_Scale(dpi, STATS_CLIENT_HEIGHT)};

    if (g_stats.hwnd && IsWindow(g_stats.hwnd)) {
        ShowWindow(g_stats.hwnd, SW_SHOWNORMAL);
        SetForegroundWindow(g_stats.hwnd);
        StatsRefresh();
        return;
    }

    existing.cbSize = sizeof(existing);
    if (!GetClassInfoExW(instance, POMODORO_STATS_WINDOW_CLASS, &existing)) {
        windowClass.cbSize = sizeof(windowClass);
        windowClass.style = CS_HREDRAW | CS_VREDRAW;
        windowClass.lpfnWndProc = StatsWindowProc;
        windowClass.hInstance = instance;
        windowClass.hCursor = LoadCursorW(NULL, IDC_ARROW);
        windowClass.hbrBackground = NULL;
        windowClass.lpszClassName = POMODORO_STATS_WINDOW_CLASS;
        if (!RegisterClassExW(&windowClass) &&
            GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
            return;
        }
    }

    AdjustWindowRectEx(&frame, style, FALSE, 0);
    hwnd = CreateWindowExW(0, POMODORO_STATS_WINDOW_CLASS,
                           GetLocalizedString(NULL, L"Pomodoro Statistics"),
                           style, CW_USEDEFAULT, CW_USEDEFAULT,
                           frame.right - frame.left, frame.bottom - frame.top,
                           NULL, NULL, instance, NULL);
    if (!hwnd) return;

    StatsCenterOnOwnerMonitor(hwnd, owner, frame.right - frame.left,
                              frame.bottom - frame.top);
    ShowWindow(hwnd, SW_SHOWNORMAL);
    UpdateWindow(hwnd);
}
