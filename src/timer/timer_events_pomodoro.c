/**
 * @file timer_events_pomodoro.c
 * @brief Pomodoro session state and interval completion handling.
 */

#include <string.h>
#include <wchar.h>

#include "timer_events_internal.h"
#include "pomodoro_stats.h"

/* Total completed work pomodoros, persisted in the config INI. */
#define POMODORO_COUNT_INI_KEY "POMODORO_COMPLETED_COUNT"

static int pomodoro_completed_count = -1; /* -1 = not loaded yet */

int GetPomodoroCompletedCount(void) {
    if (pomodoro_completed_count < 0) {
        char configPath[MAX_PATH];
        GetConfigPath(configPath, MAX_PATH);
        pomodoro_completed_count = ReadIniInt(INI_SECTION_POMODORO,
                                              POMODORO_COUNT_INI_KEY, 0,
                                              configPath);
        if (pomodoro_completed_count < 0) {
            pomodoro_completed_count = 0;
        }
    }
    return pomodoro_completed_count;
}

static void IncrementPomodoroCompletedCount(void) {
    char configPath[MAX_PATH];
    int next = GetPomodoroCompletedCount() + 1;
    pomodoro_completed_count = next;
    GetConfigPath(configPath, MAX_PATH);
    WriteIniInt(INI_SECTION_POMODORO, POMODORO_COUNT_INI_KEY, next, configPath);
}

void SetPomodoroCompletedCount(int value) {
    char configPath[MAX_PATH];
    if (value < 0) value = 0;
    pomodoro_completed_count = value;
    GetConfigPath(configPath, MAX_PATH);
    WriteIniInt(INI_SECTION_POMODORO, POMODORO_COUNT_INI_KEY, value, configPath);
}

/* Append " · Total pomodoros: N" to a notification message. */
static void AppendPomodoroTotal(wchar_t* message, size_t messageSize) {
    wchar_t totalText[64];
    wchar_t merged[320];
    _snwprintf_s(totalText, _countof(totalText), _TRUNCATE,
                 GetLocalizedString(NULL, L"Total pomodoros: %d"),
                 GetPomodoroCompletedCount());
    if (_snwprintf_s(merged, _countof(merged), _TRUNCATE, L"%ls · %ls",
                     message, totalText) > 0) {
        wcscpy_s(message, messageSize, merged);
    }
}

BOOL TimerEvents_AdvancePomodoroState(void) {
    if (pomodoro_initial_times_count == 0) {
        return FALSE;
    }

    current_pomodoro_time_index++;
    if (current_pomodoro_time_index >= pomodoro_initial_times_count) {
        current_pomodoro_time_index = 0;
        complete_pomodoro_cycles++;
        if (complete_pomodoro_cycles >= pomodoro_initial_loop_count) {
            return FALSE;
        }
    }

    return TRUE;
}

void ResetPomodoroState(void) {
    current_pomodoro_phase = POMODORO_PHASE_IDLE;
    current_pomodoro_time_index = 0;
    complete_pomodoro_cycles = 0;
    pomodoro_initial_times_count = 0;
    pomodoro_initial_loop_count = 0;
    memset(pomodoro_initial_times, 0, sizeof(pomodoro_initial_times));
}

BOOL TimerEvents_IsActivePomodoroTimer(void) {
    if (current_pomodoro_phase == POMODORO_PHASE_IDLE ||
        pomodoro_initial_times_count == 0 ||
        current_pomodoro_time_index >= pomodoro_initial_times_count) {
        return FALSE;
    }

    return CLOCK_TOTAL_TIME ==
           pomodoro_initial_times[current_pomodoro_time_index];
}

void TimerEvents_FormatPomodoroTime(int seconds,
                                    wchar_t* buffer,
                                    size_t bufferSize) {
    if (seconds < 60) {
        _snwprintf_s(buffer, bufferSize, _TRUNCATE, L"%ds", seconds);
        return;
    }

    int minutes = seconds / 60;
    int remainingSeconds = seconds % 60;
    if (remainingSeconds > 0) {
        _snwprintf_s(buffer, bufferSize, _TRUNCATE,
                     L"%dm%ds", minutes, remainingSeconds);
    } else {
        _snwprintf_s(buffer, bufferSize, _TRUNCATE, L"%dm", minutes);
    }
}

