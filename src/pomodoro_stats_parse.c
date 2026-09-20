/**
 * @file pomodoro_stats_parse.c
 * @brief Parses and filters pomodoro_stats.csv lines.
 */

#include "pomodoro_stats_data.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>

#define POMODORO_STATS_SECONDS_PER_DAY (24 * 60 * 60)

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

    if (out->tm_mon < 0 || out->tm_mon > 11 || out->tm_mday < 1 ||
        out->tm_mday > 31) {
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
        cutoff -= 6 * POMODORO_STATS_SECONDS_PER_DAY;
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

int PomodoroStats_MatchLine(const char* line, PomodoroStatsRange range,
                            const char* project, char* outProject,
                            size_t outProjectSize) {
    char name[POMODORO_STATS_NAME_MAX] = {0};
    struct tm when = {0};
    const char* fields = line;
    time_t cutoff = 0;
    int seconds = 0;

    if (!line) return 0;
    if ((unsigned char)fields[0] == 0xEF && (unsigned char)fields[1] == 0xBB &&
        (unsigned char)fields[2] == 0xBF) {
        fields += 3;
    }
    while (*fields == ' ' || *fields == '\t') fields++;
    if (*fields == '\0' || *fields == '\n' || *fields == '\r' ||
        *fields == '#') {
        return 0;
    }
    if (!ParseTimestamp(fields, &when)) return 0;

    if (range != POMODORO_STATS_RANGE_ALL) {
        time_t stamp = mktime(&when);
        cutoff = RangeCutoff(range);
        if (cutoff != 0 && stamp != (time_t)-1 && stamp < cutoff) return 0;
    }

    seconds = ParseSeconds(fields, name, sizeof(name));
    if (seconds <= 0 || name[0] == '\0') return 0;
    if (project && *project && strcmp(project, name) != 0) return 0;

    if (outProject && outProjectSize > 0) {
        size_t length = strlen(name);
        if (length >= outProjectSize) length = outProjectSize - 1;
        memcpy(outProject, name, length);
        outProject[length] = '\0';
    }
    return seconds;
}
