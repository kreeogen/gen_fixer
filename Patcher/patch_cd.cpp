/*******************************************************************************
 * patch_cd.cpp
 * 
 * MEDIA LIBRARY CD RIPPING REMOVAL PATCH
 * ПАТЧ УДАЛЕНИЯ РИППИНГА CD ИЗ БИБЛИОТЕКИ
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Removes CD ripping functionality from Winamp's Media Library (gen_ml.dll).
 * Patches a single byte to disable CD ripping feature initialization.
 * 
 * Удаляет функциональность риппинга CD из библиотеки Winamp (gen_ml.dll).
 * Патчит один байт для отключения инициализации функции риппинга CD.
 * 
 * HOW IT WORKS / КАК ЭТО РАБОТАЕТ:
 * 1. Hooks LoadLibrary* functions in winamp.exe's Import Address Table
 * 2. Monitors when gen_ml.dll is loaded
 * 3. When gen_ml.dll loads, patches single byte at RVA 0x11DB2
 * 4. Patch changes 0xF4 -> 0x90 (HLT instruction -> NOP)
 * 5. This prevents CD ripping module initialization
 * 
 * 1. Перехватывает функции LoadLibrary* в таблице импорта winamp.exe
 * 2. Отслеживает, когда загружается gen_ml.dll
 * 3. Когда gen_ml.dll загружается, патчит один байт по RVA 0x11DB2
 * 4. Патч изменяет 0xF4 -> 0x90 (инструкция HLT -> NOP)
 * 5. Это предотвращает инициализацию модуля риппинга CD
 * 
 * TECHNICAL DETAILS / ТЕХНИЧЕСКИЕ ДЕТАЛИ:
 * 
 * PATCH LOCATION / РАСПОЛОЖЕНИЕ ПАТЧА:
 * - Target DLL: gen_ml.dll (Winamp Media Library)
 * - Relative Virtual Address (RVA): 0x11DB2
 * - Original byte: 0xF4 (HLT - halt instruction)
 * - Patched byte: 0x90 (NOP - no operation)
 * 
 * - Целевая DLL: gen_ml.dll (Библиотека Winamp)
 * - Относительный виртуальный адрес (RVA): 0x11DB2
 * - Оригинальный байт: 0xF4 (HLT - инструкция остановки)
 * - Пропатченный байт: 0x90 (NOP - нет операции)
 * 
 * WHY THIS WORKS / ПОЧЕМУ ЭТО РАБОТАЕТ:
 * The byte at 0x11DB2 is likely part of CD ripping initialization code.
 * Changing HLT to NOP prevents critical initialization from completing.
 * CD ripping feature becomes unavailable in Media Library UI.
 * 
 * Байт по адресу 0x11DB2 вероятно часть кода инициализации риппинга CD.
 * Изменение HLT на NOP предотвращает завершение критической инициализации.
 * Функция риппинга CD становится недоступной в UI библиотеки.
 * 
 * TIMING / ВРЕМЕННАЯ ПРИВЯЗКА:
 * Patch must be applied BEFORE gen_ml.dll initializes its features.
 * LoadLibrary hooks ensure we patch immediately after DLL load.
 * 
 * Патч должен быть применён ДО инициализации функций gen_ml.dll.
 * Хуки LoadLibrary гарантируют, что мы патчим сразу после загрузки DLL.
 * 
 * COMPATIBILITY / СОВМЕСТИМОСТЬ:
 * - Winamp 2.95 with gen_ml.dll
 * - Windows 98 through Windows 11
 * - ANSI and Unicode builds
 * 
 * - Winamp 2.95 с gen_ml.dll
 * - Windows 98 до Windows 11
 * - ANSI и Unicode сборки
 * 
 * SAFETY / БЕЗОПАСНОСТЬ:
 * - Only patches if byte matches expected value (0xF4)
 * - One-time patch prevents multiple applications
 * - Does not modify winamp.exe itself
 * 
 * - Патчит только если байт соответствует ожидаемому значению (0xF4)
 * - Однократный патч предотвращает множественные применения
 * - Не изменяет сам winamp.exe
 * 
 ******************************************************************************/

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <string.h>
#include "patcher_core.h"

/*******************************************************************************
 * UTILITY MACROS
 * УТИЛИТАРНЫЕ МАКРОСЫ
 ******************************************************************************/

