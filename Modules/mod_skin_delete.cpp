/*******************************************************************************
 * mod_skin_delete.cpp
 * 
 * WINAMP SKIN DELETE/RENAME MODULE
 * Модуль удаления и переименования скинов Winamp
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * This module extends the Winamp skin selection dialog with additional
 * functionality for managing skins: deleting skins (with Recycle Bin support)
 * and renaming skins. It subclasses the skin list control and adds keyboard
 * shortcuts (Delete key, F2 key) and context menu operations.
 * 
 * Этот модуль расширяет диалог выбора скинов Winamp дополнительной
 * функциональностью для управления скинами: удаление скинов (с поддержкой
 * корзины) и переименование скинов. Он подменяет элемент управления списком
 * скинов и добавляет горячие клавиши (Delete, F2) и операции контекстного меню.
 * 
 * FEATURES / ВОЗМОЖНОСТИ:
 * - Delete skins using Delete key or context menu (moved to Recycle Bin)
 * - Rename skins using F2 key or context menu
 * - Automatic skin reapplication after rename if it was active
 * - Support for both ListBox (classic) and ListView (modern) skin lists
 * - Handles .wsz, .zip files and skin directories
 * - Auto-selection of active skin when dialog opens
 * - Delayed skin application to avoid conflicts
 * 
 * - Удаление скинов клавишей Delete или через контекстное меню (перемещение в корзину)
 * - Переименование скинов клавишей F2 или через контекстное меню
 * - Автоматическое повторное применение скина после переименования, если он был активным
 * - Поддержка как ListBox (классический), так и ListView (современный) списков скинов
 * - Обработка файлов .wsz, .zip и каталогов скинов
 * - Автоматический выбор активного скина при открытии диалога
 * - Отложенное применение скина для избежания конфликтов
 * 
 * HOW IT WORKS / КАК ЭТО РАБОТАЕТ:
 * 1. Monitors for skin selection dialog appearance using a timer
 * 2. When dialog is found, locates the skin list control (ID 1122)
 * 3. Subclasses the list control to intercept keyboard and mouse events
 * 4. Provides custom dialog for rename operation
 * 5. Uses shell operations for safe file deletion (Recycle Bin)
 * 6. Manages skin paths in both Program Files and AppData locations
 * 
 * 1. Отслеживает появление диалога выбора скинов с помощью таймера
 * 2. Когда диалог найден, находит элемент управления списком скинов (ID 1122)
 * 3. Подменяет список для перехвата событий клавиатуры и мыши
 * 4. Предоставляет пользовательский диалог для операции переименования
 * 5. Использует операции оболочки для безопасного удаления файлов (корзина)
 * 6. Управляет путями к скинам как в Program Files, так и в AppData
 * 
 * KEYBOARD SHORTCUTS / ГОРЯЧИЕ КЛАВИШИ:
 * Delete - Delete selected skin / Удалить выбранный скин
 * F2     - Rename selected skin / Переименовать выбранный скин
 * F5     - Refresh skin list (triggered automatically) / Обновить список скинов
 * 
 ******************************************************************************/

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>
#include <shellapi.h>
#include <shlobj.h>
#include "mod_skin_delete.h" 
#include "..\SwitchLangUI.h" 

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "shell32.lib")

// Compatibility macros for older Windows SDK versions
// Макросы совместимости для старых версий Windows SDK
#ifndef GetWindowLongPtrA
# define GetWindowLongPtrA GetWindowLongA
# define SetWindowLongPtrA SetWindowLongA
# define GWLP_WNDPROC      GWL_WNDPROC
# define DWLP_USER         DWL_USER
#endif

/*******************************************************************************
 * CONSTANTS / КОНСТАНТЫ
 ******************************************************************************/

// Control ID of the skin list in Winamp's skin selection dialog
// Идентификатор элемента управления списка скинов в диалоге выбора скинов Winamp
#define SKIN_CTL_ID     1122

// Dialog ID for skin selection dialog (used for identification)
// Идентификатор диалога выбора скинов (используется для идентификации)
#define SKIN_DIALOG_ID  236   

// Context menu command IDs
// Идентификаторы команд контекстного меню
#define IDM_CTX_DELETE  0x9F01  // Delete skin command / Команда удаления скина
#define IDM_CTX_RENAME  0x9F02  // Rename skin command / Команда переименования скина

// Timer IDs for various delayed operations
// Идентификаторы таймеров для различных отложенных операций
#define APPLY_TIMER_ID  0x55A1  // Timer for delayed skin application / Таймер для отложенного применения скина
#define SELECT_TIMER_ID 0x55A2  // Timer for auto-selecting active skin / Таймер для автоматического выбора активного скина
#define RETRY_TIMER_ID  0x66B2  // Timer for retrying skin application after rename / Таймер для повторной попытки применения скина после переименования

/*******************************************************************************
 * GLOBAL STATE VARIABLES
 * ГЛОБАЛЬНЫЕ ПЕРЕМЕННЫЕ СОСТОЯНИЯ
 ******************************************************************************/

// Handle to the skin list control (ListBox or ListView)
// Дескриптор элемента управления списком скинов (ListBox или ListView)
static HWND     g_hList = NULL;

// Original window procedure before subclassing
// Оригинальная оконная процедура до подмены
static WNDPROC  g_oldProc = NULL;

// Path to the Skins directory
// Путь к каталогу Skins
static char     g_SkinsDir[MAX_PATH] = {0};

// Global monitor timer handle (for detecting dialog appearance)
// Глобальный дескриптор таймера мониторинга (для обнаружения появления диалога)
static UINT_PTR g_hTimer = 0;

// TRUE if list is a ListBox (classic UI), FALSE if ListView (modern UI)
// TRUE если список - ListBox (классический интерфейс), FALSE если ListView (современный интерфейс)
static BOOL     g_isLB = FALSE;

// Timer handle for delayed skin application
// Дескриптор таймера для отложенного применения скина
static UINT_PTR g_tmApply = 0;

// Timer handle for active skin selection
// Дескриптор таймера для выбора активного скина
static UINT_PTR g_tmSel   = 0;

// Flag to prevent auto-apply during initial selection
// Флаг для предотвращения автоматического применения во время начального выбора
static BOOL     g_anchor  = FALSE;

// Counter for selection retry attempts
// Счётчик попыток повторного выбора
static int      g_selTries= 0;

// Path to skin that needs to be reapplied after rename
// Путь к скину, который нужно повторно применить после переименования
static char     g_pendingSkinPath[MAX_PATH] = {0};

/*******************************************************************************
 * UTILITY MACROS AND FUNCTIONS
 * УТИЛИТАРНЫЕ МАКРОСЫ И ФУНКЦИИ
 ******************************************************************************/

/*******************************************************************************
 * IsVis Macro
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Checks if a window handle is valid, points to an existing window, and
 * the window is currently visible.
 * 
 * Проверяет, является ли дескриптор окна допустимым, указывает на существующее
 * окно, и окно в данный момент видимо.
 ******************************************************************************/
#define IsVis(h) ((h) && IsWindow(h) && IsWindowVisible(h))

/*******************************************************************************
 * Exists
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Checks if a file or directory exists at the specified path.
 * 
 * Проверяет, существует ли файл или каталог по указанному пути.
 * 
 * PARAMETERS / ПАРАМЕТРЫ:
 * p - Path to check / Путь для проверки
 * 
 * RETURNS / ВОЗВРАЩАЕТ:
 * BOOL - TRUE if path exists, FALSE otherwise
 *        TRUE если путь существует, FALSE иначе
 ******************************************************************************/
static BOOL Exists(const char* p) {
    DWORD a = GetFileAttributesA(p);
    return (a != INVALID_FILE_ATTRIBUTES);
}

