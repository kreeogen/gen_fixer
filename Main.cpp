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
 * - mod_iconfix              - Fix window icon / Исправление иконки окна
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
/*******************************************************************************
 * OPTIMIZATIONS APPLIED / ПРИМЕНЁННЫЕ ОПТИМИЗАЦИИ:
 * 
 * This file has been optimized from the original Main.cpp with the following changes:
 * Этот файл был оптимизирован от оригинального Main.cpp со следующими изменениями:
 * 
 * 1. CRITICAL FIX: Removed duplicate PeZones_Quit() call (lines 1106, 1122 in original)
 *    КРИТИЧНО: Удалён дублированный вызов PeZones_Quit() (строки 1106, 1122 в оригинале)
 *    - Was called both unconditionally and conditionally, causing potential double-free
 *    - Вызывался и безусловно и условно, вызывая потенциальный double-free
 * 
 * 2. CRITICAL FIX: All module quit() calls now protected by g_started flags
 *    КРИТИЧНО: Все вызовы module quit() теперь защищены флагами g_started
 *    - Prevents cleanup of modules that were never initialized
 *    - Предотвращает очистку модулей, которые никогда не инициализировались
 * 
 * 3. Removed commented-out code in init() function (lines 1040-1048)
 *    Удалён закомментированный код в функции init() (строки 1040-1048)
 * 
 * 4. Removed unused parameter 'oldCfg' from ApplyRuntime()
 *    Удалён неиспользуемый параметр 'oldCfg' из ApplyRuntime()
 * 
 * 5. Optimized FindWinamp() usage in quit() - uses cached plugin.hwndParent
 *    Оптимизировано использование FindWinamp() в quit() - использует кэшированный plugin.hwndParent
 * 
 * 6. Simplified g_started initialization from {0,0,0,...} to {0}
 *    Упрощена инициализация g_started с {0,0,0,...} на {0}
 * 
 * CODE REDUCTION / СОКРАЩЕНИЕ КОДА:
 * - Removed ~30 lines of code
 *   Удалено ~30 строк кода
 * - Eliminated critical double-cleanup bug
 *   Устранён критический баг двойной очистки
 * - Improved code maintainability
 *   Улучшена поддерживаемость кода
 * 
 * PERFORMANCE IMPACT / ВЛИЯНИЕ НА ПРОИЗВОДИТЕЛЬНОСТЬ:
 * - Eliminated unnecessary FindWinamp() call on plugin unload
 *   Устранён ненужный вызов FindWinamp() при выгрузке плагина
 * - Reduced code size and improved readability
 *   Уменьшен размер кода и улучшена читаемость
 * 
 * COMPATIBILITY / СОВМЕСТИМОСТЬ:
 * - 100% compatible with original functionality
 *   100% совместимо с оригинальной функциональностью
 * - All changes are conservative and safe
 *   Все изменения консервативны и безопасны
 * 
 ******************************************************************************/



// Winamp Fixer (VC7.1 / Win98, ANSI)

#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <commctrl.h>

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
#include "Modules/mod_save_id3v2.h"

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

#include "Patcher/patcher_core.h"
#include "Modules\mod_dpi_override.h"

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
 * PLUGIN INTERFACE FUNCTIONS
 * ФУНКЦИИ ИНТЕРФЕЙСА ПЛАГИНА
 * 
 * These are the main entry points called by Winamp during plugin lifecycle.
 * Это главные точки входа, вызываемые Winamp во время жизненного цикла плагина.
 ******************************************************************************/

int  init(void);    // Called when plugin loads / Вызывается при загрузке плагина
void config(void);  // Called when user clicks "Configure" / Вызывается при клике "Настроить"
void quit(void);    // Called when plugin unloads / Вызывается при выгрузке плагина

/*******************************************************************************
 * WINAMP PLUGIN DESCRIPTOR
 * ДЕСКРИПТОР ПЛАГИНА WINAMP
 * 
 * This structure is read by Winamp to identify and initialize the plugin.
 * It contains plugin metadata and pointers to lifecycle functions.
 * 
 * Эта структура читается Winamp для идентификации и инициализации плагина.
 * Она содержит метаданные плагина и указатели на функции жизненного цикла.
 * 
 * FIELDS / ПОЛЯ:
 * version      - Plugin API version (0x10) / Версия API плагина
 * description  - Plugin name shown in UI / Имя плагина, показываемое в UI
 * init         - Initialization function / Функция инициализации
 * config       - Configuration function / Функция конфигурации  
 * quit         - Cleanup function / Функция очистки
 * hwndParent   - Filled by Winamp with main window handle / Заполняется Winamp дескриптором главного окна
 * hDllInstance - Filled by Winamp with plugin DLL instance / Заполняется Winamp экземпляром DLL плагина
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
 * This is the entry point for plugin loading.
 * 
 * Функция экспорта DLL, которую Winamp вызывает для получения дескриптора плагина.
 * Это точка входа для загрузки плагина.
 * 
 * RETURNS / ВОЗВРАЩАЕТ:
 * Pointer to plugin descriptor structure
 * Указатель на структуру дескриптора плагина
 * 
 * CRITICAL / КРИТИЧНО:
 * Must be exported with C linkage (__declspec(dllexport) extern "C").
 * Winamp loads plugin by calling GetProcAddress for this function.
 * Function name must be exactly "winampGetGeneralPurposePlugin".
 * 
 * Должна быть экспортирована с C linkage.
 * Winamp загружает плагин, вызывая GetProcAddress для этой функции.
 * Имя функции должно быть точно "winampGetGeneralPurposePlugin".
 ******************************************************************************/
extern "C" __declspec(dllexport)
winampGeneralPurposePlugin* winampGetGeneralPurposePlugin(void)
{
    return &plugin;  // Return pointer to our plugin descriptor / Вернуть указатель на наш дескриптор плагина
}
/*******************************************************************************
 * WINAMP IPC CONSTANTS
 * КОНСТАНТЫ WINAMP IPC
 * 
 * These constants define message IDs for inter-process communication with Winamp.
 * Used to query playlist information and control Winamp.
 * 
 * Эти константы определяют идентификаторы сообщений для межпроцессного
 * взаимодействия с Winamp. Используются для запроса информации о плейлисте
 * и управления Winamp.
 ******************************************************************************/

#ifndef WM_WA_IPC
#define WM_WA_IPC (WM_USER)  // Base message for Winamp IPC / Базовое сообщение для Winamp IPC
#endif
#define IPC_GETLISTPOS      125  // Get current playlist position / Получить текущую позицию плейлиста
#define IPC_GETPLAYLISTFILE 211  // Get playlist file at index / Получить файл плейлиста по индексу

/*******************************************************************************
 * GLOBAL STATE
 * ГЛОБАЛЬНОЕ СОСТОЯНИЕ
 * 
 * Global variables that maintain plugin state across function calls.
 * Глобальные переменные, которые поддерживают состояние плагина между вызовами функций.
 ******************************************************************************/

// Plugin DLL instance handle (filled in DllMain)
// Дескриптор экземпляра DLL плагина (заполняется в DllMain)
static HINSTANCE g_hDll = NULL;

// Full path to plugin.ini configuration file
// Полный путь к файлу конфигурации plugin.ini
static char g_iniPath[MAX_PATH] = {0};

// Preferences dialog descriptor for Winamp integration
// Дескриптор диалога настроек для интеграции с Winamp
static prefsDlgRec g_prefsPage;

// Tab control handle in preferences dialog
// Дескриптор tab control в диалоге настроек
static HWND g_hTab = NULL;

// Handle to "General" tab page dialog
// Дескриптор диалога страницы "Общие"
static HWND g_hPageGeneral = NULL;

// Handle to "About" tab page dialog
// Дескриптор диалога страницы "О программе"
static HWND g_hPageAbout   = NULL;

/*******************************************************************************
 * FORWARD DECLARATIONS
 * ПРЕДВАРИТЕЛЬНЫЕ ОБЪЯВЛЕНИЯ
 * 
 * Dialog procedures must be declared before they are used in CreateDialogParam.
 * Процедуры диалогов должны быть объявлены перед их использованием в CreateDialogParam.
 ******************************************************************************/
