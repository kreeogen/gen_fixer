/*******************************************************************************
 * WINAMP FIXER PLUGIN - MAIN MODULE
 * ПЛАГИН WINAMP FIXER - ОСНОВНОЙ МОДУЛЬ
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Main entry point for Winamp Fixer - a comprehensive plugin that fixes various
 * issues in Winamp 2.95 related to Unicode, fonts, UI, and functionality.
 * Manages initialization, configuration, and coordination of all submodules.
 * 
 * Главная точка входа для Winamp Fixer - всеобъемлющего плагина, исправляющего
 * различные проблемы в Winamp 2.95, связанные с Unicode, шрифтами, UI и
 * функциональностью. Управляет инициализацией, конфигурацией и координацией
 * всех подмодулей.
 * 
 * ARCHITECTURE / АРХИТЕКТУРА:
 * 
 * The plugin is composed of multiple independent modules:
 * Плагин состоит из нескольких независимых модулей:
 * 
 * CORE MODULES (always active) / ОСНОВНЫЕ МОДУЛИ (всегда активны):
 * - mod_unicode_tags_fix     - ID3 tag encoding fixes / Исправление кодировок ID3-тегов
 * - mod_unicode_stream_fix   - SHOUTcast metadata fixes / Исправление метаданных SHOUTcast
 * - mod_unicode_ml_fix       - Media Library encoding / Кодировка библиотеки
 * - mod_ml_fonts             - Media Library font smoothing / Сглаживание шрифтов библиотеки
 * - mod_ml_icons             - Icon tinting for themes / Тонирование иконок для тем
 * - mod_ml_search            - Cyrillic search fix / Исправление поиска кириллицы
 * - mod_m3u8                 - M3U8 playlist support / Поддержка M3U8 плейлистов
 * - mod_skininstall          - Skin installation context menu / Контекстное меню установки скинов
 * 
 * OPTIONAL MODULES (configurable) / ОПЦИОНАЛЬНЫЕ МОДУЛИ (настраиваемые):
 * - mod_startup_foreground   - Force foreground on startup / Принудительно на передний план при старте
 * - mod_iconfix              - Fix window icons / Исправление иконок окон
 * - mod_menu_restart         - Add restart menu item / Добавить пункт меню перезапуска
 * - mod_mute                 - Mute hotkey (Ctrl+Space) / Горячая клавиша выключения звука
 * - mod_skin_delete          - Skin delete/rename in dialog / Удаление/переименование скинов в диалоге
 * - mod_plist_buttons        - Playlist editor zones / Зоны редактора плейлистов
 * - mod_videofontfix         - Video plugin font fix / Исправление шрифтов видео плагинов
 * 
 * CONFIGURATION / КОНФИГУРАЦИЯ:
 * Settings stored in plugin.ini in plugin directory.
 * User can enable/disable optional modules via preferences dialog.
 * Changes applied immediately without restart.
 * 
 * Настройки хранятся в plugin.ini в каталоге плагина.
 * Пользователь может включать/отключать опциональные модули через диалог настроек.
 * Изменения применяются немедленно без перезапуска.
 * 
 * COMPATIBILITY / СОВМЕСТИМОСТЬ:
 * - Compiler: Visual C++ 7.1 (VS2003)
 * - OS: Windows 98 through Windows 11
 * - Character set: ANSI with unicows.lib for Unicode support on Win9x
 * - Winamp: 2.95 and compatible versions
 * 
 * - Компилятор: Visual C++ 7.1 (VS2003)
 * - ОС: Windows 98 до Windows 11
 * - Кодировка: ANSI с unicows.lib для поддержки Unicode на Win9x
 * - Winamp: 2.95 и совместимые версии
 * 
 ******************************************************************************/

// Winamp Fixer (VC7.1 / Win98, ANSI)

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "SwitchLangUI.h"

#include "SDK/gen.h"            // winampGeneralPurposePlugin, wa_ipc.h
#include "SDK/wa_dlg.h"         // prefsDlgRec, IPC_ADD_PREFS_DLG

#include "Resources/plugin_common.h"
#include "Resources/resource.h"       // IDD_FIXER_CFG, IDB_FIXER, IDC_CHK_*

// Core UI modules / Основные UI модули
#include "Modules/mod_startup_foreground.h"
#include "Modules/mod_iconfix.h"
#include "Modules/mod_menu_restart.h"
#include "Modules/mod_mute.h"
#include "Modules/mod_skin_delete.h"
#include "Modules/mod_plist_buttons.h"
#include "Modules/mod_skininstall.h"