/*******************************************************************************
 * JoinPath
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Joins a directory path and filename into a single path, adding a backslash
 * separator if needed.
 * 
 * Объединяет путь к каталогу и имя файла в единый путь, добавляя разделитель
 * обратной косой черты при необходимости.
 * 
 * PARAMETERS / ПАРАМЕТРЫ:
 * out  - Buffer to receive the combined path
 *        Буфер для получения объединённого пути
 * dir  - Directory path / Путь к каталогу
 * file - Filename to append / Имя файла для добавления
 ******************************************************************************/
static void JoinPath(char* out, const char* dir, const char* file) {
    lstrcpyA(out, dir);
    int len = lstrlenA(out);
    // Add backslash if directory path doesn't end with one
    // Добавить обратную косую черту, если путь каталога не заканчивается ею
    if (len && out[len-1]!='\\') lstrcatA(out, "\\");
    lstrcatA(out, file);
}

/*******************************************************************************
 * GetBaseName
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Extracts the filename from a full path by finding the last path separator.
 * 
 * Извлекает имя файла из полного пути, находя последний разделитель пути.
 * 
 * PARAMETERS / ПАРАМЕТРЫ:
 * path - Full path string / Строка полного пути
 * 
 * RETURNS / ВОЗВРАЩАЕТ:
 * const char* - Pointer to the filename portion of the path
 *               Указатель на часть имени файла в пути
 * 
 * NOTES / ПРИМЕЧАНИЯ:
 * Returns pointer to the character after the last '\' or '/', or to the
 * beginning of the string if no separator is found.
 * 
 * Возвращает указатель на символ после последнего '\' или '/', или на
 * начало строки, если разделитель не найден.
 ******************************************************************************/
static const char* GetBaseName(const char* path) {
    const char* p = path;
    // Find last backslash or forward slash
    // Найти последнюю обратную или прямую косую черту
    for (; *path; path++) if (*path == '\\' || *path == '/') p = path + 1;
    return p;
}

/*******************************************************************************
 * GetSkinsDir
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Determines the location of the Winamp Skins directory using multiple
 * fallback strategies.
 * 
 * Определяет расположение каталога скинов Winamp, используя несколько
 * резервных стратегий.
 * 
 * STRATEGY / СТРАТЕГИЯ:
 * 1. If already determined, return immediately
 * 2. Try <Winamp Installation>\Skins
 * 3. Try %APPDATA%\Winamp\Skins (for portable/user installations)
 * 
 * 1. Если уже определён, вернуться немедленно
 * 2. Попытаться <Установка Winamp>\Skins
 * 3. Попытаться %APPDATA%\Winamp\Skins (для портативных/пользовательских установок)
 * 
 * NOTES / ПРИМЕЧАНИЯ:
 * Result is cached in global variable g_SkinsDir for subsequent calls.
 * Uses dynamic API loading for SHGetSpecialFolderPathA for Win9x compatibility.
 * 
 * Результат кэшируется в глобальной переменной g_SkinsDir для последующих вызовов.
 * Использует динамическую загрузку API для SHGetSpecialFolderPathA для совместимости с Win9x.
 ******************************************************************************/
static void GetSkinsDir() {
    // If already determined, return immediately
    // Если уже определён, вернуться немедленно
    if (g_SkinsDir[0]) return;
    
    char buf[MAX_PATH];
    
    // Strategy 1: Try Winamp installation directory + Skins
    // Стратегия 1: попытаться каталог установки Winamp + Skins
    GetModuleFileNameA(NULL, buf, MAX_PATH);
    char* p = strrchr(buf, '\\'); 
    if(p) *p = 0;  // Remove winamp.exe filename / Удалить имя файла winamp.exe
    JoinPath(g_SkinsDir, buf, "Skins");
    if (Exists(g_SkinsDir)) return;  // Success! / Успех!

    // Strategy 2: Try AppData\Winamp\Skins
    // Стратегия 2: попытаться AppData\Winamp\Skins
    HMODULE hShell = LoadLibraryA("shell32.dll");
    if (hShell) {
        // Dynamically load SHGetSpecialFolderPathA for compatibility
        // Динамически загрузить SHGetSpecialFolderPathA для совместимости
        typedef BOOL (WINAPI *PFN)(HWND, LPSTR, int, BOOL);
        PFN pGet = (PFN)GetProcAddress(hShell, "SHGetSpecialFolderPathA");
        
        if (pGet && pGet(NULL, buf, CSIDL_APPDATA, FALSE)) {
            JoinPath(g_SkinsDir, buf, "Winamp\\Skins");
            if (Exists(g_SkinsDir)) { 
                FreeLibrary(hShell); 
                return;  // Success! / Успех!
            }
        }
        FreeLibrary(hShell);
    }
    // If we get here, g_SkinsDir will contain the last attempted path
    // Если мы здесь, g_SkinsDir будет содержать последний попытанный путь
}

/*******************************************************************************
 * ResolveSkin
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Resolves a skin name to its full path, checking for directory, .wsz file,
 * and .zip file variants.
 * 
 * Преобразует имя скина в его полный путь, проверяя варианты каталога,
 * файла .wsz и файла .zip.
 * 
 * PARAMETERS / ПАРАМЕТРЫ:
 * name - Skin name (without path or extension)
 *        Имя скина (без пути или расширения)
 * out  - Buffer to receive the full path
 *        Буфер для получения полного пути
 * 
 * RETURNS / ВОЗВРАЩАЕТ:
 * BOOL - TRUE if skin was found, FALSE otherwise
 *        TRUE если скин найден, FALSE иначе
 * 
 * SEARCH ORDER / ПОРЯДОК ПОИСКА:
 * 1. Directory: <SkinsDir>\<name>
 * 2. WSZ file:  <SkinsDir>\<name>.wsz
 * 3. ZIP file:  <SkinsDir>\<name>.zip
 * 
 * 1. Каталог:   <SkinsDir>\<name>
 * 2. Файл WSZ:  <SkinsDir>\<name>.wsz
 * 3. Файл ZIP:  <SkinsDir>\<name>.zip
 ******************************************************************************/
static BOOL ResolveSkin(const char* name, char* out) {
    // Try as directory first / Сначала попытаться как каталог
    JoinPath(out, g_SkinsDir, name);
    if (Exists(out)) return TRUE;
    
    char tmp[MAX_PATH];
    
    // Try with .wsz extension / Попытаться с расширением .wsz
    lstrcpyA(tmp, out); 
    lstrcatA(tmp, ".wsz");
    if (Exists(tmp)) { 
        lstrcpyA(out, tmp); 
        return TRUE; 
    }

    // Try with .zip extension / Попытаться с расширением .zip
    lstrcpyA(tmp, out); 
    lstrcatA(tmp, ".zip");
    if (Exists(tmp)) { 
        lstrcpyA(out, tmp); 
        return TRUE; 
    }
    
    return FALSE;  // Not found / Не найден
}

/*******************************************************************************
 * LIST ABSTRACTION FUNCTIONS
 * ФУНКЦИИ АБСТРАКЦИИ СПИСКА
 * 
 * These functions provide a unified interface for both ListBox and ListView
 * controls, abstracting the differences between the two control types.
 * 
 * Эти функции предоставляют единый интерфейс как для ListBox, так и для
 * ListView, абстрагируя различия между двумя типами элементов управления.
 ******************************************************************************/

/*******************************************************************************
 * List_GetSel
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Gets the index of the currently selected item in the list.
 * 
 * Получает индекс текущего выбранного элемента в списке.
 * 
 * PARAMETERS / ПАРАМЕТРЫ:
 * h - Handle to list control / Дескриптор элемента управления списком
 * 
 * RETURNS / ВОЗВРАЩАЕТ:
 * int - Index of selected item, or -1 if none selected
 *       Индекс выбранного элемента или -1, если ничего не выбрано
 ******************************************************************************/
