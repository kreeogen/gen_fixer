/*******************************************************************************
 * WINAMP MEDIA LIBRARY CYRILLIC SEARCH FIX MODULE
 * МОДУЛЬ ИСПРАВЛЕНИЯ ПОИСКА КИРИЛЛИЦЫ В МЕДИАТЕКЕ WINAMP
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Fixes case-insensitive search for Russian/Cyrillic text in Winamp 2.95
 * Media Library. The standard MSVCRT string comparison functions (_stricmp,
 * _strnicmp, strstr) do not handle Cyrillic characters correctly, causing
 * search to fail when query and database use different cases.
 * 
 * Исправляет поиск без учёта регистра для русского/кириллического текста в
 * библиотеке Winamp 2.95. Стандартные функции сравнения строк MSVCRT (_stricmp,
 * _strnicmp, strstr) не обрабатывают кириллические символы правильно, вызывая
 * сбой поиска, когда запрос и база данных используют разные регистры.
 * 
 * PROBLEM / ПРОБЛЕМА:
 * MSVCRT functions only handle ASCII case conversion (A-Z - a-z). Cyrillic
 * characters (А-Я, а-я) are not recognized as having upper/lower case variants.
 * 
 * Example:
 * _stricmp("МУЗЫКА", "музыка") returns non-zero (not equal)
 * strstr("Любимая МУЗЫКА", "музыка") returns NULL (not found)
 * 
 * Функции MSVCRT обрабатывают только ASCII преобразование регистра (A-Z - a-z).
 * Кириллические символы (А-Я, а-я) не распознаются как имеющие варианты верхнего/нижнего регистра.
 * 
 * Пример:
 * _stricmp("МУЗЫКА", "музыка") возвращает ненулевое значение (не равны)
 * strstr("Любимая МУЗЫКА", "музыка") возвращает NULL (не найдено)
 * 
 * SOLUTION / РЕШЕНИЕ:
 * Replace MSVCRT functions with Windows locale-aware equivalents via IAT hooking:
 * - _stricmp  > lstrcmpiA   (locale-aware case-insensitive compare)
 * - _strnicmp > CompareStringA with NORM_IGNORECASE
 * - strstr    > StrStrIA     (case-insensitive substring search)
 * 
 * Заменить функции MSVCRT на Windows locale-aware эквиваленты через IAT hooking:
 * - _stricmp  > lstrcmpiA   (locale-aware сравнение без учёта регистра)
 * - _strnicmp > CompareStringA с NORM_IGNORECASE
 * - strstr    > StrStrIA     (поиск подстроки без учёта регистра)
 * 
 * HOW IT WORKS / КАК ЭТО РАБОТАЕТ:
 * 
 * 1. DELAYED INITIALIZATION / ОТЛОЖЕННАЯ ИНИЦИАЛИЗАЦИЯ:
 *    Uses timer to wait for gen_ml.dll to load (Media Library plugin).
 *    gen_ml.dll may not be loaded at plugin initialization time.
 * 
 *    Использует таймер для ожидания загрузки gen_ml.dll (плагин библиотеки).
 *    gen_ml.dll может быть не загружен во время инициализации плагина.
 * 
 * 2. IAT PATCHING / ПАТЧИНГ IAT:
 *    Modifies gen_ml.dll's Import Address Table to redirect MSVCRT function
 *    calls to our locale-aware replacements.
 * 
 *    Изменяет таблицу импорта gen_ml.dll для перенаправления вызовов функций
 *    MSVCRT на наши locale-aware замены.
 * 
 * 3. LOCALE-AWARE COMPARISON / LOCALE-AWARE СРАВНЕНИЕ:
 *    Uses Windows CompareStringA with LOCALE_USER_DEFAULT to respect user's
 *    language settings. Correctly handles Cyrillic, Latin, and other scripts.
 * 
 *    Использует Windows CompareStringA с LOCALE_USER_DEFAULT для уважения
 *    языковых настроек пользователя. Правильно обрабатывает кириллицу, латиницу
 *    и другие алфавиты.
 * 
 * COMPATIBILITY / СОВМЕСТИМОСТЬ:
 * - Win98+: Uses CompareStringA (always available)
 * - Win2000+: Prefers StrStrIA from shlwapi.dll (faster)
 * - Fallback: Manual implementation using CompareStringA
 * 
 * - Win98+: Использует CompareStringA (всегда доступна)
 * - Win2000+: Предпочитает StrStrIA из shlwapi.dll (быстрее)
 * - Резерв: Ручная реализация с использованием CompareStringA
 * 
 ******************************************************************************/

