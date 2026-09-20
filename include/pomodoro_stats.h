/**
 * @file pomodoro_stats.h
 * @brief Per-project pomodoro statistics: records finished work intervals and
 *        hosts the in-app statistics window.
 *
 * Every finished work interval is appended to pomodoro_stats.csv as
 *   <timestamp>,<project>,<seconds>
 * The selected project is stored in config.ini ([Pomodoro] POMODORO_CURRENT_PROJECT),
 * the project list lives in pomodoro_projects.txt (one name per line, UTF-8).
 */

#ifndef CATIME_POMODORO_STATS_H
#define CATIME_POMODORO_STATS_H

#include <windows.h>
#include <stddef.h>
#include <stdio.h>

#define POMODORO_STATS_MAX_PROJECTS 24
#define POMODORO_STATS_NAME_MAX 40

/** @brief Aggregated statistics for one project. */
typedef struct {
    char name[POMODORO_STATS_NAME_MAX];
    int count;
    int seconds;
} PomodoroStatsRow;

/** @brief Aggregated statistics of the selected time range. */
typedef struct {
    PomodoroStatsRow rows[POMODORO_STATS_MAX_PROJECTS];
    int rowCount;
    int totalCount;
    int totalSeconds;
} PomodoroStatsReport;

typedef enum {
    POMODORO_STATS_RANGE_ALL = 0,
    POMODORO_STATS_RANGE_TODAY,
    POMODORO_STATS_RANGE_WEEK
} PomodoroStatsRange;

/* Directory shared with config.ini. */
void PomodoroStats_DataDir(char* out, size_t outSize);

/* Data files (created on demand). */
const char* PomodoroStats_ProjectsFilePath(void);
const char* PomodoroStats_StatsFilePath(void);
/* Open a data file with a wide-character mode so UTF-8 paths keep working. */
FILE* PomodoroStats_OpenFile(const char* utf8Path, const wchar_t* mode);
int PomodoroStats_ListProjects(char names[POMODORO_STATS_MAX_PROJECTS][POMODORO_STATS_NAME_MAX]);
BOOL PomodoroStats_SaveProjects(char names[POMODORO_STATS_MAX_PROJECTS][POMODORO_STATS_NAME_MAX],
                                int count);

/* Currently selected project (persisted in config.ini). */
const char* PomodoroStats_CurrentProject(void);
void PomodoroStats_SetCurrentProject(const char* name);

/* Append one finished work interval to pomodoro_stats.csv. */
void PomodoroStats_RecordSession(const char* project, int seconds);

/* Ask whether an interrupted work interval should be recorded or discarded. */
void PomodoroStats_PromptInterruptedWork(HWND hwnd);

/* Aggregate pomodoro_stats.csv for one time range. Returns the row count. */
int PomodoroStats_BuildReport(PomodoroStatsRange range, PomodoroStatsReport* report);

/* Records matching the same filter PomodoroStats_Purge() would remove. */
int PomodoroStats_CountRange(PomodoroStatsRange range, const char* project);

/* Remove matching records (project NULL/"" means every project). Returns the
 * number of removed records; clearing everything also resets the counter. */
int PomodoroStats_Purge(PomodoroStatsRange range, const char* project);

/* Format a duration such as "1 h 20 min" in the current language. */
void PomodoroStats_FormatDuration(int seconds, wchar_t* out, size_t outSize);

/* Show (or focus) the in-app statistics window. */
void PomodoroStats_ShowWindow(HWND owner);

#endif /* CATIME_POMODORO_STATS_H */