static int List_GetSel(HWND h) {
    return g_isLB ? (int)SendMessage(h, LB_GETCURSEL, 0, 0) 
                  : (int)SendMessage(h, LVM_GETNEXTITEM, -1, LVNI_SELECTED);
}

/*******************************************************************************
 * List_GetCount
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Gets the total number of items in the list.
 * 
 * Получает общее количество элементов в списке.
 * 
 * PARAMETERS / ПАРАМЕТРЫ:
 * h - Handle to list control / Дескриптор элемента управления списком
 * 
 * RETURNS / ВОЗВРАЩАЕТ:
 * int - Number of items in the list / Количество элементов в списке
 ******************************************************************************/
static int List_GetCount(HWND h) {
    return g_isLB ? (int)SendMessage(h, LB_GETCOUNT, 0, 0) 
                  : (int)SendMessage(h, LVM_GETITEMCOUNT, 0, 0);
}

/*******************************************************************************
 * List_GetText
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Retrieves the text of an item at the specified index.
 * 
 * Получает текст элемента по указанному индексу.
 * 
 * PARAMETERS / ПАРАМЕТРЫ:
 * h   - Handle to list control / Дескриптор элемента управления списком
 * i   - Item index / Индекс элемента
 * buf - Buffer to receive the text / Буфер для получения текста
 * 
 * RETURNS / ВОЗВРАЩАЕТ:
 * BOOL - TRUE if text was retrieved, FALSE otherwise
 *        TRUE если текст получен, FALSE иначе
 ******************************************************************************/
static BOOL List_GetText(HWND h, int i, char* buf) {
    buf[0]=0;  // Initialize buffer / Инициализировать буфер
    
    if (g_isLB) {
        // ListBox: use LB_GETTEXT message
        // ListBox: использовать сообщение LB_GETTEXT
        SendMessage(h, LB_GETTEXT, i, (LPARAM)buf);
    } else {
        // ListView: use LVITEM structure
        // ListView: использовать структуру LVITEM
        LVITEMA lv = {0}; 
        lv.iItem=i; 
        lv.mask=LVIF_TEXT; 
        lv.pszText=buf; 
        lv.cchTextMax=MAX_PATH;
        SendMessage(h, LVM_GETITEMTEXT, i, (LPARAM)&lv);
    }
    return buf[0] != 0;  // Return TRUE if text was retrieved / Вернуть TRUE, если текст получен
}

/*******************************************************************************
 * List_SetSel
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Selects an item at the specified index and ensures it is visible.
 * 
 * Выбирает элемент по указанному индексу и гарантирует, что он виден.
 * 
 * PARAMETERS / ПАРАМЕТРЫ:
 * h - Handle to list control / Дескриптор элемента управления списком
 * i - Item index to select / Индекс элемента для выбора
 * 
 * NOTES / ПРИМЕЧАНИЯ:
 * For ListView, also clears any previous selection and sets focus.
 * Для ListView также очищает любое предыдущее выделение и устанавливает фокус.
 ******************************************************************************/
static void List_SetSel(HWND h, int i) {
    if (i < 0) return;  // Invalid index / Недопустимый индекс
    
    if (g_isLB) {
        // ListBox: simple selection / ListBox: простой выбор
        SendMessage(h, LB_SETCURSEL, i, 0);
    } else {
        // ListView: clear previous selection and set new one
        // ListView: очистить предыдущее выделение и установить новое
        LVITEMA lv = {0}; 
        lv.stateMask = LVIS_SELECTED|LVIS_FOCUSED;
        
        // Clear all selections / Очистить все выделения
        SendMessage(h, LVM_SETITEMSTATE, -1, (LPARAM)&lv);
        
        // Set new selection and focus / Установить новое выделение и фокус
        lv.state = LVIS_SELECTED|LVIS_FOCUSED;
        SendMessage(h, LVM_SETITEMSTATE, i, (LPARAM)&lv);
        
        // Ensure item is visible / Гарантировать, что элемент виден
        SendMessage(h, LVM_ENSUREVISIBLE, i, 0);
    }
}

/*******************************************************************************
 * SKIN MANAGEMENT FUNCTIONS
 * ФУНКЦИИ УПРАВЛЕНИЯ СКИНАМИ
 ******************************************************************************/

/*******************************************************************************
 * IsActiveSkin
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Checks if the specified skin path corresponds to the currently active skin.
 * 
 * Проверяет, соответствует ли указанный путь к скину текущему активному скину.
 * 
 * PARAMETERS / ПАРАМЕТРЫ:
 * path - Full path to skin to check / Полный путь к скину для проверки
 * 
 * RETURNS / ВОЗВРАЩАЕТ:
 * BOOL - TRUE if this is the active skin, FALSE otherwise
 *        TRUE если это активный скин, FALSE иначе
 * 
 * IMPLEMENTATION / РЕАЛИЗАЦИЯ:
 * Compares base filenames (case-insensitive) of current skin and provided path.
 * Сравнивает базовые имена файлов (без учёта регистра) текущего скина и предоставленного пути.
 ******************************************************************************/
static BOOL IsActiveSkin(const char* path) {
    // Get current skin path from Winamp / Получить путь текущего скина из Winamp
    char* cur = (char*)SendMessage(FindWinamp(), WM_WA_IPC, 0, IPC_GETSKIN);
    if (!cur || !path) return FALSE;
    
    // Compare base filenames (case-insensitive)
    // Сравнить базовые имена файлов (без учёта регистра)
    return (lstrcmpiA(GetBaseName(cur), GetBaseName(path)) == 0);
}

/*******************************************************************************
 * ApplySkin
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Applies (loads) a skin by its full path using Winamp's IPC mechanism.
 * 
 * Применяет (загружает) скин по его полному пути, используя механизм IPC Winamp.
 * 
 * PARAMETERS / ПАРАМЕТРЫ:
 * fullPath - Full path to the skin file or directory
 *            Полный путь к файлу скина или каталогу
 ******************************************************************************/
static void ApplySkin(const char* fullPath) {
    SendMessage(FindWinamp(), WM_WA_IPC, (WPARAM)fullPath, IPC_SETSKIN);
}

/*******************************************************************************
 * FindActiveIndex
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Finds the list index of the currently active skin by comparing with each
 * item in the list.
 * 
 * Находит индекс списка текущего активного скина путём сравнения с каждым
 * элементом в списке.
 * 
 * PARAMETERS / ПАРАМЕТРЫ:
 * h - Handle to list control / Дескриптор элемента управления списком
 * 
 * RETURNS / ВОЗВРАЩАЕТ:
 * int - Index of active skin in list, or -1 if not found
 *       Индекс активного скина в списке или -1, если не найден
 * 
 * ALGORITHM / АЛГОРИТМ:
 * 1. Get current skin path from Winamp
 * 2. Extract base filename from current skin path
 * 3. Iterate through all list items
 * 4. For each item, try exact name match first
 * 5. If no match, resolve full path and compare base filenames
 * 6. Return index if match found
 * 
 * 1. Получить путь текущего скина из Winamp
 * 2. Извлечь базовое имя файла из пути текущего скина
 * 3. Перебрать все элементы списка
 * 4. Для каждого элемента сначала попытаться точное совпадение имени
 * 5. Если нет совпадения, преобразовать в полный путь и сравнить базовые имена файлов
 * 6. Вернуть индекс, если совпадение найдено
 ******************************************************************************/
