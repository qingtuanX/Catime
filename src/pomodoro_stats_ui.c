/**
 * @file pomodoro_stats_ui.c
 * @brief Small shared helpers for the pomodoro statistics window.
 */

#include "pomodoro_stats_internal.h"

#include <shellapi.h>
#include <string.h>

#include "color/color_picker_dialog.h"
#include "utils/string_convert.h"

#define POMODORO_STATS_CUSTOM_COLORS 16

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

COLORREF PomodoroStats_UiColorFor(const PomodoroStatsUi* ui, const char* project,
                                  int index) {
    int position = 0;

    if (ui && project && *project) {
        for (position = 0; position < ui->colorCount; position++) {
            if (strcmp(ui->colors[position].name, project) == 0) {
                return ui->colors[position].color;
            }
        }
    }
    return PomodoroStats_DefaultColor(index);
}

BOOL PomodoroStats_UiPickColor(HWND owner, COLORREF initial, COLORREF* selected) {
    COLORREF customColors[POMODORO_STATS_CUSTOM_COLORS] = {0};
    size_t customCount = 1;

    if (!selected) return FALSE;
    customColors[0] = initial;
    return ModernColorPicker_Show(owner, initial, customColors,
                                  POMODORO_STATS_CUSTOM_COLORS, &customCount,
                                  selected);
}
