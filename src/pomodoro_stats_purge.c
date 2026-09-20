/**
 * @file pomodoro_stats_purge.c
 * @brief Removes recorded intervals from pomodoro_stats.csv.
 */

#include "pomodoro_stats.h"

#include <stdio.h>
#include <string.h>

#include "pomodoro.h"
#include "pomodoro_stats_data.h"
#include "utils/string_convert.h"

#define PURGE_LINE_MAX 256
#define PURGE_BOM "\xEF\xBB\xBF"

static BOOL IsUtf8Bom(const char* text) {
    return (unsigned char)text[0] == 0xEF && (unsigned char)text[1] == 0xBB &&
           (unsigned char)text[2] == 0xBF;
}

static void AppendSuffix(char* path, size_t pathSize, const char* suffix) {
    size_t length = 0;
    size_t suffixLength = 0;

    if (!path || !suffix) return;
    length = strlen(path);
    suffixLength = strlen(suffix);
    if (length + suffixLength + 1 > pathSize) return;
    memcpy(path + length, suffix, suffixLength + 1);
}

/* Shared walk: counts matching rows and, when purging, rewrites the file. */
static int WalkStats(PomodoroStatsRange range, const char* project, BOOL purge) {
    const char* sourcePath = PomodoroStats_StatsFilePath();
    char tempPath[MAX_PATH] = {0};
    wchar_t wideSource[MAX_PATH] = L"";
    wchar_t wideTemp[MAX_PATH] = L"";
    FILE* source = NULL;
    FILE* temp = NULL;
    char line[PURGE_LINE_MAX];
    size_t sourceLength = 0;
    int matched = 0;

    if (!sourcePath || !*sourcePath) return 0;
    sourceLength = strlen(sourcePath);
    if (sourceLength + 1 > sizeof(tempPath)) return 0;
    memcpy(tempPath, sourcePath, sourceLength + 1);
    AppendSuffix(tempPath, sizeof(tempPath), ".tmp");

    source = PomodoroStats_OpenFile(sourcePath, L"rb");
    if (!source) return 0;

    if (purge) {
        temp = PomodoroStats_OpenFile(tempPath, L"wb");
        if (!temp) {
            fclose(source);
            return 0;
        }
        fputs(PURGE_BOM, temp);
    }

    while (fgets(line, sizeof(line), source)) {
        const char* fields = line;
        if (IsUtf8Bom(fields)) fields += 3;
        if (PomodoroStats_MatchLine(fields, range, project, NULL, 0) > 0) {
            matched++;
            continue;
        }
        if (temp) fputs(fields, temp);
    }

    fclose(source);
    if (!temp) return matched;

    fclose(temp);
    if (matched == 0) {
        Utf8ToWide(tempPath, wideTemp, _countof(wideTemp));
        DeleteFileW(wideTemp);
        return 0;
    }

    Utf8ToWide(tempPath, wideTemp, _countof(wideTemp));
    Utf8ToWide(sourcePath, wideSource, _countof(wideSource));
    if (wideTemp[0] == L'\0' || wideSource[0] == L'\0' ||
        !MoveFileExW(wideTemp, wideSource, MOVEFILE_REPLACE_EXISTING)) {
        if (wideTemp[0] != L'\0') DeleteFileW(wideTemp);
        return 0;
    }

    /* Clearing everything also resets the lifetime counter. */
    if (range == POMODORO_STATS_RANGE_ALL && (!project || *project == '\0')) {
        SetPomodoroCompletedCount(0);
    }
    return matched;
}

int PomodoroStats_CountRange(PomodoroStatsRange range, const char* project) {
    return WalkStats(range, project, FALSE);
}

int PomodoroStats_Purge(PomodoroStatsRange range, const char* project) {
    return WalkStats(range, project, TRUE);
}