// Safe array size calculation / Безопасное вычисление размера массива
#define ARRAYSIZE_(a) ((int)(sizeof(a) / sizeof((a)[0])))

/*******************************************************************************
 * PATCH DEFINITION
 * ОПРЕДЕЛЕНИЕ ПАТЧА
 * 
 * Single-byte patch to disable CD ripping in gen_ml.dll.
 * Однобайтовый патч для отключения риппинга CD в gen_ml.dll.
 * 
 * PATCH STRUCTURE / СТРУКТУРА ПАТЧА:
 * Each PatchByte entry contains:
 * - rva: Relative Virtual Address from module base / Относительный виртуальный адрес от базы модуля
 * - expected: Expected original byte value / Ожидаемое оригинальное значение байта
 * - patch: New byte value to write / Новое значение байта для записи
 * 
 * PATCH SAFETY / БЕЗОПАСНОСТЬ ПАТЧА:
 * If expected byte doesn't match, patch is NOT applied.
 * This prevents corrupting wrong code or wrong DLL version.
 * 
 * Если ожидаемый байт не совпадает, патч НЕ применяется.
 * Это предотвращает повреждение неправильного кода или неправильной версии DLL.
 ******************************************************************************/
static const PatchByte g_PatchCDRva[] =
{
    /* RVA       Expected  Patch  Description / Описание */
    { 0x00011DB2,  0xF4,    0x90 },  // HLT -> NOP at CD ripping init / HLT -> NOP при инициализации риппинга CD
};

/*******************************************************************************
 * IAT PATCHING HELPER
 * ВСПОМОГАТЕЛЬНАЯ ФУНКЦИЯ ПАТЧИНГА IAT
 ******************************************************************************/

/*******************************************************************************
 * IAT_Patch
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Patches Import Address Table (IAT) to redirect function calls.
 * Used to hook LoadLibrary* functions in winamp.exe.
 * 
 * Патчит таблицу адресов импорта (IAT) для перенаправления вызовов функций.
 * Используется для перехвата функций LoadLibrary* в winamp.exe.
 * 
 * PARAMETERS / ПАРАМЕТРЫ:
 * hMod    - Module handle to patch / Дескриптор модуля для патча
 * dll     - DLL name containing function to hook / Имя DLL содержащей функцию для перехвата
 * func    - Function name to hook / Имя функции для перехвата
 * newFunc - New function pointer (hook) / Новый указатель функции (хук)
 * oldFunc - Output: original function pointer / Вывод: оригинальный указатель функции
 * 
 * RETURNS / ВОЗВРАЩАЕТ:
 * TRUE if patch successful, FALSE otherwise
 * TRUE если патч успешен, FALSE иначе
 * 
 * ALGORITHM / АЛГОРИТМ:
 * 1. Validate PE structure (DOS header, NT header)
 * 2. Locate import descriptor for specified DLL
 * 3. Find import thunk for specified function name
 * 4. Change memory protection to allow writes
 * 5. Replace function pointer in IAT
 * 6. Restore original memory protection
 * 
 * 1. Проверить PE структуру (DOS заголовок, NT заголовок)
 * 2. Найти дескриптор импорта для указанной DLL
 * 3. Найти thunk импорта для указанного имени функции
 * 4. Изменить защиту памяти для разрешения записи
 * 5. Заменить указатель функции в IAT
 * 6. Восстановить оригинальную защиту памяти
 * 
 * NOTES / ПРИМЕЧАНИЯ:
 * This is standard IAT hooking technique.
 * Safe for all Windows versions (98 through 11).
 * 
 * Это стандартная техника перехвата IAT.
 * Безопасна для всех версий Windows (98 до 11).
 ******************************************************************************/
