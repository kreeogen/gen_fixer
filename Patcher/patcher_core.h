/*******************************************************************************
 * patcher_core.h
 * 
 * UNIVERSAL RVA PATCHER CORE API - HEADER
 * УНИВЕРСАЛЬНОЕ ЯДРО RVA ПАТЧЕРА - ЗАГОЛОВОК
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Public API for Win32 PE memory patching engine.
 * Defines all data structures, enums, and functions for safe binary patching.
 * 
 * Публичный API для движка патчинга памяти Win32 PE.
 * Определяет все структуры данных, перечисления и функции для безопасного бинарного патчинга.
 * 
 * FEATURES / ВОЗМОЖНОСТИ:
 * - C/C++ compatible API (extern "C")
 * - RVA-based patching (Relative Virtual Address)
 * - Atomic operations with rollback
 * - Signature verification
 * - Status checking
 * - Comprehensive error reporting
 * 
 * - C/C++ совместимый API (extern "C")
 * - Патчинг на основе RVA (относительный виртуальный адрес)
 * - Атомарные операции с откатом
 * - Проверка сигнатур
 * - Проверка статуса
 * - Комплексная отчётность об ошибках
 * 
 * USAGE / ИСПОЛЬЗОВАНИЕ:
 * 1. Include this header in your patch modules
 * 2. Link with patcher_core.cpp
 * 3. Call patcher_apply_rva_table() to apply patches
 * 4. Call patcher_revert_rva_table() on cleanup
 * 
 * 1. Включить этот заголовок в ваши модули патчей
 * 2. Связать с patcher_core.cpp
 * 3. Вызвать patcher_apply_rva_table() для применения патчей
 * 4. Вызвать patcher_revert_rva_table() при очистке
 * 
 * COMPATIBILITY / СОВМЕСТИМОСТЬ:
 * - Visual Studio 2003+ (VC7.1+)
 * - C and C++ compatible
 * - Windows 98 through Windows 11
 * - x86 architecture
 * 
 * - Visual Studio 2003+ (VC7.1+)
 * - Совместимо с C и C++
 * - Windows 98 до Windows 11
 * - Архитектура x86
 * 
 ******************************************************************************/

#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

/*******************************************************************************
 * EXTERN C LINKAGE
 * ВНЕШНЯЯ СВЯЗЬ C
 * 
 * Ensures C linkage for C++ compatibility.
 * Allows this API to be used from both C and C++ code.
 * 
 * Обеспечивает C связь для совместимости с C++.
 * Позволяет использовать этот API как из C так и из C++ кода.
 ******************************************************************************/
