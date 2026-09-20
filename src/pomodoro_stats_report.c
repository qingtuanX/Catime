/**
 * @file pomodoro_stats_report.c
 * @brief Reads pomodoro_stats.csv and aggregates it into per-project rows.
 */

#include "pomodoro_stats.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <wchar.h>

#include "config.h"
#include "language.h"

#define REPORT_LINE_MAX 256
#define REPORT_SECONDS_PER_DAY (24 * 60 * 60)

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

/* Parse "YYYY-MM-DD HH:MM:SS" without relying on locale-aware scanf. */
static BOOL ParseTimestamp(const char* text, struct tm* out) {
    int fields[6] = {0};
    int index = 0;
    const char* cursor = text;

    for (index = 0; index < 6; index++) {
        int digits = 0;
        if (index == 3) {
            while (*cursor == ' ') cursor++;
        } else if (index > 0) {
            if (*cursor != '-' && *cursor != ':' && *cursor != ' ') return FALSE;
            cursor++;
        }
        while (*cursor >= '0' && *cursor <= '9' && digits < 4) {
            fields[index] = fields[index] * 10 + (*cursor - '0');
            cursor++;
            digits++;
        }
        if (digits == 0) return FALSE;
    }

    memset(out, 0, sizeof(*out));
    out->tm_year = fields[0] - 1900;
    out->tm_mon = fields[1] - 1;
    out->tm_mday = fields[2];
    out->tm_hour = fields[3];
    out->tm_min = fields[4];
    out->tm_sec = fields[5];
    out->tm_isdst = -1;

    if (out->tm_mon < 0 || out->tm_mon > 11 || out->tm_mday < 1 || out->tm_mday > 31) {
        return FALSE;
    }
    if (out->tm_hour > 23 || out->tm_min > 59 || out->tm_sec > 59) {
        return FALSE;
    }
    return TRUE;
}

/* Cutoff in local time; 0 means "no lower bound". */
static time_t RangeCutoff(PomodoroStatsRange range) {
    time_t now = time(NULL);
    struct tm* local = localtime(&now);
    struct tm midnight = {0};
    time_t cutoff = 0;

    if (!local) return 0;

    midnight = *local;
    midnight.tm_hour = 0;
    midnight.tm_min = 0;
    midnight.tm_sec = 0;
    midnight.tm_isdst = -1;
    cutoff = mktime(&midnight);
    if (cutoff == (time_t)-1) return 0;

    if (range == POMODORO_STATS_RANGE_WEEK) {
        cutoff -= 6 * REPORT_SECONDS_PER_DAY;
    }
    return cutoff;
}

/* Last comma-separated field is the interval length in seconds. */
static int ParseSeconds(const char* line, char* project, size_t projectSize) {
    const char* lastComma = strrchr(line, ',');
    const char* firstComma = strchr(line, ',');
    const char* start = NULL;
    const char* end = NULL;
    long seconds = 0;
    size_t length = 0;

    if (!lastComma || !firstComma || firstComma >= lastComma) return 0;

    while (*lastComma == ',' || *lastComma == ' ') lastComma++;
    seconds = strtol(lastComma, NULL, 10);
    if (seconds <= 0) return 0;

    start = firstComma + 1;
    end = lastComma;
    while (end > start && (end[-1] == ' ' || end[-1] == ',')) end--;
    while (start < end && *start == ' ') start++;

    length = (size_t)(end - start);
    if (length >= projectSize) length = projectSize - 1;
    if (length > 0) memcpy(project, start, length);
    project[length] = '\0';

    return (int)seconds;
}

int PomodoroStats_BuildReport(PomodoroStatsRange range, PomodoroStatsReport* report) {
    FILE* file = NULL;
    char line[REPORT_LINE_MAX];
    time_t cutoff = 0;

    if (!report) return 0;
    memset(report, 0, sizeof(*report));

    if (range != POMODORO_STATS_RANGE_ALL) {
        cutoff = RangeCutoff(range);
        if (cutoff == 0) range = POMODORO_STATS_RANGE_ALL;
    }

    file = PomodoroStats_OpenFile(PomodoroStats_StatsFilePath(), L"rb");
    if (!file) {
        return 0;
    }

    while (fgets(line, sizeof(line), file)) {
        char project[POMODORO_STATS_NAME_MAX];
        struct tm when = {0};
        const char* fields = line;
        int seconds = 0;
        int row = 0;

        if ((unsigned char)fields[0] == 0xEF && (unsigned char)fields[1] == 0xBB &&
            (unsigned char)fields[2] == 0xBF) {
            fields += 3; /* skip the UTF-8 BOM */
        }
        if (fields[0] == '\0' || fields[0] == '\n' || fields[0] == '#') continue;
        if (!ParseTimestamp(fields, &when)) continue;

        if (range != POMODORO_STATS_RANGE_ALL) {
            time_t stamp = mktime(&when);
            if (stamp != (time_t)-1 && stamp < cutoff) continue;
        }

        seconds = ParseSeconds(fields, project, sizeof(project));
        if (seconds <= 0 || project[0] == '\0') continue;

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
    qsort(report->rows, (size_t)report->rowCount, sizeof(report->rows[0]), CompareRows);
    return report->rowCount;
}

void PomodoroStats_FormatDuration(int seconds, wchar_t* out, size_t outSize) {
    if (!out || outSize == 0) return;
    out[0] = L'\0';
    if (seconds < 0) seconds = 0;

    if (seconds < 60) {
        _snwprintf_s(out, outSize, _TRUNCATE, GetLocalizedString(NULL, L"%d s"), seconds);
    } else if (seconds < 3600) {
        _snwprintf_s(out, outSize, _TRUNCATE, GetLocalizedString(NULL, L"%d min"),
                     seconds / 60);
    } else {
        _snwprintf_s(out, outSize, _TRUNCATE, GetLocalizedString(NULL, L"%d h %d min"),
                     seconds / 3600, (seconds % 3600) / 60);
    }
}