// M3U8 support / Поддержка M3U8
#include "m3u8/mod_unicode_m3u8.h"

// Unicode fixes / Исправления Unicode
#include "Unicode/mod_unicode_fullscreen_video_fix.h"
#include "Unicode/mod_unicode_ml_fix.h"
#include "Unicode/mod_unicode_tags_fix.h"
#include "Unicode/mod_unicode_stream_fix.h"

// Media Library enhancements / Улучшения медиатеки
#include "MediaLibrary/mod_ml_fonts.h"
#include "MediaLibrary/mod_ml_icons.h"
#include "MediaLibrary/mod_ml_search.h"

/*******************************************************************************
 * COMPILER AND LINKER CONFIGURATION
 * КОНФИГУРАЦИЯ КОМПИЛЯТОРА И ЛИНКОВЩИКА
 ******************************************************************************/

#ifdef _MSC_VER
// Set subsystem version to 4.0 for Windows 95/98 compatibility
// Установить версию подсистемы 4.0 для совместимости с Windows 95/98
#pragma comment(linker, "/SUBSYSTEM:WINDOWS,4.0")
#endif

// Required libraries / Необходимые библиотеки
#pragma comment(lib, "unicows.lib")   // Unicode layer for Win9x / Слой Unicode для Win9x
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "kernel32.lib")

/*******************************************************************************
 * VC7.1 / WIN9X COMPATIBILITY TYPEDEFS
 * ОПРЕДЕЛЕНИЯ ТИПОВ ДЛЯ СОВМЕСТИМОСТИ С VC7.1 / WIN9X
 * 
 * Visual C++ 7.1 (VS2003) doesn't define intptr_t/uintptr_t in older platform SDK.
 * Visual C++ 7.1 (VS2003) не определяет intptr_t/uintptr_t в старом platform SDK.
 ******************************************************************************/
#ifndef intptr_t
typedef int intptr_t;
#endif
#ifndef uintptr_t
typedef unsigned int uintptr_t;
#endif

// Winamp plugin header version / Версия заголовка плагина Winamp
#ifndef GPPHDR_VER
#define GPPHDR_VER 0x10
#endif

/*******************************************************************************
 * EXTERNAL FUNCTION DECLARATIONS
 * ОБЪЯВЛЕНИЯ ВНЕШНИХ ФУНКЦИЙ
 ******************************************************************************/

// Album art cover fix for MP3 tags / Исправление обложек альбомов для MP3-тегов
extern "C" void __cdecl MP3_TagsCoverFix(HWND hostWnd, const char* pathA);
extern "C" void __cdecl MP3_TagsCoverFix_Quit(void);

/*******************************************************************************
 * PLUGIN INTERFACE FUNCTIONS
 * ФУНКЦИИ ИНТЕРФЕЙСА ПЛАГИНА
 ******************************************************************************/

int  init(void);    // Called when plugin loads / Вызывается при загрузке плагина
void config(void);  // Called when user clicks "Configure" / Вызывается при клике "Настроить"
void quit(void);    // Called when plugin unloads / Вызывается при выгрузке плагина

/*******************************************************************************
 * WINAMP PLUGIN DESCRIPTOR
 * ДЕСКРИПТОР ПЛАГИНА WINAMP
 * 
 * This structure is read by Winamp to identify and initialize the plugin.
 * Эта структура читается Winamp для идентификации и инициализации плагина.
 ******************************************************************************/
winampGeneralPurposePlugin plugin =
{
    GPPHDR_VER,              // Header version / Версия заголовка
    (char*)PLUGIN_NAME,      // Plugin name (from SwitchLangUI.h) / Имя плагина
    init,                    // Init function / Функция инициализации
    config,                  // Config function / Функция конфигурации
    quit,                    // Quit function / Функция выхода
    0,                       // hwndParent (filled by Winamp) / hwndParent (заполняется Winamp)
    0                        // hDllInstance (filled by Winamp) / hDllInstance (заполняется Winamp)
};

/*******************************************************************************
 * winampGetGeneralPurposePlugin
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * DLL export function that Winamp calls to get plugin descriptor.
 * Функция экспорта DLL, которую Winamp вызывает для получения дескриптора плагина.
 * 
 * CRITICAL / КРИТИЧНО:
 * Must be exported with C linkage (__declspec(dllexport) extern "C").
 * Winamp loads plugin by calling GetProcAddress for this function.
 * 
 * Должна быть экспортирована с C linkage.
 * Winamp загружает плагин, вызывая GetProcAddress для этой функции.
 ******************************************************************************/