#ifdef __cplusplus
extern "C" {
#endif

/*******************************************************************************
 * CORE DATA TYPES
 * ОСНОВНЫЕ ТИПЫ ДАННЫХ
 ******************************************************************************/

/*******************************************************************************
 * PatchByte
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Describes a single byte patch operation.
 * Core data structure for all patching operations.
 * 
 * Описывает операцию патчинга одного байта.
 * Основная структура данных для всех операций патчинга.
 * 
 * FIELDS / ПОЛЯ:
 * 
 * rva (DWORD) - Relative Virtual Address from module base
 *               Относительный виртуальный адрес от базы модуля
 *               
 *               RVA is offset from module's load address.
 *               For winamp.exe loaded at 0x00400000, RVA 0x1000 = 0x00401000.
 *               
 *               RVA это смещение от адреса загрузки модуля.
 *               Для winamp.exe загруженного по 0x00400000, RVA 0x1000 = 0x00401000.
 * 
 * oldB (BYTE) - Expected byte value before patch
 *               Ожидаемое значение байта до патча
 *               
 *               Used for verification. Patch only applies if current byte matches.
 *               Prevents corrupting wrong code or wrong version.
 *               
 *               Используется для проверки. Патч применяется только если текущий байт совпадает.
 *               Предотвращает повреждение неправильного кода или неправильной версии.
 * 
 * newB (BYTE) - New byte value to write
 *               Новое значение байта для записи
 *               
 *               The patched value. Byte will be changed from oldB to newB.
 *               Пропатченное значение. Байт будет изменён с oldB на newB.
 * 
 * EXAMPLE / ПРИМЕР:
 * ```c
 * // Change byte at RVA 0x1000 from 0x75 (JNZ) to 0xEB (JMP)
 * // Изменить байт по RVA 0x1000 с 0x75 (JNZ) на 0xEB (JMP)
 * PatchByte patch = { 0x00001000, 0x75, 0xEB };
 * ```
 * 
 * USAGE IN TABLES / ИСПОЛЬЗОВАНИЕ В ТАБЛИЦАХ:
 * ```c
 * static const PatchByte g_patches[] = {
 *     { 0x00001000, 0x75, 0xEB },  // Patch 1
 *     { 0x00001005, 0x90, 0xC3 },  // Patch 2
 * };
 * patcher_apply_rva_table(g_patches, 2);
 * ```
 ******************************************************************************/
typedef struct PatchByte {
    DWORD rva;   /* RVA from module base / RVA от базы модуля */
    BYTE  oldB;  /* expected before patch / ожидается до патча */
    BYTE  newB;  /* byte to write / байт для записи */
} PatchByte;

/*******************************************************************************
 * PatchSig
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Optional signature for stronger verification before patching.
 * Validates that a sequence of bytes matches expected pattern.
 * 
 * Опциональная сигнатура для более сильной проверки перед патчингом.
 * Валидирует что последовательность байтов совпадает с ожидаемым паттерном.
 * 
 * FIELDS / ПОЛЯ:
 * 
 * rva (DWORD) - RVA where signature starts
 *               RVA где начинается сигнатура
 * 
 * bytes (const BYTE*) - Expected byte sequence
 *                       Ожидаемая последовательность байтов
 *                       
 *                       Pointer to array of bytes that should match.
 *                       Typically points to static const array.
 *                       
 *                       Указатель на массив байтов которые должны совпадать.
 *                       Обычно указывает на статический const массив.
 * 
 * len (int) - Length of byte sequence
 *             Длина последовательности байтов
 *             
 *             Number of bytes in signature. Typically sizeof(array).
 *             Количество байтов в сигнатуре. Обычно sizeof(массив).
 * 
 * name (const char*) - Optional label for diagnostics
 *                      Опциональная метка для диагностики
 *                      
 *                      May be NULL. Used in error messages to identify which signature failed.
 *                      Может быть NULL. Используется в сообщениях об ошибках для идентификации какая сигнатура не удалась.
 * 
 * WHY USE SIGNATURES / ПОЧЕМУ ИСПОЛЬЗОВАТЬ СИГНАТУРЫ:
 * - Stronger verification than single-byte checks
 * - Ensures correct Winamp version
 * - Detects if code has been modified by other software
 * - Fails fast before any patching occurs
 * 
 * - Более сильная проверка чем проверки одного байта
 * - Гарантирует правильную версию Winamp
 * - Обнаруживает если код был изменён другим ПО
 * - Быстро прерывается до любого патчинга
 * 
 * EXAMPLE / ПРИМЕР:
 * ```c
 * // Signature for function prologue: PUSH EBP; MOV EBP, ESP
 * static const BYTE SIG_FUNC[] = { 0x55, 0x8B, 0xEC };
 * static const PatchSig g_sigs[] = {
 *     { 0x00001000, SIG_FUNC, sizeof(SIG_FUNC), "function_entry" }
 * };
 * 
 * // Verify before patching
 * if (!patcher_verify_sigs(g_sigs, 1)) {
 *     // Wrong Winamp version or modified code!
 * }
 * ```
 ******************************************************************************/
typedef struct PatchSig {
    DWORD       rva;     /* RVA from module base / RVA от базы модуля */
    const BYTE* bytes;   /* expected bytes / ожидаемые байты */
    int         len;     /* bytes length / длина байтов */
    const char* name;    /* label for diagnostics (may be NULL) / метка для диагностики (может быть NULL) */
} PatchSig;

/*******************************************************************************
 * PatcherStatus
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Enum representing current state of patches.
 * Returned by patcher_status_*() functions.
 * 
 * Перечисление представляющее текущее состояние патчей.
 * Возвращается функциями patcher_status_*().
 * 
 * VALUES / ЗНАЧЕНИЯ:
 * 
 * PATCHER_STATUS_ORIGINAL (0)
 *     All bytes match oldB (original values).
 *     No patches have been applied yet.
 *     
 *     Все байты совпадают с oldB (оригинальные значения).
 *     Патчи ещё не применены.
 * 
 * PATCHER_STATUS_PATCHED (1)
 *     All bytes match newB (patched values).
 *     Patches have been successfully applied.
 *     
 *     Все байты совпадают с newB (пропатченные значения).
 *     Патчи успешно применены.
 * 
 * PATCHER_STATUS_MIXED (2)
 *     Mixture of old/new OR unknown bytes.
 *     Possible causes:
 *     - Partial patching (some patches applied, some not)
 *     - Code modified by other software
 *     - Corruption
 *     
 *     Смесь старых/новых ИЛИ неизвестных байтов.
 *     Возможные причины:
 *     - Частичный патчинг (некоторые патчи применены, некоторые нет)
 *     - Код изменён другим ПО
 *     - Повреждение
 * 
 * PATCHER_STATUS_OOB (3)
 *     At least one RVA out of image bounds.
 *     Invalid patch table or wrong module.
 *     
 *     Хотя бы один RVA за границами образа.
 *     Недопустимая таблица патчей или неправильный модуль.
 * 
 * USAGE / ИСПОЛЬЗОВАНИЕ:
 * ```c
 * PatcherStatus status = patcher_status_rva_table(g_patches, count);
 * switch (status) {
 *     case PATCHER_STATUS_ORIGINAL:
 *         // Ready to patch / Готов к патчингу
 *         patcher_apply_rva_table(g_patches, count);
 *         break;
 *     case PATCHER_STATUS_PATCHED:
 *         // Already patched / Уже пропатчен
 *         break;
 *     case PATCHER_STATUS_MIXED:
 *         // Problem! / Проблема!
 *         break;
 * }
 * ```
 ******************************************************************************/
typedef enum PatcherStatus {
    PATCHER_STATUS_ORIGINAL = 0, /* all bytes match oldB / все байты совпадают с oldB */
    PATCHER_STATUS_PATCHED  = 1, /* all bytes match newB / все байты совпадают с newB */
    PATCHER_STATUS_MIXED    = 2, /* mixture of old/new OR unknown bytes / смесь старых/новых ИЛИ неизвестных байтов */
    PATCHER_STATUS_OOB      = 3  /* at least one rva out of image bounds / хотя бы один rva за границами образа */
} PatcherStatus;

/*******************************************************************************
 * ERROR CODES
 * КОДЫ ОШИБОК
 * 
 * Enum values for PatcherLastError.stage field.
 * Indicates which operation failed.
 * 
 * Значения перечисления для поля PatcherLastError.stage.
 * Указывает какая операция не удалась.
 ******************************************************************************/
enum {
    /***************************************************************************
     * PATCHER_OK (0)
     * No error. Operation successful.
     * Нет ошибки. Операция успешна.
     ***************************************************************************/
    PATCHER_OK = 0,
    
    /***************************************************************************
     * PATCHER_FAIL_GETMODULE
     * Failed to get module handle.
     * Module not found or invalid module name.
     * 
     * Не удалось получить дескриптор модуля.
     * Модуль не найден или недопустимое имя модуля.
     ***************************************************************************/
    PATCHER_FAIL_GETMODULE,
    
    /***************************************************************************
     * PATCHER_FAIL_MISMATCH
     * Byte value doesn't match expected.
     * Current byte is neither oldB nor newB.
     * Possible wrong version or code modified by other software.
     * 
     * Значение байта не совпадает с ожидаемым.
     * Текущий байт ни oldB ни newB.
     * Возможно неправильная версия или код изменён другим ПО.
     ***************************************************************************/
    PATCHER_FAIL_MISMATCH,
    
    /***************************************************************************
     * PATCHER_FAIL_VPROTECT
     * VirtualProtect failed.
     * Cannot change memory protection.
     * May occur on protected/system memory.
     * 
     * VirtualProtect не удался.
     * Невозможно изменить защиту памяти.
     * Может произойти на защищённой/системной памяти.
     ***************************************************************************/
    PATCHER_FAIL_VPROTECT,
    
    /***************************************************************************
     * PATCHER_FAIL_WRITEVERIFY
     * Write succeeded but verification failed.
     * Byte was written but readback doesn't match.
     * Very rare - possible hardware issue.
     * 
     * Запись удалась но проверка не удалась.
     * Байт был записан но обратное чтение не совпадает.
     * Очень редко - возможная проблема оборудования.
     ***************************************************************************/
    PATCHER_FAIL_WRITEVERIFY,

    /***************************************************************************
     * EXTENDED ERROR CODES
     * РАСШИРЕННЫЕ КОДЫ ОШИБОК
     ***************************************************************************/
    
    /***************************************************************************
     * PATCHER_FAIL_BADPE
     * Invalid PE structure.
     * Module is not a valid PE executable.
     * DOS or NT headers corrupted or missing.
     * 
     * Недопустимая PE структура.
     * Модуль не является допустимым PE исполняемым файлом.
     * DOS или NT заголовки повреждены или отсутствуют.
     ***************************************************************************/
    PATCHER_FAIL_BADPE,
    
    /***************************************************************************
     * PATCHER_FAIL_OOB
     * RVA out of bounds.
     * RVA exceeds image size.
     * Prevents writing outside module memory.
     * 
     * RVA за границами.
     * RVA превышает размер образа.
     * Предотвращает запись вне памяти модуля.
     ***************************************************************************/
    PATCHER_FAIL_OOB,
    
    /***************************************************************************
     * PATCHER_FAIL_SIG_MISMATCH
     * Signature verification failed.
     * Bytes at RVA don't match expected signature.
     * Wrong Winamp version or code modified.
     * 
     * Проверка сигнатуры не удалась.
     * Байты по RVA не совпадают с ожидаемой сигнатурой.
     * Неправильная версия Winamp или код изменён.
     ***************************************************************************/
    PATCHER_FAIL_SIG_MISMATCH
};

/*******************************************************************************
 * PatcherLastError
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Detailed error information from last patcher operation.
 * Retrieved via patcher_get_last_error().
 * 
 * Детальная информация об ошибке последней операции патчера.
 * Получается через patcher_get_last_error().
 * 
 * FIELDS / ПОЛЯ:
 * 
 * stage (int) - Error code (one of PATCHER_* enum values)
 *               Код ошибки (одно из значений перечисления PATCHER_*)
 *               
 *               Indicates which operation failed.
 *               PATCHER_OK (0) means no error.
 *               
 *               Указывает какая операция не удалась.
 *               PATCHER_OK (0) означает нет ошибки.
 * 
 * rva (DWORD) - RVA where error occurred
 *               RVA где произошла ошибка
 *               
 *               For byte patches: RVA of problematic byte.
 *               For signatures: RVA where signature starts.
 *               May be 0 if not applicable.
 *               
 *               Для байтовых патчей: RVA проблемного байта.
 *               Для сигнатур: RVA где начинается сигнатура.
 *               Может быть 0 если неприменимо.
 * 
 * haveB (BYTE) - Observed byte value
 *                Наблюдаемое значение байта
 *                
 *                Actual byte found at RVA.
 *                Used for mismatch diagnosis.
 *                
 *                Фактический байт найденный по RVA.
 *                Используется для диагностики несовпадения.
 * 
 * wantB (BYTE) - Expected byte value
 *                Ожидаемое значение байта
 *                
 *                What we expected to find (oldB for apply, newB for revert).
 *                Что ожидали найти (oldB для apply, newB для revert).
 * 
 * USAGE / ИСПОЛЬЗОВАНИЕ:
 * ```c
 * if (!patcher_apply_rva_table(patches, count)) {
 *     PatcherLastError err = patcher_get_last_error();
 *     printf("Patch failed at RVA 0x%08X\n", err.rva);
 *     printf("Expected 0x%02X, found 0x%02X\n", err.wantB, err.haveB);
 *     
 *     switch (err.stage) {
 *         case PATCHER_FAIL_MISMATCH:
 *             printf("Wrong Winamp version?\n");
 *             break;
 *         case PATCHER_FAIL_OOB:
 *             printf("RVA out of bounds\n");
 *             break;
 *     }
 * }
 * ```
 ******************************************************************************/
typedef struct PatcherLastError {
    int   stage;  /* one of PATCHER_* / один из PATCHER_* */
    DWORD rva;    /* failing RVA (or signature RVA) / RVA ошибки (или RVA сигнатуры) */
    BYTE  haveB;  /* observed byte (when applicable) / наблюдаемый байт (когда применимо) */
    BYTE  wantB;  /* expected byte (when applicable) / ожидаемый байт (когда применимо) */
} PatcherLastError;

/*******************************************************************************
 * CORE API - CONVENIENCE FUNCTIONS (EXE)
 * ОСНОВНОЙ API - УДОБНЫЕ ФУНКЦИИ (EXE)
 * 
 * These functions automatically work with current process EXE (winamp.exe).
 * Most common use case for Winamp plugins.
 * 
 * Эти функции автоматически работают с EXE текущего процесса (winamp.exe).
 * Самый распространённый случай использования для плагинов Winamp.
 ******************************************************************************/

/*******************************************************************************
 * patcher_apply_rva_table
 * 
 * Applies patch table to current process EXE (winamp.exe).
 * Применяет таблицу патчей к EXE текущего процесса (winamp.exe).
 * 
 * See patcher_apply_table_base() for detailed documentation.
 * Смотрите patcher_apply_table_base() для детальной документации.
 ******************************************************************************/
int  patcher_apply_rva_table  (const PatchByte* t, int n);

/*******************************************************************************
 * patcher_revert_rva_table
 * 
 * Reverts patch table from current process EXE (winamp.exe).
 * Откатывает таблицу патчей из EXE текущего процесса (winamp.exe).
 * 
 * See patcher_revert_table_base() for detailed documentation.
 * Смотрите patcher_revert_table_base() для детальной документации.
 ******************************************************************************/
int  patcher_revert_rva_table (const PatchByte* t, int n);

/*******************************************************************************
 * patcher_status_rva_table
 * 
 * Checks patch status of current process EXE (winamp.exe).
 * Проверяет статус патчей EXE текущего процесса (winamp.exe).
 * 
 * See patcher_status_table_base() for detailed documentation.
 * Смотрите patcher_status_table_base() для детальной документации.
 ******************************************************************************/
PatcherStatus patcher_status_rva_table(const PatchByte* t, int n);

/*******************************************************************************
 * patcher_verify_sigs
 * 
 * Verifies signatures in current process EXE (winamp.exe).
 * Проверяет сигнатуры в EXE текущего процесса (winamp.exe).
 * 
 * See patcher_verify_sigs_base() for detailed documentation.
 * Смотрите patcher_verify_sigs_base() для детальной документации.
 ******************************************************************************/
int patcher_verify_sigs(const PatchSig* s, int n);

/*******************************************************************************
 * CORE API - BASE/MODULE VARIANTS
 * ОСНОВНОЙ API - ВАРИАНТЫ BASE/MODULE
 * 
 * These functions work with explicit module base addresses.
 * Recommended for advanced use cases (patching DLLs, multiple modules).
 * 
 * Эти функции работают с явными базовыми адресами модулей.
 * Рекомендуются для продвинутых случаев использования (патчинг DLL, нескольких модулей).
 ******************************************************************************/

/*******************************************************************************
 * patcher_apply_table_base
 * 
 * Applies patches to specified module with automatic rollback.
 * Применяет патчи к указанному модулю с автоматическим откатом.
 * 
 * This is the main workhorse function. See patcher_core.cpp for full docs.
 * Это основная рабочая функция. Смотрите patcher_core.cpp для полной документации.
 ******************************************************************************/
int  patcher_apply_table_base  (BYTE* base, const PatchByte* t, int n);

/*******************************************************************************
 * patcher_revert_table_base
 * 
 * Reverts patches from specified module with automatic rollback.
 * Откатывает патчи из указанного модуля с автоматическим откатом.
 * 
 * This is the main revert function. See patcher_core.cpp for full docs.
 * Это основная функция отката. Смотрите patcher_core.cpp для полной документации.
 ******************************************************************************/
int  patcher_revert_table_base (BYTE* base, const PatchByte* t, int n);

/*******************************************************************************
 * patcher_status_table_base
 * 
 * Checks patch status of specified module.
 * Проверяет статус патчей указанного модуля.
 * 
 * Returns PATCHER_STATUS_* enum value.
 * Возвращает значение перечисления PATCHER_STATUS_*.
 ******************************************************************************/
PatcherStatus patcher_status_table_base(BYTE* base, const PatchByte* t, int n);

/*******************************************************************************
 * patcher_verify_sigs_base
 * 
 * Verifies signatures in specified module.
 * Проверяет сигнатуры в указанном модуле.
 * 
 * Returns 1 if all signatures match, 0 on mismatch or error.
 * Возвращает 1 если все сигнатуры совпадают, 0 при несовпадении или ошибке.
 ******************************************************************************/
int  patcher_verify_sigs_base(BYTE* base, const PatchSig* s, int n);

/*******************************************************************************
 * patcher_get_module_baseA
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Resolves module base address by name.
 * Определяет базовый адрес модуля по имени.
 * 
 * PARAMETERS / ПАРАМЕТРЫ:
 * moduleName - Module name (e.g., "gen_ml.dll") or NULL for main EXE
 *              Имя модуля (например "gen_ml.dll") или NULL для главного EXE
 * 
 * RETURNS / ВОЗВРАЩАЕТ:
 * Module base address or NULL if not found
 * Базовый адрес модуля или NULL если не найден
 * 
 * EXAMPLES / ПРИМЕРЫ:
 * ```c
 * BYTE* winamp = patcher_get_module_baseA(NULL);          // winamp.exe
 * BYTE* genml  = patcher_get_module_baseA("gen_ml.dll"); // Media Library
 * ```
 ******************************************************************************/
BYTE* patcher_get_module_baseA(const char* moduleName);

/*******************************************************************************
 * ERROR REPORTING API
 * API ОТЧЁТНОСТИ ОБ ОШИБКАХ
 ******************************************************************************/

/*******************************************************************************
 * patcher_get_last_error
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Retrieves detailed information about last error.
 * Получает детальную информацию о последней ошибке.
 * 
 * RETURNS / ВОЗВРАЩАЕТ:
 * PatcherLastError structure with error details
 * Структура PatcherLastError с деталями ошибки
 * 
 * USAGE / ИСПОЛЬЗОВАНИЕ:
 * Call after any patcher function returns failure (0).
 * Вызывайте после того как любая функция патчера вернёт неудачу (0).
 ******************************************************************************/
PatcherLastError patcher_get_last_error(void);

/*******************************************************************************
 * patcher_clear_last_error
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Clears last error to PATCHER_OK state.
 * Очищает последнюю ошибку в состояние PATCHER_OK.
 * 
 * USAGE / ИСПОЛЬЗОВАНИЕ:
 * Automatically called at start of each patcher operation.
 * Rarely need to call manually.
 * 
 * Автоматически вызывается в начале каждой операции патчера.
 * Редко нужно вызывать вручную.
 ******************************************************************************/
void patcher_clear_last_error(void);

#ifdef __cplusplus
}
#endif