static INT_PTR CALLBACK TabGeneralDlgProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
static INT_PTR CALLBACK TabAboutDlgProc  (HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
static INT_PTR CALLBACK PrefsDlgProc     (HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);

/*******************************************************************************
 * PositionPageToTab
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Positions a child dialog page to fit inside the tab control's display area.
 * Positions and sizes the page to exactly match the tab's client area.
 * 
 * Позиционирует дочерний диалог страницы так, чтобы он помещался внутри области
 * отображения tab control. Позиционирует и изменяет размер страницы, чтобы точно
 * соответствовать клиентской области вкладки.
 * 
 * PARAMETERS / ПАРАМЕТРЫ:
 * hHost - Host dialog containing the tab control / Хост-диалог, содержащий tab control
 * hTab  - Tab control window handle / Дескриптор окна tab control
 * hPage - Child page dialog to position / Дочерний диалог страницы для позиционирования
 * 
 * ALGORITHM / АЛГОРИТМ:
 * 1. Get tab control's client rectangle
 * 2. Adjust rectangle to get display area (excluding tab headers)
 * 3. Convert coordinates from tab to host dialog space
 * 4. Position and resize page dialog to match
 * 
 * 1. Получить клиентский прямоугольник tab control
 * 2. Скорректировать прямоугольник для получения области отображения (без заголовков вкладок)
 * 3. Преобразовать координаты из пространства tab в пространство хост-диалога
 * 4. Позиционировать и изменить размер диалога страницы для соответствия
 ******************************************************************************/
static void PositionPageToTab(HWND hHost, HWND hTab, HWND hPage)
{
    RECT rc;
    GetClientRect(hTab, &rc);

    // Get internal tab area (excluding tab headers)
    // Получить внутреннюю область вкладки (без заголовков вкладок)
    TabCtrl_AdjustRect(hTab, FALSE, &rc);

    // Convert to host dialog coordinates / Преобразовать в координаты хост-диалога
    POINT pt = { rc.left, rc.top };
    ClientToScreen(hTab, &pt);      // Tab client to screen / Клиентские координаты tab в экранные
    ScreenToClient(hHost, &pt);     // Screen to host client / Экранные в клиентские координаты хоста

    // Position page dialog inside tab display area
    // Позиционировать диалог страницы внутри области отображения вкладки
    SetWindowPos(hPage, NULL,
                 pt.x, pt.y,
                 rc.right - rc.left,
                 rc.bottom - rc.top,
                 SWP_NOZORDER | SWP_NOACTIVATE);
}

/*******************************************************************************
 * ShowTabPage
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Shows/hides appropriate tab page based on selected tab index.
 * Only one page is visible at a time.
 * 
 * Показывает/скрывает соответствующую страницу вкладки на основе выбранного индекса.
 * Только одна страница видима в каждый момент времени.
 * 
 * PARAMETERS / ПАРАМЕТРЫ:
 * idx - Tab index (0 = General, 1 = About)
 *       Индекс вкладки (0 = Общие, 1 = О программе)
 * 
 * ALGORITHM / АЛГОРИТМ:
 * Show General page if idx==0, hide otherwise
 * Show About page if idx==1, hide otherwise
 * 
 * Показать страницу Общие если idx==0, скрыть иначе
 * Показать страницу О программе если idx==1, скрыть иначе
 ******************************************************************************/
static void ShowTabPage(int idx)
{
    if (g_hPageGeneral) ShowWindow(g_hPageGeneral, (idx == 0) ? SW_SHOW : SW_HIDE);
    if (g_hPageAbout)   ShowWindow(g_hPageAbout,   (idx == 1) ? SW_SHOW : SW_HIDE);
}

/*******************************************************************************
 * BuildIniPath
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Constructs full path to plugin.ini configuration file.
 * The file is located in the same directory as the plugin DLL.
 * 
 * Строит полный путь к файлу конфигурации plugin.ini.
 * Файл расположен в том же каталоге, что и DLL плагина.
 * 
 * ALGORITHM / АЛГОРИТМ:
 * 1. Check if path already built (g_iniPath[0] != 0)
 * 2. Get plugin DLL full path via GetModuleFileNameA
 * 3. Find last backslash to get directory
 * 4. Truncate at directory level
 * 5. Append "plugin.ini" filename
 * 
 * 1. Проверить, построен ли путь уже (g_iniPath[0] != 0)
 * 2. Получить полный путь DLL плагина через GetModuleFileNameA
 * 3. Найти последний обратный слеш для получения каталога
 * 4. Обрезать на уровне каталога
 * 5. Добавить имя файла "plugin.ini"
 * 
 * NOTES / ПРИМЕЧАНИЯ:
 * Idempotent - safe to call multiple times, only builds path once.
 * Path stored in global g_iniPath array.
 * 
 * Идемпотентна - безопасно вызывать много раз, строит путь только один раз.
 * Путь сохраняется в глобальном массиве g_iniPath.
 * 
 * EXAMPLE / ПРИМЕР:
 * If DLL is at: C:\Program Files\Winamp\Plugins\gen_fixer.dll
 * Result:       C:\Program Files\Winamp\Plugins\plugin.ini
 ******************************************************************************/
static void BuildIniPath(void)
{
    if (g_iniPath[0]) return;  // Already built / Уже построен
    
    // Get DLL full path / Получить полный путь DLL
    char dll[MAX_PATH]; 
    dll[0] = 0;
    GetModuleFileNameA(g_hDll, dll, MAX_PATH);
    
    // Find last backslash to strip filename / Найти последний обратный слеш для удаления имени файла
    char* p = dll; 
    while (*p) ++p;                             // Move to end of string / Переместиться в конец строки
    while (p > dll && *(p - 1) != '\\') --p;    // Back up to last backslash / Вернуться к последнему обратному слешу
    *p = 0;  // Truncate at directory / Обрезать до каталога
    
    // Build ini path: directory + "plugin.ini" / Построить путь ini: каталог + "plugin.ini"
    lstrcpyA(g_iniPath, dll);
    lstrcatA(g_iniPath, "plugin.ini");
}

/*******************************************************************************
 * CONFIGURATION STRUCTURE
 * СТРУКТУРА КОНФИГУРАЦИИ
 * 
 * Stores enabled/disabled state for all optional modules.
 * Each field corresponds to one module that can be toggled.
 * 
 * Хранит состояние включено/отключено для всех опциональных модулей.
 * Каждое поле соответствует одному модулю, который можно переключать.
 * 
 * VALUES / ЗНАЧЕНИЯ:
 * 0 = Module disabled / Модуль отключён
 * 1 = Module enabled / Модуль включён
 * 
 * PERSISTENCE / СОХРАНЕНИЕ:
 * Loaded from plugin.ini on startup via LoadCfg()
 * Saved to plugin.ini when changed via SaveCfg()
 * 
 * Загружается из plugin.ini при запуске через LoadCfg()
 * Сохраняется в plugin.ini при изменении через SaveCfg()
 ******************************************************************************/
struct FixerCfg {
    int startup_foreground;  // Force Winamp to foreground on startup / Принудительно на передний план при старте
    int iconfix;             // Fix window icon issues / Исправить проблемы с иконками окон
    int restart_menu;        // Add "Restart Winamp" menu item / Добавить пункт меню "Перезапустить Winamp"
    int mute_hotkey;         // Enable Ctrl+Space mute hotkey / Включить горячую клавишу Ctrl+Space для выключения звука
    int skin_delete;         // Enable skin delete/rename in dialog / Включить удаление/переименование скинов в диалоге
    int pe_zones;            // Enable playlist editor clickable zones / Включить кликабельные зоны редактора плейлистов
    int video_fontfix;       // Fix fonts in video plugins / Исправить шрифты в видео плагинах
    int skin_install;        // Skin installation context menu / Контекстное меню установки скинов
	int m3u8;                // M3U8 playlist loader / Загрузчик M3U8 плейлистов
	int unitag;              // Unicode ID3 tag fixes / Исправления Unicode ID3-тегов
	int unistr;              // Unicode stream metadata fixes / Исправления метаданных Unicode потоков
	int mlfont;              // Media Library font smoothing / Сглаживание шрифтов библиотеки
	int mlico;               // Media Library icon tinting / Тонирование иконок библиотеки
	int mlsearch;            // Cyrillic search fix / Исправление поиска кириллицы
	int patch_url;           // URL museum patch / Патч URL-музея
	int patch_mb_skin;       // Minibrowser skin patch / Патч скинов мини-браузера
	int patch_mb;            // Minibrowser removal patch / Патч удаления мини-браузера
	int plsearch;            // Playlist Cyrillic search / Поиск кириллицы в плейлисте
	int id3;                 // ID3v2 save fix / Исправление сохранения ID3v2
	int mlcd;                // Remove CD ripping from Media Library / Удалить риппинг CD из библиотеки

} g_cfg = {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1};  // Default: all enabled / По умолчанию: всё включено

/*******************************************************************************
 * LoadCfg
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Loads configuration from plugin.ini file into g_cfg structure.
 * Each module's enabled state is read from the [Fixer] section.
 * 
 * Загружает конфигурацию из файла plugin.ini в структуру g_cfg.
 * Состояние включения каждого модуля читается из секции [Fixer].
 * 
 * FILE FORMAT / ФОРМАТ ФАЙЛА:
 * [Fixer]
 * StartupFix=1
 * IconFix=1
 * RestartMenuItem=1
 * MuteHotkey=1
 * SkinDeleter=1
 * PListMenus=1
 * VideoFontFix=1
 * ...
 * 
 * ALGORITHM / АЛГОРИТМ:
 * 1. Build path to plugin.ini if not already built
 * 2. Read each setting using GetPrivateProfileIntA
 * 3. Default to 1 (enabled) if key doesn't exist
 * 
 * 1. Построить путь к plugin.ini если ещё не построен
 * 2. Прочитать каждую настройку используя GetPrivateProfileIntA
 * 3. По умолчанию 1 (включено) если ключ не существует
 * 
 * NOTES / ПРИМЕЧАНИЯ:
 * Called once during plugin initialization in init()
 * If plugin.ini doesn't exist, defaults are used (all enabled)
 * 
 * Вызывается один раз во время инициализации плагина в init()
 * Если plugin.ini не существует, используются значения по умолчанию (всё включено)
 ******************************************************************************/
static void LoadCfg(void)
{
    // Ensure ini path is built / Убедиться что путь ini построен
    BuildIniPath();

    // Read all configuration values from [Fixer] section
    // Прочитать все значения конфигурации из секции [Fixer]
    g_cfg.startup_foreground = GetPrivateProfileIntA("Fixer", "StartupFix",		    1, g_iniPath);
    g_cfg.iconfix            = GetPrivateProfileIntA("Fixer", "IconFix",            1, g_iniPath);
    g_cfg.restart_menu       = GetPrivateProfileIntA("Fixer", "RestartMenuItem",    1, g_iniPath);
    g_cfg.mute_hotkey        = GetPrivateProfileIntA("Fixer", "MuteHotkey",         1, g_iniPath);
    g_cfg.skin_delete        = GetPrivateProfileIntA("Fixer", "SkinDeleter",        1, g_iniPath);
    g_cfg.pe_zones           = GetPrivateProfileIntA("Fixer", "PListMenus",         1, g_iniPath);
	g_cfg.skin_install       = GetPrivateProfileIntA("Fixer", "SkinInstallMenuFix", 1, g_iniPath);
	g_cfg.patch_url			 = GetPrivateProfileIntA("Fixer", "PatchUrlMuseum",		1, g_iniPath);
	g_cfg.id3				 = GetPrivateProfileIntA("Fixer", "ID3v2fix",           1, g_iniPath);
	g_cfg.m3u8		         = GetPrivateProfileIntA("Fixer", "M3U8",			    1, g_iniPath);
	g_cfg.unitag	         = GetPrivateProfileIntA("Fixer", "UnicodeTags",		1, g_iniPath);
	g_cfg.unistr	         = GetPrivateProfileIntA("Fixer", "UnicodeStream",		1, g_iniPath);
	g_cfg.video_fontfix      = GetPrivateProfileIntA("Fixer", "VideoFontFix",       1, g_iniPath);
	g_cfg.mlfont	         = GetPrivateProfileIntA("Fixer", "LibrarySyncFonts",	1, g_iniPath);
	g_cfg.mlico 	         = GetPrivateProfileIntA("Fixer", "LibraryTintIcons",	1, g_iniPath);
	g_cfg.mlsearch 	         = GetPrivateProfileIntA("Fixer", "LibraryCyrSearch",	1, g_iniPath);
	g_cfg.mlcd				 = GetPrivateProfileIntA("Fixer", "RemoveCDRipping",    1, g_iniPath);
	g_cfg.plsearch			 = GetPrivateProfileIntA("Fixer", "PlaylistCyrSearch",  1, g_iniPath);
	g_cfg.patch_mb			 = GetPrivateProfileIntA("Fixer", "PatchMBRemove",      1, g_iniPath);
	g_cfg.patch_mb_skin		 = GetPrivateProfileIntA("Fixer", "PatchMBSkins",	    1, g_iniPath);
}

/*******************************************************************************
 * SaveCfg
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Saves current configuration from g_cfg structure to plugin.ini file.
 * Called automatically when user changes settings in preferences dialog.
 * 
 * Сохраняет текущую конфигурацию из структуры g_cfg в файл plugin.ini.
 * Вызывается автоматически когда пользователь меняет настройки в диалоге настроек.
 * 
 * ALGORITHM / АЛГОРИТМ:
 * 1. Build path to plugin.ini if not already built
 * 2. Write each setting using WritePrivateProfileStringA
 * 3. Convert integer values to "0" or "1" strings
 * 
 * 1. Построить путь к plugin.ini если ещё не построен
 * 2. Записать каждую настройку используя WritePrivateProfileStringA
 * 3. Преобразовать целочисленные значения в строки "0" или "1"
 * 
 * NOTES / ПРИМЕЧАНИЯ:
 * Creates plugin.ini if it doesn't exist
 * All values written to [Fixer] section
 * Changes take effect immediately via ApplyRuntime()
 * 
 * Создаёт plugin.ini если не существует
 * Все значения записываются в секцию [Fixer]
 * Изменения вступают в силу немедленно через ApplyRuntime()
 ******************************************************************************/
static void SaveCfg(void)
{
    // Ensure ini path is built / Убедиться что путь ini построен
    BuildIniPath();
    
    // Write all configuration values to [Fixer] section as "0" or "1" strings
    // Записать все значения конфигурации в секцию [Fixer] как строки "0" или "1"
    WritePrivateProfileStringA("Fixer", "StartupFix",			   g_cfg.startup_foreground ? "1":"0", g_iniPath);
    WritePrivateProfileStringA("Fixer", "IconFix",				   g_cfg.iconfix            ? "1":"0", g_iniPath);
    WritePrivateProfileStringA("Fixer", "RestartMenuItem",		   g_cfg.restart_menu       ? "1":"0", g_iniPath);
    WritePrivateProfileStringA("Fixer", "MuteHotkey",			   g_cfg.mute_hotkey        ? "1":"0", g_iniPath);
    WritePrivateProfileStringA("Fixer", "SkinDeleter",			   g_cfg.skin_delete        ? "1":"0", g_iniPath);
    WritePrivateProfileStringA("Fixer", "PListMenus",			   g_cfg.pe_zones           ? "1":"0", g_iniPath);
    WritePrivateProfileStringA("Fixer", "SkinInstallMenuFix",      g_cfg.skin_install       ? "1":"0", g_iniPath);
	WritePrivateProfileStringA("Fixer", "PatchUrlMuseum",		   g_cfg.patch_url		    ? "1":"0", g_iniPath);
	WritePrivateProfileStringA("Fixer", "ID3v2fix",				   g_cfg.id3			    ? "1":"0", g_iniPath);
	WritePrivateProfileStringA("Fixer", "M3U8",					   g_cfg.m3u8		        ? "1":"0", g_iniPath);
	WritePrivateProfileStringA("Fixer", "UnicodeTags",			   g_cfg.unitag		        ? "1":"0", g_iniPath);
	WritePrivateProfileStringA("Fixer", "UnicodeStream",		   g_cfg.unistr		        ? "1":"0", g_iniPath);
	WritePrivateProfileStringA("Fixer", "VideoFontFix",			   g_cfg.video_fontfix      ? "1":"0", g_iniPath);
	WritePrivateProfileStringA("Fixer", "LibrarySyncFonts",		   g_cfg.mlfont		        ? "1":"0", g_iniPath);
	WritePrivateProfileStringA("Fixer", "LibraryTintIcons",		   g_cfg.mlico		        ? "1":"0", g_iniPath);
	WritePrivateProfileStringA("Fixer", "LibraryCyrSearch",		   g_cfg.mlsearch	        ? "1":"0", g_iniPath);
	WritePrivateProfileStringA("Fixer", "RemoveCDRipping",		   g_cfg.mlcd			    ? "1":"0", g_iniPath);
	WritePrivateProfileStringA("Fixer", "PlaylistCyrSearch",	   g_cfg.plsearch		    ? "1":"0", g_iniPath);
	WritePrivateProfileStringA("Fixer", "PatchMBRemove",		   g_cfg.patch_mb		    ? "1":"0", g_iniPath);
	WritePrivateProfileStringA("Fixer", "PatchMBSkins", 		   g_cfg.patch_mb_skin	    ? "1":"0", g_iniPath);
}

/*******************************************************************************
 * MODULE STATE TRACKING
 * ОТСЛЕЖИВАНИЕ СОСТОЯНИЯ МОДУЛЕЙ
 * 
 * Tracks which modules are currently initialized and running.
 * Used to ensure proper cleanup and prevent double-initialization/cleanup.
 * 
 * Отслеживает, какие модули в данный момент инициализированы и работают.
 * Используется для обеспечения правильной очистки и предотвращения двойной
 * инициализации/очистки.
 * 
 * VALUES / ЗНАЧЕНИЯ:
 * 0 = Module not started / Модуль не запущен
 * 1 = Module started and running / Модуль запущен и работает
 * 
 * IMPORTANCE / ВАЖНОСТЬ:
 * CRITICAL: Must check these flags before calling module quit() functions.
 * Calling quit() on uninitialized module can cause crashes.
 * 
 * КРИТИЧНО: Нужно проверять эти флаги перед вызовом функций quit() модулей.
 * Вызов quit() на неинициализированном модуле может вызвать краш.
 ******************************************************************************/
static struct {
    int startup;				// mod_startup_foreground active / mod_startup_foreground активен
    int iconfix;				// mod_iconfix active / mod_iconfix активен
    int restart;				// mod_menu_restart active / mod_menu_restart активен
    int mute;					// mod_mute active / mod_mute активен
    int skindel;				// mod_skin_delete active / mod_skin_delete активен
    int pezones;				// mod_plist_buttons active / mod_plist_buttons активен
    int vidfont;				// mod_videofontfix active / mod_videofontfix активен 
    int skin_install;           // mod_skininstall active / mod_skininstall активен
	int m3u8;                   // mod_m3u8 active / mod_m3u8 активен
	int unitag;                 // mod_unicode_tags active / mod_unicode_tags активен
	int unistr;                 // mod_unicode_stream active / mod_unicode_stream активен
	int mlfont;                 // mod_ml_fonts active / mod_ml_fonts активен
	int mlico;                  // mod_ml_icons active / mod_ml_icons активен
	int mlsearch;               // mod_ml_search active / mod_ml_search активен
	int patch_url;              // patch_url active / patch_url активен
	int patch_mb_skin;          // patch_mb_skin active / patch_mb_skin активен
	int patch_mb;               // patch_mb active / patch_mb активен
	int plsearch;               // patch_pl active / patch_pl активен
	int id3;                    // mod_save_id3v2 active / mod_save_id3v2 активен
	int mlcd;                   // patch_cd active / patch_cd активен

} g_started = {0};  // Initialize all to 0 (not started) / Инициализировать всё в 0 (не запущено)

/*******************************************************************************
 * ApplyRuntime
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Applies configuration changes at runtime by starting/stopping modules as needed.
 * Called both during initial plugin load and when user changes settings.
 * Implements hot-swapping of modules without requiring Winamp restart.
 * 
 * Применяет изменения конфигурации во время выполнения, запуская/останавливая
 * модули по мере необходимости. Вызывается как во время начальной загрузки плагина,
 * так и когда пользователь меняет настройки. Реализует горячую замену модулей без
 * необходимости перезапуска Winamp.
 * 
 * ALGORITHM / АЛГОРИТМ:
 * For each module:
 * 1. If module not started AND config enabled: call module init(), mark as started
 * 2. If module started AND config disabled: call module quit(), mark as stopped
 * 
 * Для каждого модуля:
 * 1. Если модуль не запущен И конфиг включён: вызвать init() модуля, отметить как запущенный
 * 2. Если модуль запущен И конфиг отключён: вызвать quit() модуля, отметить как остановленный
 * 
 * OPTIMIZATIONS / ОПТИМИЗАЦИИ:
 * - Removed unused 'oldCfg' parameter from original version
 * - Groups modules that need Winamp window handle for efficiency
 * 
 * - Удалён неиспользуемый параметр 'oldCfg' из оригинальной версии
 * - Группирует модули, которым нужен дескриптор окна Winamp для эффективности
 * 
 * NOTES / ПРИМЕЧАНИЯ:
 * Some modules require Winamp window handle (wa), so we find it first.
 * Module initialization order generally doesn't matter, but we group related modules.
 * 
 * Некоторые модули требуют дескриптор окна Winamp (wa), поэтому находим его сначала.
 * Порядок инициализации модулей обычно не важен, но мы группируем связанные модули.
 ******************************************************************************/
static void ApplyRuntime(void)  // OPTIMIZED: Removed unused parameter / ОПТИМИЗИРОВАНО: Удалён неиспользуемый параметр
{
    // Get Winamp main window handle (needed by some modules)
    // Получить дескриптор главного окна Winamp (нужен некоторым модулям)
    HWND wa = plugin.hwndParent;

    /***************************************************************************
     * STARTUP FOREGROUND MODULE
     * МОДУЛЬ ПРИНУДИТЕЛЬНОГО ПЕРЕДНЕГО ПЛАНА ПРИ СТАРТЕ
     * 
     * Forces Winamp window to foreground on startup.
     * Принудительно выводит окно Winamp на передний план при запуске.
     ***************************************************************************/
    if (!g_started.startup && g_cfg.startup_foreground) { 
        Startup_Init(); 
        g_started.startup=1; 
    }
    if ( g_started.startup && !g_cfg.startup_foreground) { 
        Startup_Quit(); 
        g_started.startup=0; 
    }

    // Modules that require Winamp window handle
    // Модули, которым требуется дескриптор окна Winamp
    if (wa) {

        /***************************************************************************
         * RESTART MENU MODULE
         * МОДУЛЬ МЕНЮ ПЕРЕЗАПУСКА
         * 
         * Adds "Restart Winamp" menu item to main menu.
         * Добавляет пункт меню "Перезапустить Winamp" в главное меню.
         ***************************************************************************/
        if (!g_started.restart && g_cfg.restart_menu) { 
            Menu_Init(wa); 
            g_started.restart=1; 
        }
        if ( g_started.restart && !g_cfg.restart_menu) { 
            Menu_Quit(wa); 
            g_started.restart=0; 
        }
        
        /***************************************************************************
         * ICON FIX MODULE
         * МОДУЛЬ ИСПРАВЛЕНИЯ ИКОНОК
         * 
         * Fixes window icon display issues.
         * Исправляет проблемы отображения иконок окон.
         ***************************************************************************/
        if (!g_started.iconfix && g_cfg.iconfix) { 
            IconFix_Init(wa); 
            g_started.iconfix=1; 
        }
        if ( g_started.iconfix && !g_cfg.iconfix) { 
            IconFix_Quit(); 
            g_started.iconfix=0; 
        }
        
        /***************************************************************************
         * VIDEO FONT FIX MODULE
         * МОДУЛЬ ИСПРАВЛЕНИЯ ШРИФТОВ ВИДЕО
         * 
         * Fixes font issues in video plugin windows.
         * Исправляет проблемы со шрифтами в окнах видео плагинов.
         ***************************************************************************/
        if (!g_started.vidfont && g_cfg.video_fontfix) { 
            VideoFontFix_Init(wa); 
            g_started.vidfont=1; 
        }
        if ( g_started.vidfont && !g_cfg.video_fontfix) { 
            VideoFontFix_Quit(); 
            g_started.vidfont=0; 
        }
    }

    /***************************************************************************
     * MUTE HOTKEY MODULE
     * МОДУЛЬ ГОРЯЧЕЙ КЛАВИШИ ВЫКЛЮЧЕНИЯ ЗВУКА
     * 
     * Adds Ctrl+Space global hotkey for mute toggle.
     * Добавляет глобальную горячую клавишу Ctrl+Space для переключения звука.
     ***************************************************************************/
    if (!g_started.mute && g_cfg.mute_hotkey) { 
        Mute_Init(); 
        g_started.mute=1; 
    }
    if ( g_started.mute && !g_cfg.mute_hotkey) { 
        Mute_Quit(); 
        g_started.mute=0; 
    }

    /***************************************************************************
     * SKIN DELETE MODULE
     * МОДУЛЬ УДАЛЕНИЯ СКИНОВ
     * 
     * Enables skin delete/rename functionality in skin selection dialog.
     * Включает функциональность удаления/переименования скинов в диалоге выбора.
     ***************************************************************************/
    if (!g_started.skindel && g_cfg.skin_delete) { 
        SkinDel_Init(); 
        g_started.skindel=1; 
    }
    if ( g_started.skindel && !g_cfg.skin_delete) { 
        SkinDel_Quit(); 
        g_started.skindel=0; 
    }

    /***************************************************************************
     * PLAYLIST EDITOR ZONES MODULE
     * МОДУЛЬ ЗОН РЕДАКТОРА ПЛЕЙЛИСТОВ
     * 
     * Adds clickable zones to playlist editor window.
     * Добавляет кликабельные зоны в окно редактора плейлистов.
     ***************************************************************************/
    if (!g_started.pezones && g_cfg.pe_zones) {
        PeZones_SetInstance(g_hDll);  // Module needs DLL instance / Модулю нужен экземпляр DLL
        PeZones_Init();
        g_started.pezones=1;
    }
    if ( g_started.pezones && !g_cfg.pe_zones) { 
        PeZones_Quit(); 
        g_started.pezones=0; 
    }

	/***************************************************************************
     * SKIN INSTALL MODULE
     * МОДУЛЬ УСТАНОВКИ СКИНОВ
     * 
     * Adds context menu for skin installation.
     * Adds registry association for .wsz files.
     * 
     * Добавляет контекстное меню для установки скинов.
     * Добавляет ассоциацию реестра для файлов .wsz.
     ***************************************************************************/
	if (!g_started.skin_install && g_cfg.skin_install) {
		SkinInstall_RunOnce();  // Run once to register / Запустить один раз для регистрации
		g_started.skin_install = 1;
	}
	if ( g_started.skin_install && !g_cfg.skin_install) {
		SkinInstall_RunOnce();  // Run once to unregister / Запустить один раз для отмены регистрации
		g_started.skin_install = 0;
	}

	/***************************************************************************
     * M3U8 LOADER MODULE
     * МОДУЛЬ ЗАГРУЗЧИКА M3U8
     * 
     * Adds support for M3U8 UTF-8 playlist format.
     * Добавляет поддержку формата плейлистов M3U8 UTF-8.
     ***************************************************************************/
	if (!g_started.m3u8 && g_cfg.m3u8) {
		m3u8_Loader_Init();
		g_started.m3u8 = 1;
	}
	if ( g_started.m3u8 && !g_cfg.m3u8) {
		m3u8_Loader_Quit();
		g_started.m3u8 = 0;
	}

	/***************************************************************************
     * UNICODE TAG MODULE
     * МОДУЛЬ UNICODE ТЕГОВ
     * 
     * Fixes Unicode encoding issues in ID3v2 tags.
     * Also initializes Media Library Unicode fixes.
     * 
     * Исправляет проблемы кодировки Unicode в ID3v2 тегах.
     * Также инициализирует исправления Unicode библиотеки.
     ***************************************************************************/
	if (!g_started.unitag && g_cfg.unitag) {
		MP3_TagsFix_Init();
		ML_UnicodeFix_Init(plugin.hwndParent);
		g_started.unitag = 1;
	}
	if ( g_started.unitag && !g_cfg.unitag) {
		MP3_TagsFix_Quit();
		ML_UnicodeFix_Quit(plugin.hwndParent);
		g_started.unitag = 0;
	}

	/***************************************************************************
     * UNICODE STREAM MODULE
     * МОДУЛЬ UNICODE ПОТОКОВ
     * 
     * Fixes Unicode encoding in SHOUTcast stream metadata.
     * Исправляет кодировку Unicode в метаданных SHOUTcast потоков.
     ***************************************************************************/
	if (!g_started.unistr && g_cfg.unistr) {
		MP3_StreamFix_Init();
		g_started.unistr = 1;
	}
	if ( g_started.unistr && !g_cfg.unistr) {
		MP3_StreamFix_Quit();
		g_started.unistr = 0;
	}

	/***************************************************************************
     * MEDIA LIBRARY FONT MODULE
     * МОДУЛЬ ШРИФТОВ БИБЛИОТЕКИ
     * 
     * Enables ClearType/font smoothing in Media Library windows.
     * Включает ClearType/сглаживание шрифтов в окнах библиотеки.
     ***************************************************************************/
	if (!g_started.mlfont && g_cfg.mlfont) {
		ML_SmoothFonts_Init();
		g_started.mlfont = 1;
	}
	if ( g_started.mlfont && !g_cfg.mlfont) {
		ML_SmoothFonts_Quit();
		g_started.mlfont = 0;
	}

	/***************************************************************************
     * MEDIA LIBRARY ICON TINT MODULE
     * МОДУЛЬ ТОНИРОВАНИЯ ИКОНОК БИБЛИОТЕКИ
     * 
     * Tints Media Library icons to match skin colors.
     * Тонирует иконки библиотеки для соответствия цветам скина.
     ***************************************************************************/
	if (!g_started.mlico && g_cfg.mlico) {
		ML_IconsTint_Start(GetModuleHandleA(NULL));
		g_started.mlico = 1;
	}
	if ( g_started.mlico && !g_cfg.mlico) {
		ML_IconsTint_Stop(); 
		g_started.mlico = 0;
	}

	/***************************************************************************
     * MEDIA LIBRARY CYRILLIC SEARCH MODULE
     * МОДУЛЬ ПОИСКА КИРИЛЛИЦЫ В БИБЛИОТЕКЕ
     * 
     * Fixes Cyrillic character search in Media Library.
     * Исправляет поиск кириллических символов в библиотеке.
     ***************************************************************************/
	if (!g_started.mlsearch && g_cfg.mlsearch) {
		ML_CyrSearchFix_Init();
		g_started.mlsearch = 1;
	}
	if ( g_started.mlsearch && !g_cfg.mlsearch) {
		ML_CyrSearchFix_Quit();
		g_started.mlsearch = 0;
	}

	/***************************************************************************
     * URL MUSEUM PATCH MODULE
     * МОДУЛЬ ПАТЧА URL-МУЗЕЯ
     * 
     * Patches URLs to use archive/museum versions.
     * Патчит URL для использования архивных/музейных версий.
     ***************************************************************************/
	if (!g_started.patch_url && g_cfg.patch_url) {
		patch_url_init();
		g_started.patch_url = 1;
	}
	if ( g_started.patch_url && !g_cfg.patch_url) {
		patch_url_quit();
		g_started.patch_url = 0;
	}

	/***************************************************************************
     * MINIBROWSER SKIN PATCH MODULE
     * МОДУЛЬ ПАТЧА СКИНОВ МИНИ-БРАУЗЕРА
     * 
     * Patches minibrowser skin handling.
     * Патчит обработку скинов мини-браузера.
     ***************************************************************************/
	if (!g_started.patch_mb_skin && g_cfg.patch_mb_skin) {
		patch_mb_skin_init();
		g_started.patch_mb_skin = 1;
	}
	if ( g_started.patch_mb_skin && !g_cfg.patch_mb_skin) {
		patch_mb_skin_quit();
		g_started.patch_mb_skin = 0;
	}

	/***************************************************************************
     * MINIBROWSER REMOVAL PATCH MODULE
     * МОДУЛЬ ПАТЧА УДАЛЕНИЯ МИНИ-БРАУЗЕРА
     * 
     * Removes/disables minibrowser functionality.
     * Удаляет/отключает функциональность мини-браузера.
     ***************************************************************************/
	if (!g_started.patch_mb && g_cfg.patch_mb) {
		patch_mb_init();
		g_started.patch_mb = 1;
	}
	if ( g_started.patch_mb && !g_cfg.patch_mb) {
		patch_mb_quit();
		g_started.patch_mb = 0;
	}

	/***************************************************************************
     * PLAYLIST CYRILLIC SEARCH MODULE
     * МОДУЛЬ ПОИСКА КИРИЛЛИЦЫ В ПЛЕЙЛИСТЕ
     * 
     * Fixes Cyrillic character search in playlist editor.
     * Исправляет поиск кириллических символов в редакторе плейлистов.
     ***************************************************************************/
	if (!g_started.plsearch && g_cfg.plsearch) {
		patch_pl_init();
		g_started.plsearch = 1;
	}
	if ( g_started.plsearch && !g_cfg.plsearch) {
		patch_pl_quit();
		g_started.plsearch = 0;
	}

	/***************************************************************************
     * ID3V2 SAVE FIX MODULE
     * МОДУЛЬ ИСПРАВЛЕНИЯ СОХРАНЕНИЯ ID3V2
     * 
     * Fixes issues with saving ID3v2 tags.
     * Исправляет проблемы с сохранением ID3v2 тегов.
     ***************************************************************************/
	if (!g_started.id3 && g_cfg.id3) {
		MP3_SaveFix_Init();
		g_started.id3 = 1;
	}
	if ( g_started.id3 && !g_cfg.id3) {
		MP3_SaveFix_Quit();
		g_started.id3 = 0;
	}

	/***************************************************************************
     * CD RIPPING REMOVAL PATCH MODULE
     * МОДУЛЬ ПАТЧА УДАЛЕНИЯ РИППИНГА CD
     * 
     * Removes CD ripping functionality from Media Library.
     * Удаляет функциональность риппинга CD из библиотеки.
     ***************************************************************************/
	if (!g_started.mlcd && g_cfg.mlcd) {
		patch_cd_init();
		g_started.mlcd = 1;
	}
	if ( g_started.mlcd && !g_cfg.mlcd) {
		patch_cd_quit();
		g_started.mlcd = 0;
	}
}

/*******************************************************************************
 * UpdateFlagFromClick
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Updates configuration and applies changes when user clicks a checkbox in
 * preferences dialog. Provides immediate feedback by applying changes without
 * requiring Winamp restart.
 * 
 * Обновляет конфигурацию и применяет изменения, когда пользователь кликает
 * чекбокс в диалоге настроек. Обеспечивает немедленную обратную связь, применяя
 * изменения без необходимости перезапуска Winamp.
 * 
 * PARAMETERS / ПАРАМЕТРЫ:
 * hDlg   - Dialog window handle / Дескриптор окна диалога
 * ctrlID - Control ID of clicked checkbox / ID контрола нажатого чекбокса
 * 
 * ALGORITHM / АЛГОРИТМ:
 * 1. Save old configuration (for potential future use)
 * 2. Read checkbox state (checked/unchecked)
 * 3. Map control ID to corresponding config flag
 * 4. Update config flag based on checkbox state
 * 5. Save configuration to plugin.ini
 * 6. Apply runtime changes (start/stop affected modules)
 * 
 * 1. Сохранить старую конфигурацию (для потенциального будущего использования)
 * 2. Прочитать состояние чекбокса (отмечен/не отмечен)
 * 3. Сопоставить ID контрола с соответствующим флагом конфигурации
 * 4. Обновить флаг конфигурации на основе состояния чекбокса
 * 5. Сохранить конфигурацию в plugin.ini
 * 6. Применить изменения во время выполнения (запустить/остановить затронутые модули)
 * 
 * NOTES / ПРИМЕЧАНИЯ:
 * Changes take effect immediately - no restart required.
 * Old config saved but currently unused - available for rollback features.
 * 
 * Изменения вступают в силу немедленно - перезапуск не требуется.
 * Старая конфигурация сохраняется но не используется - доступна для функций отката.
 ******************************************************************************/
static void UpdateFlagFromClick(HWND hDlg, int ctrlID)
{

    BOOL checked = (IsDlgButtonChecked(hDlg, ctrlID) == BST_CHECKED);

    // Map control ID to config flag / Сопоставить ID контрола с флагом конфигурации	
    if	    (ctrlID == IDC_CHK_STARTUP)			 g_cfg.startup_foreground = checked;
	else if (ctrlID == IDC_CHK_ICONFIX)			 g_cfg.iconfix		      = checked;
    else if (ctrlID == IDC_CHK_RESTART)			 g_cfg.restart_menu       = checked;
    else if (ctrlID == IDC_CHK_MUTE)			 g_cfg.mute_hotkey        = checked;
    else if (ctrlID == IDC_CHK_SKINDEL)			 g_cfg.skin_delete        = checked;
    else if (ctrlID == IDC_CHK_PEZONES)			 g_cfg.pe_zones           = checked;
	else if (ctrlID == IDC_CHK_VIDEOFONTFIX)	 g_cfg.video_fontfix      = checked;
	else if (ctrlID == IDC_CHK_SKININSTALL)		 g_cfg.skin_install       = checked;
	else if (ctrlID == IDC_CHK_M3U8)			 g_cfg.m3u8			      = checked;
	else if (ctrlID == IDC_CHK_UNITAG)			 g_cfg.unitag		      = checked;
	else if (ctrlID == IDC_CHK_UNISTR)			 g_cfg.unistr		      = checked;
	else if (ctrlID == IDC_CHK_MLFONT)			 g_cfg.mlfont		      = checked;
	else if (ctrlID == IDC_CHK_MLICO)			 g_cfg.mlico		      = checked;
	else if (ctrlID == IDC_CHK_MLSEARCH)	     g_cfg.mlsearch		      = checked;
    else if (ctrlID == IDC_CHK_PATCHURL)		 g_cfg.patch_url	      = checked;
	else if (ctrlID == IDC_CHK_PATCHMBSKIN)		 g_cfg.patch_mb_skin      = checked;
	else if (ctrlID == IDC_CHK_PATCHMB)			 g_cfg.patch_mb		      = checked;
	else if (ctrlID == IDC_CHK_PLSEARCH)		 g_cfg.plsearch		      = checked;
	else if (ctrlID == IDC_CHK_ID3)				 g_cfg.id3			      = checked;
	else if (ctrlID == IDC_CHK_MLCD)			 g_cfg.mlcd			      = checked;

    SaveCfg();          // Persist changes to plugin.ini / Сохранить изменения в plugin.ini
    ApplyRuntime();     // Apply changes immediately / Применить изменения немедленно
}

/*******************************************************************************
 * PrefsDlgProc
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Main dialog procedure for plugin preferences page in Winamp settings.
 * Handles tab control creation and management for multi-page preferences.
 * 
 * Главная процедура диалога для страницы настроек плагина в настройках Winamp.
 * Обрабатывает создание и управление tab control для многостраничных настроек.
 * 
 * MESSAGES / СООБЩЕНИЯ:
 * WM_INITDIALOG - Create tab control and child page dialogs
 *                 Создать tab control и дочерние диалоги страниц
 * WM_NOTIFY     - Handle tab selection changes
 *                 Обработать изменения выбора вкладки
 * WM_DESTROY    - Cleanup child dialogs
 *                 Очистить дочерние диалоги
 * 
 * PARAMETERS / ПАРАМЕТРЫ:
 * hDlg   - Dialog window handle / Дескриптор окна диалога
 * uMsg   - Message ID / Идентификатор сообщения
 * wParam - Message parameter 1 / Параметр сообщения 1
 * lParam - Message parameter 2 / Параметр сообщения 2
 * 
 * RETURNS / ВОЗВРАЩАЕТ:
 * TRUE if message processed, FALSE otherwise
 * TRUE если сообщение обработано, FALSE иначе
 * 
 * TAB STRUCTURE / СТРУКТУРА ВКЛАДОК:
 * Tab 0: General settings (main features)
 *        Общие настройки (основные функции)
 * Tab 1: About/Advanced settings
 *        О программе/Дополнительные настройки
 ******************************************************************************/
static INT_PTR CALLBACK PrefsDlgProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_INITDIALOG:

        // Get tab control handle / Получить дескриптор tab control
        g_hTab = GetDlgItem(hDlg, IDC_TAB_MAIN);
        if (g_hTab)
        {
            // Initialize tab item structure / Инициализировать структуру элемента вкладки
            TCITEMA ti;
            ZeroMemory(&ti, sizeof(ti));
            ti.mask = TCIF_TEXT;

            // Insert "General" tab / Вставить вкладку "Общие"
            ti.pszText = TAB_OPT1;
            TabCtrl_InsertItem(g_hTab, 0, &ti);

            // Insert "About" tab / Вставить вкладку "О программе"
            ti.pszText = TAB_OPT2;
            TabCtrl_InsertItem(g_hTab, 1, &ti);

            // Create page dialogs as children of main dialog
            // Создать диалоги страниц как дочерние элементы главного диалога
            g_hPageGeneral = CreateDialogParamA(g_hDll, MAKEINTRESOURCEA(IDD_FIXER_PAGE_GENERAL),
                                                hDlg, TabGeneralDlgProc, 0);
            g_hPageAbout   = CreateDialogParamA(g_hDll, MAKEINTRESOURCEA(IDD_FIXER_PAGE_ABOUT),
                                                hDlg, TabAboutDlgProc, 0);

            // Position pages to fit inside tab display area
            // Позиционировать страницы для соответствия области отображения вкладки
            if (g_hPageGeneral) PositionPageToTab(hDlg, g_hTab, g_hPageGeneral);
            if (g_hPageAbout)   PositionPageToTab(hDlg, g_hTab, g_hPageAbout);

            // Show initial tab (General) / Показать начальную вкладку (Общие)
            TabCtrl_SetCurSel(g_hTab, 0);
            ShowTabPage(0);
        }

        return TRUE;

    case WM_NOTIFY:
        if (lParam)
        {
            NMHDR* nh = (NMHDR*)lParam;
            // Handle tab selection change / Обработать изменение выбора вкладки
            if (nh->idFrom == IDC_TAB_MAIN && nh->code == TCN_SELCHANGE)
            {
                int idx = TabCtrl_GetCurSel(g_hTab);
                ShowTabPage(idx);  // Show selected page, hide others / Показать выбранную страницу, скрыть остальные
                return TRUE;
            }
        }
        break;

    case WM_DESTROY:
        // Cleanup child page dialogs / Очистить дочерние диалоги страниц
        if (g_hPageGeneral) { DestroyWindow(g_hPageGeneral); g_hPageGeneral = NULL; }
        if (g_hPageAbout)   { DestroyWindow(g_hPageAbout);   g_hPageAbout = NULL; }
        g_hTab = NULL;
        break;
    }
    return FALSE;
}



