/**
 * @file pomodoro_stats.h
 * @brief Per-project pomodoro statistics: records finished work intervals and
 *        links to the statistics viewer.
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

#define POMODORO_STATS_MAX_PROJECTS 24
#define POMODORO_STATS_NAME_MAX 40

/* Directory shared with config.ini. */
void PomodoroStats_DataDir(char* out, size_t outSize);

/* Project list file (created with a default entry on first use). */
const char* PomodoroStats_ProjectsFilePath(void);
int PomodoroStats_ListProjects(char names[POMODORO_STATS_MAX_PROJECTS][POMODORO_STATS_NAME_MAX]);

/* Currently selected project (persisted in config.ini). */
const char* PomodoroStats_CurrentProject(void);
void PomodoroStats_SetCurrentProject(const char* name);

/* Append one finished work interval to pomodoro_stats.csv. */
void PomodoroStats_RecordSession(const char* project, int seconds);

/* Ask whether an interrupted work interval should be recorded or discarded. */
void PomodoroStats_PromptInterruptedWork(HWND hwnd);

/* Open the statistics viewer (pomodoro-stats.exe next to catime.exe). */
void PomodoroStats_OpenViewer(HWND hwnd);

#endif /* CATIME_POMODORO_STATS_H */