/*******************************************************************************
 * PATCH MODULE FUNCTIONS
 * ФУНКЦИИ МОДУЛЕЙ ПАТЧЕЙ
 * 
 * Forward declarations for all patch modules.
 * Each module provides init() and quit() functions.
 * 
 * Предварительные объявления для всех модулей патчей.
 * Каждый модуль предоставляет функции init() и quit().
 ******************************************************************************/

/*******************************************************************************
 * patch_pl - Playlist Search Fix
 * Исправление поиска в плейлисте
 * 
 * Fixes playlist search to support Unicode characters (Cyrillic, etc.).
 * Исправляет поиск в плейлисте для поддержки Unicode символов (кириллица и т.д.).
 ******************************************************************************/
int  patch_pl_init(void);
void patch_pl_quit(void);

/*******************************************************************************
 * patch_url - Skins Server URL Redirection
 * Перенаправление URL сервера скинов
 * 
 * Redirects skin download URL from www.winamp.com to skins.webamp.org.
 * Перенаправляет URL загрузки скинов с www.winamp.com на skins.webamp.org.
 ******************************************************************************/
int  patch_url_init(void);
void patch_url_quit(void);

/*******************************************************************************
 * patch_mb_skin - Skin-Triggered Minibrowser Blocking
 * Блокировка мини-браузера запускаемого скином
 * 
 * Prevents skins from auto-opening minibrowser via mb.ini.
 * Предотвращает автоматическое открытие мини-браузера скинами через mb.ini.
 ******************************************************************************/
int  patch_mb_skin_init(void);
void patch_mb_skin_quit(void);

/*******************************************************************************
 * patch_mb - Minibrowser Removal
 * Удаление мини-браузера
 * 
 * Blocks minibrowser creation and hides menu item.
 * Блокирует создание мини-браузера и скрывает пункт меню.
 ******************************************************************************/
int  patch_mb_init(void);
void patch_mb_quit(void);

/*******************************************************************************
 * patch_cd - CD Ripping Removal
 * Удаление риппинга CD
 * 
 * Removes CD ripping functionality from Media Library.
 * Удаляет функциональность риппинга CD из библиотеки.
 ******************************************************************************/
int  patch_cd_init(void);
void patch_cd_quit(void);