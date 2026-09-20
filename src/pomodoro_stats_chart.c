/**
 * @file pomodoro_stats_chart.c
 * @brief Pie chart and legend of the statistics window.
 */

#include "pomodoro_stats_internal.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "language.h"

#define STATS_PI 3.14159265358979323846
#define STATS_ARC_POINTS 32

/* Legend geometry of the last paint, used to hit-test clicks. */
static RECT s_legendRects[POMODORO_STATS_MAX_PROJECTS];
static int s_legendCount = 0;

/* Fill one clockwise sector. A polygon fan avoids GDI's arc-direction rules. */
static void FillSlice(HDC hdc, int centerX, int centerY, int radius,
                      double startAngle, double endAngle) {
    POINT points[STATS_ARC_POINTS + 2];
    double step = (endAngle - startAngle) / (STATS_ARC_POINTS - 1);
    int count = 0;
    int index = 0;

    points[count].x = centerX;
    points[count].y = centerY;
    count++;
    for (index = 0; index < STATS_ARC_POINTS; index++) {
        double angle = startAngle + step * index;
        points[count].x = centerX + (int)(radius * cos(angle));
        points[count].y = centerY + (int)(radius * sin(angle));
        count++;
    }
    Polygon(hdc, points, count);
}

void PomodoroStats_DrawPie(HDC hdc, const PomodoroStatsUi* ui,
                           const DialogModernPalette* palette, const RECT* rect,
                           int topOffset) {
    int width = (int)(rect->right - rect->left);
    int height = (int)(rect->bottom - rect->top) - topOffset;
    int diameter = width - PomodoroStats_UiScale(ui, 32);
    int limit = (height * 50) / 100;
    int centerX = 0;
    int centerY = 0;
    double startAngle = -STATS_PI / 2.0;
    int index = 0;
    HGDIOBJ previousPen = NULL;
    HPEN edgePen = NULL;

    if (diameter > limit) diameter = limit;
    if (diameter < PomodoroStats_UiScale(ui, 80)) {
        diameter = PomodoroStats_UiScale(ui, 80);
    }

    centerX = (int)rect->left + width / 2;
    centerY = (int)rect->top + topOffset + limit / 2;

    edgePen = CreatePen(PS_SOLID, 1, palette->background);
    if (edgePen) previousPen = SelectObject(hdc, edgePen);

    for (index = 0; index < ui->report.rowCount && ui->report.totalSeconds > 0; index++) {
        const PomodoroStatsRow* row = &ui->report.rows[index];
        double sweep = 2.0 * STATS_PI *
            ((double)row->seconds / (double)ui->report.totalSeconds);
        double endAngle = startAngle + sweep;
        HBRUSH brush = CreateSolidBrush(PomodoroStats_UiColorFor(ui, row->name, index));
        HGDIOBJ previousBrush = NULL;

        if (!brush) break;
        previousBrush = SelectObject(hdc, brush);
        FillSlice(hdc, centerX, centerY, diameter / 2, startAngle, endAngle);
        SelectObject(hdc, previousBrush);
        DeleteObject(brush);
        startAngle = endAngle;
    }

    if (edgePen) {
        SelectObject(hdc, previousPen);
        DeleteObject(edgePen);
    }
}

void PomodoroStats_DrawLegend(HDC hdc, const PomodoroStatsUi* ui,
                              const DialogModernPalette* palette, const RECT* rect,
                              int topOffset) {
    int rowHeight = PomodoroStats_UiScale(ui, 22);
    int swatch = PomodoroStats_UiScale(ui, 10);
    int index = 0;
    int top = (int)rect->top + topOffset;

    s_legendCount = 0;

    if (ui->report.totalSeconds <= 0) {
        RECT empty = *rect;
        empty.top = (int)rect->top + topOffset;
        DialogModern_DrawText(hdc, ui->bodyFont, palette->mutedText, &empty,
                              GetLocalizedString(NULL,
                                  L"No data yet - finish a pomodoro to see statistics here"),
                              DT_LEFT | DT_TOP | DT_WORDBREAK);
        return;
    }

    for (index = 0; index < ui->report.rowCount; index++) {
        const PomodoroStatsRow* row = &ui->report.rows[index];
        wchar_t name[POMODORO_STATS_NAME_MAX * 2] = L"";
        wchar_t line[POMODORO_STATS_NAME_MAX * 2 + 32] = L"";
        RECT swatchRect;
        RECT textRect;
        double ratio = (double)row->seconds / (double)ui->report.totalSeconds;

        if (top + rowHeight > (int)rect->bottom) break;
        PomodoroStats_UiProjectName(row->name, name, _countof(name));
        _snwprintf_s(line, _countof(line), _TRUNCATE, L"%ls  (%d)  %.1f%%",
                     name, row->count, ratio * 100.0);

        swatchRect.left = (int)rect->left + PomodoroStats_UiScale(ui, 4);
        swatchRect.right = swatchRect.left + swatch;
        swatchRect.top = top + (rowHeight - swatch) / 2;
        swatchRect.bottom = swatchRect.top + swatch;
        PomodoroStats_UiFill(hdc, &swatchRect,
                             PomodoroStats_UiColorFor(ui, row->name, index));

        textRect.left = swatchRect.right + PomodoroStats_UiScale(ui, 8);
        textRect.right = (int)rect->right;
        textRect.top = top;
        textRect.bottom = top + rowHeight;
        DialogModern_DrawText(hdc, ui->bodyFont, palette->text, &textRect, line,
                              DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

        if (s_legendCount < POMODORO_STATS_MAX_PROJECTS) {
            s_legendRects[s_legendCount].left = swatchRect.left;
            s_legendRects[s_legendCount].right = textRect.right;
            s_legendRects[s_legendCount].top = textRect.top;
            s_legendRects[s_legendCount].bottom = textRect.bottom;
            s_legendCount++;
        }
        top += rowHeight;
    }

    if (s_legendCount > 0) {
        RECT hint = *rect;
        hint.top = top + PomodoroStats_UiScale(ui, 8);
        hint.bottom = hint.top + rowHeight;
        DialogModern_DrawText(hdc, ui->bodyFont, palette->mutedText, &hint,
                              GetLocalizedString(NULL,
                                  L"Click a color swatch to change that project's color"),
                              DT_LEFT | DT_TOP | DT_SINGLELINE | DT_END_ELLIPSIS);
    }
}

int PomodoroStats_LegendHitTest(const POINT* point) {
    int index = 0;

    if (!point) return -1;
    for (index = 0; index < s_legendCount; index++) {
        if (point->x >= s_legendRects[index].left &&
            point->x < s_legendRects[index].right &&
            point->y >= s_legendRects[index].top &&
            point->y < s_legendRects[index].bottom) {
            return index;
        }
    }
    return -1;
}
