/**
 * @file pomodoro_stats_projects_dialog.c
 * @brief Editor window for the pomodoro project list (pomodoro_projects.txt).
 */

#include "pomodoro_stats_internal.h"

#include <commctrl.h>
#include <stdio.h>
#include <string.h>

#include "dialog/dialog_modern.h"
#include "language.h"
#include "utils/string_convert.h"

#define PROJECTS_DIALOG_CLASS L"CatimePomodoroProjectsDialog"
#define PROJECTS_CLIENT_WIDTH 380
#define PROJECTS_CLIENT_HEIGHT 290

#define PROJECTS_ID_LIST 4101
#define PROJECTS_ID_EDIT 4102
#define PROJECTS_ID_ADD 4103
#define PROJECTS_ID_REMOVE 4104
#define PROJECTS_ID_DONE 4105

typedef struct {
    HWND hwnd;
    HWND list;
    HWND edit;
    char names[POMODORO_STATS_MAX_PROJECTS][POMODORO_STATS_NAME_MAX];
    int count;
} ProjectsDialogState;

static ProjectsDialogState g_projects;

static void ProjectsCopyName(char* destination, const char* source) {
    size_t length = 0;
    if (!destination || !source) return;
    length = strlen(source);
    if (length >= POMODORO_STATS_NAME_MAX) {
        length = POMODORO_STATS_NAME_MAX - 1;
    }
    memcpy(destination, source, length);
    destination[length] = '\0';
}

static void ProjectsSyncList(void) {
    int index = 0;
    if (!g_projects.list) return;
    SendMessageW(g_projects.list, LB_RESETCONTENT, 0, 0);
    for (index = 0; index < g_projects.count; index++) {
        wchar_t wide[POMODORO_STATS_NAME_MAX * 2] = L"";
        Utf8ToWide(g_projects.names[index], wide, _countof(wide));
        SendMessageW(g_projects.list, LB_ADDSTRING, 0, (LPARAM)wide);
    }
}

static void ProjectsLoad(void) {
    g_projects.count = PomodoroStats_ListProjects(g_projects.names);
    ProjectsSyncList();
}

static void ProjectsAdd(void) {
    wchar_t wide[POMODORO_STATS_NAME_MAX * 2] = L"";
    char name[POMODORO_STATS_NAME_MAX] = {0};
    char trimmed[POMODORO_STATS_NAME_MAX] = {0};
    const char* start = NULL;
    size_t length = 0;
    int index = 0;

    if (!g_projects.edit) return;
    GetWindowTextW(g_projects.edit, wide, (int)_countof(wide));
    if (wide[0] == L'\0') return;
    if (WideCharToMultiByte(CP_UTF8, 0, wide, -1, name, (int)sizeof(name),
                            NULL, NULL) <= 0) {
        return;
    }

    start = name;
    while (*start == ' ' || *start == '\t') start++;
    length = strlen(start);
    while (length > 0 && (start[length - 1] == ' ' || start[length - 1] == '\t')) {
        length--;
    }
    if (length == 0) return;
    if (length >= sizeof(trimmed)) length = sizeof(trimmed) - 1;
    memcpy(trimmed, start, length);
    trimmed[length] = '\0';

    for (index = 0; index < g_projects.count; index++) {
        if (strcmp(g_projects.names[index], trimmed) == 0) break;
    }
    if (index >= g_projects.count) {
        if (g_projects.count >= POMODORO_STATS_MAX_PROJECTS) return;
        ProjectsCopyName(g_projects.names[g_projects.count], trimmed);
        g_projects.count++;
        PomodoroStats_SaveProjects(g_projects.names, g_projects.count);
    }

    SetWindowTextW(g_projects.edit, L"");
    ProjectsSyncList();
    SendMessageW(g_projects.list, LB_SETCURSEL, (WPARAM)index, 0);
}

