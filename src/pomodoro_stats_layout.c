/**
 * @file pomodoro_stats_layout.c
 * @brief Layout of the statistics window: painted areas and child buttons.
 */

#include "pomodoro_stats_internal.h"

/* Design values are 96-dpi units; painted rectangles are physical pixels. */
#define STATS_PADDING 16
#define STATS_GAP 12
#define STATS_TOOLBAR_TOP 14
#define STATS_TOOLBAR_HEIGHT 30
#define STATS_SUMMARY_HEIGHT 36
#define STATS_CHIP_GAP 6
#define STATS_ACTION_GAP 8

static const int kChipWidths[3] = {72, 72, 96};
static const int kActionWidths[3] = {84, 132, 150};
static const int kChipIds[3] = {
    POMODORO_STATS_ID_RANGE_ALL, POMODORO_STATS_ID_RANGE_TODAY,
    POMODORO_STATS_ID_RANGE_WEEK
};
static const int kActionIds[3] = {
    POMODORO_STATS_ID_REFRESH, POMODORO_STATS_ID_MANAGE,
    POMODORO_STATS_ID_OPEN_FOLDER
};

void PomodoroStats_UiLayout(PomodoroStatsUi* ui) {
    RECT client = {0};
    int toolbarTop = 0;
    int clientWidth = 0;
    int clientHeight = 0;
    int available = 0;
    int width96 = 0;
    int x96 = 0;
    int index = 0;

    if (!ui || !ui->hwnd || !GetClientRect(ui->hwnd, &client)) return;
    clientWidth = (int)client.right;
    clientHeight = (int)client.bottom;
    width96 = (clientWidth * 96) / (int)(ui->dpi ? ui->dpi : 96u);
    toolbarTop = STATS_TOOLBAR_TOP;

    ui->summaryRect.left = 0;
    ui->summaryRect.right = clientWidth;
    ui->summaryRect.top = clientHeight - PomodoroStats_UiScale(ui, STATS_SUMMARY_HEIGHT);
    ui->summaryRect.bottom = clientHeight;

    available = clientWidth - PomodoroStats_UiScale(ui, STATS_PADDING * 2 + STATS_GAP);
    ui->tableRect.left = PomodoroStats_UiScale(ui, STATS_PADDING);
    ui->tableRect.right = ui->tableRect.left + (available * 52) / 100;
    ui->tableRect.top = PomodoroStats_UiScale(ui, toolbarTop + STATS_TOOLBAR_HEIGHT + STATS_GAP);
    ui->tableRect.bottom = ui->summaryRect.top - PomodoroStats_UiScale(ui, STATS_GAP);
    ui->chartRect.left = ui->tableRect.right + PomodoroStats_UiScale(ui, STATS_GAP);
    ui->chartRect.right = clientWidth - PomodoroStats_UiScale(ui, STATS_PADDING);
    ui->chartRect.top = ui->tableRect.top;
    ui->chartRect.bottom = ui->tableRect.bottom;

    x96 = STATS_PADDING;
    for (index = 0; index < 3; index++) {
        DialogModern_SetChildRect96(ui->hwnd, kChipIds[index], ui->dpi, x96, toolbarTop,
                                    kChipWidths[index], STATS_TOOLBAR_HEIGHT);
        x96 += kChipWidths[index] + STATS_CHIP_GAP;
    }

    x96 = width96 - STATS_PADDING - (kActionWidths[0] + kActionWidths[1] +
                                    kActionWidths[2] + 2 * STATS_ACTION_GAP);
    for (index = 0; index < 3; index++) {
        DialogModern_SetChildRect96(ui->hwnd, kActionIds[index], ui->dpi, x96, toolbarTop,
                                    kActionWidths[index], STATS_TOOLBAR_HEIGHT);
        x96 += kActionWidths[index] + STATS_ACTION_GAP;
    }
}