static int FindActiveIndex(HWND h) {
    // Get current skin path / Получить путь текущего скина
    char* cur = (char*)SendMessage(FindWinamp(), WM_WA_IPC, 0, IPC_GETSKIN);
    if (!cur) return -1;
    
    const char* base = GetBaseName(cur);
    
    int cnt = List_GetCount(h);
    char buf[MAX_PATH], full[MAX_PATH];
    
    // Search through all list items / Поиск по всем элементам списка
    for (int i=0; i<cnt; i++) {
        if (List_GetText(h, i, buf)) {
            // Try exact name match first / Сначала попытаться точное совпадение имени
            if (lstrcmpiA(buf, base) == 0) return i;
            
            // Try resolving to full path and comparing / Попытаться преобразовать в полный путь и сравнить
            if (ResolveSkin(buf, full) && lstrcmpiA(GetBaseName(full), base) == 0) return i;
        }
    }
    return -1;  // Not found / Не найден
}

/*******************************************************************************
 * RENAME DIALOG FUNCTIONS
 * ФУНКЦИИ ДИАЛОГА ПЕРЕИМЕНОВАНИЯ
 ******************************************************************************/

/*******************************************************************************
 * Dialog Template Structures
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Packed structures for creating a dialog template in memory. This allows
 * creating a custom dialog without a resource file.
 * 
 * Упакованные структуры для создания шаблона диалога в памяти. Это позволяет
 * создавать пользовательский диалог без файла ресурсов.
 ******************************************************************************/
#pragma pack(push, 1)
struct DLG_TEMPLATE {
    DLGTEMPLATE head;
    WORD menu; WORD cls; WORD title;
};
struct DLG_ITEM {
    DLGITEMTEMPLATE head;
    WORD cls; WORD title; WORD creat;
};
#pragma pack(pop)

/*******************************************************************************
 * RenameDlgProc
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Dialog procedure for the rename dialog. Handles initialization and
 * user input for renaming a skin.
 * 
 * Процедура диалога для диалога переименования. Обрабатывает инициализацию
 * и пользовательский ввод для переименования скина.
 * 
 * PARAMETERS / ПАРАМЕТРЫ:
 * h - Handle to dialog window / Дескриптор окна диалога
 * m - Message identifier / Идентификатор сообщения
 * w - First message parameter / Первый параметр сообщения
 * l - Second message parameter (pointer to buffer on WM_INITDIALOG)
 *     Второй параметр сообщения (указатель на буфер при WM_INITDIALOG)
 * 
 * DIALOG CONTROLS / ЭЛЕМЕНТЫ УПРАВЛЕНИЯ ДИАЛОГА:
 * Control ID 100: Static text label (prompt)
 * Control ID 101: Edit box for entering new name
 * IDOK:           OK button
 * IDCANCEL:       Cancel button
 * 
 * Элемент управления ID 100: статический текстовый ярлык (подсказка)
 * Элемент управления ID 101: поле редактирования для ввода нового имени
 * IDOK:                      кнопка OK
 * IDCANCEL:                  кнопка Отмена
 ******************************************************************************/
static INT_PTR CALLBACK RenameDlgProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    // Static pointer to output buffer (shared across dialog lifetime)
    // Статический указатель на выходной буфер (общий на время жизни диалога)
    static char* pOut;
    
    if (m == WM_INITDIALOG) {
        // Store output buffer pointer / Сохранить указатель на выходной буфер
        pOut = (char*)l;
        
        // Set dialog title and prompt text (localized strings)
        // Установить заголовок диалога и текст подсказки (локализованные строки)
        SetWindowText(h, RENAME_TITLE);
        SetDlgItemText(h, 100, RENAME_PROMPT);
        
        // Set initial text in edit box (current skin name)
        // Установить начальный текст в поле редактирования (текущее имя скина)
        SetDlgItemText(h, 101, pOut);
        
        // Set focus to edit box and select all text for easy editing
        // Установить фокус на поле редактирования и выделить весь текст для лёгкого редактирования
        SetFocus(GetDlgItem(h, 101));
        SendMessage(GetDlgItem(h, 101), EM_SETSEL, 0, -1);
        
        return FALSE;  // We set focus manually / Мы установили фокус вручную
    }
    
    if (m == WM_COMMAND) {
        if (LOWORD(w) == IDOK) {
            // OK button clicked - retrieve edited text
            // Кнопка OK нажата - получить отредактированный текст
            GetDlgItemText(h, 101, pOut, MAX_PATH);
            EndDialog(h, 1);  // Return 1 to indicate success / Вернуть 1 для обозначения успеха
        } else if (LOWORD(w) == IDCANCEL) {
            // Cancel button clicked
            // Кнопка Отмена нажата
            EndDialog(h, 0);  // Return 0 to indicate cancellation / Вернуть 0 для обозначения отмены
        }
    }
    return FALSE;
}

/*******************************************************************************
 * PromptRename
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Creates and displays a modal rename dialog by building a dialog template
 * in memory. This avoids the need for a resource file.
 * 
 * Создаёт и отображает модальный диалог переименования, создавая шаблон
 * диалога в памяти. Это избавляет от необходимости файла ресурсов.
 * 
 * PARAMETERS / ПАРАМЕТРЫ:
 * owner - Handle to owner window / Дескриптор окна-владельца
 * buf   - Buffer containing current name (input) and receiving new name (output)
 *         Буфер, содержащий текущее имя (вход) и принимающий новое имя (выход)
 * 
 * RETURNS / ВОЗВРАЩАЕТ:
 * BOOL - TRUE if user clicked OK, FALSE if cancelled
 *        TRUE если пользователь нажал OK, FALSE если отменил
 * 
 * DIALOG LAYOUT / МАКЕТ ДИАЛОГА:
 * - Size: 180x55 dialog units / Размер: 180x55 единиц диалога
 * - Font: MS Sans Serif, 8pt / Шрифт: MS Sans Serif, 8pt
 * - Controls: Label, Edit box, OK button, Cancel button
 *             Элементы: ярлык, поле редактирования, кнопка OK, кнопка Отмена
 * 
 * IMPLEMENTATION NOTES / ПРИМЕЧАНИЯ К РЕАЛИЗАЦИИ:
 * This function manually constructs a DLGTEMPLATE structure in memory,
 * including all dialog items. The ALIGN macro ensures proper alignment
 * of structures as required by DialogBoxIndirect.
 * 
 * Эта функция вручную создаёт структуру DLGTEMPLATE в памяти, включая все
 * элементы диалога. Макрос ALIGN обеспечивает правильное выравнивание
 * структур, требуемое DialogBoxIndirect.
 ******************************************************************************/