extern "C" __declspec(dllexport)
winampGeneralPurposePlugin* winampGetGeneralPurposePlugin(void)
{
    return &plugin;
}

/*******************************************************************************
 * WINAMP IPC CONSTANTS
 * КОНСТАНТЫ WINAMP IPC
 ******************************************************************************/

#ifndef WM_WA_IPC
#define WM_WA_IPC (WM_USER)  // Base message for Winamp IPC / Базовое сообщение для Winamp IPC
#endif
#define IPC_GETLISTPOS      125  // Get current playlist position / Получить текущую позицию плейлиста
#define IPC_GETPLAYLISTFILE 211  // Get playlist file at index / Получить файл плейлиста по индексу

/*******************************************************************************
 * GetCurrentFileA
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Gets currently playing file path from Winamp.
 * Получает путь текущего проигрываемого файла из Winamp.
 * 
 * PARAMETERS / ПАРАМЕТРЫ:
 * hwndWa - Winamp main window handle / Дескриптор главного окна Winamp
 * 
 * RETURNS / ВОЗВРАЩАЕТ:
 * Pointer to filename string (managed by Winamp, do not free)
 * Указатель на строку имени файла (управляется Winamp, не освобождать)
 ******************************************************************************/
static const char* GetCurrentFileA(HWND hwndWa)
{
    // Get current playlist position / Получить текущую позицию плейлиста
    int index = (int)SendMessage(hwndWa, WM_WA_IPC, 0, IPC_GETLISTPOS);
    
    // Get filename at that position / Получить имя файла на этой позиции
    return (const char*)SendMessage(hwndWa, WM_WA_IPC, (WPARAM)index, IPC_GETPLAYLISTFILE);
}

/*******************************************************************************
 * GLOBAL STATE
 * ГЛОБАЛЬНОЕ СОСТОЯНИЕ
 ******************************************************************************/

static HINSTANCE g_hDll = NULL;           // Plugin DLL instance / Экземпляр DLL плагина
static char g_iniPath[MAX_PATH] = {0};    // Path to plugin.ini / Путь к plugin.ini
static prefsDlgRec g_prefsPage;           // Preferences dialog descriptor / Дескриптор диалога настроек

/*******************************************************************************
 * BuildIniPath
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Constructs path to plugin.ini in plugin directory.
 * Строит путь к plugin.ini в каталоге плагина.
 * 
 * ALGORITHM / АЛГОРИТМ:
 * 1. Get plugin DLL path via GetModuleFileNameA
 * 2. Strip filename to get directory
 * 3. Append "plugin.ini"
 * 
 * 1. Получить путь DLL плагина через GetModuleFileNameA
 * 2. Удалить имя файла, чтобы получить каталог
 * 3. Добавить "plugin.ini"
 * 
 * NOTES / ПРИМЕЧАНИЯ:
 * Idempotent - only builds path once (checks g_iniPath[0]).
 * Идемпотентна - строит путь только один раз (проверяет g_iniPath[0]).
 ******************************************************************************/
static void BuildIniPath(void)
{
    if (g_iniPath[0]) return;  // Already built / Уже построен
    
    // Get DLL path / Получить путь DLL
    char dll[MAX_PATH]; 
    dll[0] = 0;
    GetModuleFileNameA(g_hDll, dll, MAX_PATH);
    
    // Find last backslash / Найти последний обратный слеш
    char* p = dll; 
    while (*p) ++p;
    while (p > dll && *(p - 1) != '\\') --p;
    *p = 0;  // Truncate at directory / Обрезать до каталога
    
    // Build ini path / Построить путь ini
    lstrcpyA(g_iniPath, dll);
    lstrcatA(g_iniPath, "plugin.ini");
}

/*******************************************************************************
 * CONFIGURATION STRUCTURE
 * СТРУКТУРА КОНФИГУРАЦИИ
 * 
 * Stores enabled/disabled state for all optional modules.
 * Values: 0=disabled, 1=enabled
 * 
 * Хранит состояние включено/отключено для всех опциональных модулей.
 * Значения: 0=отключено, 1=включено
 ******************************************************************************/