// mod_ml_search.cpp
// IAT Hook to fix Russian/Cyrillic search in Winamp 2.95 Media Library
//
// VS2003 / Win98-Win11 compatible

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shlwapi.h>

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "shlwapi.lib")

/*******************************************************************************
 * CONFIGURATION
 * КОНФИГУРАЦИЯ
 ******************************************************************************/

// Target module to patch / Целевой модуль для патчинга
static const char* TARGET_MODULE = "gen_ml.dll";

/*******************************************************************************
 * IAT HOOK STRUCTURES
 * СТРУКТУРЫ ПЕРЕХВАТА IAT
 ******************************************************************************/

typedef struct {
    const char* funcName;  // Function name to hook / Имя функции для перехвата
    void* newFunc;         // Replacement function / Функция-замена
    void* origFunc;        // Original function pointer / Указатель на оригинальную функцию
} HookEntry;

/*******************************************************************************
 * REPLACEMENT FUNCTIONS
 * ФУНКЦИИ-ЗАМЕНЫ
 ******************************************************************************/

/*******************************************************************************
 * My_strstr_fallback
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Fallback implementation of case-insensitive substring search using
 * CompareStringA. Used when StrStrIA is not available (Win98).
 * 
 * Резервная реализация поиска подстроки без учёта регистра с использованием
 * CompareStringA. Используется, когда StrStrIA недоступна (Win98).
 * 
 * PARAMETERS / ПАРАМЕТРЫ:
 * haystack - String to search in / Строка, в которой искать
 * needle   - Substring to find / Подстрока для поиска
 * 
 * RETURNS / ВОЗВРАЩАЕТ:
 * Pointer to first occurrence of needle in haystack (case-insensitive),
 * or NULL if not found.
 * 
 * Указатель на первое вхождение needle в haystack (без учёта регистра),
 * или NULL если не найдено.
 * 
 * ALGORITHM / АЛГОРИТМ:
 * 1. Validate inputs
 * 2. Check if needle is empty (special case)
 * 3. For each position in haystack:
 *    - Compare substring of needleLen characters
 *    - Use CompareStringA with NORM_IGNORECASE
 *    - Return position if match found
 * 
 * 1. Проверить входы
 * 2. Проверить, пуста ли needle (специальный случай)
 * 3. Для каждой позиции в haystack:
 *    - Сравнить подстроку длиной needleLen символов
 *    - Использовать CompareStringA с NORM_IGNORECASE
 *    - Вернуть позицию, если найдено совпадение
 ******************************************************************************/
static char* My_strstr_fallback(const char* haystack, const char* needle)
{
    if (!haystack || !needle) return NULL;
    if (!*needle) return (char*)haystack;  // Empty needle always matches / Пустая needle всегда совпадает

    int needleLen = lstrlenA(needle);
    int haystackLen = lstrlenA(haystack);

    if (needleLen > haystackLen) return NULL;

    // Scan through haystack / Сканировать haystack
    for (int i = 0; i <= haystackLen - needleLen; i++) {
        // Compare substring at position i / Сравнить подстроку на позиции i
        int cmp = CompareStringA(
            LOCALE_USER_DEFAULT,  // Use user's locale / Использовать локаль пользователя
            NORM_IGNORECASE,      // Ignore case / Игнорировать регистр
            haystack + i, needleLen,
            needle, needleLen);
        
        if (cmp == CSTR_EQUAL) {
            return (char*)(haystack + i);
        }
    }
    return NULL;
}

/*******************************************************************************
 * StrStrIA dynamic loading
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * StrStrIA is faster than our fallback but only available on Win2000+.
 * We load it dynamically and fall back if unavailable.
 * 
 * StrStrIA быстрее нашего fallback, но доступна только на Win2000+.
 * Загружаем её динамически и используем fallback, если недоступна.
 ******************************************************************************/
