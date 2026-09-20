/**
 * @file pomodoro_stats_table.c
 * @brief Project table and summary bar of the statistics window.
 */

#include "pomodoro_stats_internal.h"

#include <stdio.h>

#include "language.h"

static void TableColumns(const PomodoroStatsUi* ui, const RECT* rect, int* offsets) {
    int padding = PomodoroStats_UiScale(ui, 12);
    int width = (int)(rect->right - rect->left) - padding;

    if (width < 4) width = 4;
    offsets[0] = (int)rect->left;
    offsets[1] = offsets[0] + (width * 36) / 100;
    offsets[2] = offsets[1] + (width * 14) / 100;
    offsets[3] = offsets[2] + (width * 33) / 100;
    offsets[4] = (int)rect->right - padding;
}

static void DrawCell(HDC hdc, const PomodoroStatsUi* ui, COLORREF color, int left, int right,
                     int top, int height, const wchar_t* text, UINT format) {
    RECT cell;

    cell.left = left;
    cell.right = right;
    cell.top = top;
    cell.bottom = top + height;
    DialogModern_DrawText(hdc, ui->bodyFont, color, &cell, text,
                          format | DT_VCENTER | DT_SINGLELINE);
}

void PomodoroStats_DrawTable(HDC hdc, const PomodoroStatsUi* ui,
                             const DialogModernPalette* palette, const RECT* rect) {
    const wchar_t* headerKeys[4] = {L"Project", L"Pomodoros", L"Total time", L"Share"};
    const UINT headerFormats[4] = {DT_LEFT, DT_RIGHT, DT_RIGHT, DT_RIGHT};
    int offsets[5] = {0};
    int padding = PomodoroStats_UiScale(ui, 12);
    int headerHeight = PomodoroStats_UiScale(ui, 30);
    int rowHeight = PomodoroStats_UiScale(ui, 26);
    int index = 0;
    int top = 0;
    RECT band;

    TableColumns(ui, rect, offsets);

    band.left = rect->left;
    band.right = rect->right;
    band.top = rect->top;
    band.bottom = rect->top + headerHeight;
    PomodoroStats_UiFill(hdc, &band, palette->surface);

    for (index = 0; index < 4; index++) {
        wchar_t text[64] = L"";
        const wchar_t* localized = GetLocalizedString(NULL, headerKeys[index]);
        if (localized) {
            _snwprintf_s(text, _countof(text), _TRUNCATE, L"%ls", localized);
        }
        DrawCell(hdc, ui, palette->mutedText,
                 offsets[index] + (index == 0 ? padding : 0), offsets[index + 1],
                 (int)rect->top, headerHeight, text, headerFormats[index]);
    }

    top = (int)rect->top + headerHeight;
    for (index = 0; index < ui->report.rowCount; index++) {
        const PomodoroStatsRow* row = &ui->report.rows[index];
        wchar_t name[POMODORO_STATS_NAME_MAX * 2] = L"";
        wchar_t count[24] = L"";
        wchar_t duration[64] = L"";
        wchar_t share[24] = L"";
        double ratio = ui->report.totalSeconds > 0
            ? (double)row->seconds / (double)ui->report.totalSeconds : 0.0;

        if (top + rowHeight > (int)rect->bottom) break;
        if ((index % 2) != 0) {
            band.left = rect->left;
            band.right = rect->right;
            band.top = top;
            band.bottom = top + rowHeight;
            PomodoroStats_UiFill(hdc, &band, palette->surface);
        }

        PomodoroStats_UiProjectName(row->name, name, _countof(name));
        _snwprintf_s(count, _countof(count), _TRUNCATE, L"%d", row->count);
        PomodoroStats_FormatDuration(row->seconds, duration, _countof(duration));
        _snwprintf_s(share, _countof(share), _TRUNCATE, L"%.1f%%", ratio * 100.0);

        DrawCell(hdc, ui, palette->text, offsets[0] + padding, offsets[1], top, rowHeight,
                 name, DT_LEFT | DT_END_ELLIPSIS);
        DrawCell(hdc, ui, palette->text, offsets[1], offsets[2], top, rowHeight, count,
                 DT_RIGHT);
        DrawCell(hdc, ui, palette->text, offsets[2], offsets[3], top, rowHeight, duration,
                 DT_RIGHT);
        DrawCell(hdc, ui, palette->accent, offsets[3], offsets[4], top, rowHeight, share,
                 DT_RIGHT);
        top += rowHeight;
    }
}

void PomodoroStats_DrawSummary(HDC hdc, const PomodoroStatsUi* ui,
                               const DialogModernPalette* palette, const RECT* rect) {
    wchar_t duration[64] = L"";
    wchar_t summary[160] = L"";
    wchar_t path[192] = L"";
    wchar_t widePath[MAX_PATH] = L"";
    RECT left = *rect;
    RECT right = *rect;

    PomodoroStats_FormatDuration(ui->report.totalSeconds, duration, _countof(duration));
    _snwprintf_s(summary, _countof(summary), _TRUNCATE,
                 GetLocalizedString(NULL, L"Total: %d pomodoros \u00b7 %s \u00b7 %d projects"),
                 ui->report.totalCount, duration, ui->report.rowCount);

    PomodoroStats_UiProjectName(PomodoroStats_StatsFilePath(), widePath,
                                _countof(widePath));
    _snwprintf_s(path, _countof(path), _TRUNCATE,
                 GetLocalizedString(NULL, L"Data file: %s"), widePath);

    left.right = (int)rect->left + ((int)(rect->right - rect->left) * 60) / 100;
    DialogModern_DrawText(hdc, ui->bodyFont, palette->text, &left, summary,
                          DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    right.left = left.right;
    DialogModern_DrawText(hdc, ui->bodyFont, palette->mutedText, &right, path,
                          DT_RIGHT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
}