struct FixerCfg {
    int startup_foreground;  // Force Winamp to foreground on startup / Принудительно на передний план при старте
    int iconfix;             // Fix window icon issues / Исправить проблемы с иконками окон
    int restart_menu;        // Add "Restart Winamp" menu item / Добавить пункт меню "Перезапустить Winamp"
    int mute_hotkey;         // Enable Ctrl+Space mute hotkey / Включить горячую клавишу Ctrl+Space для выключения звука
    int skin_delete;         // Enable skin delete/rename in dialog / Включить удаление/переименование скинов в диалоге
    int pe_zones;            // Enable playlist editor clickable zones / Включить кликабельные зоны редактора плейлистов
    int video_fontfix;       // Fix fonts in video plugins / Исправить шрифты в видео плагинах
} g_cfg = {1,1,1,1,1,1,1};  // Default: all enabled / По умолчанию: всё включено

/*******************************************************************************
 * LoadCfg
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Loads configuration from plugin.ini file.
 * Загружает конфигурацию из файла plugin.ini.
 * 
 * FORMAT / ФОРМАТ:
 * [Fixer]
 * StartupForeground=1
 * IconFix=1
 * RestartMenu=1
 * MuteHotkey=1
 * SkinDelete=1
 * PeZones=1
 * VideoFontFix=1
 ******************************************************************************/
static void LoadCfg(void)
{
    BuildIniPath();
    g_cfg.startup_foreground = GetPrivateProfileIntA("Fixer", "StartupForeground", 1, g_iniPath);
    g_cfg.iconfix            = GetPrivateProfileIntA("Fixer", "IconFix",           1, g_iniPath);
    g_cfg.restart_menu       = GetPrivateProfileIntA("Fixer", "RestartMenu",       1, g_iniPath);
    g_cfg.mute_hotkey        = GetPrivateProfileIntA("Fixer", "MuteHotkey",        1, g_iniPath);
    g_cfg.skin_delete        = GetPrivateProfileIntA("Fixer", "SkinDelete",        1, g_iniPath);
    g_cfg.pe_zones           = GetPrivateProfileIntA("Fixer", "PeZones",           1, g_iniPath);
    g_cfg.video_fontfix      = GetPrivateProfileIntA("Fixer", "VideoFontFix",      1, g_iniPath);
}

/*******************************************************************************
 * SaveCfg
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Saves current configuration to plugin.ini file.
 * Сохраняет текущую конфигурацию в файл plugin.ini.
 ******************************************************************************/
static void SaveCfg(void)
{
    BuildIniPath();
    WritePrivateProfileStringA("Fixer", "StartupForeground", g_cfg.startup_foreground ? "1":"0", g_iniPath);
    WritePrivateProfileStringA("Fixer", "IconFix",           g_cfg.iconfix            ? "1":"0", g_iniPath);
    WritePrivateProfileStringA("Fixer", "RestartMenu",       g_cfg.restart_menu       ? "1":"0", g_iniPath);
    WritePrivateProfileStringA("Fixer", "MuteHotkey",        g_cfg.mute_hotkey        ? "1":"0", g_iniPath);
    WritePrivateProfileStringA("Fixer", "SkinDelete",        g_cfg.skin_delete        ? "1":"0", g_iniPath);
    WritePrivateProfileStringA("Fixer", "PeZones",           g_cfg.pe_zones           ? "1":"0", g_iniPath);
    WritePrivateProfileStringA("Fixer", "VideoFontFix",      g_cfg.video_fontfix      ? "1":"0", g_iniPath);
}

/*******************************************************************************
 * MODULE STATE TRACKING
 * ОТСЛЕЖИВАНИЕ СОСТОЯНИЯ МОДУЛЕЙ
 * 
 * Tracks which modules are currently initialized.
 * Used to ensure proper cleanup and prevent double-initialization.
 * 
 * Отслеживает, какие модули в данный момент инициализированы.
 * Используется для обеспечения правильной очистки и предотвращения двойной инициализации.
 ******************************************************************************/
static struct {
    int startup;   // mod_startup_foreground active / mod_startup_foreground активен
    int iconfix;   // mod_iconfix active / mod_iconfix активен
    int restart;   // mod_menu_restart active / mod_menu_restart активен
    int mute;      // mod_mute active / mod_mute активен
    int skindel;   // mod_skin_delete active / mod_skin_delete активен
    int pezones;   // mod_plist_buttons active / mod_plist_buttons активен
    int vidfont;   // mod_videofontfix active / mod_videofontfix активен
} g_started = {0,0,0,0,0,0,0};