/*******************************************************************************
 * TOOLTIP HELPER FUNCTIONS
 * ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ ДЛЯ ВСПЛЫВАЮЩИХ ПОДСКАЗОК
 * 
 * Compatible with Windows 98 through Windows 11
 * Совместимо с Windows 98 до Windows 11
 ******************************************************************************/

/**
 * @brief Creates tooltip control and adds tooltips to all checkboxes
 * @param hDlg Parent dialog handle
 * @return Tooltip control handle or NULL on failure
 */
static HWND CreateCheckboxTooltips(HWND hDlg)
{
    // Create tooltip control / Создать контрол всплывающих подсказок
    HWND hwndTT = CreateWindowExA(
        0,
        TOOLTIPS_CLASSA,      // Use ANSI for Win98 compatibility
        NULL,
        WS_POPUP | TTS_ALWAYSTIP | TTS_NOPREFIX,
        CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
        hDlg,
        NULL,
        GetModuleHandleA(NULL),
        NULL
    );
    
    if (!hwndTT)
        return NULL;
    
    // Set max width for multiline tooltips / Установить макс. ширину для многострочных подсказок
    SendMessageA(hwndTT, TTM_SETMAXTIPWIDTH, 0, 250);
    
    // Tooltip structure / Структура подсказки
    TOOLINFOA ti;
    ZeroMemory(&ti, sizeof(ti));
    ti.cbSize   = sizeof(TOOLINFOA);
    ti.uFlags   = TTF_IDISHWND | TTF_SUBCLASS;  // TTF_SUBCLASS for auto handling
    ti.hwnd     = hDlg;
    ti.hinst    = GetModuleHandleA(NULL);
    
    // Tooltip texts (multiline with \r\n) / Тексты подсказок (многострочные с \r\n)
    // Using numbered format as requested / Используем нумерованный формат как запрошено
    
    // 1. M3U8 Support
    ti.uId = (UINT_PTR)GetDlgItem(hDlg, IDC_CHK_M3U8);
    ti.lpszText = TIP_M3U8;
    SendMessageA(hwndTT, TTM_ADDTOOLA, 0, (LPARAM)&ti);
    
    // 2. Skin Delete/Rename
    ti.uId = (UINT_PTR)GetDlgItem(hDlg, IDC_CHK_SKINDEL);
    ti.lpszText = TIP_SKINDEL;
    SendMessageA(hwndTT, TTM_ADDTOOLA, 0, (LPARAM)&ti);
    
    // 3. Playlist Editor Zones
    ti.uId = (UINT_PTR)GetDlgItem(hDlg, IDC_CHK_PEZONES);
    ti.lpszText = TIP_PEZONES;
    SendMessageA(hwndTT, TTM_ADDTOOLA, 0, (LPARAM)&ti);
    
    // 4. Restart Menu Item
    ti.uId = (UINT_PTR)GetDlgItem(hDlg, IDC_CHK_RESTART);
    ti.lpszText = TIP_RESTART;
    SendMessageA(hwndTT, TTM_ADDTOOLA, 0, (LPARAM)&ti);
    
    // 5. Mute Hotkey
    ti.uId = (UINT_PTR)GetDlgItem(hDlg, IDC_CHK_MUTE);
    ti.lpszText = TIP_MUTE;
    SendMessageA(hwndTT, TTM_ADDTOOLA, 0, (LPARAM)&ti);
    
    // 6. Media Library Font Smoothing
    ti.uId = (UINT_PTR)GetDlgItem(hDlg, IDC_CHK_MLFONT);
    ti.lpszText = TIP_MLFONT;
    SendMessageA(hwndTT, TTM_ADDTOOLA, 0, (LPARAM)&ti);
    
    // 7. Media Library Icon Tinting
    ti.uId = (UINT_PTR)GetDlgItem(hDlg, IDC_CHK_MLICO);
    ti.lpszText = TIP_MLICO;
    SendMessageA(hwndTT, TTM_ADDTOOLA, 0, (LPARAM)&ti);
    
    // 8. Media Library remove CD Ropping
    ti.uId = (UINT_PTR)GetDlgItem(hDlg, IDC_CHK_MLCD);
    ti.lpszText = TIP_MLCD;
    SendMessageA(hwndTT, TTM_ADDTOOLA, 0, (LPARAM)&ti);
    
    // 9. Modern Skin MiniBrowser Patch
    ti.uId = (UINT_PTR)GetDlgItem(hDlg, IDC_CHK_PATCHMB);
    ti.lpszText = TIP_PATCHMB;
    SendMessageA(hwndTT, TTM_ADDTOOLA, 0, (LPARAM)&ti);
    
    // 10. Modern Skin Customization
    ti.uId = (UINT_PTR)GetDlgItem(hDlg, IDC_CHK_PATCHMBSKIN);
    ti.lpszText = TIP_PATCHMBSKIN;
    SendMessageA(hwndTT, TTM_ADDTOOLA, 0, (LPARAM)&ti);
    
    return hwndTT;
}