static BOOL IAT_Patch(HMODULE hMod, const char* dll, const char* func, void* newFunc, void** oldFunc)
{
    if (!hMod || !dll || !func || !newFunc) return FALSE;

    // Validate DOS header / Проверить DOS заголовок
    PIMAGE_DOS_HEADER dos = (PIMAGE_DOS_HEADER)hMod;
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) return FALSE;

    // Validate NT header / Проверить NT заголовок
    PIMAGE_NT_HEADERS nt = (PIMAGE_NT_HEADERS)((BYTE*)hMod + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) return FALSE;

    // Get import directory / Получить каталог импорта
    DWORD va = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress;
    if (!va) return FALSE;

    // Iterate through import descriptors / Перебрать дескрипторы импорта
    PIMAGE_IMPORT_DESCRIPTOR imp = (PIMAGE_IMPORT_DESCRIPTOR)((BYTE*)hMod + va);
    for (; imp->Name; imp++)
    {
        // Check if this is the target DLL / Проверить, это ли целевая DLL
        const char* name = (const char*)((BYTE*)hMod + imp->Name);
        if (lstrcmpiA(name, dll) != 0) continue;

        // Get thunk arrays / Получить массивы thunk
        PIMAGE_THUNK_DATA oft = (PIMAGE_THUNK_DATA)((BYTE*)hMod + imp->OriginalFirstThunk);
        PIMAGE_THUNK_DATA ft  = (PIMAGE_THUNK_DATA)((BYTE*)hMod + imp->FirstThunk);

        // Iterate through imports / Перебрать импорты
        for (; oft->u1.Function; oft++, ft++)
        {
            // Skip ordinal imports / Пропустить импорты по порядковому номеру
            if (IMAGE_SNAP_BY_ORDINAL(oft->u1.Ordinal)) continue;

            // Get import name / Получить имя импорта
            PIMAGE_IMPORT_BY_NAME ibn = (PIMAGE_IMPORT_BY_NAME)((BYTE*)hMod + oft->u1.AddressOfData);
            if (lstrcmpiA((char*)ibn->Name, func) != 0) continue;

            // Found target function - patch it / Найдена целевая функция - пропатчить её
            DWORD old;
            VirtualProtect(&ft->u1.Function, 4, PAGE_READWRITE, &old);  // Make writable / Сделать доступной для записи
            if (oldFunc) *oldFunc = (void*)ft->u1.Function;              // Save original / Сохранить оригинал
            ft->u1.Function = (DWORD)newFunc;                            // Install hook / Установить хук
            VirtualProtect(&ft->u1.Function, 4, old, &old);              // Restore protection / Восстановить защиту
            return TRUE;
        }
    }
    return FALSE;
}

/*******************************************************************************
 * LOADLIBRARY HOOK INFRASTRUCTURE
 * ИНФРАСТРУКТУРА ХУКОВ LOADLIBRARY
 ******************************************************************************/

// Function pointer types for hooked functions
// Типы указателей функций для перехваченных функций
typedef HMODULE (WINAPI *PFN_LoadLibraryA)(LPCSTR);
typedef HMODULE (WINAPI *PFN_LoadLibraryW)(LPCWSTR);
typedef HMODULE (WINAPI *PFN_LoadLibraryExA)(LPCSTR, HANDLE, DWORD);
typedef HMODULE (WINAPI *PFN_LoadLibraryExW)(LPCWSTR, HANDLE, DWORD);

// Real function pointers (updated during IAT patching)
// Реальные указатели функций (обновляются во время патчинга IAT)
static PFN_LoadLibraryA   Real_LoadLibraryA   = NULL;
static PFN_LoadLibraryW   Real_LoadLibraryW   = NULL;
static PFN_LoadLibraryExA Real_LoadLibraryExA = NULL;
static PFN_LoadLibraryExW Real_LoadLibraryExW = NULL;

/*******************************************************************************
 * STATE FLAGS
 * ФЛАГИ СОСТОЯНИЯ
 * 
 * Thread-safe flags using interlocked operations.
 * Потокобезопасные флаги используя interlocked операции.
 ******************************************************************************/

// Flag: TRUE if gen_ml.dll has been patched
// Флаг: TRUE если gen_ml.dll был пропатчен
static volatile LONG g_genmlPatched = 0;

// Flag: TRUE if LoadLibrary hooks have been installed
// Флаг: TRUE если хуки LoadLibrary были установлены
static volatile LONG g_hookInstalled = 0;

/*******************************************************************************
 * FILENAME DETECTION HELPERS
 * ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ ОПРЕДЕЛЕНИЯ ИМЕНИ ФАЙЛА
 ******************************************************************************/