/*******************************************************************************
 * ApplyRuntime
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Applies configuration changes at runtime by starting/stopping modules as needed.
 * Called when user changes settings in preferences dialog.
 * 
 * Применяет изменения конфигурации во время выполнения, запуская/останавливая
 * модули по мере необходимости. Вызывается, когда пользователь меняет настройки
 * в диалоге настроек.
 * 
 * PARAMETERS / ПАРАМЕТРЫ:
 * oldCfg - Previous configuration (unused currently, for future extensions)
 *          Предыдущая конфигурация (не используется сейчас, для будущих расширений)
 * 
 * ALGORITHM / АЛГОРИТМ:
 * For each optional module:
 * 1. If not started AND now enabled: initialize module, mark as started
 * 2. If started AND now disabled: cleanup module, mark as stopped
 * 
 * Для каждого опционального модуля:
 * 1. Если не запущен И теперь включён: инициализировать модуль, отметить как запущенный
 * 2. Если запущен И теперь отключён: очистить модуль, отметить как остановленный
 * 
 * NOTES / ПРИМЕЧАНИЯ:
 * Some modules require Winamp window handle, so we find it first.
 * Некоторые модули требуют дескриптор окна Winamp, поэтому сначала находим его.
 ******************************************************************************/
static void ApplyRuntime(const FixerCfg& oldCfg)
{
    HWND wa = FindWinamp();

    // Startup foreground (no window handle needed) / Принудительный передний план при старте (не требует дескриптора окна)
    if (!g_started.startup && g_cfg.startup_foreground) { 
        Startup_Init(); 
        g_started.startup=1; 
    }
    if ( g_started.startup && !g_cfg.startup_foreground) { 
        Startup_Quit(); 
        g_started.startup=0; 
    }

    // Window-dependent modules / Модули, зависящие от окна
    if (wa) {
        // Restart menu / Меню перезапуска
        if (!g_started.restart && g_cfg.restart_menu) { 
            Menu_Init(wa); 
            g_started.restart=1; 
        }
        if ( g_started.restart && !g_cfg.restart_menu) { 
            Menu_Quit(wa); 
            g_started.restart=0; 
        }
        
        // Icon fix / Исправление иконок
        if (!g_started.iconfix && g_cfg.iconfix) { 
            IconFix_Init(wa); 
            g_started.iconfix=1; 
        }
        if ( g_started.iconfix && !g_cfg.iconfix) { 
            IconFix_Quit(); 
            g_started.iconfix=0; 
        }
        
        // Video font fix / Исправление шрифтов видео
        if (!g_started.vidfont && g_cfg.video_fontfix) { 
            VideoFontFix_Init(wa); 
            g_started.vidfont=1; 
        }
        if ( g_started.vidfont && !g_cfg.video_fontfix) { 
            VideoFontFix_Quit(); 
            g_started.vidfont=0; 
        }
    }

    // Mute hotkey (global) / Горячая клавиша выключения звука (глобальная)
    if (!g_started.mute && g_cfg.mute_hotkey) { 
        Mute_Init(); 
        g_started.mute=1; 
    }
    if ( g_started.mute && !g_cfg.mute_hotkey) { 
        Mute_Quit(); 
        g_started.mute=0; 
    }

    // Skin delete / Удаление скинов
    if (!g_started.skindel && g_cfg.skin_delete) { 
        SkinDel_Init(); 
        g_started.skindel=1; 
    }
    if ( g_started.skindel && !g_cfg.skin_delete) { 
        SkinDel_Quit(); 
        g_started.skindel=0; 
    }

    // Playlist editor zones / Зоны редактора плейлистов
    if (!g_started.pezones && g_cfg.pe_zones) {
        PeZones_SetInstance(g_hDll);  // Needs DLL instance for resources / Нужен экземпляр DLL для ресурсов
        PeZones_Init();
        g_started.pezones=1;
    }
    if ( g_started.pezones && !g_cfg.pe_zones) { 
        PeZones_Quit(); 
        g_started.pezones=0; 
    }
}

/*******************************************************************************
 * UpdateFlagFromClick
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Updates configuration and applies changes when user clicks a checkbox in
 * preferences dialog.
 * 
 * Обновляет конфигурацию и применяет изменения, когда пользователь кликает
 * чекбокс в диалоге настроек.
 * 
 * PARAMETERS / ПАРАМЕТРЫ:
 * hDlg   - Dialog window handle / Дескриптор окна диалога
 * ctrlID - Control ID of clicked checkbox / ID контрола нажатого чекбокса
 * 
 * ALGORITHM / АЛГОРИТМ:
 * 1. Save old configuration
 * 2. Read checkbox state
 * 3. Update corresponding config flag
 * 4. Save configuration to file
 * 5. Apply runtime changes
 * 
 * 1. Сохранить старую конфигурацию
 * 2. Прочитать состояние чекбокса
 * 3. Обновить соответствующий флаг конфигурации
 * 4. Сохранить конфигурацию в файл
 * 5. Применить изменения во время выполнения
 ******************************************************************************/
