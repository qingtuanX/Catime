/**
 * @file pomodoro_stats_internal.h
 * @brief Shared state and pieces of the in-app pomodoro statistics window.
 */

#ifndef POMODORO_STATS_INTERNAL_H
#define POMODORO_STATS_INTERNAL_H

#include "pomodoro_stats.h"
#include "dialog/dialog_modern.h"

#define POMODORO_STATS_WINDOW_CLASS L"CatimePomodoroStatsWindow"

/* Command ids stay above the tray menu range (CLOCK_IDM_* / CLOCK_IDC_*). */
#define POMODORO_STATS_ID_RANGE_ALL 4001
#define POMODORO_STATS_ID_RANGE_TODAY 4002
#define POMODORO_STATS_ID_RANGE_WEEK 4003
#define POMODORO_STATS_ID_REFRESH 4004
#define POMODORO_STATS_ID_MANAGE 4005
#define POMODORO_STATS_ID_OPEN_FOLDER 4006
#define POMODORO_STATS_ID_CLEAR 4007

#define POMODORO_STATS_SLICE_COUNT 8
#define POMODORO_STATS_REFRESH_MS 3000

typedef struct {
    HWND hwnd;
    UINT dpi;
    HFONT bodyFont;
    HFONT titleFont;
    PomodoroStatsRange range;
    PomodoroStatsReport report;
    RECT tableRect;
    RECT chartRect;
    RECT summaryRect;
} PomodoroStatsUi;

/* --- pomodoro_stats_ui.c ------------------------------------------------ */
int PomodoroStats_UiScale(const PomodoroStatsUi* ui, int value);
void PomodoroStats_UiFill(HDC hdc, const RECT* rect, COLORREF color);
COLORREF PomodoroStats_UiSliceColor(int index);
void PomodoroStats_UiProjectName(const char* name, wchar_t* out, size_t outSize);
void PomodoroStats_UiOpenDataFolder(void);

/* --- pomodoro_stats_layout.c ------------------------------------------- */
void PomodoroStats_UiLayout(PomodoroStatsUi* ui);

/* --- pomodoro_stats_paint.c -------------------------------------------- */
void PomodoroStats_PaintContent(HDC hdc, const PomodoroStatsUi* ui, const RECT* client);
void PomodoroStats_PaintButton(HDC hdc, const PomodoroStatsUi* ui, const RECT* rect,
                               const wchar_t* text, BOOL selected, UINT state);
void PomodoroStats_PaintWindow(HWND hwnd, const PomodoroStatsUi* ui);

/* --- pomodoro_stats_table.c / pomodoro_stats_chart.c ------------------- */
void PomodoroStats_DrawTable(HDC hdc, const PomodoroStatsUi* ui,
                             const DialogModernPalette* palette, const RECT* rect);
void PomodoroStats_DrawSummary(HDC hdc, const PomodoroStatsUi* ui,
                               const DialogModernPalette* palette, const RECT* rect);
void PomodoroStats_DrawPie(HDC hdc, const PomodoroStatsUi* ui,
                           const DialogModernPalette* palette, const RECT* rect,
                           int topOffset);
void PomodoroStats_DrawLegend(HDC hdc, const PomodoroStatsUi* ui,
                              const DialogModernPalette* palette, const RECT* rect,
                              int topOffset);

/* --- pomodoro_stats_projects_dialog.c ---------------------------------- */
void PomodoroStats_ShowProjectsDialog(HWND owner);

/* --- pomodoro_stats_purge_dialog.c ------------------------------------- */
void PomodoroStats_ShowPurgeDialog(HWND owner, PomodoroStatsRange initialRange);

#endif /* POMODORO_STATS_INTERNAL_H */