/*******************************************************************************
 * is_genml_a
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Checks if ANSI path ends with "gen_ml.dll" (case-insensitive).
 * Проверяет, заканчивается ли ANSI путь на "gen_ml.dll" (без учёта регистра).
 * 
 * PARAMETERS / ПАРАМЕТРЫ:
 * s - ANSI file path / ANSI путь к файлу
 * 
 * RETURNS / ВОЗВРАЩАЕТ:
 * 1 if filename is gen_ml.dll, 0 otherwise
 * 1 если имя файла gen_ml.dll, 0 иначе
 * 
 * ALGORITHM / АЛГОРИТМ:
 * 1. Find last path separator (\ or /)
 * 2. Extract filename portion
 * 3. Compare case-insensitively with "gen_ml.dll"
 * 
 * 1. Найти последний разделитель пути (\ или /)
 * 2. Извлечь часть с именем файла
 * 3. Сравнить без учёта регистра с "gen_ml.dll"
 ******************************************************************************/
static int is_genml_a(LPCSTR s)
{
    if (!s) return 0;
    
    // Find last path separator / Найти последний разделитель пути
    const char* p = s;
    const char* last = s;
    while (*p) {
        if (*p == '\\' || *p == '/') last = p + 1;
        p++;
    }
    
    // Compare filename / Сравнить имя файла
    return (lstrcmpiA(last, "gen_ml.dll") == 0);
}

/*******************************************************************************
 * is_genml_w
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Checks if Unicode path ends with "gen_ml.dll" (case-insensitive).
 * Проверяет, заканчивается ли Unicode путь на "gen_ml.dll" (без учёта регистра).
 * 
 * PARAMETERS / ПАРАМЕТРЫ:
 * s - Unicode file path / Unicode путь к файлу
 * 
 * RETURNS / ВОЗВРАЩАЕТ:
 * 1 if filename is gen_ml.dll, 0 otherwise
 * 1 если имя файла gen_ml.dll, 0 иначе
 * 
 * ALGORITHM / АЛГОРИТМ:
 * Same as is_genml_a but for Unicode strings.
 * То же что is_genml_a но для Unicode строк.
 ******************************************************************************/
static int is_genml_w(LPCWSTR s)
{
    if (!s) return 0;
    
    // Find last path separator / Найти последний разделитель пути
    const wchar_t* p = s;
    const wchar_t* last = s;
    while (*p) {
        if (*p == L'\\' || *p == L'/') last = p + 1;
        p++;
    }
    
    // Compare filename / Сравнить имя файла
    return (lstrcmpiW(last, L"gen_ml.dll") == 0);
}

/*******************************************************************************
 * PATCHING LOGIC
 * ЛОГИКА ПАТЧИНГА
 ******************************************************************************/

/*******************************************************************************
 * patch_genml_if_needed
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Applies CD ripping removal patch to gen_ml.dll if not already patched.
 * Uses atomic flag to ensure one-time patching.
 * 
 * Применяет патч удаления риппинга CD к gen_ml.dll если ещё не пропатчен.
 * Использует атомарный флаг для обеспечения однократного патчинга.
 * 
 * PARAMETERS / ПАРАМЕТРЫ:
 * h - Module handle to gen_ml.dll / Дескриптор модуля gen_ml.dll
 * 
 * ALGORITHM / АЛГОРИТМ:
 * 1. Validate module handle
 * 2. Check if already patched using interlocked compare-exchange
 * 3. If not patched, apply byte patch from g_PatchCDRva table
 * 4. Set patched flag to prevent re-patching
 * 
 * 1. Проверить дескриптор модуля
 * 2. Проверить, уже ли пропатчен используя interlocked compare-exchange
 * 3. Если не пропатчен, применить байтовый патч из таблицы g_PatchCDRva
 * 4. Установить флаг патчинга для предотвращения повторного патчинга
 * 
 * NOTES / ПРИМЕЧАНИЯ:
 * Thread-safe. Multiple threads calling this will only patch once.
 * Uses patcher_apply_table_base from patcher_core.h.
 * 
 * Потокобезопасна. Множественные потоки вызывающие это пропатчат только один раз.
 * Использует patcher_apply_table_base из patcher_core.h.
 ******************************************************************************/
static void patch_genml_if_needed(HMODULE h)
{
    if (!h) return;
    
    // Atomic check: only first thread proceeds / Атомарная проверка: только первый поток продолжает
    if (InterlockedCompareExchange(&g_genmlPatched, 1, 0) != 0)
        return;  // Already patched by another thread / Уже пропатчен другим потоком

    // Apply patch table to gen_ml.dll base address
    // Применить таблицу патчей к базовому адресу gen_ml.dll
    patcher_apply_table_base((BYTE*)h, g_PatchCDRva, ARRAYSIZE_(g_PatchCDRva));
}