static void UpdateFlagFromClick(HWND hDlg, int ctrlID)
{
    FixerCfg old = g_cfg;
    BOOL checked = (IsDlgButtonChecked(hDlg, ctrlID) == BST_CHECKED);

    // Map control ID to config flag / Сопоставить ID контрола с флагом конфигурации
    if      (ctrlID == IDC_CHK_STARTUP)  g_cfg.startup_foreground = checked;
    else if (ctrlID == IDC_CHK_ICONFIX)  g_cfg.iconfix            = checked;
    else if (ctrlID == IDC_CHK_RESTART)  g_cfg.restart_menu       = checked;
    else if (ctrlID == IDC_CHK_MUTE)     g_cfg.mute_hotkey        = checked;
    else if (ctrlID == IDC_CHK_SKINDEL)  g_cfg.skin_delete        = checked;
    else if (ctrlID == IDC_CHK_PEZONES)  g_cfg.pe_zones           = checked;
    else if (ctrlID == IDC_CHK_VIDFONT)  g_cfg.video_fontfix      = checked;

    SaveCfg();
    ApplyRuntime(old);
}

/*******************************************************************************
 * PrefsDlgProc
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Dialog procedure for plugin preferences page in Winamp settings.
 * Процедура диалога для страницы настроек плагина в настройках Winamp.
 * 
 * MESSAGES / СООБЩЕНИЯ:
 * WM_INITDIALOG - Initialize checkboxes and load logo bitmap
 *                 Инициализировать чекбоксы и загрузить битмап логотипа
 * WM_COMMAND    - Handle checkbox clicks
 *                 Обработать клики чекбоксов
 ******************************************************************************/