/*******************************************************************************
 * TabGeneralDlgProc
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Dialog procedure for the "General" settings tab page.
 * Handles initialization and checkbox clicks for main plugin features.
 * 
 * Процедура диалога для страницы вкладки "Общие" настройки.
 * Обрабатывает инициализацию и клики чекбоксов для основных функций плагина.
 * 
 * MESSAGES / СООБЩЕНИЯ:
 * WM_INITDIALOG - Set checkbox labels and states from current config
 *                 Установить метки чекбоксов и состояния из текущей конфигурации
 * WM_COMMAND    - Handle checkbox click events
 *                 Обработать события клика чекбоксов
 * 
 * FEATURES ON THIS PAGE / ФУНКЦИИ НА ЭТОЙ СТРАНИЦЕ:
 * - M3U8 playlist support / Поддержка плейлистов M3U8
 * - Skin delete/rename / Удаление/переименование скинов
 * - Playlist editor zones / Зоны редактора плейлистов
 * - Restart menu item / Пункт меню перезапуска
 * - Mute hotkey / Горячая клавиша выключения звука
 * - Media Library fonts / Шрифты библиотеки
 * - Media Library icons / Иконки библиотеки
 * - CD ripping removal / Удаление риппинга CD
 * - Minibrowser patches / Патчи мини-браузера
 ******************************************************************************/