/*******************************************************************************
 * LOADLIBRARY HOOK FUNCTIONS
 * ФУНКЦИИ ХУКОВ LOADLIBRARY
 * 
 * These functions intercept LoadLibrary* calls from winamp.exe.
 * When gen_ml.dll is loaded, they apply the CD ripping patch.
 * 
 * Эти функции перехватывают вызовы LoadLibrary* из winamp.exe.
 * Когда gen_ml.dll загружается, они применяют патч риппинга CD.
 ******************************************************************************/

/*******************************************************************************
 * Hook_LoadLibraryA
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Hook for LoadLibraryA. Patches gen_ml.dll when it's loaded.
 * Хук для LoadLibraryA. Патчит gen_ml.dll когда он загружается.
 * 
 * PARAMETERS / ПАРАМЕТРЫ:
 * lpFile - ANSI path to DLL to load / ANSI путь к DLL для загрузки
 * 
 * RETURNS / ВОЗВРАЩАЕТ:
 * Module handle or NULL on failure
 * Дескриптор модуля или NULL при ошибке
 * 
 * ALGORITHM / АЛГОРИТМ:
 * 1. Call real LoadLibraryA
 * 2. If successful and DLL is gen_ml.dll, apply patch
 * 3. Return module handle
 * 
 * 1. Вызвать реальную LoadLibraryA
 * 2. Если успешно и DLL - gen_ml.dll, применить патч
 * 3. Вернуть дескриптор модуля
 ******************************************************************************/
static HMODULE WINAPI Hook_LoadLibraryA(LPCSTR lpFile)
{
    // Call real function / Вызвать реальную функцию
    HMODULE h = Real_LoadLibraryA ? Real_LoadLibraryA(lpFile) : NULL;
    
    // If gen_ml.dll loaded, patch it / Если gen_ml.dll загружен, пропатчить его
    if (h && is_genml_a(lpFile)) {
        patch_genml_if_needed(h);
    }
    
    return h;
}

/*******************************************************************************
 * Hook_LoadLibraryW
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Hook for LoadLibraryW. Unicode version of Hook_LoadLibraryA.
 * Хук для LoadLibraryW. Unicode версия Hook_LoadLibraryA.
 ******************************************************************************/
static HMODULE WINAPI Hook_LoadLibraryW(LPCWSTR lpFile)
{
    HMODULE h = Real_LoadLibraryW ? Real_LoadLibraryW(lpFile) : NULL;
    if (h && is_genml_w(lpFile)) {
        patch_genml_if_needed(h);
    }
    return h;
}

/*******************************************************************************
 * Hook_LoadLibraryExA
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Hook for LoadLibraryExA. Extended version with flags support.
 * Хук для LoadLibraryExA. Расширенная версия с поддержкой флагов.
 ******************************************************************************/
static HMODULE WINAPI Hook_LoadLibraryExA(LPCSTR lpFile, HANDLE hf, DWORD fl)
{
    HMODULE h = Real_LoadLibraryExA ? Real_LoadLibraryExA(lpFile, hf, fl) : NULL;
    if (h && is_genml_a(lpFile)) {
        patch_genml_if_needed(h);
    }
    return h;
}

/*******************************************************************************
 * Hook_LoadLibraryExW
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Hook for LoadLibraryExW. Extended Unicode version.
 * Хук для LoadLibraryExW. Расширенная Unicode версия.
 ******************************************************************************/
static HMODULE WINAPI Hook_LoadLibraryExW(LPCWSTR lpFile, HANDLE hf, DWORD fl)
{
    HMODULE h = Real_LoadLibraryExW ? Real_LoadLibraryExW(lpFile, hf, fl) : NULL;
    if (h && is_genml_w(lpFile)) {
        patch_genml_if_needed(h);
    }
    return h;
}

/*******************************************************************************
 * HOOK INSTALLATION
 * УСТАНОВКА ХУКОВ
 ******************************************************************************/

