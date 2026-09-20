/**
 * @file pomodoro_stats_report.c
 * @brief Aggregates pomodoro_stats.csv into per-project rows.
 */

#include "pomodoro_stats.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#include "language.h"
#include "pomodoro_stats_data.h"

#define REPORT_LINE_MAX 256

/* More time first, then by name so the order stays stable. */
static int CompareRows(const void* left, const void* right) {
    const PomodoroStatsRow* a = (const PomodoroStatsRow*)left;
    const PomodoroStatsRow* b = (const PomodoroStatsRow*)right;
    if (a->seconds != b->seconds) {
        return b->seconds - a->seconds;
    }
    return strcmp(a->name, b->name);
}

static int FindRow(const PomodoroStatsReport* report, const char* name) {
    int index = 0;
    for (index = 0; index < report->rowCount; index++) {
        if (strcmp(report->rows[index].name, name) == 0) {
            return index;
        }
    }
    return -1;
}

int PomodoroStats_BuildReport(PomodoroStatsRange range, PomodoroStatsReport* report) {
    FILE* file = NULL;
    char line[REPORT_LINE_MAX];

    if (!report) return 0;
    memset(report, 0, sizeof(*report));

    file = PomodoroStats_OpenFile(PomodoroStats_StatsFilePath(), L"rb");
    if (!file) return 0;

    while (fgets(line, sizeof(line), file)) {
        char project[POMODORO_STATS_NAME_MAX] = {0};
        int seconds = PomodoroStats_MatchLine(line, range, NULL, project,
                                              sizeof(project));
        int row = 0;

        if (seconds <= 0) continue;

        row = FindRow(report, project);
        if (row < 0) {
            size_t nameLength = strlen(project);
            if (report->rowCount >= POMODORO_STATS_MAX_PROJECTS) continue;
            row = report->rowCount++;
            if (nameLength >= sizeof(report->rows[0].name)) {
                nameLength = sizeof(report->rows[0].name) - 1;
            }
            memcpy(report->rows[row].name, project, nameLength);
            report->rows[row].name[nameLength] = '\0';
        }
        report->rows[row].count++;
        report->rows[row].seconds += seconds;
        report->totalCount++;
        report->totalSeconds += seconds;
    }

    fclose(file);
    qsort(report->rows, (size_t)report->rowCount, sizeof(report->rows[0]),
          CompareRows);
    return report->rowCount;
}

void PomodoroStats_FormatDuration(int seconds, wchar_t* out, size_t outSize) {
    if (!out || outSize == 0) return;
    out[0] = L'\0';
    if (seconds < 0) seconds = 0;

    if (seconds < 60) {
        _snwprintf_s(out, outSize, _TRUNCATE, GetLocalizedString(NULL, L"%d s"),
                     seconds);
    } else if (seconds < 3600) {
        _snwprintf_s(out, outSize, _TRUNCATE, GetLocalizedString(NULL, L"%d min"),
                     seconds / 60);
    } else {
        _snwprintf_s(out, outSize, _TRUNCATE,
                     GetLocalizedString(NULL, L"%d h %d min"),
                     seconds / 3600, (seconds % 3600) / 60);
    }
}
