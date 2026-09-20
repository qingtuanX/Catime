/**
 * @file pomodoro_stats.c
 * @brief Per-project pomodoro statistics (record + viewer integration).
 */

#include "pomodoro_stats.h"

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <wchar.h>

#include "config.h"
#include "language.h"
#include "log.h"
#include "pomodoro.h"
#include "timer/timer.h"

static char g_currentProject[POMODORO_STATS_NAME_MAX] = {0};
static BOOL g_currentProjectLoaded = FALSE;

/* Open a UTF-8 path with a wide-character mode so non-ASCII paths work. */
static FILE* OpenUtf8File(const char* utf8Path, const wchar_t* mode) {
    wchar_t widePath[MAX_PATH];
    if (!utf8Path || !*utf8Path) return NULL;
    if (MultiByteToWideChar(CP_UTF8, 0, utf8Path, -1, widePath, MAX_PATH) == 0) {
        return NULL;
    }
    return _wfopen(widePath, mode);
}

/* Bounded copy: avoids strncpy/format truncation warnings. */
static void CopyText(char* destination, size_t destinationSize, const char* source) {
    size_t length = 0;
    if (!destination || destinationSize < 2) return;
    if (!source) source = "";
    length = strlen(source);
    if (length > destinationSize - 1) length = destinationSize - 1;
    if (length > 0) memcpy(destination, source, length);
    destination[length] = '\0';
}

/* Join a directory and a file name into a fixed buffer, without sprintf. */
static void JoinPath(char* destination, size_t destinationSize,
                     const char* directory, const char* fileName) {
    size_t length = 0;
    if (!destination || destinationSize < 2) return;
    destination[0] = '\0';
    if (!directory) directory = "";
    if (!fileName) fileName = "";

    length = strlen(directory);
    if (length > destinationSize - 1) length = destinationSize - 1;
    if (length > 0) memcpy(destination, directory, length);

    if (length + 1 < destinationSize && fileName[0] != '\0') {
        size_t fileNameLength = strlen(fileName);
        size_t room = destinationSize - length - 2; /* keep space for '\' and NUL */
        if (fileNameLength > room) fileNameLength = room;
        destination[length] = '\\';
        if (fileNameLength > 0) {
            memcpy(destination + length + 1, fileName, fileNameLength);
        }
        length += 1 + fileNameLength;
    }
    destination[length] = '\0';
}

void PomodoroStats_DataDir(char* out, size_t outSize) {
    char configPath[MAX_PATH];
    if (!out || outSize < 2) return;
    out[0] = '\0';

    GetConfigPath(configPath, MAX_PATH);
    CopyText(out, outSize, configPath);

    char* separator = strrchr(out, '\\');
    if (!separator) separator = strrchr(out, '/');
    if (separator) *separator = '\0';
}

const char* PomodoroStats_ProjectsFilePath(void) {
    static char path[MAX_PATH];
    char dir[MAX_PATH];
    PomodoroStats_DataDir(dir, sizeof(dir));
    JoinPath(path, sizeof(path), dir, "pomodoro_projects.txt");
    return path;
}

static void EnsureProjectsFile(void) {
    const char* path = PomodoroStats_ProjectsFilePath();
    FILE* probe = OpenUtf8File(path, L"rb");
    if (probe) {
        fclose(probe);
        return;
    }

    FILE* file = OpenUtf8File(path, L"wb");
    if (!file) return;
    fputs("# One project name per line (UTF-8). Also editable in the statistics window.\n", file);
    fputs("Default\n", file);
    fclose(file);
}

int PomodoroStats_ListProjects(char names[POMODORO_STATS_MAX_PROJECTS][POMODORO_STATS_NAME_MAX]) {
    int count = 0;
    char line[512];
    FILE* file = NULL;

    if (!names) return 0;

    EnsureProjectsFile();

    file = OpenUtf8File(PomodoroStats_ProjectsFilePath(), L"rb");
    if (!file) return 0;

    while (count < POMODORO_STATS_MAX_PROJECTS && fgets(line, sizeof(line), file)) {
        char* start = line;
        char* end = NULL;
        while (*start == ' ' || *start == '\t') start++;
        end = start + strlen(start);
        while (end > start && (end[-1] == '\n' || end[-1] == '\r' ||
                               end[-1] == ' ' || end[-1] == '\t')) {
            end--;
        }
        *end = '\0';
        if (*start == '\0' || *start == '#' || *start == ';') continue;
        CopyText(names[count], POMODORO_STATS_NAME_MAX, start);
        count++;
    }

    fclose(file);
    return count;
}

const char* PomodoroStats_CurrentProject(void) {
    char value[POMODORO_STATS_NAME_MAX];
    char configPath[MAX_PATH];

    if (g_currentProjectLoaded) return g_currentProject;

    value[0] = '\0';
    GetConfigPath(configPath, MAX_PATH);
    ReadIniString(INI_SECTION_POMODORO, "POMODORO_CURRENT_PROJECT", "",
                  value, sizeof(value), configPath);

    if (value[0] == '\0') {
        char projects[POMODORO_STATS_MAX_PROJECTS][POMODORO_STATS_NAME_MAX];
        int count = PomodoroStats_ListProjects(projects);
        if (count > 0) {
            CopyText(value, sizeof(value), projects[0]);
        } else {
            CopyText(value, sizeof(value), "Default");
        }
    }

    CopyText(g_currentProject, sizeof(g_currentProject), value);
    g_currentProjectLoaded = TRUE;
    return g_currentProject;
}