/*******************************************************************************
 * install_hook_once
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Installs LoadLibrary hooks in winamp.exe (one-time operation).
 * Also patches gen_ml.dll immediately if it's already loaded.
 * 
 * Устанавливает хуки LoadLibrary в winamp.exe (однократная операция).
 * Также немедленно патчит gen_ml.dll если он уже загружен.
 * 
 * ALGORITHM / АЛГОРИТМ:
 * 1. Atomic check: ensure one-time installation
 * 2. Check if gen_ml.dll already loaded - patch immediately if so
 * 3. Get winamp.exe and kernel32.dll module handles
 * 4. Get real addresses of LoadLibrary* functions
 * 5. Patch winamp.exe's IAT to install hooks for all LoadLibrary variants
 * 
 * 1. Атомарная проверка: обеспечить однократную установку
 * 2. Проверить, загружен ли уже gen_ml.dll - пропатчить немедленно если да
 * 3. Получить дескрипторы модулей winamp.exe и kernel32.dll
 * 4. Получить реальные адреса функций LoadLibrary*
 * 5. Пропатчить IAT winamp.exe для установки хуков для всех вариантов LoadLibrary
 * 
 * WHY HOOK ALL VARIANTS / ПОЧЕМУ ХУКАЕМ ВСЕ ВАРИАНТЫ:
 * Winamp might use any of these functions to load plugins.
 * Hooking all variants ensures we catch gen_ml.dll load.
 * 
 * Winamp может использовать любую из этих функций для загрузки плагинов.
 * Перехват всех вариантов гарантирует, что мы поймаем загрузку gen_ml.dll.
 * 
 * NOTES / ПРИМЕЧАНИЯ:
 * Thread-safe. Multiple threads calling this will only install once.
 * 
 * Потокобезопасна. Множественные потоки вызывающие это установят только один раз.
 ******************************************************************************/
static void install_hook_once(void)
{
    // Atomic check: only first thread proceeds / Атомарная проверка: только первый поток продолжает
    if (InterlockedCompareExchange(&g_hookInstalled, 1, 0) != 0)
        return;  // Already installed / Уже установлено

    /***************************************************************************
     * STEP 1: PATCH IF ALREADY LOADED
     * ШАГ 1: ПРОПАТЧИТЬ ЕСЛИ УЖЕ ЗАГРУЖЕН
     * 
     * If gen_ml.dll is already loaded (rare but possible), patch it now.
     * Если gen_ml.dll уже загружен (редко но возможно), пропатчить его сейчас.
     ***************************************************************************/
    HMODULE hML = GetModuleHandleA("gen_ml.dll");
    if (hML) {
        patch_genml_if_needed(hML);
    }

    /***************************************************************************
     * STEP 2: GET MODULE HANDLES
     * ШАГ 2: ПОЛУЧИТЬ ДЕСКРИПТОРЫ МОДУЛЕЙ
     ***************************************************************************/
    HMODULE hExe = GetModuleHandleA(NULL);         // winamp.exe
    HMODULE hK32 = GetModuleHandleA("KERNEL32.dll");
    if (!hExe || !hK32) return;  // Critical modules not found / Критичные модули не найдены

    /***************************************************************************
     * STEP 3: GET REAL FUNCTION ADDRESSES
     * ШАГ 3: ПОЛУЧИТЬ РЕАЛЬНЫЕ АДРЕСА ФУНКЦИЙ
     * 
     * Get original kernel32 function addresses.
     * These will be called from our hooks.
     * 
     * Получить оригинальные адреса функций kernel32.
     * Они будут вызываться из наших хуков.
     ***************************************************************************/
    if (!Real_LoadLibraryA)   Real_LoadLibraryA   = (PFN_LoadLibraryA)GetProcAddress(hK32, "LoadLibraryA");
    if (!Real_LoadLibraryW)   Real_LoadLibraryW   = (PFN_LoadLibraryW)GetProcAddress(hK32, "LoadLibraryW");
    if (!Real_LoadLibraryExA) Real_LoadLibraryExA = (PFN_LoadLibraryExA)GetProcAddress(hK32, "LoadLibraryExA");
    if (!Real_LoadLibraryExW) Real_LoadLibraryExW = (PFN_LoadLibraryExW)GetProcAddress(hK32, "LoadLibraryExW");

    /***************************************************************************
     * STEP 4: INSTALL IAT HOOKS
     * ШАГ 4: УСТАНОВИТЬ ХУКИ IAT
     * 
     * Patch winamp.exe's Import Address Table to redirect LoadLibrary* calls.
     * Пропатчить таблицу адресов импорта winamp.exe для перенаправления вызовов LoadLibrary*.
     ***************************************************************************/
    IAT_Patch(hExe, "KERNEL32.dll", "LoadLibraryA",   (void*)Hook_LoadLibraryA,   (void**)&Real_LoadLibraryA);
    IAT_Patch(hExe, "KERNEL32.dll", "LoadLibraryW",   (void*)Hook_LoadLibraryW,   (void**)&Real_LoadLibraryW);
    IAT_Patch(hExe, "KERNEL32.dll", "LoadLibraryExA", (void*)Hook_LoadLibraryExA, (void**)&Real_LoadLibraryExA);
    IAT_Patch(hExe, "KERNEL32.dll", "LoadLibraryExW", (void*)Hook_LoadLibraryExW, (void**)&Real_LoadLibraryExW);
}