static void ProjectsRemove(void) {
    LRESULT selection = SendMessageW(g_projects.list, LB_GETCURSEL, 0, 0);
    int index = (int)selection;
    int step = 0;

    if (index < 0 || index >= g_projects.count) return;
    for (step = index; step + 1 < g_projects.count; step++) {
        memcpy(g_projects.names[step], g_projects.names[step + 1], POMODORO_STATS_NAME_MAX);
    }
    memset(g_projects.names[g_projects.count - 1], 0, POMODORO_STATS_NAME_MAX);
    g_projects.count--;
    PomodoroStats_SaveProjects(g_projects.names, g_projects.count);
    ProjectsSyncList();
    if (g_projects.count > 0) {
        int next = index < g_projects.count ? index : g_projects.count - 1;
        SendMessageW(g_projects.list, LB_SETCURSEL, (WPARAM)next, 0);
    }
}

static void ProjectsLayout(void) {
    RECT client = {0};
    UINT dpi = DialogModern_GetDpi(g_projects.hwnd);
    if (!g_projects.hwnd || !GetClientRect(g_projects.hwnd, &client)) return;

    DialogModern_SetChildRect96(g_projects.hwnd, PROJECTS_ID_LIST, dpi, 16, 16, 348, 170);
    DialogModern_SetChildRect96(g_projects.hwnd, PROJECTS_ID_EDIT, dpi, 16, 196, 230, 28);
    DialogModern_SetChildRect96(g_projects.hwnd, PROJECTS_ID_ADD, dpi, 254, 196, 110, 28);
    DialogModern_SetChildRect96(g_projects.hwnd, PROJECTS_ID_REMOVE, dpi, 16, 236, 110, 28);
    DialogModern_SetChildRect96(g_projects.hwnd, PROJECTS_ID_DONE, dpi, 254, 236, 110, 28);
}

static BOOL ProjectsCreateControls(HWND hwnd) {
    HINSTANCE instance = GetModuleHandleW(NULL);
    DialogModernPalette palette = {0};
    DWORD buttonStyle = WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON;

    g_projects.list = CreateWindowExW(WS_EX_CLIENTEDGE, L"LISTBOX", L"",
                                      WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL |
                                      LBS_NOTIFY,
                                      0, 0, 0, 0, hwnd,
                                      (HMENU)(INT_PTR)PROJECTS_ID_LIST, instance, NULL);
    g_projects.edit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
                                      WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
                                      0, 0, 0, 0, hwnd,
                                      (HMENU)(INT_PTR)PROJECTS_ID_EDIT, instance, NULL);
    if (!g_projects.list || !g_projects.edit) return FALSE;

    CreateWindowExW(0, L"BUTTON", GetLocalizedString(NULL, L"Add"), buttonStyle,
                    0, 0, 0, 0, hwnd, (HMENU)(INT_PTR)PROJECTS_ID_ADD, instance, NULL);
    CreateWindowExW(0, L"BUTTON", GetLocalizedString(NULL, L"Remove"), buttonStyle,
                    0, 0, 0, 0, hwnd, (HMENU)(INT_PTR)PROJECTS_ID_REMOVE, instance, NULL);
    CreateWindowExW(0, L"BUTTON", GetLocalizedString(NULL, L"Done"), buttonStyle,
                    0, 0, 0, 0, hwnd, (HMENU)(INT_PTR)PROJECTS_ID_DONE, instance, NULL);

    SendMessageW(g_projects.edit, EM_SETCUEBANNER, TRUE,
                 (LPARAM)GetLocalizedString(NULL, L"New project name"));
    DialogModern_ResolvePalette(&palette);
    DialogModern_ApplyTheme(hwnd, palette.darkMode);
    return TRUE;
}