static INT_PTR CALLBACK TabGeneralDlgProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM)
{
    static HWND s_hwndTooltip = NULL;  // Store tooltip handle / Хранить хэндл подсказок

    switch (uMsg)
    {
    case WM_INITDIALOG:

		// Set checkbox labels from language strings / Установить метки чекбоксов из языковых строк
		SetDlgItemTextA(hDlg, IDC_CHK_M3U8, M3U8);
		SetDlgItemTextA(hDlg, IDC_CHK_SKINDEL, DELSKIN);
		SetDlgItemTextA(hDlg, IDC_CHK_PEZONES, PL_MENUS);
		SetDlgItemTextA(hDlg, IDC_CHK_RESTART, REST_ITEM);
		SetDlgItemTextA(hDlg, IDC_CHK_MUTE, MUTE);
		SetDlgItemTextA(hDlg, IDC_CHK_MLFONT, MLFONT);
		SetDlgItemTextA(hDlg, IDC_CHK_MLICO, MLICO);
		SetDlgItemTextA(hDlg, IDC_CHK_MLCD, MLCD);
		SetDlgItemTextA(hDlg, IDC_CHK_PATCHMBSKIN, PATCHMBSKIN);
		SetDlgItemTextA(hDlg, IDC_CHK_PATCHMB, PATCHMB);


        // Initialize checkboxes from current config / Инициализировать чекбоксы из текущей конфигурации
        CheckDlgButton(hDlg, IDC_CHK_M3U8, g_cfg.m3u8					    ? BST_CHECKED : BST_UNCHECKED);
		CheckDlgButton(hDlg, IDC_CHK_SKINDEL, g_cfg.skin_delete			    ? BST_CHECKED : BST_UNCHECKED);
		CheckDlgButton(hDlg, IDC_CHK_PEZONES, g_cfg.pe_zones				? BST_CHECKED : BST_UNCHECKED);
		CheckDlgButton(hDlg, IDC_CHK_RESTART, g_cfg.restart_menu		    ? BST_CHECKED : BST_UNCHECKED);
		CheckDlgButton(hDlg, IDC_CHK_MUTE,    g_cfg.mute_hotkey			    ? BST_CHECKED : BST_UNCHECKED);
		CheckDlgButton(hDlg, IDC_CHK_MLFONT, g_cfg.mlfont				    ? BST_CHECKED : BST_UNCHECKED);
		CheckDlgButton(hDlg, IDC_CHK_MLICO, g_cfg.mlico					    ? BST_CHECKED : BST_UNCHECKED);
		CheckDlgButton(hDlg, IDC_CHK_MLCD, g_cfg.mlcd					    ? BST_CHECKED : BST_UNCHECKED);
		CheckDlgButton(hDlg, IDC_CHK_PATCHMB, g_cfg.patch_mb				? BST_CHECKED : BST_UNCHECKED);
		CheckDlgButton(hDlg, IDC_CHK_PATCHMBSKIN, g_cfg.patch_mb_skin		? BST_CHECKED : BST_UNCHECKED);
		
        if (g_cfg.patch_mb)
        {
            CheckDlgButton(hDlg, IDC_CHK_PATCHMBSKIN, BST_CHECKED);
            EnableWindow(GetDlgItem(hDlg, IDC_CHK_PATCHMBSKIN), FALSE);
        }
        
        // Create tooltips for all checkboxes / Создать подсказки для всех чекбоксов
        s_hwndTooltip = CreateCheckboxTooltips(hDlg);
        
        return TRUE;
    
    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case IDC_CHK_M3U8:
        case IDC_CHK_SKINDEL:
        case IDC_CHK_PEZONES:
        case IDC_CHK_RESTART:
        case IDC_CHK_MUTE:
        case IDC_CHK_MLFONT:
        case IDC_CHK_MLICO:
        case IDC_CHK_MLCD:
        case IDC_CHK_PATCHMBSKIN:
            if (HIWORD(wParam) == BN_CLICKED)  // Only handle click events / Обрабатывать только события клика
                UpdateFlagFromClick(hDlg, LOWORD(wParam));
            return TRUE;
            
        case IDC_CHK_PATCHMB:
            if (HIWORD(wParam) == BN_CLICKED)
            {
                // Update PATCHMB flag / Обновить флаг PATCHMB
                UpdateFlagFromClick(hDlg, LOWORD(wParam));
                
                // Check if PATCHMB is now checked / Проверить включен ли теперь PATCHMB
                BOOL isPatchMbChecked = (IsDlgButtonChecked(hDlg, IDC_CHK_PATCHMB) == BST_CHECKED);
                
                if (isPatchMbChecked)
                {
                    // PATCHMB enabled: check and disable PATCHMBSKIN
                    // PATCHMB включен: включить и сделать недоступным PATCHMBSKIN
                    CheckDlgButton(hDlg, IDC_CHK_PATCHMBSKIN, BST_CHECKED);
                    EnableWindow(GetDlgItem(hDlg, IDC_CHK_PATCHMBSKIN), FALSE);
                    UpdateFlagFromClick(hDlg, IDC_CHK_PATCHMBSKIN);
                }
                else
                {
                    // PATCHMB disabled: re-enable PATCHMBSKIN
                    // PATCHMB выключен: сделать PATCHMBSKIN доступным
                    EnableWindow(GetDlgItem(hDlg, IDC_CHK_PATCHMBSKIN), TRUE);
                }
            }
            return TRUE;
        }
        break;
        
    case WM_DESTROY:
        // Clean up tooltip control / Очистить контрол подсказок
        if (s_hwndTooltip)
        {
            DestroyWindow(s_hwndTooltip);
            s_hwndTooltip = NULL;
        }
        break;
    }
    return FALSE;
}

