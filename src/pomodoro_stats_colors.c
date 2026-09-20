/**
 * @file pomodoro_stats_colors.c
 * @brief Per-project chart colors, stored in pomodoro_colors.txt.
 */

#include "pomodoro_stats.h"

#include <stdio.h>
#include <string.h>

#include "color/color_parser.h"

#define COLORS_LINE_MAX 128
#define COLORS_HEX_MAX 16

static const COLORREF kDefaultColors[POMODORO_STATS_DEFAULT_COLOR_COUNT] = {
    RGB(255, 107, 91), RGB(78, 203, 113), RGB(86, 156, 214), RGB(220, 170, 60),
    RGB(180, 120, 220), RGB(90, 200, 210), RGB(230, 120, 170), RGB(150, 160, 180)
};

static BOOL HasUtf8Bom(const char* text) {
    return (unsigned char)text[0] == 0xEF && (unsigned char)text[1] == 0xBB &&
           (unsigned char)text[2] == 0xBF;
}

static void TrimTrailing(char* text) {
    size_t length = 0;
    if (!text) return;
    length = strlen(text);
    while (length > 0 && (text[length - 1] == ' ' || text[length - 1] == '\t' ||
                          text[length - 1] == '\r' || text[length - 1] == '\n')) {
        text[--length] = '\0';
    }
}

static void CopyName(char* destination, const char* source) {
    size_t length = 0;
    if (!destination || !source) return;
    length = strlen(source);
    if (length >= POMODORO_STATS_NAME_MAX) {
        length = POMODORO_STATS_NAME_MAX - 1;
    }
    memcpy(destination, source, length);
    destination[length] = '\0';
}

COLORREF PomodoroStats_DefaultColor(int index) {
    if (index < 0) index = 0;
    return kDefaultColors[index % POMODORO_STATS_DEFAULT_COLOR_COUNT];
}

int PomodoroStats_LoadColors(PomodoroStatsColorEntry* entries, int maxCount) {
    FILE* file = NULL;
    char line[COLORS_LINE_MAX];
    int count = 0;

    if (!entries || maxCount <= 0) return 0;

    file = PomodoroStats_OpenFile(PomodoroStats_ColorsFilePath(), L"rb");
    if (!file) return 0;

    while (count < maxCount && fgets(line, sizeof(line), file)) {
        char* name = line;
        char* separator = NULL;
        char* value = NULL;
        COLORREF color = 0;

        if (HasUtf8Bom(name)) name += 3;
        while (*name == ' ' || *name == '\t') name++;
        if (*name == '\0' || *name == '\r' || *name == '\n' ||
            *name == '#' || *name == ';') {
            continue;
        }

        separator = strchr(name, '=');
        if (!separator) continue;
        *separator = '\0';
        value = separator + 1;
        while (*value == ' ' || *value == '\t') value++;
        TrimTrailing(name);
        TrimTrailing(value);
        if (*name == '\0' || *value == '\0') continue;
        if (!ColorStringToColorRef(value, &color)) continue;

        CopyName(entries[count].name, name);
        entries[count].color = color;
        count++;
    }

    fclose(file);
    return count;
}

BOOL PomodoroStats_SetProjectColor(const char* project, COLORREF color) {
    PomodoroStatsColorEntry entries[POMODORO_STATS_MAX_PROJECTS];
    FILE* file = NULL;
    char hex[COLORS_HEX_MAX] = {0};
    int count = 0;
    int found = -1;
    int index = 0;
    BOOL removing = (color == (COLORREF)-1);

    if (!project || *project == '\0') return FALSE;

    count = PomodoroStats_LoadColors(entries, POMODORO_STATS_MAX_PROJECTS);
    for (index = 0; index < count; index++) {
        if (strcmp(entries[index].name, project) == 0) {
            found = index;
            break;
        }
    }

    if (removing) {
        if (found < 0) return TRUE;
        for (index = found; index + 1 < count; index++) {
            entries[index] = entries[index + 1];
        }
        count--;
    } else if (found >= 0) {
        entries[found].color = color;
    } else {
        if (count >= POMODORO_STATS_MAX_PROJECTS) return FALSE;
        CopyName(entries[count].name, project);
        entries[count].color = color;
        count++;
    }

    file = PomodoroStats_OpenFile(PomodoroStats_ColorsFilePath(), L"wb");
    if (!file) return FALSE;

    fputs("# One \"project=#RRGGBB\" per line (UTF-8), edited from the "
          "statistics window.\n", file);
    for (index = 0; index < count; index++) {
        ColorRefToHex(entries[index].color, hex, sizeof(hex));
        fputs(entries[index].name, file);
        fputc('=', file);
        fputs(hex, file);
        fputc('\n', file);
    }
    fclose(file);
    return TRUE;
}