static BOOL PromptRename(HWND owner, char* buf) {
    const int W=180, H=55;  // Dialog dimensions in dialog units / Размеры диалога в единицах диалога
    
    // Allocate memory for dialog template (1024 bytes should be enough)
    // Выделить память для шаблона диалога (1024 байта должно быть достаточно)
    BYTE* mem = (BYTE*)LocalAlloc(LPTR, 1024);
    if (!mem) return FALSE;
    
    BYTE* p = mem;  // Pointer for building template / Указатель для создания шаблона
    
    // Build dialog header / Создать заголовок диалога
    DLGTEMPLATE* head = (DLGTEMPLATE*)p;
    head->style = DS_SETFONT | DS_MODALFRAME | DS_CENTER | WS_POPUP | WS_CAPTION | WS_SYSMENU;
    head->cdit = 4;  // Number of controls / Количество элементов управления
    head->cx = W; 
    head->cy = H;
    p += sizeof(DLGTEMPLATE); 
    p += 6;  // Skip menu, class, title (all empty) / Пропустить меню, класс, заголовок (все пустые)
    
    // Set font / Установить шрифт
    *(WORD*)p = 8; p += 2;  // Font size / Размер шрифта
    lstrcpyW((WCHAR*)p, L"MS Sans Serif"); 
    p += 28;

    // Macro to align pointer to DWORD boundary (required by DialogBoxIndirect)
    // Макрос для выравнивания указателя на границу DWORD (требуется DialogBoxIndirect)
    #define ALIGN(x) x = (BYTE*)(((DWORD_PTR)(x) + 3) & ~3)
    
    // Control 1: Static text label (prompt)
    // Элемент управления 1: статический текстовый ярлык (подсказка)
    ALIGN(p); {
        DLGITEMTEMPLATE* it = (DLGITEMTEMPLATE*)p;
        it->style = WS_CHILD | WS_VISIBLE | SS_LEFT;
        it->x = 5; it->y = 5; it->cx = 170; it->cy = 10; it->id = 100;
        p += sizeof(DLGITEMTEMPLATE);
        *(WORD*)p = 0xFFFF; p+=2;  // Class: predefined / Класс: предопределённый
        *(WORD*)p = 0x0082; p+=2;  // Static control / Статический элемент управления
        *(WORD*)p = 0; p+=2;       // No text (set later) / Нет текста (установить позже)
        *(WORD*)p = 0; p+=2;       // No creation data / Нет данных создания
    }
    
    // Control 2: Edit box for entering new name
    // Элемент управления 2: поле редактирования для ввода нового имени
    ALIGN(p); {
        DLGITEMTEMPLATE* it = (DLGITEMTEMPLATE*)p;
        it->style = WS_CHILD | WS_VISIBLE | WS_BORDER | WS_TABSTOP | ES_AUTOHSCROLL;
        it->x = 5; it->y = 18; it->cx = 170; it->cy = 12; it->id = 101;
        p += sizeof(DLGITEMTEMPLATE);
        *(WORD*)p = 0xFFFF; p+=2;  // Class: predefined / Класс: предопределённый
        *(WORD*)p = 0x0081; p+=2;  // Edit control / Элемент управления редактирования
        *(WORD*)p = 0; p+=2;       // No initial text / Нет начального текста
        *(WORD*)p = 0; p+=2;       // No creation data / Нет данных создания
    }
    
    // Control 3: OK button (default button)
    // Элемент управления 3: кнопка OK (кнопка по умолчанию)
    ALIGN(p); {
        DLGITEMTEMPLATE* it = (DLGITEMTEMPLATE*)p;
        it->style = WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON;
        it->x = 65; it->y = 35; it->cx = 50; it->cy = 14; it->id = IDOK;
        p += sizeof(DLGITEMTEMPLATE);
        *(WORD*)p = 0xFFFF; p+=2;  // Class: predefined / Класс: предопределённый
        *(WORD*)p = 0x0080; p+=2;  // Button control / Элемент управления кнопки
        lstrcpyW((WCHAR*)p, L"OK"); p += 6;  // Button text / Текст кнопки
        *(WORD*)p = 0; p+=2;  // No creation data / Нет данных создания
    }
    
    // Control 4: Cancel button
    // Элемент управления 4: кнопка Отмена
    ALIGN(p); {
        DLGITEMTEMPLATE* it = (DLGITEMTEMPLATE*)p;
        it->style = WS_CHILD | WS_VISIBLE | WS_TABSTOP;
        it->x = 120; it->y = 35; it->cx = 50; it->cy = 14; it->id = IDCANCEL;
        p += sizeof(DLGITEMTEMPLATE);
        *(WORD*)p = 0xFFFF; p+=2;  // Class: predefined / Класс: предопределённый
        *(WORD*)p = 0x0080; p+=2;  // Button control / Элемент управления кнопки
        
        // Use localized Cancel text if available, otherwise use default
        // Использовать локализованный текст Отмена, если доступен, иначе использовать по умолчанию
        if (WINDOW_CANCEL && WINDOW_CANCEL[0]) {
            MultiByteToWideChar(CP_ACP, 0, WINDOW_CANCEL, -1, (WCHAR*)p, 64);
        } else {
            lstrcpyW((WCHAR*)p, L"Cancel");
        }
        p += (lstrlenW((WCHAR*)p) + 1) * sizeof(WCHAR); 

        *(WORD*)p = 0; p+=2;  // No creation data / Нет данных создания
    }

    // Display the dialog and get result
    // Отобразить диалог и получить результат
    INT_PTR res = DialogBoxIndirectParamA(GetModuleHandle(NULL), (DLGTEMPLATE*)mem, owner, RenameDlgProc, (LPARAM)buf);
    
    // Free allocated memory / Освободить выделенную память
    LocalFree(mem);
    
    return (res == 1);  // Return TRUE if OK was clicked / Вернуть TRUE, если была нажата OK
}

/*******************************************************************************
 * SKIN OPERATION FUNCTIONS
 * ФУНКЦИИ ОПЕРАЦИЙ СО СКИНАМИ
 ******************************************************************************/

/*******************************************************************************
 * DoDelete
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Deletes the currently selected skin using shell file operations (moves to
 * Recycle Bin for safety).
 * 
 * Удаляет текущий выбранный скин, используя операции оболочки с файлами
 * (перемещает в корзину для безопасности).
 * 
 * PARAMETERS / ПАРАМЕТРЫ:
 * h - Handle to list control / Дескриптор элемента управления списком
 * 
 * PROCESS / ПРОЦЕСС:
 * 1. Get selected item index and text (skin name)
 * 2. Resolve skin name to full path
 * 3. If this is the active skin, switch to default skin first
 * 4. Use SHFileOperation to delete (move to Recycle Bin)
 * 5. Remove item from list
 * 6. Select previous item
 * 7. Trigger list refresh (F5)
 * 
 * 1. Получить индекс и текст выбранного элемента (имя скина)
 * 2. Преобразовать имя скина в полный путь
 * 3. Если это активный скин, сначала переключиться на скин по умолчанию
 * 4. Использовать SHFileOperation для удаления (перемещение в корзину)
 * 5. Удалить элемент из списка
 * 6. Выбрать предыдущий элемент
 * 7. Вызвать обновление списка (F5)
 * 
 * SAFETY FEATURES / ФУНКЦИИ БЕЗОПАСНОСТИ:
 * - Uses FOF_ALLOWUNDO to enable Recycle Bin
 * - Switches away from active skin before deletion
 * - Brief delay after skin switch to ensure stability
 * 
 * - Использует FOF_ALLOWUNDO для включения корзины
 * - Переключается с активного скина перед удалением
 * - Короткая задержка после переключения скина для обеспечения стабильности
 ******************************************************************************/
static void DoDelete(HWND h) {
    // Get selected item / Получить выбранный элемент
    int sel = List_GetSel(h);
    char buf[MAX_PATH], full[MAX_PATH];
    
    // Validate selection and resolve to full path
    // Проверить выбор и преобразовать в полный путь
    if (sel < 0 || !List_GetText(h, sel, buf) || !ResolveSkin(buf, full)) return;

    // If deleting active skin, switch to default first
    // Если удаляем активный скин, сначала переключиться на скин по умолчанию
    if (IsActiveSkin(full)) {
        SendMessage(FindWinamp(), WM_WA_IPC, (WPARAM)"", IPC_SETSKIN);
        Sleep(100);  // Brief delay to ensure skin is unloaded / Короткая задержка для гарантии выгрузки скина
    }

    // Prepare path for SHFileOperation (must be double-null terminated)
    // Подготовить путь для SHFileOperation (должен заканчиваться двумя нулями)
    char from[MAX_PATH+2] = {0}; 
    lstrcpyA(from, full);
    
    // Configure file operation structure
    // Настроить структуру операции с файлом
    SHFILEOPSTRUCTA op = {0};
    op.wFunc = FO_DELETE;                                      // Delete operation / Операция удаления
    op.pFrom = from;                                           // Source file / Исходный файл
    op.fFlags = FOF_ALLOWUNDO|FOF_NOCONFIRMATION|FOF_SILENT;  // Use Recycle Bin, no UI / Использовать корзину, без интерфейса
    
    // Perform deletion / Выполнить удаление
    if (SHFileOperationA(&op) == 0) {
        // Remove from list control / Удалить из элемента управления списком
        if (g_isLB) SendMessage(h, LB_DELETESTRING, sel, 0);
        else        SendMessage(h, LVM_DELETEITEM, sel, 0);
        
        // Select previous item (or first if we deleted first item)
        // Выбрать предыдущий элемент (или первый, если удалили первый элемент)
        List_SetSel(h, (sel > 0) ? sel - 1 : 0);
        
        // Trigger list refresh / Вызвать обновление списка
        SendMessage(h, WM_KEYDOWN, VK_F5, 0); 
        SendMessage(h, WM_KEYUP, VK_F5, 0); 
    }
}