/*******************************************************************************
 * CreateAdvancedCheckboxTooltips
 * 
 * PURPOSE: Creates tooltip control for Advanced/About tab checkboxes
 * НАЗНАЧЕНИЕ: Создаёт tooltips для вкладки Advanced/About
 ******************************************************************************/
static HWND CreateAdvancedCheckboxTooltips(HWND hDlg)
{
    HWND hwndTT = CreateWindowExA(
        0,
        TOOLTIPS_CLASSA,
        NULL,
        WS_POPUP | TTS_ALWAYSTIP | TTS_NOPREFIX,
        CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
        hDlg,
        NULL,
        GetModuleHandleA(NULL),
        NULL
    );
    
    if (!hwndTT)
        return NULL;
    
    SendMessageA(hwndTT, TTM_SETMAXTIPWIDTH, 0, 250);
    
    TOOLINFOA ti;
    ZeroMemory(&ti, sizeof(ti));
    ti.cbSize   = sizeof(TOOLINFOA);
    ti.uFlags   = TTF_IDISHWND | TTF_SUBCLASS;
    ti.hwnd     = hDlg;
    ti.hinst    = GetModuleHandleA(NULL);
    
    // 11. Unicode Tag Fixes
    ti.uId = (UINT_PTR)GetDlgItem(hDlg, IDC_CHK_UNITAG);
    ti.lpszText = TIP_UNITAG;
    SendMessageA(hwndTT, TTM_ADDTOOLA, 0, (LPARAM)&ti);
    
    // 12. Unicode Stream Fixes
    ti.uId = (UINT_PTR)GetDlgItem(hDlg, IDC_CHK_UNISTR);
    ti.lpszText = TIP_UNISTR;
    SendMessageA(hwndTT, TTM_ADDTOOLA, 0, (LPARAM)&ti);
    
    // 13. Playlist Cyrillic Search
    ti.uId = (UINT_PTR)GetDlgItem(hDlg, IDC_CHK_PLSEARCH);
    ti.lpszText = TIP_PLSEARCH;
    SendMessageA(hwndTT, TTM_ADDTOOLA, 0, (LPARAM)&ti);
    
    // 14. Media Library Cyrillic Search
    ti.uId = (UINT_PTR)GetDlgItem(hDlg, IDC_CHK_MLSEARCH);
    ti.lpszText = TIP_MLSEARCH;
    SendMessageA(hwndTT, TTM_ADDTOOLA, 0, (LPARAM)&ti);
    
    // 15. Video Font Fix
    ti.uId = (UINT_PTR)GetDlgItem(hDlg, IDC_CHK_VIDEOFONTFIX);
    ti.lpszText = TIP_VIDEOFONTFIX;
    SendMessageA(hwndTT, TTM_ADDTOOLA, 0, (LPARAM)&ti);
    
    // 16. Startup Foreground
    ti.uId = (UINT_PTR)GetDlgItem(hDlg, IDC_CHK_STARTUP);
    ti.lpszText = TIP_STARTUP;
    SendMessageA(hwndTT, TTM_ADDTOOLA, 0, (LPARAM)&ti);
    
    // 17. Icon Fix
    ti.uId = (UINT_PTR)GetDlgItem(hDlg, IDC_CHK_ICONFIX);
    ti.lpszText = TIP_ICONFIX;
    SendMessageA(hwndTT, TTM_ADDTOOLA, 0, (LPARAM)&ti);
    
    // 18. ID3v2 Save Fix
    ti.uId = (UINT_PTR)GetDlgItem(hDlg, IDC_CHK_ID3);
    ti.lpszText = TIP_ID3;
    SendMessageA(hwndTT, TTM_ADDTOOLA, 0, (LPARAM)&ti);
    
    // 19. URL Museum Patch
    ti.uId = (UINT_PTR)GetDlgItem(hDlg, IDC_CHK_PATCHURL);
    ti.lpszText = TIP_PATCHURL;
    SendMessageA(hwndTT, TTM_ADDTOOLA, 0, (LPARAM)&ti);
    
    // 20. Skin Install Menu
    ti.uId = (UINT_PTR)GetDlgItem(hDlg, IDC_CHK_SKININSTALL);
    ti.lpszText = TIP_SKININSTALL;
    SendMessageA(hwndTT, TTM_ADDTOOLA, 0, (LPARAM)&ti);
    
    return hwndTT;
}