void PomodoroStats_SetCurrentProject(const char* name) {
    char configPath[MAX_PATH];
    if (!name || !*name) return;

    CopyText(g_currentProject, sizeof(g_currentProject), name);
    g_currentProjectLoaded = TRUE;

    GetConfigPath(configPath, MAX_PATH);
    WriteIniString(INI_SECTION_POMODORO, "POMODORO_CURRENT_PROJECT",
                   g_currentProject, configPath);
}

static void SanitizeCsvField(const char* source, char* destination, size_t destinationSize) {
    size_t written = 0;
    if (!destination || destinationSize == 0) return;
    while (source && source[written] != '\0' && written + 1 < destinationSize) {
        char current = source[written];
        if (current == ',' || current == '\r' || current == '\n') current = ' ';
        destination[written] = current;
        written++;
    }
    destination[written] = '\0';
}

void PomodoroStats_RecordSession(const char* project, int seconds) {
    char dir[MAX_PATH];
    char path[MAX_PATH];
    char safeProject[POMODORO_STATS_NAME_MAX];
    char timestamp[32];
    BOOL needsBom = TRUE;
    FILE* probe = NULL;
    FILE* file = NULL;
    time_t now = 0;
    struct tm* localTime = NULL;

    if (seconds <= 0) return;

    PomodoroStats_DataDir(dir, sizeof(dir));
    JoinPath(path, sizeof(path), dir, "pomodoro_stats.csv");

    probe = OpenUtf8File(path, L"rb");
    if (probe) {
        needsBom = FALSE;
        fclose(probe);
    }

    file = OpenUtf8File(path, L"ab");
    if (!file) {
        LOG_WARNING("PomodoroStats: failed to open the statistics file");
        return;
    }
    if (needsBom) {
        fwrite("\xEF\xBB\xBF", 1, 3, file); /* UTF-8 BOM so Excel reads it correctly */
    }

    SanitizeCsvField((project && *project) ? project : "Default",
                     safeProject, sizeof(safeProject));

    timestamp[0] = '\0';
    now = time(NULL);
    localTime = localtime(&now);
    if (localTime) {
        strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", localTime);
    }

    fprintf(file, "%s,%s,%d\n", timestamp, safeProject, seconds);
    fclose(file);
}

void PomodoroStats_PromptInterruptedWork(HWND hwnd) {
    wchar_t message[256];
    int elapsedMinutes = 0;

    if (current_pomodoro_phase == POMODORO_PHASE_IDLE) return;
    if ((current_pomodoro_time_index % 2) != 0) return; /* break interval */
    elapsedMinutes = (int)(countdown_elapsed_time / 60);
    if (elapsedMinutes < 1) return;

    _snwprintf_s(message, _countof(message), _TRUNCATE,
                 GetLocalizedString(NULL,
                     L"Timer already used %d min. Record it to this project?"),
                 elapsedMinutes);

    if (MessageBoxW(hwnd, message, L"Catime",
                    MB_YESNO | MB_ICONQUESTION | MB_TOPMOST) == IDYES) {
        PomodoroStats_RecordSession(PomodoroStats_CurrentProject(),
                                    elapsedMinutes * 60);
    }
}

FILE* PomodoroStats_OpenFile(const char* utf8Path, const wchar_t* mode) {
    return OpenUtf8File(utf8Path, mode);
}

const char* PomodoroStats_StatsFilePath(void) {
    static char path[MAX_PATH];
    char directory[MAX_PATH];

    PomodoroStats_DataDir(directory, sizeof(directory));
    JoinPath(path, sizeof(path), directory, "pomodoro_stats.csv");
    return path;
}

const char* PomodoroStats_ColorsFilePath(void) {
    static char path[MAX_PATH];
    char directory[MAX_PATH];

    PomodoroStats_DataDir(directory, sizeof(directory));
    JoinPath(path, sizeof(path), directory, "pomodoro_colors.txt");
    return path;
}

BOOL PomodoroStats_SaveProjects(char names[POMODORO_STATS_MAX_PROJECTS][POMODORO_STATS_NAME_MAX],
                                int count) {
    FILE* file = NULL;
    int index = 0;

    if (count < 0) count = 0;
    if (count > POMODORO_STATS_MAX_PROJECTS) count = POMODORO_STATS_MAX_PROJECTS;

    file = OpenUtf8File(PomodoroStats_ProjectsFilePath(), L"wb");
    if (!file) return FALSE;

    fputs("# One project name per line (UTF-8). Also editable in the statistics window.\n",
          file);
    for (index = 0; index < count; index++) {
        if (names[index][0] == '\0') continue;
        fputs(names[index], file);
        fputc('\n', file);
    }
    fclose(file);
    return TRUE;
}
