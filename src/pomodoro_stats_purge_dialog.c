/**
 * @file pomodoro_stats_purge_dialog.c
 * @brief "Clear statistics" dialog: choose a time range and a project, then delete.
 */

#include "pomodoro_stats_internal.h"

#include <commctrl.h>
#include <stdio.h>

#include "dialog/dialog_modern.h"
#include "language.h"
#include "utils/string_convert.h"

#define PURGE_DIALOG_CLASS L"CatimePomodoroPurgeDialog"
#define PURGE_CLIENT_WIDTH 360
#define PURGE_CLIENT_HEIGHT 240

#define PURGE_ID_RANGE_ALL 4201
#define PURGE_ID_RANGE_TODAY 4202
#define PURGE_ID_RANGE_WEEK 4203
#define PURGE_ID_PROJECT 4204
#define PURGE_ID_CLEAR 4205
#define PURGE_ID_CANCEL 4206
#define PURGE_ID_LABEL_RANGE 4207
#define PURGE_ID_LABEL_PROJECT 4208

typedef struct {
    HWND hwnd;
    HWND projectCombo;
    HBRUSH backgroundBrush;
    char projects[POMODORO_STATS_MAX_PROJECTS][POMODORO_STATS_NAME_MAX];
    int projectCount;
} PurgeDialogState;

static PurgeDialogState g_purge;

static PomodoroStatsRange PurgeSelectedRange(void) {
    if (IsDlgButtonChecked(g_purge.hwnd, PURGE_ID_RANGE_TODAY) == BST_CHECKED) {
        return POMODORO_STATS_RANGE_TODAY;
    }
    if (IsDlgButtonChecked(g_purge.hwnd, PURGE_ID_RANGE_WEEK) == BST_CHECKED) {
        return POMODORO_STATS_RANGE_WEEK;
    }
    return POMODORO_STATS_RANGE_ALL;
}

static const char* PurgeSelectedProject(void) {
    LRESULT selection = SendMessageW(g_purge.projectCombo, CB_GETCURSEL, 0, 0);
    if (selection <= 0 || (int)selection > g_purge.projectCount) return NULL;
    return g_purge.projects[selection - 1];
}

static void PurgeFillProjects(void) {
    int index = 0;

    if (!g_purge.projectCombo) return;
    SendMessageW(g_purge.projectCombo, CB_ADDSTRING, 0,
                 (LPARAM)GetLocalizedString(NULL, L"All projects"));
    g_purge.projectCount = PomodoroStats_ListProjects(g_purge.projects);
    for (index = 0; index < g_purge.projectCount; index++) {
        wchar_t wide[POMODORO_STATS_NAME_MAX * 2] = L"";
        Utf8ToWide(g_purge.projects[index], wide, _countof(wide));
        SendMessageW(g_purge.projectCombo, CB_ADDSTRING, 0, (LPARAM)wide);
    }
    SendMessageW(g_purge.projectCombo, CB_SETCURSEL, 0, 0);
}

static void PurgeApply(void) {
    const wchar_t* title = GetLocalizedString(NULL, L"Clear statistics");
    wchar_t message[256] = L"";
    PomodoroStatsRange range = PurgeSelectedRange();
    const char* project = PurgeSelectedProject();
    int matched = PomodoroStats_CountRange(range, project);
    int removed = 0;

    _snwprintf_s(message, _countof(message), _TRUNCATE,
                 GetLocalizedString(NULL,
                     L"Delete %d records? This cannot be undone."),
                 matched);
    if (MessageBoxW(g_purge.hwnd, message, title,
                    MB_YESNO | MB_ICONWARNING) != IDYES) {
        return;
    }

    removed = PomodoroStats_Purge(range, project);
    _snwprintf_s(message, _countof(message), _TRUNCATE,
                 GetLocalizedString(NULL, L"Deleted %d records"), removed);
    MessageBoxW(g_purge.hwnd, message, title, MB_OK | MB_ICONINFORMATION);
    DestroyWindow(g_purge.hwnd);
}