/*******************************************************************************
 * TabAboutDlgProc
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Dialog procedure for the "About/Advanced" settings tab page.
 * Handles initialization and checkbox clicks for advanced/experimental features.
 * 
 * Процедура диалога для страницы вкладки "О программе/Дополнительные" настройки.
 * Обрабатывает инициализацию и клики чекбоксов для дополнительных/экспериментальных функций.
 * 
 * MESSAGES / СООБЩЕНИЯ:
 * WM_INITDIALOG - Set checkbox labels and states from current config
 *                 Установить метки чекбоксов и состояния из текущей конфигурации
 * WM_COMMAND    - Handle checkbox click events
 *                 Обработать события клика чекбоксов
 * 
 * FEATURES ON THIS PAGE / ФУНКЦИИ НА ЭТОЙ СТРАНИЦЕ:
 * - Unicode tag fixes / Исправления Unicode тегов
 * - Unicode stream fixes / Исправления Unicode потоков
 * - Playlist Cyrillic search / Поиск кириллицы в плейлисте
 * - Media Library Cyrillic search / Поиск кириллицы в библиотеке
 * - Video font fix / Исправление шрифтов видео
 * - Startup foreground / Передний план при запуске
 * - Icon fix / Исправление иконок
 * - ID3v2 save fix / Исправление сохранения ID3v2
 * - URL museum patch / Патч URL-музея
 * - Skin install menu / Меню установки скинов
 ******************************************************************************/
