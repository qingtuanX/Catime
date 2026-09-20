/**
 * @file pomodoro_stats_paint.c
 * @brief Composes and presents the statistics window content.
 */

#include "pomodoro_stats_internal.h"

#include "language.h"

/* The chart area reserves this share of its height for its title. */
#define STATS_CHART_TITLE_PERCENT 14

void PomodoroStats_PaintContent(HDC hdc, const PomodoroStatsUi* ui, const RECT* client) {
    DialogModernPalette palette = {0};
    RECT chartTitle;
    int chartHeight = 0;
    int titleHeight = 0;
    int chartTop = 0;

    if (!hdc || !ui || !client) return;
    DialogModern_ResolvePalette(&palette);

    PomodoroStats_UiFill(hdc, client, palette.background);
    PomodoroStats_UiFill(hdc, &ui->summaryRect, palette.surface);

    PomodoroStats_DrawTable(hdc, ui, &palette, &ui->tableRect);

    chartHeight = (int)(ui->chartRect.bottom - ui->chartRect.top);
    titleHeight = (chartHeight * STATS_CHART_TITLE_PERCENT) / 100;
    chartTop = titleHeight + PomodoroStats_UiScale(ui, 4);

    chartTitle = ui->chartRect;
    chartTitle.bottom = chartTitle.top + titleHeight;
    DialogModern_DrawText(hdc, ui->titleFont, palette.text, &chartTitle,
                          GetLocalizedString(NULL, L"Share of time"),
                          DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    PomodoroStats_DrawPie(hdc, ui, &palette, &ui->chartRect, chartTop);
    PomodoroStats_DrawLegend(hdc, ui, &palette, &ui->chartRect,
                             chartTop + (chartHeight - titleHeight) / 2);
    PomodoroStats_DrawSummary(hdc, ui, &palette, &ui->summaryRect);
}

void PomodoroStats_PaintButton(HDC hdc, const PomodoroStatsUi* ui, const RECT* rect,
                               const wchar_t* text, BOOL selected, UINT state) {
    DialogModernPalette palette = {0};
    COLORREF fill = 0;
    COLORREF border = 0;
    COLORREF color = 0;
    RECT face;

    if (!hdc || !ui || !rect) return;
    DialogModern_ResolvePalette(&palette);

    PomodoroStats_UiFill(hdc, rect, palette.background);

    face = *rect;
    face.right -= 1;
    face.bottom -= 1;

    if (selected) {
        fill = palette.accent;
        border = palette.accent;
        color = palette.background;
    } else {
        fill = (state & ODS_FOCUS) ? palette.surface : palette.background;
        border = palette.border;
        color = (state & ODS_DISABLED) ? palette.mutedText : palette.text;
    }
    if (state & ODS_SELECTED) {
        fill = selected ? palette.accentHover : palette.surface;
    }

    DialogModern_DrawRoundedRect(hdc, &face, DialogModern_Scale(ui->dpi, 6), fill, border, 1);
    DialogModern_DrawText(hdc, ui->bodyFont, color, &face, text,
                          DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
}

void PomodoroStats_PaintWindow(HWND hwnd, const PomodoroStatsUi* ui) {
    PAINTSTRUCT ps = {0};
    HDC hdc = NULL;
    RECT client = {0};

    if (!hwnd || !ui) return;
    hdc = BeginPaint(hwnd, &ps);
    if (hdc && GetClientRect(hwnd, &client)) {
        HDC memoryDc = CreateCompatibleDC(hdc);
        HBITMAP bitmap = memoryDc
            ? CreateCompatibleBitmap(hdc, (int)client.right, (int)client.bottom) : NULL;
        if (memoryDc && bitmap) {
            HGDIOBJ previous = SelectObject(memoryDc, bitmap);
            PomodoroStats_PaintContent(memoryDc, ui, &client);
            BitBlt(hdc, 0, 0, (int)client.right, (int)client.bottom, memoryDc, 0, 0, SRCCOPY);
            SelectObject(memoryDc, previous);
        }
        if (bitmap) DeleteObject(bitmap);
        if (memoryDc) DeleteDC(memoryDc);
    }
    EndPaint(hwnd, &ps);
}