static void PurgeLayout(void) {
    RECT client = {0};
    UINT dpi = DialogModern_GetDpi(g_purge.hwnd);

    if (!g_purge.hwnd || !GetClientRect(g_purge.hwnd, &client)) return;

    DialogModern_SetChildRect96(g_purge.hwnd, PURGE_ID_LABEL_RANGE, dpi, 16, 16, 160, 20);
    DialogModern_SetChildRect96(g_purge.hwnd, PURGE_ID_LABEL_PROJECT, dpi, 16, 74, 160, 20);
    DialogModern_SetChildRect96(g_purge.hwnd, PURGE_ID_RANGE_ALL, dpi, 16, 40, 90, 22);
    DialogModern_SetChildRect96(g_purge.hwnd, PURGE_ID_RANGE_TODAY, dpi, 116, 40, 90, 22);
    DialogModern_SetChildRect96(g_purge.hwnd, PURGE_ID_RANGE_WEEK, dpi, 216, 40, 120, 22);
    DialogModern_SetChildRect96(g_purge.hwnd, PURGE_ID_PROJECT, dpi, 16, 96, 328, 80);
    DialogModern_SetChildRect96(g_purge.hwnd, PURGE_ID_CLEAR, dpi, 16, 186, 110, 30);
    DialogModern_SetChildRect96(g_purge.hwnd, PURGE_ID_CANCEL, dpi, 234, 186, 110, 30);
}

static void PurgeCreateLabels(HWND hwnd) {
    HINSTANCE instance = GetModuleHandleW(NULL);
    DWORD style = WS_CHILD | WS_VISIBLE | SS_LEFT;

    CreateWindowExW(0, L"STATIC", GetLocalizedString(NULL, L"Range"),
                    style, 0, 0, 0, 0, hwnd,
                    (HMENU)(INT_PTR)PURGE_ID_LABEL_RANGE, instance, NULL);
    CreateWindowExW(0, L"STATIC", GetLocalizedString(NULL, L"Project"),
                    style, 0, 0, 0, 0, hwnd,
                    (HMENU)(INT_PTR)PURGE_ID_LABEL_PROJECT, instance, NULL);
}

static BOOL PurgeCreateControls(HWND hwnd) {
    HINSTANCE instance = GetModuleHandleW(NULL);
    DialogModernPalette palette = {0};
    DWORD radioStyle = WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTORADIOBUTTON;
    DWORD buttonStyle = WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON;

    PurgeCreateLabels(hwnd);

    CreateWindowExW(0, L"BUTTON", GetLocalizedString(NULL, L"All"),
                    radioStyle | WS_GROUP, 0, 0, 0, 0, hwnd,
                    (HMENU)(INT_PTR)PURGE_ID_RANGE_ALL, instance, NULL);
    CreateWindowExW(0, L"BUTTON", GetLocalizedString(NULL, L"Today"),
                    radioStyle, 0, 0, 0, 0, hwnd,
                    (HMENU)(INT_PTR)PURGE_ID_RANGE_TODAY, instance, NULL);
    CreateWindowExW(0, L"BUTTON", GetLocalizedString(NULL, L"Last 7 days"),
                    radioStyle, 0, 0, 0, 0, hwnd,
                    (HMENU)(INT_PTR)PURGE_ID_RANGE_WEEK, instance, NULL);
    CheckRadioButton(hwnd, PURGE_ID_RANGE_ALL, PURGE_ID_RANGE_WEEK,
                     PURGE_ID_RANGE_ALL);

    g_purge.projectCombo = CreateWindowExW(
        0, L"COMBOBOX", L"",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL | CBS_DROPDOWNLIST,
        0, 0, 0, 0, hwnd, (HMENU)(INT_PTR)PURGE_ID_PROJECT, instance, NULL);
    if (!g_purge.projectCombo) return FALSE;

    CreateWindowExW(0, L"BUTTON", GetLocalizedString(NULL, L"Clear statistics"),
                    buttonStyle, 0, 0, 0, 0, hwnd,
                    (HMENU)(INT_PTR)PURGE_ID_CLEAR, instance, NULL);
    CreateWindowExW(0, L"BUTTON", GetLocalizedString(NULL, L"Cancel"),
                    buttonStyle, 0, 0, 0, 0, hwnd,
                    (HMENU)(INT_PTR)PURGE_ID_CANCEL, instance, NULL);

    DialogModern_ResolvePalette(&palette);
    g_purge.backgroundBrush = CreateSolidBrush(palette.background);
    DialogModern_ApplyTheme(hwnd, palette.darkMode);
    PurgeFillProjects();
    return TRUE;
}