typedef LPSTR (WINAPI *PFN_StrStrIA)(LPCSTR, LPCSTR);
static PFN_StrStrIA g_pStrStrIA = NULL;
static BOOL g_StrStrIA_checked = FALSE;

/*******************************************************************************
 * My_stricmp
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Replacement for _stricmp that handles Cyrillic characters correctly.
 * 
 * Замена для _stricmp, которая правильно обрабатывает кириллические символы.
 * 
 * PARAMETERS / ПАРАМЕТРЫ:
 * s1, s2 - Strings to compare / Строки для сравнения
 * 
 * RETURNS / ВОЗВРАЩАЕТ:
 * < 0 if s1 < s2
 * = 0 if s1 == s2
 * > 0 if s1 > s2
 * 
 * IMPLEMENTATION / РЕАЛИЗАЦИЯ:
 * Uses lstrcmpiA which is locale-aware and handles all Unicode characters
 * correctly in their ANSI (codepage) representation.
 * 
 * Использует lstrcmpiA, которая учитывает локаль и правильно обрабатывает
 * все Unicode символы в их ANSI (кодовая страница) представлении.
 ******************************************************************************/
static int __cdecl My_stricmp(const char* s1, const char* s2)
{
    return lstrcmpiA(s1, s2);
}

/*******************************************************************************
 * My_strstr
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Replacement for strstr that performs case-insensitive substring search
 * with proper Cyrillic support.
 * 
 * Замена для strstr, которая выполняет поиск подстроки без учёта регистра
 * с правильной поддержкой кириллицы.
 * 
 * IMPLEMENTATION / РЕАЛИЗАЦИЯ:
 * First tries to load StrStrIA from shlwapi.dll (Win2000+).
 * Falls back to manual implementation if unavailable.
 * 
 * Сначала пытается загрузить StrStrIA из shlwapi.dll (Win2000+).
 * Использует ручную реализацию, если недоступна.
 ******************************************************************************/
static char* __cdecl My_strstr(const char* haystack, const char* needle)
{
    // One-time dynamic loading of StrStrIA / Одноразовая динамическая загрузка StrStrIA
    if (!g_StrStrIA_checked) {
        HMODULE hShlwapi = GetModuleHandleA("shlwapi.dll");
        if (!hShlwapi) hShlwapi = LoadLibraryA("shlwapi.dll");
        if (hShlwapi) {
            g_pStrStrIA = (PFN_StrStrIA)GetProcAddress(hShlwapi, "StrStrIA");
        }
        g_StrStrIA_checked = TRUE;
    }

    // Prefer StrStrIA if available (faster) / Предпочесть StrStrIA, если доступна (быстрее)
    if (g_pStrStrIA) {
        return g_pStrStrIA(haystack, needle);
    }

    // Fallback to manual implementation / Резерв - ручная реализация
    return My_strstr_fallback(haystack, needle);
}

/*******************************************************************************
 * My_strnicmp
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Replacement for _strnicmp that handles Cyrillic characters correctly.
 * 
 * Замена для _strnicmp, которая правильно обрабатывает кириллические символы.
 * 
 * PARAMETERS / ПАРАМЕТРЫ:
 * s1, s2 - Strings to compare / Строки для сравнения
 * count  - Maximum number of characters to compare / Максимальное количество символов для сравнения
 * 
 * RETURNS / ВОЗВРАЩАЕТ:
 * < 0 if s1 < s2
 * = 0 if s1 == s2
 * > 0 if s1 > s2
 * 
 * IMPLEMENTATION / РЕАЛИЗАЦИЯ:
 * Uses CompareStringA with count parameter to limit comparison length.
 * LOCALE_USER_DEFAULT respects user's codepage and language settings.
 * 
 * Использует CompareStringA с параметром count для ограничения длины сравнения.
 * LOCALE_USER_DEFAULT уважает кодовую страницу и языковые настройки пользователя.
 ******************************************************************************/
static int __cdecl My_strnicmp(const char* s1, const char* s2, size_t count)
{
    int result = CompareStringA(
        LOCALE_USER_DEFAULT,  // Use user's locale / Использовать локаль пользователя
        NORM_IGNORECASE,      // Ignore case / Игнорировать регистр
        s1, (int)count, 
        s2, (int)count);
    
    // Convert CompareStringA result to standard comparison result
    // Преобразовать результат CompareStringA в стандартный результат сравнения
    if (result == CSTR_LESS_THAN) return -1;
    if (result == CSTR_GREATER_THAN) return 1;
    return 0;
}

