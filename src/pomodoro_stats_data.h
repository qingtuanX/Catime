/**
 * @file pomodoro_stats_data.h
 * @brief Shared parsing of pomodoro_stats.csv lines.
 */

#ifndef POMODORO_STATS_DATA_H
#define POMODORO_STATS_DATA_H

#include "pomodoro_stats.h"

/**
 * @brief Match one csv line against a time range and a project.
 * @param line          One line of pomodoro_stats.csv (may carry the UTF-8 BOM).
 * @param range         Time range filter; POMODORO_STATS_RANGE_ALL means no limit.
 * @param project       Project to keep, or NULL/"" for every project.
 * @param outProject    Optional buffer that receives the line's project name.
 * @param outProjectSize Size of outProject in bytes.
 * @return The recorded interval in seconds when the line matches, otherwise 0.
 */
int PomodoroStats_MatchLine(const char* line, PomodoroStatsRange range,
                            const char* project, char* outProject,
                            size_t outProjectSize);

#endif /* POMODORO_STATS_DATA_H */
