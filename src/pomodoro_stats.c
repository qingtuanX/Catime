/**
 * @file pomodoro_stats.c
 * @brief Per-project pomodoro statistics (record + viewer integration).
 */

#include "pomodoro_stats.h"

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <wchar.h>
#include <shellapi.h>

#include "config.h"
#include "language.h"
#include "log.h"

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

void PomodoroStats_DataDir(char* out, size_t outSize) {
    if (!out || outSize == 0) return;
    out[0] = '\0';

    char configPath[MAX_PATH];
    GetConfigPath(configPath, MAX_PATH);
    strncpy(out, configPath, outSize - 1);
    out[outSize - 1] = '\0';

    char* separator = strrchr(out, '\\');
    if (!separator) separator = strrchr(out, '/');
    if (separator) *separator = '\0';
}

const char* PomodoroStats_ProjectsFilePath(void) {
    static char path[MAX_PATH];
    char dir[MAX_PATH];
    PomodoroStats_DataDir(dir, sizeof(dir));
    snprintf(path, sizeof(path), "%s\\pomodoro_projects.txt", dir);
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
    if (!names) return 0;

    EnsureProjectsFile();

    FILE* file = OpenUtf8File(PomodoroStats_ProjectsFilePath(), L"rb");
    if (!file) return 0;

    char line[512];
    while (count < POMODORO_STATS_MAX_PROJECTS && fgets(line, sizeof(line), file)) {
        char* start = line;
        while (*start == ' ' || *start == '\t') start++;
        char* end = start + strlen(start);
        while (end > start && (end[-1] == '\n' || end[-1] == '\r' ||
                               end[-1] == ' ' || end[-1] == '\t')) {
            end--;
        }
        *end = '\0';
        if (*start == '\0' || *start == '#' || *start == ';') continue;
        strncpy(names[count], start, POMODORO_STATS_NAME_MAX - 1);
        names[count][POMODORO_STATS_NAME_MAX - 1] = '\0';
        count++;
    }

    fclose(file);
    return count;
}

const char* PomodoroStats_CurrentProject(void) {
    if (!g_currentProjectLoaded) {
        char value[POMODORO_STATS_NAME_MAX] = {0};
        char configPath[MAX_PATH];
        GetConfigPath(configPath, MAX_PATH);
        ReadIniString(INI_SECTION_POMODORO, "POMODORO_CURRENT_PROJECT", "",
                      value, sizeof(value), configPath);

        if (value[0] == '\0') {
            char projects[POMODORO_STATS_MAX_PROJECTS][POMODORO_STATS_NAME_MAX];
            int count = PomodoroStats_ListProjects(projects);
            if (count > 0) {
                strncpy(value, projects[0], sizeof(value) - 1);
            } else {
                strncpy(value, "Default", sizeof(value) - 1);
            }
        }

        strncpy(g_currentProject, value, sizeof(g_currentProject) - 1);
        g_currentProject[sizeof(g_currentProject) - 1] = '\0';
        g_currentProjectLoaded = TRUE;
    }
    return g_currentProject;
}

void PomodoroStats_SetCurrentProject(const char* name) {
    if (!name || !*name) return;

    strncpy(g_currentProject, name, sizeof(g_currentProject) - 1);
    g_currentProject[sizeof(g_currentProject) - 1] = '\0';
    g_currentProjectLoaded = TRUE;

    char configPath[MAX_PATH];
    GetConfigPath(configPath, MAX_PATH);
    WriteIniString(INI_SECTION_POMODORO, "POMODORO_CURRENT_PROJECT",
                   g_currentProject, configPath);
}

static void SanitizeCsvField(const char* source, char* destination, size_t destinationSize) {
    size_t written = 0;
    if (!destination || destinationSize == 0) return;
    for (size_t i = 0; source && source[i] != '\0' && written + 1 < destinationSize; i++) {
        char current = source[i];
        if (current == ',' || current == '\r' || current == '\n') current = ' ';
        destination[written++] = current;
    }
    destination[written] = '\0';
}

void PomodoroStats_RecordSession(const char* project, int seconds) {
    if (seconds <= 0) return;

    char dir[MAX_PATH];
    PomodoroStats_DataDir(dir, sizeof(dir));

    char path[MAX_PATH];
    snprintf(path, sizeof(path), "%s\\pomodoro_stats.csv", dir);

    BOOL needsBom = TRUE;
    FILE* probe = OpenUtf8File(path, L"rb");
    if (probe) {
        needsBom = FALSE;
        fclose(probe);
    }

    FILE* file = OpenUtf8File(path, L"ab");
    if (!file) {
        LOG_WARNING("PomodoroStats: failed to open the statistics file");
        return;
    }
    if (needsBom) {
        fwrite("\xEF\xBB\xBF", 1, 3, file); /* UTF-8 BOM so Excel reads it correctly */
    }

    char safeProject[POMODORO_STATS_NAME_MAX];
    SanitizeCsvField(project && *project ? project : "Default", safeProject, sizeof(safeProject));

    char timestamp[32] = "";
    time_t now = time(NULL);
    struct tm* localTime = localtime(&now);
    if (localTime) {
        strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", localTime);
    }

    fprintf(file, "%s,%s,%d\n", timestamp, safeProject, seconds);
    fclose(file);
}

void PomodoroStats_PromptRecordElapsed(HWND hwnd, int elapsedSeconds) {
    if (elapsedSeconds < 60) return;

    wchar_t message[256];
    _snwprintf_s(message, _countof(message), _TRUNCATE,
                 GetLocalizedString(NULL,
                     L"Timer already used %d min. Record it to this project?"),
                 elapsedSeconds / 60);

    if (MessageBoxW(hwnd, message, L"Catime",
                    MB_YESNO | MB_ICONQUESTION | MB_TOPMOST) == IDYES) {
        PomodoroStats_RecordSession(PomodoroStats_CurrentProject(),
                                    (elapsedSeconds / 60) * 60);
    }
}

void PomodoroStats_OpenViewer(HWND hwnd) {
    wchar_t exePath[MAX_PATH];
    if (GetModuleFileNameW(NULL, exePath, MAX_PATH) == 0) return;

    wchar_t* separator = wcsrchr(exePath, L'\\');
    if (!separator) return;
    *separator = L'\0';

    wchar_t viewerPath[MAX_PATH];
    _snwprintf_s(viewerPath, _countof(viewerPath), _TRUNCATE,
                 L"%ls\\pomodoro-stats.exe", exePath);

    if (GetFileAttributesW(viewerPath) == INVALID_FILE_ATTRIBUTES) {
        MessageBoxW(hwnd,
                    L"\u627e\u4e0d\u5230 pomodoro-stats.exe\uff08\u5e94\u548c catime.exe \u653e\u5728\u540c\u4e00\u76ee\u5f55\uff09\n"
                    L"pomodoro-stats.exe was not found next to catime.exe",
                    L"Catime", MB_OK | MB_ICONWARNING | MB_TOPMOST);
        return;
    }

    ShellExecuteW(hwnd, L"open", viewerPath, NULL, NULL, SW_SHOWNORMAL);
}
