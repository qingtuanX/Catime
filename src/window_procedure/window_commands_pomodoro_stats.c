/**
 * @file window_commands_pomodoro_stats.c
 * @brief Pomodoro project selection and statistics commands.
 */

#include "window_procedure/window_commands.h"
#include "pomodoro_stats.h"

LRESULT CmdPomodoroStats(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)wp; (void)lp;
    PomodoroStats_ShowWindow(hwnd);
    return 0;
}

BOOL HandlePomodoroProject(HWND hwnd, UINT cmd, int index) {
    (void)hwnd; (void)cmd;
    char projects[POMODORO_STATS_MAX_PROJECTS][POMODORO_STATS_NAME_MAX];
    int count = PomodoroStats_ListProjects(projects);
    if (index < 0 || index >= count) return FALSE;
    PomodoroStats_SetCurrentProject(projects[index]);
    return TRUE;
}