/*******************************************************************************
 * DoRename
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Renames the currently selected skin using a custom dialog, handling both
 * files (.wsz, .zip) and directories, with special handling for active skins.
 * 
 * Переименовывает текущий выбранный скин, используя пользовательский диалог,
 * обрабатывая как файлы (.wsz, .zip), так и каталоги, со специальной
 * обработкой активных скинов.
 * 
 * PARAMETERS / ПАРАМЕТРЫ:
 * h - Handle to list control / Дескриптор элемента управления списком
 * 
 * PROCESS / ПРОЦЕСС:
 * 1. Get selected item and resolve to full path
 * 2. Determine if it's a directory or file, and save extension if file
 * 3. Strip extension from display name for editing
 * 4. Show rename dialog
 * 5. If confirmed, build new path with extension restored
 * 6. If active skin, switch to default temporarily
 * 7. Perform file rename operation
 * 8. Update list to reflect new name
 * 9. Trigger list refresh
 * 10. If was active, schedule reapplication of renamed skin
 * 
 * 1. Получить выбранный элемент и преобразовать в полный путь
 * 2. Определить, является ли это каталогом или файлом, и сохранить расширение, если файл
 * 3. Убрать расширение из отображаемого имени для редактирования
 * 4. Показать диалог переименования
 * 5. Если подтверждено, создать новый путь с восстановленным расширением
 * 6. Если активный скин, временно переключиться на скин по умолчанию
 * 7. Выполнить операцию переименования файла
 * 8. Обновить список для отражения нового имени
 * 9. Вызвать обновление списка
 * 10. Если был активным, запланировать повторное применение переименованного скина
 * 
 * SPECIAL HANDLING / СПЕЦИАЛЬНАЯ ОБРАБОТКА:
 * - Preserves file extension (.wsz or .zip) transparently
 * - Switches away from active skin before rename
 * - Schedules reapplication after rename completes
 * - Updates both ListBox and ListView appropriately
 * 
 * - Прозрачно сохраняет расширение файла (.wsz или .zip)
 * - Переключается с активного скина перед переименованием
 * - Планирует повторное применение после завершения переименования
 * - Соответствующим образом обновляет как ListBox, так и ListView
 ******************************************************************************/
static void DoRename(HWND h) {
    // Get selected item / Получить выбранный элемент
    int sel = List_GetSel(h);
    char buf[MAX_PATH], full[MAX_PATH]; 
    
    // Validate selection and resolve to full path
    // Проверить выбор и преобразовать в полный путь
    if (sel < 0 || !List_GetText(h, sel, buf) || !ResolveSkin(buf, full)) return;

    // Determine if this is a directory or file
    // Определить, является ли это каталогом или файлом
    BOOL isDir = (GetFileAttributesA(full) & FILE_ATTRIBUTE_DIRECTORY);
    char savedExt[16] = {0};

    // For files, detect and save extension (.wsz or .zip)
    // Для файлов обнаружить и сохранить расширение (.wsz или .zip)
    if (!isDir) {
        int fullLen = lstrlenA(full);
        if (fullLen > 4) {
            char* ext = full + fullLen - 4;
            if (lstrcmpiA(ext, ".wsz") == 0) lstrcpyA(savedExt, ".wsz");
            else if (lstrcmpiA(ext, ".zip") == 0) lstrcpyA(savedExt, ".zip");
        }
    }

    // Prepare display name for editing (without extension)
    // Подготовить отображаемое имя для редактирования (без расширения)
    char editName[MAX_PATH]; 
    lstrcpyA(editName, buf);

    // Strip extension from display name if it exists
    // Убрать расширение из отображаемого имени, если оно существует
    if (savedExt[0]) {
        int editLen = lstrlenA(editName);
        if (editLen > 4) {
            if (lstrcmpiA(editName + editLen - 4, savedExt) == 0) {
                editName[editLen - 4] = 0;  // Null-terminate before extension / Завершить нулём перед расширением
            }
        }
    }

    // Show rename dialog / Показать диалог переименования
    if (PromptRename(h, editName)) {
        // Build new full path / Создать новый полный путь
        char newFull[MAX_PATH];
        
        lstrcpyA(newFull, g_SkinsDir);
        int len = lstrlenA(newFull);
        if (len && newFull[len-1]!='\\') lstrcatA(newFull, "\\");
        
        lstrcatA(newFull, editName);  // Append new name / Добавить новое имя
        
        // Restore extension if this was a file
        // Восстановить расширение, если это был файл
        if (!isDir && savedExt[0]) {
            lstrcatA(newFull, savedExt);
        }

        // Check if this is the active skin / Проверить, является ли это активным скином
        BOOL wasActive = IsActiveSkin(full);
        
        // If active, switch to default temporarily
        // Если активный, временно переключиться на скин по умолчанию
        if (wasActive) {
            SendMessage(FindWinamp(), WM_WA_IPC, (WPARAM)"", IPC_SETSKIN);
            Sleep(50);  // Brief delay / Короткая задержка
        }

        // Perform the rename operation / Выполнить операцию переименования
        if (MoveFileA(full, newFull)) {
            // Update list control / Обновить элемент управления списком
            if (g_isLB) {
                // ListBox: delete and reinsert / ListBox: удалить и повторно вставить
                SendMessage(h, LB_DELETESTRING, sel, 0);
                int n = SendMessage(h, LB_INSERTSTRING, sel, (LPARAM)editName);
                List_SetSel(h, n);
            } else {
                // ListView: update text in place / ListView: обновить текст на месте
                LVITEMA lv={0}; 
                lv.iItem=sel; 
                lv.mask=LVIF_TEXT; 
                lv.pszText=editName;
                SendMessage(h, LVM_SETITEMTEXT, sel, (LPARAM)&lv);
            }

            // Trigger list refresh / Вызвать обновление списка
            SendMessage(h, WM_KEYDOWN, VK_F5, 0);
            SendMessage(h, WM_KEYUP, VK_F5, 0);

            // If this was active, schedule reapplication of renamed skin
            // Если это был активный скин, запланировать повторное применение переименованного скина
            if (wasActive) {
                lstrcpynA(g_pendingSkinPath, newFull, MAX_PATH);
                SetTimer(h, RETRY_TIMER_ID, 200, NULL);  // 200ms delay / Задержка 200мс
            }
        }
    }
}

/*******************************************************************************
 * SUBCLASS WINDOW PROCEDURE
 * ПРОЦЕДУРА ПОДМЕНЫ ОКНА
 ******************************************************************************/