static void BuildCompletionMessage(wchar_t* completionMsg,
                                   size_t completionMsgSize,
                                   int completedIndex,
                                   int timesCount,
                                   int loopCount,
                                   int currentCycle,
                                   int stepInCycle) {
    wchar_t timeStr[32];
    if (completedIndex < pomodoro_initial_times_count) {
        TimerEvents_FormatPomodoroTime(pomodoro_initial_times[completedIndex],
                                       timeStr, _countof(timeStr));
    } else {
        wcscpy_s(timeStr, _countof(timeStr), L"?");
    }

    const wchar_t* completedText =
        GetLocalizedString(NULL, L"Pomodoro completed");
    if (timesCount <= 1 && loopCount <= 1) {
        _snwprintf_s(completionMsg, completionMsgSize, _TRUNCATE,
                     L"%ls %ls", timeStr, completedText);
        return;
    }

    const wchar_t* cycleText = GetLocalizedString(NULL, L"Cycle");
    const wchar_t* roundText = GetLocalizedString(NULL, L"Round");
    _snwprintf_s(completionMsg, completionMsgSize, _TRUNCATE,
                 L"%ls %ls (%ls%d/%d%ls %d/%d)",
                 timeStr, completedText, cycleText, currentCycle, loopCount,
                 roundText, stepInCycle, timesCount);
}

BOOL TimerEvents_HandlePomodoroCompletion(HWND hwnd) {
    wchar_t completionMsg[256];
    int completedIndex = current_pomodoro_time_index;
    int timesCount = pomodoro_initial_times_count;
    int loopCount = pomodoro_initial_loop_count;

    if (timesCount <= 0) timesCount = 1;
    if (loopCount <= 0) loopCount = 1;

    int stepInCycle = completedIndex + 1;
    int currentCycle = complete_pomodoro_cycles + 1;
    BuildCompletionMessage(completionMsg, _countof(completionMsg),
                           completedIndex, timesCount, loopCount,
                           currentCycle, stepInCycle);

    /* The interval sequence alternates work / break (default 25m, 5m, 25m, 10m),
       so every other interval - index 0, 2, ... - is a work interval and counts
       as one completed pomodoro. */
    if ((completedIndex % 2) == 0) {
        IncrementPomodoroCompletedCount();
        if (completedIndex < pomodoro_initial_times_count) {
            PomodoroStats_RecordSession(PomodoroStats_CurrentProject(),
                                        pomodoro_initial_times[completedIndex]);
        }
    }
    AppendPomodoroTotal(completionMsg, _countof(completionMsg));

    if (!TimerEvents_AdvancePomodoroState()) {
        ShowNotification(hwnd, completionMsg);
        TimerEvents_ResetTimerState(0);
        ResetPomodoroState();

        wchar_t allCompletedMsg[256];
        wcscpy_s(allCompletedMsg, _countof(allCompletedMsg),
                 GetLocalizedString(NULL, L"All Pomodoro cycles completed!"));
        AppendPomodoroTotal(allCompletedMsg, _countof(allCompletedMsg));
        ShowNotification(hwnd, allCompletedMsg);
        PlayNotificationSound(hwnd);

        CLOCK_COUNT_UP = false;
        CLOCK_SHOW_CURRENT_TIME = false;
        message_shown = TRUE;
        InvalidateRect(hwnd, NULL, TRUE);
        MainTimer_Stop();
        return FALSE;
    }

    ShowNotification(hwnd, completionMsg);
    PlayNotificationSound(hwnd);

    /* Preserve the absolute deadline so notification work introduces no drift. */
    int nextDurationSec =
        pomodoro_initial_times[current_pomodoro_time_index];
    TimerEvents_ResetTimerState(nextDurationSec);
    g_target_end_time += (int64_t)nextDurationSec * 1000;
    countdown_message_shown = false;

    InitializeHighPrecisionTimer();
    TimerEvents_ResetMillisecondAccumulator();
    InvalidateRect(hwnd, NULL, TRUE);
    return TRUE;
}

void InitializePomodoro(void) {
    current_pomodoro_phase = POMODORO_PHASE_WORK;
    current_pomodoro_time_index = 0;
    complete_pomodoro_cycles = 0;

    pomodoro_initial_times_count = g_AppConfig.pomodoro.times_count;
    if (pomodoro_initial_times_count < 0) {
        pomodoro_initial_times_count = 0;
    }
    if (pomodoro_initial_times_count > MAX_POMODORO_TIMES) {
        pomodoro_initial_times_count = MAX_POMODORO_TIMES;
    }

    pomodoro_initial_loop_count = g_AppConfig.pomodoro.loop_count;
    if (pomodoro_initial_loop_count < MIN_POMODORO_LOOP_COUNT) {
        pomodoro_initial_loop_count = MIN_POMODORO_LOOP_COUNT;
    }
    if (pomodoro_initial_loop_count > MAX_POMODORO_LOOP_COUNT) {
        pomodoro_initial_loop_count = MAX_POMODORO_LOOP_COUNT;
    }

    memset(pomodoro_initial_times, 0, sizeof(pomodoro_initial_times));
    for (int i = 0; i < pomodoro_initial_times_count; i++) {
        pomodoro_initial_times[i] = g_AppConfig.pomodoro.times[i];
    }

    CLOCK_TOTAL_TIME = pomodoro_initial_times_count > 0
        ? pomodoro_initial_times[0]
        : DEFAULT_POMODORO_DURATION;
    countdown_elapsed_time = 0;
    countdown_message_shown = false;
    TimerEvents_ResetMillisecondAccumulator();
}
