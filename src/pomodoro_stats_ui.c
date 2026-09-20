/**
 * @file pomodoro_stats_ui.c
 * @brief Small shared helpers for the pomodoro statistics window.
 */

#include "pomodoro_stats_internal.h"

#include <shellapi.h>

#include "utils/string_convert.h"

static const COLORREF kSliceColors[POMODORO_STATS_SLICE_COUNT] = {
    RGB(255, 107, 91), RGB(78, 203, 113), RGB(86, 156, 214), RGB(220, 170, 60),
    RGB(180, 120, 220), RGB(90, 200, 210), RGB(230, 120, 170), RGB(150, 160, 180)
};

int PomodoroStats_UiScale(const PomodoroStatsUi* ui, int value) {
    return DialogModern_Scale(ui ? ui->dpi : 96u, value);
}

void PomodoroStats_UiFill(HDC hdc, const RECT* rect, COLORREF color) {
    HBRUSH brush = NULL;

    if (!hdc || !rect) return;
    brush = CreateSolidBrush(color);
    if (!brush) return;
    FillRect(hdc, rect, brush);
    DeleteObject(brush);
}

COLORREF PomodoroStats_UiSliceColor(int index) {
    if (index < 0) index = 0;
    return kSliceColors[index % POMODORO_STATS_SLICE_COUNT];
}

void PomodoroStats_UiProjectName(const char* name, wchar_t* out, size_t outSize) {
    if (!out || outSize == 0) return;
    out[0] = L'\0';
    Utf8ToWide(name, out, outSize);
}

void PomodoroStats_UiOpenDataFolder(void) {
    char directory[MAX_PATH] = {0};
    wchar_t wideDirectory[MAX_PATH] = L"";

    PomodoroStats_DataDir(directory, sizeof(directory));
    if (directory[0] == '\0') return;
    Utf8ToWide(directory, wideDirectory, _countof(wideDirectory));
    if (wideDirectory[0] == L'\0') return;
    ShellExecuteW(NULL, L"open", wideDirectory, NULL, NULL, SW_SHOWNORMAL);
}