/*******************************************************************************
 * SkinProc
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Custom window procedure for the skin list control. Intercepts keyboard
 * events, mouse events, and timers to provide delete, rename, and auto-
 * selection functionality.
 * 
 * Пользовательская оконная процедура для элемента управления списком скинов.
 * Перехватывает события клавиатуры, мыши и таймеры для предоставления
 * функциональности удаления, переименования и автоматического выбора.
 * 
 * PARAMETERS / ПАРАМЕТРЫ:
 * h - Handle to list control / Дескриптор элемента управления списком
 * m - Message identifier / Идентификатор сообщения
 * w - First message parameter / Первый параметр сообщения
 * l - Second message parameter / Второй параметр сообщения
 * 
 * HANDLED MESSAGES / ОБРАБАТЫВАЕМЫЕ СООБЩЕНИЯ:
 * 
 * WM_KEYDOWN:
 *   Delete - Deletes selected skin
 *   F2     - Renames selected skin
 * 
 *   Delete - удаляет выбранный скин
 *   F2     - переименовывает выбранный скин
 * 
 * WM_KEYUP / WM_LBUTTONUP:
 *   Triggers delayed skin application (250ms after selection change)
 * 
 *   Вызывает отложенное применение скина (250мс после изменения выбора)
 * 
 * WM_CONTEXTMENU:
 *   Shows context menu with Rename and Delete options
 * 
 *   Показывает контекстное меню с опциями Переименовать и Удалить
 * 
 * WM_TIMER:
 *   RETRY_TIMER_ID  - Reapplies skin after rename
 *   APPLY_TIMER_ID  - Applies selected skin after delay
 *   SELECT_TIMER_ID - Auto-selects active skin when dialog opens
 * 
 *   RETRY_TIMER_ID  - повторно применяет скин после переименования
 *   APPLY_TIMER_ID  - применяет выбранный скин после задержки
 *   SELECT_TIMER_ID - автоматически выбирает активный скин при открытии диалога
 ******************************************************************************/
static LRESULT CALLBACK SkinProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    // Handle keyboard shortcuts / Обработка горячих клавиш
    if (m == WM_KEYDOWN) {
        if (w == VK_DELETE) { DoDelete(h); return 0; }  // Delete key / Клавиша Delete
        if (w == VK_F2)     { DoRename(h); return 0; }  // F2 key / Клавиша F2
    }
    
    // Handle selection changes - set up delayed skin application
    // Обработка изменений выбора - настроить отложенное применение скина
    if (m == WM_KEYUP || m == WM_LBUTTONUP) {
        // Only apply if not in initial selection phase (g_anchor)
        // Применять только если не в фазе начального выбора (g_anchor)
        if (!g_anchor) {
            // Cancel any existing timer / Отменить любой существующий таймер
            if (g_tmApply) KillTimer(h, APPLY_TIMER_ID);
            
            // Set new timer for delayed application (250ms)
            // Установить новый таймер для отложенного применения (250мс)
            g_tmApply = SetTimer(h, APPLY_TIMER_ID, 250, NULL);
        }
    }

    // Handle context menu / Обработка контекстного меню
    if (m == WM_CONTEXTMENU) {
        int sel = List_GetSel(h);
        if (sel >= 0) {  // Only show menu if item is selected / Показывать меню только если элемент выбран
            // Create context menu / Создать контекстное меню
            HMENU menu = CreatePopupMenu();
            AppendMenuA(menu, MF_STRING, IDM_CTX_RENAME, MENU_RENAME);
            AppendMenuA(menu, MF_STRING, IDM_CTX_DELETE, MENU_DELITEM);
            
            // Show menu at cursor position / Показать меню в позиции курсора
            POINT pt; GetCursorPos(&pt);
            int cmd = TrackPopupMenu(menu, TPM_RETURNCMD, pt.x, pt.y, 0, h, NULL);
            DestroyMenu(menu);
            
            // Execute selected command / Выполнить выбранную команду
            if (cmd == IDM_CTX_RENAME) DoRename(h);
            if (cmd == IDM_CTX_DELETE) DoDelete(h);
            return 0;
        }
    }

    // Handle timers / Обработка таймеров
    if (m == WM_TIMER) {
        // Retry timer: reapply skin after rename
        // Таймер повтора: повторно применить скин после переименования
        if (w == RETRY_TIMER_ID) {
            KillTimer(h, RETRY_TIMER_ID);
            if (g_pendingSkinPath[0]) {
                ApplySkin(g_pendingSkinPath);
                g_pendingSkinPath[0] = 0;  // Clear pending path / Очистить ожидающий путь
            }
            return 0;
        }

        // Apply timer: apply selected skin after delay
        // Таймер применения: применить выбранный скин после задержки
        if (w == APPLY_TIMER_ID) {
            KillTimer(h, APPLY_TIMER_ID); 
            g_tmApply = 0;
            
            // Get selected skin and apply it / Получить выбранный скин и применить его
            int sel = List_GetSel(h);
            char buf[MAX_PATH], full[MAX_PATH];
            if (sel >= 0 && List_GetText(h, sel, buf) && ResolveSkin(buf, full)) {
                ApplySkin(full);
            }
            return 0;
        }
        
        // Select timer: auto-select active skin when dialog opens
        // Таймер выбора: автоматически выбрать активный скин при открытии диалога
        if (w == SELECT_TIMER_ID) {
            int idx = FindActiveIndex(h);
            if (idx >= 0) {
                // Found active skin - select it and stop timer
                // Найден активный скин - выбрать его и остановить таймер
                KillTimer(h, SELECT_TIMER_ID); 
                g_tmSel = 0;
                g_anchor = FALSE;  // Allow normal skin application now / Разрешить обычное применение скина теперь
                List_SetSel(h, idx);
            } else if (++g_selTries > 30) {
                // Give up after 30 attempts (300ms)
                // Сдаться после 30 попыток (300мс)
                KillTimer(h, SELECT_TIMER_ID); 
                g_tmSel = 0;
                g_anchor = FALSE;
            }
            return 0;
        }
    }

    // Forward all other messages to original window procedure
    // Пересылка всех остальных сообщений оригинальной оконной процедуре
    return CallWindowProc(g_oldProc, h, m, w, l);
}

/*******************************************************************************
 * DIALOG MONITORING FUNCTIONS
 * ФУНКЦИИ МОНИТОРИНГА ДИАЛОГА
 ******************************************************************************/

/*******************************************************************************
 * EnumChild
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Callback function for EnumChildWindows. Searches for the skin list control
 * within a dialog and subclasses it if found.
 * 
 * Функция обратного вызова для EnumChildWindows. Ищет элемент управления
 * списком скинов в диалоге и подменяет его, если найден.
 * 
 * PARAMETERS / ПАРАМЕТРЫ:
 * h - Handle to child window being enumerated
 *     Дескриптор дочернего окна, которое перечисляется
 * 
 * RETURNS / ВОЗВРАЩАЕТ:
 * BOOL - TRUE to continue enumeration, FALSE to stop
 *        TRUE для продолжения перечисления, FALSE для остановки
 * 
 * IDENTIFICATION LOGIC / ЛОГИКА ИДЕНТИФИКАЦИИ:
 * 1. Check if control ID matches SKIN_CTL_ID (1122)
 * 2. Verify parent dialog doesn't have OK/Cancel buttons (to avoid wrong dialogs)
 * 3. Check if control is ListBox or ListView (SysListView32)
 * 4. If not already subclassed, install our window procedure
 * 5. Initialize state variables and start auto-selection timer
 * 
 * 1. Проверить, соответствует ли ID элемента управления SKIN_CTL_ID (1122)
 * 2. Проверить, что родительский диалог не имеет кнопок OK/Отмена (чтобы избежать неправильных диалогов)
 * 3. Проверить, является ли элемент управления ListBox или ListView (SysListView32)
 * 4. Если ещё не подменён, установить нашу оконную процедуру
 * 5. Инициализировать переменные состояния и запустить таймер автоматического выбора
 ******************************************************************************/