/*******************************************************************************
 * IAT PATCHING CODE
 * КОД ПАТЧИНГА IAT
 ******************************************************************************/

/*******************************************************************************
 * PatchIAT
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Patches Import Address Table (IAT) of a module to redirect function calls.
 * 
 * Патчит таблицу адресов импорта (IAT) модуля для перенаправления вызовов функций.
 * 
 * PARAMETERS / ПАРАМЕТРЫ:
 * hModule  - Module to patch / Модуль для патчинга
 * dllName  - DLL name containing function (e.g., "MSVCRT.dll")
 *            Имя DLL, содержащей функцию (напр., "MSVCRT.dll")
 * funcName - Function name to hook (e.g., "_stricmp")
 *            Имя функции для перехвата (напр., "_stricmp")
 * newFunc  - Replacement function pointer / Указатель на функцию-замену
 * oldFunc  - Output: original function pointer (optional)
 *            Выход: указатель на оригинальную функцию (необязательно)
 * 
 * RETURNS / ВОЗВРАЩАЕТ:
 * TRUE on success, FALSE on failure
 * TRUE при успехе, FALSE при неудаче
 * 
 * ALGORITHM / АЛГОРИТМ:
 * 1. Validate PE headers (DOS + NT)
 * 2. Locate import directory in PE
 * 3. Scan import descriptors for matching DLL
 * 4. For each import from that DLL:
 *    - Check if function name matches
 *    - If match: modify IAT entry with VirtualProtect
 *    - Store old pointer if requested
 * 5. Restore memory protection
 * 
 * 1. Проверить PE заголовки (DOS + NT)
 * 2. Найти каталог импорта в PE
 * 3. Сканировать дескрипторы импорта для совпадения DLL
 * 4. Для каждого импорта из этой DLL:
 *    - Проверить совпадение имени функции
 *    - При совпадении: изменить запись IAT с VirtualProtect
 *    - Сохранить старый указатель, если запрошено
 * 5. Восстановить защиту памяти
 * 
 * CRITICAL / КРИТИЧНО:
 * Must use VirtualProtect to make IAT writable before modification.
 * IAT is normally in read-only section of PE.
 * 
 * Должен использовать VirtualProtect для записи в IAT перед изменением.
 * IAT обычно находится в read-only секции PE.
 ******************************************************************************/