static INT_PTR CALLBACK TabAboutDlgProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM)
{
    static HWND s_hwndTooltip = NULL;  // Store tooltip handle / Хранить хэндл подсказок

    switch (uMsg)
    {
    case WM_INITDIALOG:
		
		// Set checkbox labels from language strings / Установить метки чекбоксов из языковых строк
		SetDlgItemTextA(hDlg, IDC_CHK_UNITAG, UNITAG);
		SetDlgItemTextA(hDlg, IDC_CHK_UNISTR, UNISTR);
		SetDlgItemTextA(hDlg, IDC_CHK_PLSEARCH, PLSEARCH);
		SetDlgItemTextA(hDlg, IDC_CHK_MLSEARCH, MLSEARCH);
		SetDlgItemTextA(hDlg, IDC_CHK_VIDEOFONTFIX, VIDEOFONTFIX);
		SetDlgItemTextA(hDlg, IDC_CHK_STARTUP, STARTUP);
		SetDlgItemTextA(hDlg, IDC_CHK_ICONFIX, ICONFIX);
		SetDlgItemTextA(hDlg, IDC_CHK_ID3, ID3);
		SetDlgItemTextA(hDlg, IDC_CHK_PATCHURL, PATCH_URL);
		SetDlgItemTextA(hDlg, IDC_CHK_SKININSTALL, SKININSTALL);
		

		// Initialize checkboxes from current config / Инициализировать чекбоксы из текущей конфигурации
		CheckDlgButton(hDlg, IDC_CHK_UNITAG, g_cfg.unitag				    ? BST_CHECKED : BST_UNCHECKED);
		CheckDlgButton(hDlg, IDC_CHK_UNISTR, g_cfg.unistr				    ? BST_CHECKED : BST_UNCHECKED);
		CheckDlgButton(hDlg, IDC_CHK_PLSEARCH, g_cfg.plsearch				? BST_CHECKED : BST_UNCHECKED);	
		CheckDlgButton(hDlg, IDC_CHK_MLSEARCH, g_cfg.mlsearch				? BST_CHECKED : BST_UNCHECKED);			
		CheckDlgButton(hDlg, IDC_CHK_VIDEOFONTFIX, g_cfg.video_fontfix      ? BST_CHECKED : BST_UNCHECKED);
		CheckDlgButton(hDlg, IDC_CHK_STARTUP, g_cfg.startup_foreground		? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(hDlg, IDC_CHK_ICONFIX, g_cfg.iconfix				    ? BST_CHECKED : BST_UNCHECKED);
		CheckDlgButton(hDlg, IDC_CHK_ID3, g_cfg.id3							? BST_CHECKED : BST_UNCHECKED);
		CheckDlgButton(hDlg, IDC_CHK_PATCHURL, g_cfg.patch_url			    ? BST_CHECKED : BST_UNCHECKED);
		CheckDlgButton(hDlg, IDC_CHK_SKININSTALL, g_cfg.skin_install        ? BST_CHECKED : BST_UNCHECKED);
		



        
        // Create tooltips for all advanced checkboxes / Создать подсказки для дополнительных чекбоксов
        s_hwndTooltip = CreateAdvancedCheckboxTooltips(hDlg);
        
        return TRUE;

case WM_COMMAND:

    if (HIWORD(wParam) == BN_CLICKED)  // Only handle click events / Обрабатывать только события клика
    {
        switch (LOWORD(wParam))
        {
		case IDC_CHK_UNITAG:
		case IDC_CHK_UNISTR:
		case IDC_CHK_PLSEARCH:
		case IDC_CHK_MLSEARCH:
		case IDC_CHK_VIDEOFONTFIX:
		case IDC_CHK_STARTUP:
		case IDC_CHK_ICONFIX:
		case IDC_CHK_ID3:
		case IDC_CHK_PATCHURL:
		case IDC_CHK_SKININSTALL:

            UpdateFlagFromClick(hDlg, LOWORD(wParam));
            return TRUE;
        }
    }
    break;

case WM_DESTROY:
    // Clean up tooltip control / Очистить контрол подсказок
    if (s_hwndTooltip)
    {
        DestroyWindow(s_hwndTooltip);
        s_hwndTooltip = NULL;
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
 * Loads configuration and initializes all enabled modules.
 * 
 * Функция инициализации плагина. Вызывается Winamp при загрузке плагина.
 * Загружает конфигурацию и инициализирует все включённые модули.
 * 
 * INITIALIZATION ORDER / ПОРЯДОК ИНИЦИАЛИЗАЦИИ:
 * 1. Load configuration from plugin.ini via LoadCfg()
 * 2. Apply configuration by starting enabled modules via ApplyRuntime()
 * 3. Register preferences dialog with Winamp
 * 
 * 1. Загрузить конфигурацию из plugin.ini через LoadCfg()
 * 2. Применить конфигурацию, запустив включённые модули через ApplyRuntime()
 * 3. Зарегистрировать диалог настроек в Winamp
 * 
 * RETURNS / ВОЗВРАЩАЕТ:
 * 0 on success / 0 при успехе
 * 
 * NOTES / ПРИМЕЧАНИЯ:
 * This is the main entry point for plugin functionality.
 * All module initialization happens through ApplyRuntime().
 * Preferences dialog allows user to enable/disable modules.
 * 
 * Это главная точка входа для функциональности плагина.
 * Вся инициализация модулей происходит через ApplyRuntime().
 * Диалог настроек позволяет пользователю включать/отключать модули.
 ******************************************************************************/
int init(void)

{
    // Load configuration from plugin.ini / Загрузить конфигурацию из plugin.ini
    LoadCfg();
	DpiOverride_EnsureSystemEnhanced(plugin.hwndParent);

	// Initialize all enabled modules / Инициализировать все включённые модули
	ApplyRuntime();


    // Register preferences dialog with Winamp / Зарегистрировать диалог настроек в Winamp
    g_prefsPage.hInst = g_hDll;
    g_prefsPage.dlgID = IDD_FIXER_CFG;
    g_prefsPage.name  = (char*)PREFFS_NAME;          // Dialog title from SwitchLangUI.h / Заголовок диалога из SwitchLangUI.h
    g_prefsPage.where = (intptr_t)&g_prefsPage;      // Unique identifier / Уникальный идентификатор
    g_prefsPage.proc  = PrefsDlgProc;
    SendMessage(plugin.hwndParent, WM_WA_IPC, (WPARAM)&g_prefsPage, IPC_ADD_PREFS_DLG);



    
    return 0;  // Success / Успех
}

/*******************************************************************************
 * config
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Called when user clicks plugin's "Configure" button in Winamp preferences.
 * Shows informational message directing user to the preferences page.
 * 
 * Вызывается, когда пользователь кликает кнопку "Настроить" плагина в настройках Winamp.
 * Показывает информационное сообщение, направляющее пользователя на страницу настроек.
 * 
 * NOTES / ПРИМЕЧАНИЯ:
 * Actual configuration UI is in preferences dialog (IDD_FIXER_CFG), not here.
 * This is just a helper message to guide users.
 * The real configuration happens through the registered preferences page.
 * 
 * Фактический UI конфигурации находится в диалоге настроек (IDD_FIXER_CFG), не здесь.
 * Это просто вспомогательное сообщение для направления пользователей.
 * Реальная конфигурация происходит через зарегистрированную страницу настроек.
 ******************************************************************************/
void config(void)
{
    // Show message directing to preferences page / Показать сообщение, направляющее на страницу настроек
    MessageBoxA(0, APPCONFIG, APPCONFIG_TITLE, MB_OK | MB_ICONINFORMATION | MB_SETFOREGROUND);
}


/*******************************************************************************
 * DllMain
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * DLL entry point. Called by Windows when DLL is loaded/unloaded.
 * Performs minimal initialization - just stores DLL instance handle.
 * 
 * Точка входа DLL. Вызывается Windows при загрузке/выгрузке DLL.
 * Выполняет минимальную инициализацию - просто сохраняет дескриптор экземпляра DLL.
 * 
 * PARAMETERS / ПАРАМЕТРЫ:
 * hInst  - DLL instance handle / Дескриптор экземпляра DLL
 * reason - Reason for calling (DLL_PROCESS_ATTACH, etc.) / Причина вызова
 * 
 * RETURNS / ВОЗВРАЩАЕТ:
 * TRUE to allow DLL loading, FALSE to abort
 * TRUE для разрешения загрузки DLL, FALSE для отмены
 * 
 * CRITICAL / КРИТИЧНО:
 * Must call DisableThreadLibraryCalls to avoid per-thread notifications.
 * This improves performance and avoids potential issues with thread creation/destruction.
 * 
 * Должен вызвать DisableThreadLibraryCalls для избежания уведомлений на каждый поток.
 * Это улучшает производительность и избегает потенциальных проблем с созданием/уничтожением потоков.
 * 
 * NOTES / ПРИМЕЧАНИЯ:
 * Actual plugin initialization happens in init(), not here.
 * DllMain is just for DLL-level setup.
 * 
 * Фактическая инициализация плагина происходит в init(), не здесь.
 * DllMain только для настройки уровня DLL.
 ******************************************************************************/
BOOL APIENTRY DllMain(HINSTANCE hInst, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH) {
        g_hDll = hInst;      // Store DLL instance handle / Сохранить дескриптор экземпляра DLL
        DisableThreadLibraryCalls(hInst);  // Optimize: disable per-thread callbacks / Оптимизация: отключить обратные вызовы на поток
    }
    return TRUE;
}
/*******************************************************************************
 * quit
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Plugin cleanup function. Called by Winamp when plugin is unloaded.
 * Must cleanup all modules, free resources, and unregister hooks.
 * 
 * Функция очистки плагина. Вызывается Winamp при выгрузке плагина.
 * Должна очистить все модули, освободить ресурсы и отменить регистрацию хуков.
 * 
 * CRITICAL / КРИТИЧНО:
 * Order matters! Cleanup in reverse order of initialization where dependencies exist.
 * Must not leave any hooks, subclasses, or threads running.
 * All module quit() calls protected by g_started flags to prevent double-cleanup.
 * 
 * Порядок важен! Очистка в обратном порядке инициализации, где существуют зависимости.
 * Не должно остаться никаких хуков, субклассов или запущенных потоков.
 * Все вызовы quit() модулей защищены флагами g_started для предотвращения двойной очистки.
 * 
 * ALGORITHM / АЛГОРИТМ:
 * 1. Cleanup optional UI modules (skin delete, icon fix, etc.)
 * 2. Cleanup menu and startup modules
 * 3. Cleanup core modules (Unicode fixes, Media Library, etc.)
 * 4. Cleanup patch modules
 * 5. Unregister preferences dialog from Winamp
 * 
 * 1. Очистить опциональные UI модули (удаление скинов, исправление иконок и т.д.)
 * 2. Очистить модули меню и запуска
 * 3. Очистить основные модули (исправления Unicode, библиотека и т.д.)
 * 4. Очистить модули патчей
 * 5. Отменить регистрацию диалога настроек в Winamp
 * 
 * OPTIMIZATIONS / ОПТИМИЗАЦИИ:
 * - All quit calls now protected by g_started flags (prevents crashes)
 * - Uses cached plugin.hwndParent instead of FindWinamp() call
 * - Removed duplicate PeZones_Quit() call (was a critical bug!)
 * 
 * - Все вызовы quit теперь защищены флагами g_started (предотвращает краши)
 * - Использует кэшированный plugin.hwndParent вместо вызова FindWinamp()
 * - Удалён дублированный вызов PeZones_Quit() (был критическим багом!)
 ******************************************************************************/
void quit(void)
{
    /***************************************************************************
     * CLEANUP OPTIONAL UI MODULES
     * ОЧИСТКА ОПЦИОНАЛЬНЫХ UI МОДУЛЕЙ
     ***************************************************************************/
    
    // Cleanup skin delete module (must unhook window procedure)
    // Очистить модуль удаления скинов (нужно отцепить оконную процедуру)
    if (g_started.skindel) SkinDel_Quit();
    
    // Cleanup icon fix module / Очистить модуль исправления иконок
    if (g_started.iconfix) IconFix_Quit();
    
    // OPTIMIZED: Use cached plugin.hwndParent instead of FindWinamp()
    // ОПТИМИЗИРОВАНО: Используем кэшированный plugin.hwndParent вместо FindWinamp()
    // Cleanup restart menu module (must remove menu items)
    // Очистить модуль меню перезапуска (нужно удалить пункты меню)
    if (g_started.restart && plugin.hwndParent) {
        Menu_Quit(plugin.hwndParent);
    }
    
    // Cleanup startup foreground module / Очистить модуль переднего плана при запуске
    if (g_started.startup) Startup_Quit();
    
    // Cleanup mute hotkey module (must unregister hotkey)
    // Очистить модуль горячей клавиши выключения звука (нужно отменить регистрацию горячей клавиши)
    if (g_started.mute)    Mute_Quit();
    
    // OPTIMIZED: Only one PeZones_Quit() call, protected by flag (was duplicated!)
    // ОПТИМИЗИРОВАНО: Только один вызов PeZones_Quit(), защищён флагом (был дублирован!)
    // Cleanup playlist editor zones module
    // Очистить модуль зон редактора плейлистов
    if (g_started.pezones) PeZones_Quit();
    
    // Cleanup video font fix module / Очистить модуль исправления шрифтов видео
    if (g_started.vidfont) VideoFontFix_Quit();
    
    /***************************************************************************
     * CLEANUP CORE MODULES (protected by flags)
     * ОЧИСТКА ОСНОВНЫХ МОДУЛЕЙ (защищено флагами)
     ***************************************************************************/
    
    // Cleanup Unicode tag and Media Library fixes
    // Очистить исправления Unicode тегов и библиотеки
    if (g_started.unitag) {
        MP3_TagsFix_Quit();
        ML_UnicodeFix_Quit(plugin.hwndParent);
    }
    
    // Cleanup Unicode stream metadata fixes / Очистить исправления метаданных Unicode потоков
    if (g_started.unistr)   MP3_StreamFix_Quit();
    
    // Cleanup Media Library font smoothing / Очистить сглаживание шрифтов библиотеки
    if (g_started.mlfont)   ML_SmoothFonts_Quit();
    
    // Cleanup Media Library icon tinting / Очистить тонирование иконок библиотеки
    if (g_started.mlico)    ML_IconsTint_Stop();
    
    // Cleanup Media Library Cyrillic search / Очистить поиск кириллицы в библиотеке
    if (g_started.mlsearch) ML_CyrSearchFix_Quit();
    
    // Cleanup M3U8 playlist loader / Очистить загрузчик M3U8 плейлистов
    if (g_started.m3u8)     m3u8_Loader_Quit();
    
    /***************************************************************************
     * CLEANUP PATCH MODULES
     * ОЧИСТКА МОДУЛЕЙ ПАТЧЕЙ
     ***************************************************************************/
    
    // Cleanup various patch modules (URL museum, minibrowser, etc.)
    // Очистить различные модули патчей (URL-музей, мини-браузер и т.д.)
    if (g_started.patch_url)     patch_url_quit();
    if (g_started.patch_mb_skin) patch_mb_skin_quit();
    if (g_started.patch_mb)      patch_mb_quit();
    if (g_started.plsearch)      patch_pl_quit();
    if (g_started.id3)           MP3_SaveFix_Quit();
    if (g_started.mlcd)          patch_cd_quit();

    /***************************************************************************
     * CLEANUP WINAMP INTEGRATION
     * ОЧИСТКА ИНТЕГРАЦИИ С WINAMP
     ***************************************************************************/
    
    // Unregister preferences dialog from Winamp (if API available)
    // Отменить регистрацию диалога настроек в Winamp (если API доступен)
#ifdef IPC_REMOVE_PREFS_DLG
    SendMessage(plugin.hwndParent, WM_WA_IPC, (WPARAM)&g_prefsPage, IPC_REMOVE_PREFS_DLG);
#endif
}