static INT_PTR CALLBACK PrefsDlgProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM)
{
    switch (uMsg)
    {
    case WM_INITDIALOG:
        // Initialize checkboxes from current config / Инициализировать чекбоксы из текущей конфигурации
        CheckDlgButton(hDlg, IDC_CHK_STARTUP, g_cfg.startup_foreground ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(hDlg, IDC_CHK_ICONFIX, g_cfg.iconfix            ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(hDlg, IDC_CHK_RESTART, g_cfg.restart_menu       ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(hDlg, IDC_CHK_MUTE,    g_cfg.mute_hotkey        ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(hDlg, IDC_CHK_SKINDEL, g_cfg.skin_delete        ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(hDlg, IDC_CHK_PEZONES, g_cfg.pe_zones           ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(hDlg, IDC_CHK_VIDFONT, g_cfg.video_fontfix      ? BST_CHECKED : BST_UNCHECKED);

        // Load and display plugin logo / Загрузить и отобразить логотип плагина
        {
            HBITMAP hbmp = (HBITMAP)LoadImage(g_hDll, MAKEINTRESOURCE(IDB_FIXER),
                                              IMAGE_BITMAP, 0, 0, LR_CREATEDIBSECTION);
            if (hbmp) 
                SendDlgItemMessage(hDlg, IDC_IMG, STM_SETIMAGE, IMAGE_BITMAP, (LPARAM)hbmp);
        }
        return TRUE;

    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        // Handle checkbox clicks / Обработать клики чекбоксов
        case IDC_CHK_STARTUP:
        case IDC_CHK_ICONFIX:
        case IDC_CHK_RESTART:
        case IDC_CHK_MUTE:
        case IDC_CHK_SKINDEL:
        case IDC_CHK_PEZONES:
        case IDC_CHK_VIDFONT:
            if (HIWORD(wParam) == BN_CLICKED)
                UpdateFlagFromClick(hDlg, LOWORD(wParam));
            return TRUE;
        }
        break;
    }
    return FALSE;
}

/*******************************************************************************
 * init
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Plugin initialization function. Called by Winamp when plugin is loaded.
 * Функция инициализации плагина. Вызывается Winamp при загрузке плагина.
 * 
 * INITIALIZATION ORDER / ПОРЯДОК ИНИЦИАЛИЗАЦИИ:
 * 1. Load configuration from plugin.ini
 * 2. Initialize core modules (always active)
 * 3. Register preferences dialog
 * 4. Initialize optional modules (based on config)
 * 
 * 1. Загрузить конфигурацию из plugin.ini
 * 2. Инициализировать основные модули (всегда активны)
 * 3. Зарегистрировать диалог настроек
 * 4. Инициализировать опциональные модули (на основе конфигурации)
 * 
 * CORE MODULES (always initialized) / ОСНОВНЫЕ МОДУЛИ (всегда инициализированы):
 * - Skin install context menu (one-time registry check)
 * - Unicode tag fixes (ID3v2 encoding)
 * - Unicode stream fixes (SHOUTcast metadata)
 * - Media Library font smoothing
 * - Media Library Unicode fixes
 * - Media Library icon tinting
 * - Cyrillic search fix
 * - M3U8 playlist loader
 * 
 * - Контекстное меню установки скинов (одноразовая проверка реестра)
 * - Исправления Unicode тегов (кодировка ID3v2)
 * - Исправления Unicode потоков (метаданные SHOUTcast)
 * - Сглаживание шрифтов библиотеки
 * - Исправления Unicode библиотеки
 * - Тонирование иконок библиотеки
 * - Исправление поиска кириллицы
 * - Загрузчик M3U8 плейлистов
 ******************************************************************************/
int init(void)
{
    // Load configuration / Загрузить конфигурацию
    LoadCfg();

    // Initialize core modules (always active) / Инициализировать основные модули (всегда активны)
    SkinInstall_RunOnce();                           // Ensure skin install registry entries / Обеспечить записи реестра установки скинов
    MP3_TagsFix_Init();                              // ID3v2 tag encoding fixes / Исправления кодировки ID3v2 тегов
    MP3_StreamFix_Init();                            // SHOUTcast/Icecast metadata fixes / Исправления метаданных SHOUTcast/Icecast
    PeZones_SetInstance(g_hDll);                     // Set DLL instance for playlist zones / Установить экземпляр DLL для зон плейлиста
    ML_SmoothFonts_Init();                           // Media Library font smoothing / Сглаживание шрифтов библиотеки
    ML_UnicodeFix_Init(plugin.hwndParent);           // Media Library Unicode fixes / Исправления Unicode библиотеки
    ML_IconsTint_Start(GetModuleHandleA(NULL));      // Media Library icon tinting / Тонирование иконок библиотеки
    ML_CyrSearchFix_Init();                          // Cyrillic search fix / Исправление поиска кириллицы
    m3u8_Loader_Init();                              // M3U8 playlist support / Поддержка M3U8 плейлистов

    // Register preferences dialog / Зарегистрировать диалог настроек
    g_prefsPage.hInst = g_hDll;
    g_prefsPage.dlgID = IDD_FIXER_CFG;
    g_prefsPage.name  = (char*)PREFFS_NAME;          // Dialog title from SwitchLangUI.h / Заголовок диалога из SwitchLangUI.h
    g_prefsPage.where = (intptr_t)&g_prefsPage;      // Unique identifier / Уникальный идентификатор
    g_prefsPage.proc  = PrefsDlgProc;
    SendMessage(plugin.hwndParent, WM_WA_IPC, (WPARAM)&g_prefsPage, IPC_ADD_PREFS_DLG);

    // Initialize optional modules based on config / Инициализировать опциональные модули на основе конфигурации
    HWND wa = FindWinamp();
    
    if (g_cfg.startup_foreground)  { 
        Startup_Init(); 
        g_started.startup = 1; 
    }
    if (wa && g_cfg.restart_menu)  { 
        Menu_Init(wa);  
        g_started.restart = 1; 
    }
    if (wa && g_cfg.iconfix)       { 
        IconFix_Init(wa); 
        g_started.iconfix = 1; 
    }
    if (g_cfg.mute_hotkey)         { 
        Mute_Init();   
        g_started.mute = 1; 
    }
    if (g_cfg.skin_delete)         { 
        SkinDel_Init(); 
        g_started.skindel = 1; 
    }
    if (g_cfg.pe_zones)            { 
        PeZones_Init(); 
        g_started.pezones = 1; 
    }
    if (wa && g_cfg.video_fontfix) { 
        VideoFontFix_Init(wa); 
        g_started.vidfont = 1; 
    }
    
    return 0;  // Success / Успех
}

/*******************************************************************************
 * config
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Called when user clicks plugin's "Configure" button in Winamp preferences.
 * Shows message directing user to preferences page.
 * 
 * Вызывается, когда пользователь кликает кнопку "Настроить" плагина в настройках Winamp.
 * Показывает сообщение, направляющее пользователя на страницу настроек.
 * 
 * NOTES / ПРИМЕЧАНИЯ:
 * Actual configuration UI is in preferences dialog (IDD_FIXER_CFG), not here.
 * This is just a helper message.
 * 
 * Фактический UI конфигурации находится в диалоге настроек (IDD_FIXER_CFG), не здесь.
 * Это просто вспомогательное сообщение.
 ******************************************************************************/
void config(void)
{
    MessageBoxA(0, APPCONFIG, APPCONFIG_TITLE, MB_OK | MB_ICONINFORMATION | MB_SETFOREGROUND);
}

/*******************************************************************************
 * quit
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Plugin cleanup function. Called by Winamp when plugin is unloaded.
 * Must cleanup all modules and free all resources.
 * 
 * Функция очистки плагина. Вызывается Winamp при выгрузке плагина.
 * Должна очистить все модули и освободить все ресурсы.
 * 
 * CRITICAL / КРИТИЧНО:
 * Order matters! Cleanup in reverse order of initialization where dependencies exist.
 * Must not leave any hooks, subclasses, or threads running.
 * 
 * Порядок важен! Очистка в обратном порядке инициализации, где существуют зависимости.
 * Не должно остаться никаких хуков, субклассов или запущенных потоков.
 * 
 * CLEANUP ORDER / ПОРЯДОК ОЧИСТКИ:
 * 1. Core modules (always active)
 * 2. Optional modules (if started)
 * 3. Unregister preferences dialog
 * 
 * 1. Основные модули (всегда активны)
 * 2. Опциональные модули (если запущены)
 * 3. Отменить регистрацию диалога настроек
 ******************************************************************************/
void quit(void)
{
    // Cleanup core modules / Очистить основные модули
    MP3_TagsFix_Quit();                              // Tag encoding fixes / Исправления кодировки тегов
    MP3_StreamFix_Quit();                            // Stream metadata fixes / Исправления метаданных потоков
    PeZones_Quit();                                  // Playlist zones / Зоны плейлиста
    ML_UnicodeFix_Quit(plugin.hwndParent);           // Media Library Unicode / Unicode библиотеки
    ML_SmoothFonts_Quit();                           // Media Library fonts / Шрифты библиотеки
    ML_IconsTint_Stop();                             // Icon tinting / Тонирование иконок
    ML_CyrSearchFix_Quit();                          // Cyrillic search / Поиск кириллицы
    m3u8_Loader_Quit();                              // M3U8 loader / Загрузчик M3U8

    // Cleanup optional modules (if started) / Очистить опциональные модули (если запущены)
    if (g_started.skindel) SkinDel_Quit();
    if (g_started.iconfix) IconFix_Quit();
    if (g_started.restart) { 
        HWND wa = FindWinamp(); 
        if (wa) Menu_Quit(wa); 
    }
    if (g_started.startup) Startup_Quit();
    if (g_started.mute)    Mute_Quit();
    if (g_started.pezones) PeZones_Quit();
    if (g_started.vidfont) VideoFontFix_Quit();

    // Unregister preferences dialog / Отменить регистрацию диалога настроек
#ifdef IPC_REMOVE_PREFS_DLG
    SendMessage(plugin.hwndParent, WM_WA_IPC, (WPARAM)&g_prefsPage, IPC_REMOVE_PREFS_DLG);
#endif
}

/*******************************************************************************
 * DllMain
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * DLL entry point. Called by Windows when DLL is loaded/unloaded.
 * Точка входа DLL. Вызывается Windows при загрузке/выгрузке DLL.
 * 
 * PARAMETERS / ПАРАМЕТРЫ:
 * hInst  - DLL instance handle / Дескриптор экземпляра DLL
 * reason - Load/unload reason / Причина загрузки/выгрузки
 * 
 * CRITICAL / КРИТИЧНО:
 * Must call DisableThreadLibraryCalls to avoid per-thread notifications.
 * Improves performance and avoids potential issues.
 * 
 * Должен вызвать DisableThreadLibraryCalls для избежания уведомлений на каждый поток.
 * Улучшает производительность и избегает потенциальных проблем.
 ******************************************************************************/
BOOL APIENTRY DllMain(HINSTANCE hInst, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH) {
        g_hDll = hInst;
        
        // Disable per-thread attach/detach notifications
        // Отключить уведомления attach/detach на каждый поток
        DisableThreadLibraryCalls(hInst);
    }
    return TRUE;
}