static BOOL PatchIAT(HMODULE hModule, const char* dllName, const char* funcName, 
                     void* newFunc, void** oldFunc)
{
    if (!hModule || !dllName || !funcName || !newFunc) return FALSE;

    // Validate DOS header / Проверить DOS заголовок
    PIMAGE_DOS_HEADER pDosHeader = (PIMAGE_DOS_HEADER)hModule;
    if (pDosHeader->e_magic != IMAGE_DOS_SIGNATURE) return FALSE;

    // Validate NT headers / Проверить NT заголовки
    PIMAGE_NT_HEADERS pNtHeaders = (PIMAGE_NT_HEADERS)((BYTE*)hModule + pDosHeader->e_lfanew);
    if (pNtHeaders->Signature != IMAGE_NT_SIGNATURE) return FALSE;

    // Get import directory / Получить каталог импорта
    DWORD importDirRVA = pNtHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress;
    if (importDirRVA == 0) return FALSE;

    PIMAGE_IMPORT_DESCRIPTOR pImportDesc = (PIMAGE_IMPORT_DESCRIPTOR)((BYTE*)hModule + importDirRVA);

    // Scan import descriptors / Сканировать дескрипторы импорта
    while (pImportDesc->Name != 0) {
        const char* modName = (const char*)((BYTE*)hModule + pImportDesc->Name);

        // Check if this is the target DLL / Проверить, является ли это целевой DLL
        if (lstrcmpiA(modName, dllName) == 0) {
            // OriginalFirstThunk: points to Import Name Table (INT)
            // FirstThunk: points to Import Address Table (IAT)
            // OriginalFirstThunk: указывает на таблицу имён импорта (INT)
            // FirstThunk: указывает на таблицу адресов импорта (IAT)
            
            PIMAGE_THUNK_DATA pOrigThunk = (PIMAGE_THUNK_DATA)((BYTE*)hModule + pImportDesc->OriginalFirstThunk);
            PIMAGE_THUNK_DATA pThunk = (PIMAGE_THUNK_DATA)((BYTE*)hModule + pImportDesc->FirstThunk);

            // Scan imports from this DLL / Сканировать импорты из этой DLL
            while (pOrigThunk->u1.AddressOfData != 0) {
                // Check if import by name (not by ordinal)
                // Проверить, импорт по имени (не по ординалу)
                if (!(pOrigThunk->u1.Ordinal & IMAGE_ORDINAL_FLAG)) {
                    PIMAGE_IMPORT_BY_NAME pImportByName = 
                        (PIMAGE_IMPORT_BY_NAME)((BYTE*)hModule + pOrigThunk->u1.AddressOfData);

                    // Check if function name matches / Проверить совпадение имени функции
                    if (lstrcmpA((const char*)pImportByName->Name, funcName) == 0) {
                        DWORD oldProtect;

                        // Make IAT writable / Сделать IAT записываемой
                        if (VirtualProtect(&pThunk->u1.Function, sizeof(void*), 
                                         PAGE_READWRITE, &oldProtect)) {
                            // Save original function pointer / Сохранить оригинальный указатель функции
                            if (oldFunc) {
                                *oldFunc = (void*)pThunk->u1.Function;
                            }

                            // Patch IAT entry / Пропатчить запись IAT
                            pThunk->u1.Function = (DWORD)newFunc;

                            // Restore original protection / Восстановить оригинальную защиту
                            VirtualProtect(&pThunk->u1.Function, sizeof(void*), 
                                         oldProtect, &oldProtect);
                            return TRUE;
                        }
                    }
                }

                pOrigThunk++;
                pThunk++;
            }
        }

        pImportDesc++;
    }

    return FALSE;
}

/*******************************************************************************
 * MODULE STATE
 * СОСТОЯНИЕ МОДУЛЯ
 ******************************************************************************/

static HMODULE g_hGenML = NULL;  // Handle to gen_ml.dll / Дескриптор gen_ml.dll
static BOOL g_Hooked = FALSE;    // TRUE if hooks installed / TRUE если хуки установлены

// Original function pointers / Оригинальные указатели функций
static void* g_Orig_stricmp = NULL;
static void* g_Orig_strnicmp = NULL;
static void* g_Orig_strstr = NULL;

// Timer for delayed initialization / Таймер для отложенной инициализации
static UINT_PTR g_InitTimerID = 0;

/*******************************************************************************
 * TIMER-BASED INITIALIZATION
 * ИНИЦИАЛИЗАЦИЯ НА ОСНОВЕ ТАЙМЕРА
 ******************************************************************************/

/*******************************************************************************
 * InitTimerProc
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Timer callback that waits for gen_ml.dll to load, then installs IAT hooks.
 * 
 * Функция обратного вызова таймера, которая ждёт загрузки gen_ml.dll, затем
 * устанавливает хуки IAT.
 * 
 * WHY NEEDED / ЗАЧЕМ НУЖНО:
 * gen_ml.dll (Media Library plugin) may not be loaded when our plugin initializes.
 * We use a timer to poll for its presence and hook when available.
 * 
 * gen_ml.dll (плагин библиотеки) может быть не загружен, когда наш плагин
 * инициализируется. Используем таймер для опроса его наличия и хука при доступности.
 * 
 * PROCESS / ПРОЦЕСС:
 * 1. Kill timer (will be restarted if needed)
 * 2. Check if already hooked (idempotent)
 * 3. Try to get gen_ml.dll module handle
 * 4. If not found: restart timer and wait
 * 5. If found: patch IAT for all three functions
 * 6. Mark as hooked
 * 
 * 1. Убить таймер (будет перезапущен при необходимости)
 * 2. Проверить, уже захучено (идемпотентно)
 * 3. Попытаться получить дескриптор модуля gen_ml.dll
 * 4. Если не найдено: перезапустить таймер и ждать
 * 5. Если найдено: пропатчить IAT для всех трёх функций
 * 6. Отметить как захученное
 ******************************************************************************/