/*******************************************************************************
 * PUBLIC API
 * ПУБЛИЧНЫЙ API
 ******************************************************************************/

/*******************************************************************************
 * patch_cd_init
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Initializes CD ripping removal patch.
 * Installs LoadLibrary hooks to catch gen_ml.dll loading.
 * 
 * Инициализирует патч удаления риппинга CD.
 * Устанавливает хуки LoadLibrary для перехвата загрузки gen_ml.dll.
 * 
 * RETURNS / ВОЗВРАЩАЕТ:
 * 1 on success / 1 при успехе
 * 
 * WHEN TO CALL / КОГДА ВЫЗЫВАТЬ:
 * Call during plugin initialization, before Winamp loads gen_ml.dll.
 * Safe to call multiple times - only installs once.
 * 
 * Вызывайте во время инициализации плагина, до загрузки gen_ml.dll в Winamp.
 * Безопасно вызывать много раз - устанавливает только один раз.
 * 
 * HOW IT WORKS / КАК ЭТО РАБОТАЕТ:
 * Sets up hooks one time. Later, when winamp loads gen_ml.dll,
 * we patch the byte BEFORE Media Library init() is called.
 * 
 * Настраивает хуки один раз. Позже, когда winamp загружает gen_ml.dll,
 * мы патчим байт ДО вызова init() библиотеки.
 * 
 * EXAMPLE / ПРИМЕР:
 * ```c
 * int plugin_init() {
 *     patch_cd_init();  // Enable CD ripping removal
 *     return 0;
 * }
 * ```
 ******************************************************************************/
int patch_cd_init(void)
{
    // Install hook one time / Установить хук один раз
    // Later when winamp loads gen_ml.dll, we'll patch the byte BEFORE init()
    // Позже когда winamp загрузит gen_ml.dll, мы пропатчим байт ДО init()
    install_hook_once();
    return 1;
}

/*******************************************************************************
 * patch_cd_quit
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Cleanup function for CD ripping removal patch.
 * Currently does nothing - hooks can remain until process exit.
 * 
 * Функция очистки для патча удаления риппинга CD.
 * В настоящее время ничего не делает - хуки могут оставаться до выхода процесса.
 * 
 * NOTES / ПРИМЕЧАНИЯ:
 * Usually hooks in winamp.exe can remain until process exit.
 * If your project has "unpatch IAT" mechanism, you can add it here.
 * 
 * Обычно хуки в winamp.exe могут оставаться до выхода процесса.
 * Если у вас в проекте есть механизм "unpatch IAT", можно добавить его здесь.
 * 
 * WHY NOT UNHOOK / ПОЧЕМУ НЕ ОТЦЕПЛЯЕМ:
 * Unhooking IAT during runtime is risky and unnecessary.
 * Hooks are benign and use minimal resources.
 * Process exit will clean everything up automatically.
 * 
 * Отцепление IAT во время выполнения рискованно и не нужно.
 * Хуки безвредны и используют минимальные ресурсы.
 * Выход процесса очистит всё автоматически.
 ******************************************************************************/
void patch_cd_quit(void)
{
    // Usually hooks in winamp.exe don't need to be removed until process exit.
    // If you have an "unpatch IAT" mechanism in your project, you can add it here.
    
    // Обычно хуки в winamp.exe не нужно снимать до выхода процесса.
    // Если у тебя в проекте есть механизм "unpatch IAT" — можно добавить.
}