static LRESULT CALLBACK PurgeDialogProc(HWND hwnd, UINT message,
                                        WPARAM wp, LPARAM lp) {
    switch (message) {
    case WM_CREATE:
        g_purge.hwnd = hwnd;
        if (!PurgeCreateControls(hwnd)) {
            DestroyWindow(hwnd);
            return -1;
        }
        PurgeLayout();
        return 0;

    case WM_SIZE:
        PurgeLayout();
        return 0;

    case WM_CTLCOLORSTATIC: {
        DialogModernPalette palette = {0};
        DialogModern_ResolvePalette(&palette);
        SetTextColor((HDC)wp, palette.text);
        SetBkColor((HDC)wp, palette.background);
        SetBkMode((HDC)wp, TRANSPARENT);
        if (!g_purge.backgroundBrush) break;
        return (LRESULT)g_purge.backgroundBrush;
    }

    case WM_ERASEBKGND: {
        RECT client = {0};
        if (g_purge.backgroundBrush && GetClientRect(hwnd, &client)) {
            FillRect((HDC)wp, &client, g_purge.backgroundBrush);
        }
        return 1;
    }

    case WM_COMMAND:
        if (HIWORD(wp) != 0 && HIWORD(wp) != BN_CLICKED) break;
        switch (LOWORD(wp)) {
        case PURGE_ID_CLEAR:
            PurgeApply();
            return 0;
        case PURGE_ID_CANCEL:
            DestroyWindow(hwnd);
            return 0;
        default:
            break;
        }
        break;

    case WM_DESTROY:
        if (g_purge.backgroundBrush) {
            DeleteObject(g_purge.backgroundBrush);
            g_purge.backgroundBrush = NULL;
        }
        g_purge.hwnd = NULL;
        g_purge.projectCombo = NULL;
        return 0;

    default:
        break;
    }
    return DefWindowProcW(hwnd, message, wp, lp);
}

void PomodoroStats_ShowPurgeDialog(HWND owner, PomodoroStatsRange initialRange) {
    DWORD style = WS_OVERLAPPEDWINDOW & ~(WS_MAXIMIZEBOX | WS_THICKFRAME);
    HINSTANCE instance = GetModuleHandleW(NULL);
    WNDCLASSEXW existing = {0};
    WNDCLASSEXW windowClass = {0};
    UINT dpi = DialogModern_GetDpi(owner && IsWindow(owner) ? owner : NULL);
    RECT frame = {0, 0, DialogModern_Scale(dpi, PURGE_CLIENT_WIDTH),
                  DialogModern_Scale(dpi, PURGE_CLIENT_HEIGHT)};
    RECT anchor = {0};
    HWND hwnd = NULL;
    int checked = PURGE_ID_RANGE_ALL;
    int x = CW_USEDEFAULT;
    int y = CW_USEDEFAULT;

    if (initialRange == POMODORO_STATS_RANGE_TODAY) checked = PURGE_ID_RANGE_TODAY;
    if (initialRange == POMODORO_STATS_RANGE_WEEK) checked = PURGE_ID_RANGE_WEEK;

    if (g_purge.hwnd && IsWindow(g_purge.hwnd)) {
        SetForegroundWindow(g_purge.hwnd);
        return;
    }

    existing.cbSize = sizeof(existing);
    if (!GetClassInfoExW(instance, PURGE_DIALOG_CLASS, &existing)) {
        windowClass.cbSize = sizeof(windowClass);
        windowClass.style = CS_HREDRAW | CS_VREDRAW;
        windowClass.lpfnWndProc = PurgeDialogProc;
        windowClass.hInstance = instance;
        windowClass.hCursor = LoadCursorW(NULL, IDC_ARROW);
        windowClass.lpszClassName = PURGE_DIALOG_CLASS;
        if (!RegisterClassExW(&windowClass) &&
            GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
            return;
        }
    }

    AdjustWindowRectEx(&frame, style, FALSE, 0);
    if (owner && IsWindow(owner) && GetWindowRect(owner, &anchor)) {
        x = anchor.left + 60;
        y = anchor.top + 60;
    }
    hwnd = CreateWindowExW(0, PURGE_DIALOG_CLASS,
                           GetLocalizedString(NULL, L"Clear statistics"),
                           style, x, y, frame.right - frame.left,
                           frame.bottom - frame.top,
                           owner && IsWindow(owner) ? owner : NULL, NULL,
                           instance, NULL);
    if (!hwnd) return;
    CheckRadioButton(hwnd, PURGE_ID_RANGE_ALL, PURGE_ID_RANGE_WEEK, checked);
    ShowWindow(hwnd, SW_SHOWNORMAL);
    UpdateWindow(hwnd);
}