static VOID CALLBACK InitTimerProc(HWND hwnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime)
{
    (void)hwnd;
    (void)uMsg;
    (void)idEvent;
    (void)dwTime;

    // Kill current timer / Убить текущий таймер
    if (g_InitTimerID) {
        KillTimer(NULL, g_InitTimerID);
        g_InitTimerID = 0;
    }

    // Already hooked? / Уже захучено?
    if (g_Hooked) return;

    // Try to get gen_ml.dll / Попытаться получить gen_ml.dll
    g_hGenML = GetModuleHandleA(TARGET_MODULE);
    if (!g_hGenML) {
        // Not loaded yet, restart timer / Ещё не загружен, перезапустить таймер
        g_InitTimerID = SetTimer(NULL, 0, 250, InitTimerProc);  // Poll every 250ms / Опрашивать каждые 250мс
        return;
    }

    // Patch IAT for all three functions / Пропатчить IAT для всех трёх функций
    PatchIAT(g_hGenML, "MSVCRT.dll", "_stricmp", (void*)My_stricmp, &g_Orig_stricmp);
    PatchIAT(g_hGenML, "MSVCRT.dll", "_strnicmp", (void*)My_strnicmp, &g_Orig_strnicmp);
    PatchIAT(g_hGenML, "MSVCRT.dll", "strstr", (void*)My_strstr, &g_Orig_strstr);

    g_Hooked = TRUE;
}

/*******************************************************************************
 * PUBLIC API
 * ПУБЛИЧНЫЙ API
 ******************************************************************************/

/*******************************************************************************
 * ML_CyrSearchFix_Init
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Initializes the Cyrillic search fix module. Starts timer-based waiting
 * for gen_ml.dll to load.
 * 
 * Инициализирует модуль исправления поиска кириллицы. Запускает ожидание
 * на основе таймера для загрузки gen_ml.dll.
 * 
 * RETURNS / ВОЗВРАЩАЕТ:
 * TRUE on success, FALSE on failure
 * TRUE при успехе, FALSE при неудаче
 * 
 * NOTES / ПРИМЕЧАНИЯ:
 * Safe to call multiple times (idempotent).
 * Безопасно вызывать несколько раз (идемпотентно).
 ******************************************************************************/
BOOL ML_CyrSearchFix_Init(void)
{
    // Already initialized? / Уже инициализировано?
    if (g_Hooked) return TRUE;
    if (g_InitTimerID) return TRUE;

    // Start timer / Запустить таймер
    g_InitTimerID = SetTimer(NULL, 0, 250, InitTimerProc);
    return (g_InitTimerID != 0);
}

/*******************************************************************************
 * ML_CyrSearchFix_Quit
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Cleans up the Cyrillic search fix module. Stops timer and restores
 * original IAT entries.
 * 
 * Очищает модуль исправления поиска кириллицы. Останавливает таймер и
 * восстанавливает оригинальные записи IAT.
 * 
 * CRITICAL / КРИТИЧНО:
 * Must restore original function pointers before module unload to prevent
 * crashes from dangling pointers.
 * 
 * Должен восстановить оригинальные указатели функций перед выгрузкой модуля
 * для предотвращения крашей от висячих указателей.
 ******************************************************************************/
void ML_CyrSearchFix_Quit(void)
{
    // Stop timer if running / Остановить таймер, если запущен
    if (g_InitTimerID) {
        KillTimer(NULL, g_InitTimerID);
        g_InitTimerID = 0;
    }

    if (!g_Hooked || !g_hGenML) return;

    // Restore original IAT entries / Восстановить оригинальные записи IAT
    if (g_Orig_stricmp) {
        PatchIAT(g_hGenML, "MSVCRT.dll", "_stricmp", g_Orig_stricmp, NULL);
    }
    if (g_Orig_strnicmp) {
        PatchIAT(g_hGenML, "MSVCRT.dll", "_strnicmp", g_Orig_strnicmp, NULL);
    }
    if (g_Orig_strstr) {
        PatchIAT(g_hGenML, "MSVCRT.dll", "strstr", g_Orig_strstr, NULL);
    }

    g_hGenML = NULL;
    g_Hooked = FALSE;
}