static BOOL CALLBACK EnumChild(HWND h, LPARAM) {
    // Check if this is the skin list control (ID 1122)
    // Проверить, является ли это элементом управления списком скинов (ID 1122)
    if (GetDlgCtrlID(h) == SKIN_CTL_ID) { 
        // Get parent dialog / Получить родительский диалог
        HWND hParent = GetParent(h);
        if (!hParent) return TRUE;

        // Skip dialogs with Cancel button (e.g., About dialog)
        // Пропустить диалоги с кнопкой Отмена (например, диалог О программе)
        if (GetDlgItem(hParent, IDCANCEL) != NULL) {
            return TRUE; 
        }

        // Skip dialogs with OK button (e.g., preference dialogs)
        // Пропустить диалоги с кнопкой OK (например, диалоги настроек)
        if (GetDlgItem(hParent, IDOK) != NULL) {
            return TRUE; 
        }

        // Check control class / Проверить класс элемента управления
        char cls[32]; 
        GetClassNameA(h, cls, 31);
        BOOL isLB = (lstrcmpiA(cls, "ListBox") == 0);
        
        // Only handle ListBox or ListView / Обрабатывать только ListBox или ListView
        if (isLB || lstrcmpiA(cls, "SysListView32") == 0) {
            // Only subclass if not already subclassed
            // Подменять только если ещё не подменено
            if (GetWindowLongPtrA(h, GWLP_WNDPROC) != (LONG_PTR)SkinProc) {
                // Cache control handle and type / Кэшировать дескриптор и тип элемента управления
                g_hList = h;
                g_isLB = isLB;
                
                // Install our window procedure / Установить нашу оконную процедуру
                g_oldProc = (WNDPROC)SetWindowLongPtrA(h, GWLP_WNDPROC, (LONG_PTR)SkinProc);
                
                // Initialize state / Инициализировать состояние
                GetSkinsDir();  // Determine skins directory / Определить каталог скинов
                g_selTries = 0;
                g_anchor = TRUE;  // Prevent auto-apply during initial selection / Предотвратить автоматическое применение во время начального выбора
                
                // Start auto-selection timer (10ms interval)
                // Запустить таймер автоматического выбора (интервал 10мс)
                g_tmSel = SetTimer(h, SELECT_TIMER_ID, 10, NULL);
            }
            return FALSE;  // Stop enumeration / Остановить перечисление
        }
    }
    return TRUE;  // Continue enumeration / Продолжить перечисление
}

/*******************************************************************************
 * MonitorTimer
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Timer callback that monitors for the appearance of Winamp's skin selection
 * dialog. Called every second to check for the dialog.
 * 
 * Обратный вызов таймера, который отслеживает появление диалога выбора скинов
 * Winamp. Вызывается каждую секунду для проверки диалога.
 * 
 * PARAMETERS / ПАРАМЕТРЫ:
 * Standard timer callback parameters (unused)
 * Стандартные параметры обратного вызова таймера (не используются)
 * 
 * ALGORITHM / АЛГОРИТМ:
 * 1. If list already found and visible, return immediately
 * 2. Find main Winamp window and get its process ID
 * 3. Enumerate all #32770 (dialog) windows
 * 4. Check if each dialog belongs to Winamp process
 * 5. If dialog is visible, enumerate its children to find skin list
 * 6. Stop when skin list is found
 * 
 * 1. Если список уже найден и виден, вернуться немедленно
 * 2. Найти главное окно Winamp и получить его идентификатор процесса
 * 3. Перечислить все окна #32770 (диалоги)
 * 4. Проверить, принадлежит ли каждый диалог процессу Winamp
 * 5. Если диалог видим, перечислить его дочерние элементы для поиска списка скинов
 * 6. Остановиться, когда список скинов найден
 * 
 * NOTES / ПРИМЕЧАНИЯ:
 * This timer-based approach ensures we catch the skin dialog whenever it opens,
 * regardless of how it was opened (menu, hotkey, etc.).
 * 
 * Этот подход на основе таймера гарантирует, что мы поймаем диалог скинов
 * когда бы он ни открылся, независимо от способа открытия (меню, горячая клавиша и т.д.).
 ******************************************************************************/
static VOID CALLBACK MonitorTimer(HWND, UINT, UINT_PTR, DWORD) {
    // If list already found and visible, nothing to do
    // Если список уже найден и виден, нечего делать
    if (g_hList && IsVis(g_hList)) return;
    
    // Find main Winamp window / Найти главное окно Winamp
    HWND wa = FindWinamp();
    if (!wa) return;
    
    // Get Winamp process ID / Получить идентификатор процесса Winamp
    DWORD pid; 
    GetWindowThreadProcessId(wa, &pid);
    
    // Enumerate all dialogs / Перечислить все диалоги
    HWND hDlg = NULL;
    while ((hDlg = FindWindowEx(NULL, hDlg, "#32770", NULL))) {
        // Check if dialog belongs to Winamp process
        // Проверить, принадлежит ли диалог процессу Winamp
        DWORD tpid; 
        GetWindowThreadProcessId(hDlg, &tpid);
        
        if (tpid == pid && IsWindowVisible(hDlg)) {
            // Dialog is from Winamp and visible - enumerate children
            // Диалог от Winamp и видим - перечислить дочерние элементы
            EnumChildWindows(hDlg, EnumChild, 0);
            
            // If we found the list, stop searching
            // Если нашли список, остановить поиск
            if (g_hList) break;
        }
    }
}

/*******************************************************************************
 * EXPORTED FUNCTIONS
 * ЭКСПОРТИРУЕМЫЕ ФУНКЦИИ
 ******************************************************************************/

/*******************************************************************************
 * SkinDel_Init
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Initializes the skin delete/rename module by starting the monitor timer.
 * 
 * Инициализирует модуль удаления/переименования скинов, запуская таймер
 * мониторинга.
 * 
 * NOTES / ПРИМЕЧАНИЯ:
 * This is called when the plugin/module is loaded. The timer will run every
 * second to check for the skin selection dialog.
 * 
 * Это вызывается при загрузке плагина/модуля. Таймер будет работать каждую
 * секунду для проверки диалога выбора скинов.
 ******************************************************************************/
void SkinDel_Init(void) {
    // Start monitor timer (1 second interval)
    // Запустить таймер мониторинга (интервал 1 секунда)
    g_hTimer = SetTimer(NULL, 0, 1000, MonitorTimer);
}

/*******************************************************************************
 * SkinDel_Quit
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Cleans up the skin delete/rename module by stopping the monitor timer
 * and restoring the original window procedure.
 * 
 * Очищает модуль удаления/переименования скинов, останавливая таймер
 * мониторинга и восстанавливая оригинальную оконную процедуру.
 * 
 * IMPORTANCE / ВАЖНОСТЬ:
 * CRITICAL: This function must be called before module unload to prevent
 * crashes. Failing to restore the window procedure will cause Winamp to
 * call unloaded code.
 * 
 * КРИТИЧНО: Эта функция должна быть вызвана перед выгрузкой модуля для
 * предотвращения крашей. Неспособность восстановить оконную процедуру
 * приведёт к тому, что Winamp попытается вызвать выгруженный код.
 ******************************************************************************/
void SkinDel_Quit(void) {
    // Stop monitor timer / Остановить таймер мониторинга
    if (g_hTimer) KillTimer(NULL, g_hTimer);
    
    // Restore original window procedure if we subclassed it
    // Восстановить оригинальную оконную процедуру, если мы её подменили
    if (g_hList && IsWindow(g_hList) && g_oldProc) {
        SetWindowLongPtrA(g_hList, GWLP_WNDPROC, (LONG_PTR)g_oldProc);
    }
}