static LRESULT CALLBACK ProjectsDialogProc(HWND hwnd, UINT message,
                                           WPARAM wp, LPARAM lp) {
    switch (message) {
    case WM_CREATE:
        g_projects.hwnd = hwnd;
        if (!ProjectsCreateControls(hwnd)) {
            DestroyWindow(hwnd);
            return -1;
        }
        ProjectsLoad();
        ProjectsLayout();
        SetFocus(g_projects.edit);
        return 0;

    case WM_SIZE:
        ProjectsLayout();
        return 0;

    case WM_ERASEBKGND: {
        DialogModernPalette palette = {0};
        RECT client = {0};
        HBRUSH brush = NULL;
        DialogModern_ResolvePalette(&palette);
        brush = CreateSolidBrush(palette.background);
        if (brush && GetClientRect(hwnd, &client)) {
            FillRect((HDC)wp, &client, brush);
        }
        if (brush) DeleteObject(brush);
        return 1;
    }

    case WM_COMMAND: {
        UINT notification = HIWORD(wp);
        if (notification != 0 && notification != BN_CLICKED) break;
        switch (LOWORD(wp)) {
        case PROJECTS_ID_ADD:
            ProjectsAdd();
            return 0;
        case PROJECTS_ID_REMOVE:
            ProjectsRemove();
            return 0;
        case PROJECTS_ID_DONE:
            DestroyWindow(hwnd);
            return 0;
        case PROJECTS_ID_LIST:
            if (notification == LBN_DBLCLK) {
                SetFocus(g_projects.edit);
                return 0;
            }
            break;
        default:
            break;
        }
        break;
    }

    case WM_DESTROY:
        g_projects.hwnd = NULL;
        g_projects.list = NULL;
        g_projects.edit = NULL;
        return 0;

    default:
        break;
    }
    return DefWindowProcW(hwnd, message, wp, lp);
}

void PomodoroStats_ShowProjectsDialog(HWND owner) {
    DWORD style = WS_OVERLAPPEDWINDOW & ~(WS_MAXIMIZEBOX | WS_THICKFRAME);
    HINSTANCE instance = GetModuleHandleW(NULL);
    WNDCLASSEXW existing = {0};
    WNDCLASSEXW windowClass = {0};
    HWND hwnd = NULL;
    UINT dpi = DialogModern_GetDpi(owner && IsWindow(owner) ? owner : NULL);
    RECT frame = {0, 0, DialogModern_Scale(dpi, PROJECTS_CLIENT_WIDTH),
                  DialogModern_Scale(dpi, PROJECTS_CLIENT_HEIGHT)};
    RECT anchor = {0};
    int x = 0;
    int y = 0;

    if (g_projects.hwnd && IsWindow(g_projects.hwnd)) {
        SetForegroundWindow(g_projects.hwnd);
        return;
    }

    existing.cbSize = sizeof(existing);
    if (!GetClassInfoExW(instance, PROJECTS_DIALOG_CLASS, &existing)) {
        windowClass.cbSize = sizeof(windowClass);
        windowClass.style = CS_HREDRAW | CS_VREDRAW;
        windowClass.lpfnWndProc = ProjectsDialogProc;
        windowClass.hInstance = instance;
        windowClass.hCursor = LoadCursorW(NULL, IDC_ARROW);
        windowClass.lpszClassName = PROJECTS_DIALOG_CLASS;
        if (!RegisterClassExW(&windowClass) &&
            GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
            return;
        }
    }

    AdjustWindowRectEx(&frame, style, FALSE, 0);
    x = CW_USEDEFAULT;
    y = CW_USEDEFAULT;
    if (owner && IsWindow(owner) && GetWindowRect(owner, &anchor)) {
        x = anchor.left + 40;
        y = anchor.top + 40;
    }
    hwnd = CreateWindowExW(0, PROJECTS_DIALOG_CLASS,
                           GetLocalizedString(NULL, L"Manage projects"),
                           style, x, y, frame.right - frame.left, frame.bottom - frame.top,
                           owner && IsWindow(owner) ? owner : NULL, NULL, instance, NULL);
    if (!hwnd) return;
    ShowWindow(hwnd, SW_SHOWNORMAL);
    UpdateWindow(hwnd);
}
