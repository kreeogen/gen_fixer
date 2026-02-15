/*******************************************************************************
 * patcher_core.cpp
 * 
 * UNIVERSAL RVA PATCHER CORE FOR WIN32
 * УНИВЕРСАЛЬНОЕ ЯДРО RVA ПАТЧЕРА ДЛЯ WIN32
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Low-level patching engine for modifying Win32 PE executables in memory.
 * Provides safe, atomic, and reversible byte-level patching with rollback support.
 * 
 * Низкоуровневый движок патчинга для модификации Win32 PE исполняемых файлов в памяти.
 * Предоставляет безопасный, атомарный и обратимый патчинг на уровне байтов с поддержкой отката.
 * 
 * KEY FEATURES / КЛЮЧЕВЫЕ ВОЗМОЖНОСТИ:
 * - RVA-based patching (Relative Virtual Address)
 * - Signature verification before patching
 * - Atomic operations with automatic rollback on failure
 * - Idempotent apply/revert (safe to call multiple times)
 * - Comprehensive error reporting
 * - Memory protection handling
 * - PE structure validation
 * 
 * - Патчинг на основе RVA (относительный виртуальный адрес)
 * - Проверка сигнатуры перед патчингом
 * - Атомарные операции с автоматическим откатом при ошибке
 * - Идемпотентное применение/откат (безопасно вызывать много раз)
 * - Комплексная отчётность об ошибках
 * - Обработка защиты памяти
 * - Валидация PE структуры
 * 
 * SAFETY GUARANTEES / ГАРАНТИИ БЕЗОПАСНОСТИ:
 * 1. Pre-validation: Checks PE structure, image bounds before any writes
 * 2. Rollback: If any patch fails, all previous patches are reverted
 * 3. Verification: After writing, reads back to confirm successful write
 * 4. Idempotent: Can apply patches multiple times without corruption
 * 5. Error tracking: Detailed error information for diagnostics
 * 
 * 1. Предварительная валидация: Проверяет PE структуру, границы образа перед записью
 * 2. Откат: Если какой-либо патч не удался, все предыдущие патчи откатываются
 * 3. Проверка: После записи читает обратно для подтверждения успешной записи
 * 4. Идемпотентность: Можно применять патчи много раз без повреждения
 * 5. Отслеживание ошибок: Детальная информация об ошибках для диагностики
 * 
 * ARCHITECTURE / АРХИТЕКТУРА:
 * This is the core (level 1) module that all patch modules depend on.
 * It provides primitives that higher-level modules use to implement specific fixes.
 * 
 * Это ядро (уровень 1) модуль от которого зависят все модули патчей.
 * Он предоставляет примитивы которые модули более высокого уровня используют для реализации конкретных исправлений.
 * 
 * COMPATIBILITY / СОВМЕСТИМОСТЬ:
 * - Visual Studio 2003 compatible (VC7.1)
 * - Works with C and C++
 * - Windows 98 through Windows 11
 * - x86 architecture
 * 
 * - Совместимо с Visual Studio 2003 (VC7.1)
 * - Работает с C и C++
 * - Windows 98 до Windows 11
 * - Архитектура x86
 * 
 ******************************************************************************/

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "patcher_core.h"

/*******************************************************************************
 * ERROR TRACKING SYSTEM
 * СИСТЕМА ОТСЛЕЖИВАНИЯ ОШИБОК
 ******************************************************************************/

/*******************************************************************************
 * g_last
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Global variable storing last error information.
 * Simple approach suitable for typical plugin use (single-threaded context).
 * 
 * Глобальная переменная хранящая информацию о последней ошибке.
 * Простой подход подходящий для типичного использования плагина (однопоточный контекст).
 * 
 * THREAD SAFETY / ПОТОКОБЕЗОПАСНОСТЬ:
 * Not thread-safe. If multiple threads use patcher, errors may be mixed.
 * For multi-threaded use, consider thread-local storage (TLS).
 * 
 * Не потокобезопасно. Если несколько потоков используют патчер, ошибки могут смешаться.
 * Для многопоточного использования рассмотрите thread-local storage (TLS).
 * 
 * FIELDS / ПОЛЯ:
 * stage - Error code indicating what operation failed
 * rva   - RVA address where error occurred
 * haveB - Actual byte value found at address
 * wantB - Expected byte value
 * 
 * stage - Код ошибки указывающий какая операция не удалась
 * rva   - Адрес RVA где произошла ошибка
 * haveB - Фактическое значение байта найденное по адресу
 * wantB - Ожидаемое значение байта
 ******************************************************************************/
static PatcherLastError g_last = { PATCHER_OK, 0, 0, 0 };

/*******************************************************************************
 * set_err
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Internal helper to set last error information.
 * Внутренняя вспомогательная функция для установки информации о последней ошибке.
 * 
 * PARAMETERS / ПАРАМЕТРЫ:
 * stage - Error code / Код ошибки
 * rva   - RVA where error occurred / RVA где произошла ошибка
 * haveB - Actual byte value / Фактическое значение байта
 * wantB - Expected byte value / Ожидаемое значение байта
 ******************************************************************************/
static void set_err(int stage, DWORD rva, BYTE haveB, BYTE wantB)
{
    g_last.stage = stage;
    g_last.rva   = rva;
    g_last.haveB = haveB;
    g_last.wantB = wantB;
}

/*******************************************************************************
 * patcher_get_last_error
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Retrieves last error information for diagnostics.
 * Получает информацию о последней ошибке для диагностики.
 * 
 * RETURNS / ВОЗВРАЩАЕТ:
 * PatcherLastError structure with error details
 * Структура PatcherLastError с деталями ошибки
 * 
 * USAGE / ИСПОЛЬЗОВАНИЕ:
 * Call after patcher operation fails to understand what went wrong.
 * Вызывайте после неудачи операции патчера для понимания что пошло не так.
 ******************************************************************************/
PatcherLastError patcher_get_last_error(void) { 
    return g_last; 
}

/*******************************************************************************
 * patcher_clear_last_error
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Clears last error to PATCHER_OK state.
 * Очищает последнюю ошибку в состояние PATCHER_OK.
 * 
 * USAGE / ИСПОЛЬЗОВАНИЕ:
 * Called at start of each patcher operation to reset error state.
 * Вызывается в начале каждой операции патчера для сброса состояния ошибки.
 ******************************************************************************/
void patcher_clear_last_error(void) { 
    set_err(PATCHER_OK, 0, 0, 0); 
}

/*******************************************************************************
 * PE (PORTABLE EXECUTABLE) HELPERS
 * ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ PE (ПЕРЕНОСИМЫЙ ИСПОЛНЯЕМЫЙ ФАЙЛ)
 * 
 * These functions work with PE file format structures in memory.
 * Эти функции работают со структурами формата файла PE в памяти.
 ******************************************************************************/

/*******************************************************************************
 * get_image_size
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Retrieves size of loaded PE image from PE headers.
 * Получает размер загруженного PE образа из PE заголовков.
 * 
 * PARAMETERS / ПАРАМЕТРЫ:
 * base - Pointer to module base address / Указатель на базовый адрес модуля
 * 
 * RETURNS / ВОЗВРАЩАЕТ:
 * Image size in bytes, or 0 if invalid PE structure
 * Размер образа в байтах, или 0 если недопустимая PE структура
 * 
 * ALGORITHM / АЛГОРИТМ:
 * 1. Verify DOS header signature (MZ)
 * 2. Follow e_lfanew to NT headers
 * 3. Verify NT header signature (PE)
 * 4. Return SizeOfImage from Optional Header
 * 
 * 1. Проверить сигнатуру DOS заголовка (MZ)
 * 2. Следовать e_lfanew к NT заголовкам
 * 3. Проверить сигнатуру NT заголовка (PE)
 * 4. Вернуть SizeOfImage из опционального заголовка
 * 
 * SAFETY / БЕЗОПАСНОСТЬ:
 * Uses __try/__except to catch access violations if base pointer is invalid.
 * Использует __try/__except для перехвата нарушений доступа если базовый указатель недопустим.
 ******************************************************************************/
static DWORD get_image_size(BYTE* base)
{
    if (!base) return 0;

    __try {
        // Verify DOS header / Проверить DOS заголовок
        IMAGE_DOS_HEADER* dos = (IMAGE_DOS_HEADER*)base;
        if (dos->e_magic != IMAGE_DOS_SIGNATURE) return 0;  // Not MZ signature / Не MZ сигнатура

        // Get NT headers / Получить NT заголовки
        IMAGE_NT_HEADERS* nt = (IMAGE_NT_HEADERS*)(base + dos->e_lfanew);
        if (nt->Signature != IMAGE_NT_SIGNATURE) return 0;  // Not PE signature / Не PE сигнатура

        // Return image size / Вернуть размер образа
        return nt->OptionalHeader.SizeOfImage;
        
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        // Access violation - invalid pointer / Нарушение доступа - недопустимый указатель
        return 0;
    }
}

/*******************************************************************************
 * is_range_in_image
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Validates that an RVA range is within image boundaries.
 * Проверяет что диапазон RVA находится в границах образа.
 * 
 * PARAMETERS / ПАРАМЕТРЫ:
 * base - Module base address / Базовый адрес модуля
 * rva  - Relative Virtual Address / Относительный виртуальный адрес
 * len  - Length of range to check / Длина диапазона для проверки
 * 
 * RETURNS / ВОЗВРАЩАЕТ:
 * 1 if range is valid and within image, 0 otherwise
 * 1 если диапазон допустим и в пределах образа, 0 иначе
 * 
 * VALIDATION CHECKS / ПРОВЕРКИ ВАЛИДАЦИИ:
 * 1. Image size is valid (PE headers accessible)
 * 2. RVA doesn't exceed image size
 * 3. Length doesn't exceed image size
 * 4. RVA + length doesn't overflow or exceed image size
 * 
 * 1. Размер образа допустим (PE заголовки доступны)
 * 2. RVA не превышает размер образа
 * 3. Длина не превышает размер образа
 * 4. RVA + длина не переполняется и не превышает размер образа
 * 
 * IMPORTANCE / ВАЖНОСТЬ:
 * CRITICAL for safety. Prevents writing outside image boundaries which could:
 * - Crash the process
 * - Corrupt other modules' memory
 * - Cause undefined behavior
 * 
 * КРИТИЧНО для безопасности. Предотвращает запись за границами образа что может:
 * - Крашнуть процесс
 * - Повредить память других модулей
 * - Вызвать неопределённое поведение
 ******************************************************************************/
static int is_range_in_image(BYTE* base, DWORD rva, DWORD len)
{
    DWORD img = get_image_size(base);
    if (!img) return 0;                // Invalid PE structure / Недопустимая PE структура
    if (rva > img) return 0;           // RVA out of bounds / RVA за границами
    if (len > img) return 0;           // Length exceeds image / Длина превышает образ
    if (rva + len > img) return 0;     // Range overflows image / Диапазон переполняет образ
    return 1;
}

/*******************************************************************************
 * MEMORY WRITE HELPER
 * ВСПОМОГАТЕЛЬНАЯ ФУНКЦИЯ ЗАПИСИ В ПАМЯТЬ
 ******************************************************************************/

/*******************************************************************************
 * write_byte
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Safely writes a single byte to memory with protection handling and verification.
 * Безопасно записывает один байт в память с обработкой защиты и проверкой.
 * 
 * PARAMETERS / ПАРАМЕТРЫ:
 * addr         - Address to write to / Адрес для записи
 * value        - Byte value to write / Значение байта для записи
 * rva_for_err  - RVA for error reporting / RVA для отчёта об ошибках
 * want_for_err - Expected value for error reporting / Ожидаемое значение для отчёта об ошибках
 * 
 * RETURNS / ВОЗВРАЩАЕТ:
 * 1 if write successful and verified, 0 on failure
 * 1 если запись успешна и проверена, 0 при ошибке
 * 
 * ALGORITHM / АЛГОРИТМ:
 * 1. Change memory protection to writable (PAGE_EXECUTE_READWRITE)
 * 2. Write byte value
 * 3. Flush instruction cache (critical for code modifications!)
 * 4. Restore original memory protection
 * 5. Verify byte was written correctly
 * 6. Report error if any step fails
 * 
 * 1. Изменить защиту памяти на записываемую (PAGE_EXECUTE_READWRITE)
 * 2. Записать значение байта
 * 3. Сбросить кэш инструкций (критично для модификаций кода!)
 * 4. Восстановить оригинальную защиту памяти
 * 5. Проверить что байт записан правильно
 * 6. Сообщить об ошибке если какой-либо шаг не удался
 * 
 * WHY FLUSH INSTRUCTION CACHE / ПОЧЕМУ СБРАСЫВАТЬ КЭШ ИНСТРУКЦИЙ:
 * CPU may cache instructions. After modifying code, we must flush cache
 * so CPU re-fetches modified instructions from memory.
 * 
 * CPU может кэшировать инструкции. После модификации кода мы должны сбросить кэш
 * чтобы CPU повторно загрузил модифицированные инструкции из памяти.
 * 
 * ERROR CASES / СЛУЧАИ ОШИБОК:
 * - PATCHER_FAIL_VPROTECT: VirtualProtect failed (common on protected memory)
 * - PATCHER_FAIL_WRITEVERIFY: Write succeeded but verification failed (rare)
 * 
 * - PATCHER_FAIL_VPROTECT: VirtualProtect не удался (часто на защищённой памяти)
 * - PATCHER_FAIL_WRITEVERIFY: Запись удалась но проверка не удалась (редко)
 ******************************************************************************/
static int write_byte(BYTE* addr, BYTE value, DWORD rva_for_err, BYTE want_for_err)
{
    DWORD oldProt = 0;

    /***************************************************************************
     * STEP 1: CHANGE MEMORY PROTECTION
     * ШАГ 1: ИЗМЕНИТЬ ЗАЩИТУ ПАМЯТИ
     * 
     * Make memory writable. Most code/data sections are read-only or read-execute.
     * Сделать память записываемой. Большинство секций кода/данных только для чтения или чтение-выполнение.
     ***************************************************************************/
    if (!VirtualProtect(addr, 1, PAGE_EXECUTE_READWRITE, &oldProt)) {
        set_err(PATCHER_FAIL_VPROTECT, rva_for_err, *addr, want_for_err);
        return 0;
    }

    /***************************************************************************
     * STEP 2: WRITE BYTE
     * ШАГ 2: ЗАПИСАТЬ БАЙТ
     ***************************************************************************/
    *addr = value;
    
    /***************************************************************************
     * STEP 3: FLUSH INSTRUCTION CACHE
     * ШАГ 3: СБРОСИТЬ КЭШ ИНСТРУКЦИЙ
     * 
     * CRITICAL: Without this, CPU may execute old cached instructions!
     * КРИТИЧНО: Без этого CPU может выполнить старые кэшированные инструкции!
     ***************************************************************************/
    FlushInstructionCache(GetCurrentProcess(), addr, 1);

    /***************************************************************************
     * STEP 4: RESTORE MEMORY PROTECTION
     * ШАГ 4: ВОССТАНОВИТЬ ЗАЩИТУ ПАМЯТИ
     * 
     * Good practice to restore original protection.
     * Хорошая практика восстанавливать оригинальную защиту.
     ***************************************************************************/
    {
        DWORD tmp = 0;
        VirtualProtect(addr, 1, oldProt, &tmp);
    }

    /***************************************************************************
     * STEP 5: VERIFY WRITE
     * ШАГ 5: ПРОВЕРИТЬ ЗАПИСЬ
     * 
     * Read back to confirm write succeeded.
     * Прочитать обратно для подтверждения что запись удалась.
     ***************************************************************************/
    if (*addr != value) {
        set_err(PATCHER_FAIL_WRITEVERIFY, rva_for_err, *addr, value);
        return 0;
    }
    
    return 1;  // Success / Успех
}

/*******************************************************************************
 * MODULE BASE ADDRESS RETRIEVAL
 * ПОЛУЧЕНИЕ БАЗОВОГО АДРЕСА МОДУЛЯ
 ******************************************************************************/

/*******************************************************************************
 * patcher_get_module_baseA
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Retrieves base address of a loaded module (DLL or EXE).
 * Получает базовый адрес загруженного модуля (DLL или EXE).
 * 
 * PARAMETERS / ПАРАМЕТРЫ:
 * moduleName - Module name (e.g., "winamp.exe") or NULL for current process EXE
 *              Имя модуля (например "winamp.exe") или NULL для EXE текущего процесса
 * 
 * RETURNS / ВОЗВРАЩАЕТ:
 * Pointer to module base address, or NULL if module not found
 * Указатель на базовый адрес модуля, или NULL если модуль не найден
 * 
 * USAGE EXAMPLES / ПРИМЕРЫ ИСПОЛЬЗОВАНИЯ:
 * patcher_get_module_baseA(NULL)          -> winamp.exe base / база winamp.exe
 * patcher_get_module_baseA("gen_ml.dll")  -> Media Library DLL base / база DLL библиотеки
 * 
 * ERROR HANDLING / ОБРАБОТКА ОШИБОК:
 * Sets PATCHER_FAIL_GETMODULE error if module not found.
 * Устанавливает ошибку PATCHER_FAIL_GETMODULE если модуль не найден.
 ******************************************************************************/
BYTE* patcher_get_module_baseA(const char* moduleName)
{
    HMODULE h = NULL;
    
    // Clear previous error / Очистить предыдущую ошибку
    patcher_clear_last_error();

    // Get module handle / Получить дескриптор модуля
    if (!moduleName) {
        h = GetModuleHandleA(NULL);  // NULL = current process EXE / текущий EXE процесса
    } else {
        h = GetModuleHandleA(moduleName);
    }

    // Check if found / Проверить, найден ли
    if (!h) {
        set_err(PATCHER_FAIL_GETMODULE, 0, 0, 0);
        return NULL;
    }
    
    return (BYTE*)h;
}

/*******************************************************************************
 * SIGNATURE VERIFICATION
 * ПРОВЕРКА СИГНАТУРЫ
 * 
 * Verifies that bytes at specified RVAs match expected signatures.
 * This ensures we're patching the correct code/data.
 * 
 * Проверяет что байты по указанным RVA совпадают с ожидаемыми сигнатурами.
 * Это гарантирует что мы патчим правильный код/данные.
 ******************************************************************************/

/*******************************************************************************
 * patcher_verify_sigs_base
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Verifies multiple byte signatures against module memory.
 * Проверяет несколько байтовых сигнатур в памяти модуля.
 * 
 * PARAMETERS / ПАРАМЕТРЫ:
 * base - Module base address / Базовый адрес модуля
 * s    - Array of signature structures / Массив структур сигнатур
 * n    - Number of signatures / Количество сигнатур
 * 
 * RETURNS / ВОЗВРАЩАЕТ:
 * 1 if all signatures match, 0 if any mismatch or error
 * 1 если все сигнатуры совпадают, 0 если какое-либо несовпадение или ошибка
 * 
 * ALGORITHM / АЛГОРИТМ:
 * 1. Validate module base and PE structure
 * 2. For each signature:
 *    - Check RVA is within image bounds
 *    - Compare each byte with expected value
 *    - Report first mismatch found
 * 3. Return success only if all signatures match
 * 
 * 1. Проверить базу модуля и PE структуру
 * 2. Для каждой сигнатуры:
 *    - Проверить что RVA в границах образа
 *    - Сравнить каждый байт с ожидаемым значением
 *    - Сообщить о первом найденном несовпадении
 * 3. Вернуть успех только если все сигнатуры совпадают
 * 
 * ERROR CODES / КОДЫ ОШИБОК:
 * - PATCHER_FAIL_GETMODULE: Invalid base address
 * - PATCHER_FAIL_BADPE: Invalid PE structure
 * - PATCHER_FAIL_OOB: RVA out of image bounds
 * - PATCHER_FAIL_SIG_MISMATCH: Byte doesn't match expected value
 * 
 * - PATCHER_FAIL_GETMODULE: Недопустимый базовый адрес
 * - PATCHER_FAIL_BADPE: Недопустимая PE структура
 * - PATCHER_FAIL_OOB: RVA за границами образа
 * - PATCHER_FAIL_SIG_MISMATCH: Байт не совпадает с ожидаемым значением
 * 
 * IMPORTANCE / ВАЖНОСТЬ:
 * Signature verification prevents patching wrong Winamp version or corrupted code.
 * Always verify signatures before applying patches!
 * 
 * Проверка сигнатуры предотвращает патчинг неправильной версии Winamp или повреждённого кода.
 * Всегда проверяйте сигнатуры перед применением патчей!
 ******************************************************************************/
int patcher_verify_sigs_base(BYTE* base, const PatchSig* s, int n)
{
    int i;
    
    patcher_clear_last_error();

    // Validate base address / Проверить базовый адрес
    if (!base) {
        set_err(PATCHER_FAIL_GETMODULE, 0, 0, 0);
        return 0;
    }
    
    // Empty signature list is valid / Пустой список сигнатур допустим
    if (!s || n <= 0) return 1;

    // Validate PE structure / Проверить PE структуру
    if (!get_image_size(base)) {
        set_err(PATCHER_FAIL_BADPE, 0, 0, 0);
        return 0;
    }

    // Check each signature / Проверить каждую сигнатуру
    for (i = 0; i < n; i++) {
        const PatchSig* ps = &s[i];
        
        // Skip empty signatures / Пропустить пустые сигнатуры
        if (!ps->bytes || ps->len <= 0) continue;

        // Validate range is within image / Проверить что диапазон в пределах образа
        if (!is_range_in_image(base, ps->rva, (DWORD)ps->len)) {
            set_err(PATCHER_FAIL_OOB, ps->rva, 0, 0);
            return 0;
        }

        // Compare bytes / Сравнить байты
        {
            BYTE* p = base + ps->rva;
            int k;
            for (k = 0; k < ps->len; k++) {
                BYTE have = p[k];
                BYTE want = ps->bytes[k];
                if (have != want) {
                    // Mismatch found - report and fail / Несовпадение найдено - сообщить и прервать
                    set_err(PATCHER_FAIL_SIG_MISMATCH, ps->rva + (DWORD)k, have, want);
                    return 0;
                }
            }
        }
    }

    return 1;  // All signatures match / Все сигнатуры совпадают
}

/*******************************************************************************
 * patcher_verify_sigs
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Convenience wrapper: verifies signatures against current process EXE.
 * Удобная обёртка: проверяет сигнатуры в EXE текущего процесса.
 * 
 * PARAMETERS / ПАРАМЕТРЫ:
 * s - Array of signature structures / Массив структур сигнатур
 * n - Number of signatures / Количество сигнатур
 * 
 * RETURNS / ВОЗВРАЩАЕТ:
 * 1 if all signatures match, 0 otherwise
 * 1 если все сигнатуры совпадают, 0 иначе
 ******************************************************************************/
int patcher_verify_sigs(const PatchSig* s, int n)
{
    BYTE* base = patcher_get_module_baseA(NULL);
    if (!base) return 0;
    return patcher_verify_sigs_base(base, s, n);
}

/*******************************************************************************
 * PATCH STATUS CHECKING
 * ПРОВЕРКА СТАТУСА ПАТЧА
 * 
 * Determines current state of patches without modifying anything.
 * Определяет текущее состояние патчей без изменения чего-либо.
 ******************************************************************************/

/*******************************************************************************
 * patcher_status_table_base
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Checks status of patch table without applying or reverting anything.
 * Проверяет статус таблицы патчей без применения или отката чего-либо.
 * 
 * PARAMETERS / ПАРАМЕТРЫ:
 * base - Module base address / Базовый адрес модуля
 * t    - Array of patch byte structures / Массив структур байтов патчей
 * n    - Number of patches / Количество патчей
 * 
 * RETURNS / ВОЗВРАЩАЕТ:
 * PatcherStatus enum value:
 * - PATCHER_STATUS_ORIGINAL: All bytes at original values
 * - PATCHER_STATUS_PATCHED: All bytes at patched values
 * - PATCHER_STATUS_MIXED: Some original, some patched, or unknown values
 * - PATCHER_STATUS_OOB: Out of bounds or PE error
 * 
 * Значение enum PatcherStatus:
 * - PATCHER_STATUS_ORIGINAL: Все байты в оригинальных значениях
 * - PATCHER_STATUS_PATCHED: Все байты в пропатченных значениях
 * - PATCHER_STATUS_MIXED: Некоторые оригинальные, некоторые пропатченные, или неизвестные значения
 * - PATCHER_STATUS_OOB: За границами или PE ошибка
 * 
 * ALGORITHM / АЛГОРИТМ:
 * 1. Validate module and PE structure
 * 2. Count bytes that match original value (cntOld)
 * 3. Count bytes that match patched value (cntNew)
 * 4. Count bytes that match neither (cntOther)
 * 5. Return status based on counts
 * 
 * 1. Проверить модуль и PE структуру
 * 2. Подсчитать байты совпадающие с оригинальным значением (cntOld)
 * 3. Подсчитать байты совпадающие с пропатченным значением (cntNew)
 * 4. Подсчитать байты не совпадающие ни с чем (cntOther)
 * 5. Вернуть статус на основе подсчётов
 * 
 * USAGE / ИСПОЛЬЗОВАНИЕ:
 * Call this to check if patches are already applied before patching.
 * Useful for UI to show current patch state.
 * 
 * Вызывайте это для проверки применены ли уже патчи перед патчингом.
 * Полезно для UI для показа текущего состояния патчей.
 ******************************************************************************/
PatcherStatus patcher_status_table_base(BYTE* base, const PatchByte* t, int n)
{
    int i;
    int cntOld = 0, cntNew = 0, cntOther = 0;

    patcher_clear_last_error();

    // Validate base / Проверить базу
    if (!base) {
        set_err(PATCHER_FAIL_GETMODULE, 0, 0, 0);
        return PATCHER_STATUS_OOB;
    }
    
    // Empty table is considered original / Пустая таблица считается оригинальной
    if (!t || n <= 0) return PATCHER_STATUS_ORIGINAL;

    // Validate PE / Проверить PE
    if (!get_image_size(base)) {
        set_err(PATCHER_FAIL_BADPE, 0, 0, 0);
        return PATCHER_STATUS_OOB;
    }

    // Check each byte and categorize / Проверить каждый байт и категоризировать
    for (i = 0; i < n; i++) {
        DWORD rva = t[i].rva;
        
        // Validate bounds / Проверить границы
        if (!is_range_in_image(base, rva, 1)) {
            set_err(PATCHER_FAIL_OOB, rva, 0, 0);
            return PATCHER_STATUS_OOB;
        }

        // Read current byte and categorize / Прочитать текущий байт и категоризировать
        {
            BYTE have = *(base + rva);
            if (have == t[i].oldB) cntOld++;        // Original value / Оригинальное значение
            else if (have == t[i].newB) cntNew++;   // Patched value / Пропатченное значение
            else cntOther++;                         // Unknown value / Неизвестное значение
        }
    }

    // Determine status from counts / Определить статус из подсчётов
    if (cntOther) return PATCHER_STATUS_MIXED;      // Has unexpected values / Имеет неожиданные значения
    if (cntNew == n) return PATCHER_STATUS_PATCHED; // All patched / Все пропатчены
    return PATCHER_STATUS_ORIGINAL;                 // All original (or empty) / Все оригинальные (или пусто)
}

/*******************************************************************************
 * patcher_status_rva_table
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Convenience wrapper: checks patch status for current process EXE.
 * Удобная обёртка: проверяет статус патчей для EXE текущего процесса.
 ******************************************************************************/
PatcherStatus patcher_status_rva_table(const PatchByte* t, int n)
{
    BYTE* base = patcher_get_module_baseA(NULL);
    if (!base) return PATCHER_STATUS_OOB;
    return patcher_status_table_base(base, t, n);
}

/*******************************************************************************
 * ATOMIC PATCH APPLY/REVERT WITH ROLLBACK
 * АТОМАРНОЕ ПРИМЕНЕНИЕ/ОТКАТ ПАТЧА С ОТКАТОМ
 * 
 * These are the core patching functions with automatic rollback on failure.
 * Это основные функции патчинга с автоматическим откатом при ошибке.
 ******************************************************************************/

/*******************************************************************************
 * patcher_apply_table_base
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Applies patch table with automatic rollback on any failure.
 * Idempotent: safe to call multiple times.
 * 
 * Применяет таблицу патчей с автоматическим откатом при любой ошибке.
 * Идемпотентно: безопасно вызывать много раз.
 * 
 * PARAMETERS / ПАРАМЕТРЫ:
 * base - Module base address / Базовый адрес модуля
 * t    - Array of patch byte structures / Массив структур байтов патчей
 * n    - Number of patches / Количество патчей
 * 
 * RETURNS / ВОЗВРАЩАЕТ:
 * 1 if all patches applied successfully, 0 if any failed (with rollback)
 * 1 если все патчи применены успешно, 0 если какой-либо не удался (с откатом)
 * 
 * ALGORITHM / АЛГОРИТМ:
 * 1. Clear last error
 * 2. Validate module base and PE structure
 * 3. Pre-check all RVAs are within image bounds
 * 4. Allocate rollback list (best effort, works without if allocation fails)
 * 5. For each patch:
 *    a. Read current byte
 *    b. If matches oldB: Apply patch, add to rollback list
 *    c. If matches newB: Skip (already patched - idempotent)
 *    d. If neither: FAIL and rollback all applied patches
 * 6. Return success if all patches applied or skipped
 * 
 * 1. Очистить последнюю ошибку
 * 2. Проверить базу модуля и PE структуру
 * 3. Предварительно проверить что все RVA в границах образа
 * 4. Выделить список отката (лучшая попытка, работает без если выделение не удалось)
 * 5. Для каждого патча:
 *    a. Прочитать текущий байт
 *    b. Если совпадает с oldB: Применить патч, добавить в список отката
 *    c. Если совпадает с newB: Пропустить (уже пропатчен - идемпотентно)
 *    d. Если ни с чем: НЕУДАЧА и откатить все применённые патчи
 * 6. Вернуть успех если все патчи применены или пропущены
 * 
 * ROLLBACK MECHANISM / МЕХАНИЗМ ОТКАТА:
 * If any patch fails to apply, all previously applied patches are reverted.
 * This ensures atomic operation: either all patches apply or none do.
 * 
 * Если какой-либо патч не удалось применить, все ранее применённые патчи откатываются.
 * Это гарантирует атомарную операцию: либо все патчи применяются либо ни один.
 * 
 * IDEMPOTENT BEHAVIOR / ИДЕМПОТЕНТНОЕ ПОВЕДЕНИЕ:
 * If byte is already at patched value (newB), it's considered success.
 * This allows calling apply multiple times without error.
 * 
 * Если байт уже в пропатченном значении (newB), это считается успехом.
 * Это позволяет вызывать apply много раз без ошибки.
 * 
 * ERROR HANDLING / ОБРАБОТКА ОШИБОК:
 * On error, sets g_last with detailed failure information and rolls back.
 * При ошибке устанавливает g_last с детальной информацией об ошибке и откатывает.
 ******************************************************************************/
int patcher_apply_table_base(BYTE* base, const PatchByte* t, int n)
{
    int i;
    int appliedCount = 0;     // Number of patches actually applied / Количество фактически применённых патчей
    int* appliedIdx = NULL;   // List of applied patch indices for rollback / Список индексов применённых патчей для отката

    patcher_clear_last_error();

    /***************************************************************************
     * VALIDATION PHASE
     * ФАЗА ВАЛИДАЦИИ
     ***************************************************************************/
    
    if (!base) {
        set_err(PATCHER_FAIL_GETMODULE, 0, 0, 0);
        return 0;
    }
    
    // Empty table is success / Пустая таблица - успех
    if (!t || n <= 0) return 1;

    if (!get_image_size(base)) {
        set_err(PATCHER_FAIL_BADPE, 0, 0, 0);
        return 0;
    }

    // Pre-check all RVAs are in bounds / Предварительно проверить что все RVA в границах
    for (i = 0; i < n; i++) {
        if (!is_range_in_image(base, t[i].rva, 1)) {
            set_err(PATCHER_FAIL_OOB, t[i].rva, 0, 0);
            return 0;
        }
    }

    /***************************************************************************
     * ALLOCATE ROLLBACK LIST
     * ВЫДЕЛИТЬ СПИСОК ОТКАТА
     * 
     * Best effort: if allocation fails, we can still patch but without
     * rollback safety. In practice, this allocation rarely fails.
     * 
     * Лучшая попытка: если выделение не удалось, мы всё ещё можем пропатчить
     * но без безопасности отката. На практике это выделение редко терпит неудачу.
     ***************************************************************************/
    appliedIdx = (int*)HeapAlloc(GetProcessHeap(), 0, sizeof(int) * n);
    if (!appliedIdx) {
        appliedIdx = NULL;  // Continue without rollback list / Продолжить без списка отката
    }

    /***************************************************************************
     * PATCHING PHASE
     * ФАЗА ПАТЧИНГА
     ***************************************************************************/
    
    for (i = 0; i < n; i++) {
        DWORD rva = t[i].rva;
        BYTE* p = base + rva;
        BYTE have = *p;  // Current byte value / Текущее значение байта

        if (have == t[i].oldB) {
            /*******************************************************************
             * CASE 1: Byte needs patching (matches original value)
             * СЛУЧАЙ 1: Байт нуждается в патчинге (совпадает с оригинальным значением)
             *******************************************************************/
            if (!write_byte(p, t[i].newB, rva, t[i].newB)) {
                /***************************************************************
                 * Write failed - ROLLBACK all applied patches
                 * Запись не удалась - ОТКАТИТЬ все применённые патчи
                 ***************************************************************/
                int j;
                if (appliedIdx) {
                    for (j = appliedCount - 1; j >= 0; j--) {
                        int idx = appliedIdx[j];
                        DWORD rrva = t[idx].rva;
                        BYTE* rp = base + rrva;
                        // Best effort rollback / Лучшая попытка отката
                        write_byte(rp, t[idx].oldB, rrva, t[idx].oldB);
                    }
                }
                if (appliedIdx) HeapFree(GetProcessHeap(), 0, appliedIdx);
                return 0;
            }

            // Track this patch for potential rollback / Отслеживать этот патч для потенциального отката
            if (appliedIdx) appliedIdx[appliedCount] = i;
            appliedCount++;
        }
        else if (have == t[i].newB) {
            /*******************************************************************
             * CASE 2: Already patched - OK (idempotent)
             * СЛУЧАЙ 2: Уже пропатчен - OK (идемпотентно)
             *******************************************************************/
            // Do nothing - already at target value / Ничего не делать - уже целевое значение
        }
        else {
            /*******************************************************************
             * CASE 3: Unexpected value - FAIL and ROLLBACK
             * СЛУЧАЙ 3: Неожиданное значение - НЕУДАЧА и ОТКАТ
             *******************************************************************/
            set_err(PATCHER_FAIL_MISMATCH, rva, have, t[i].oldB);
            
            // Rollback applied patches / Откатить применённые патчи
            if (appliedIdx) {
                int j;
                for (j = appliedCount - 1; j >= 0; j--) {
                    int idx = appliedIdx[j];
                    DWORD rrva = t[idx].rva;
                    BYTE* rp = base + rrva;
                    write_byte(rp, t[idx].oldB, rrva, t[idx].oldB);
                }
                HeapFree(GetProcessHeap(), 0, appliedIdx);
            }
            return 0;
        }
    }

    // Success - free rollback list / Успех - освободить список отката
    if (appliedIdx) HeapFree(GetProcessHeap(), 0, appliedIdx);
    return 1;
}

/*******************************************************************************
 * patcher_revert_table_base
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Reverts patch table with automatic rollback on any failure.
 * Idempotent: safe to call multiple times.
 * 
 * Откатывает таблицу патчей с автоматическим откатом при любой ошибке.
 * Идемпотентно: безопасно вызывать много раз.
 * 
 * PARAMETERS / ПАРАМЕТРЫ:
 * base - Module base address / Базовый адрес модуля
 * t    - Array of patch byte structures / Массив структур байтов патчей
 * n    - Number of patches / Количество патчей
 * 
 * RETURNS / ВОЗВРАЩАЕТ:
 * 1 if all patches reverted successfully, 0 if any failed (with rollback)
 * 1 если все патчи откачены успешно, 0 если какой-либо не удался (с откатом)
 * 
 * ALGORITHM / АЛГОРИТМ:
 * Mirror of patcher_apply_table_base, but:
 * - Changes newB back to oldB
 * - Idempotent: if already oldB, skip
 * - Rollback: if fails, re-apply newB to what was reverted
 * 
 * Зеркало patcher_apply_table_base, но:
 * - Меняет newB обратно на oldB
 * - Идемпотентно: если уже oldB, пропустить
 * - Откат: если не удалось, повторно применить newB к тому что было откачено
 * 
 * ROLLBACK MECHANISM / МЕХАНИЗМ ОТКАТА:
 * If any revert fails, previously reverted bytes are re-patched to newB.
 * This maintains consistent state: either all reverted or all patched.
 * 
 * Если какой-либо откат не удался, ранее откаченные байты повторно патчатся в newB.
 * Это поддерживает согласованное состояние: либо все откачены либо все пропатчены.
 ******************************************************************************/
int patcher_revert_table_base(BYTE* base, const PatchByte* t, int n)
{
    int i;
    int revertedCount = 0;    // Number of patches actually reverted / Количество фактически откаченных патчей
    int* revertedIdx = NULL;  // List of reverted patch indices for rollback / Список индексов откаченных патчей для отката

    patcher_clear_last_error();

    /***************************************************************************
     * VALIDATION PHASE
     * ФАЗА ВАЛИДАЦИИ
     ***************************************************************************/
    
    if (!base) {
        set_err(PATCHER_FAIL_GETMODULE, 0, 0, 0);
        return 0;
    }
    
    // Empty table is success / Пустая таблица - успех
    if (!t || n <= 0) return 1;

    if (!get_image_size(base)) {
        set_err(PATCHER_FAIL_BADPE, 0, 0, 0);
        return 0;
    }

    // Pre-check bounds / Предварительно проверить границы
    for (i = 0; i < n; i++) {
        if (!is_range_in_image(base, t[i].rva, 1)) {
            set_err(PATCHER_FAIL_OOB, t[i].rva, 0, 0);
            return 0;
        }
    }

    /***************************************************************************
     * ALLOCATE ROLLBACK LIST
     * ВЫДЕЛИТЬ СПИСОК ОТКАТА
     ***************************************************************************/
    revertedIdx = (int*)HeapAlloc(GetProcessHeap(), 0, sizeof(int) * n);
    if (!revertedIdx) revertedIdx = NULL;

    /***************************************************************************
     * REVERTING PHASE
     * ФАЗА ОТКАТА
     ***************************************************************************/
    
    for (i = 0; i < n; i++) {
        DWORD rva = t[i].rva;
        BYTE* p = base + rva;
        BYTE have = *p;

        if (have == t[i].newB) {
            /*******************************************************************
             * CASE 1: Byte needs reverting (matches patched value)
             * СЛУЧАЙ 1: Байт нуждается в откате (совпадает с пропатченным значением)
             *******************************************************************/
            if (!write_byte(p, t[i].oldB, rva, t[i].oldB)) {
                /***************************************************************
                 * Write failed - ROLLBACK (re-apply newB to reverted bytes)
                 * Запись не удалась - ОТКАТ (повторно применить newB к откаченным байтам)
                 ***************************************************************/
                if (revertedIdx) {
                    int j;
                    for (j = revertedCount - 1; j >= 0; j--) {
                        int idx = revertedIdx[j];
                        DWORD rrva = t[idx].rva;
                        BYTE* rp = base + rrva;
                        write_byte(rp, t[idx].newB, rrva, t[idx].newB);
                    }
                    HeapFree(GetProcessHeap(), 0, revertedIdx);
                }
                return 0;
            }

            // Track for rollback / Отслеживать для отката
            if (revertedIdx) revertedIdx[revertedCount] = i;
            revertedCount++;
        }
        else if (have == t[i].oldB) {
            /*******************************************************************
             * CASE 2: Already at original value - OK (idempotent)
             * СЛУЧАЙ 2: Уже оригинальное значение - OK (идемпотентно)
             *******************************************************************/
            // Do nothing / Ничего не делать
        }
        else {
            /*******************************************************************
             * CASE 3: Unexpected value - FAIL and ROLLBACK
             * СЛУЧАЙ 3: Неожиданное значение - НЕУДАЧА и ОТКАТ
             *******************************************************************/
            set_err(PATCHER_FAIL_MISMATCH, rva, have, t[i].newB);
            
            // Rollback: re-apply newB / Откат: повторно применить newB
            if (revertedIdx) {
                int j;
                for (j = revertedCount - 1; j >= 0; j--) {
                    int idx = revertedIdx[j];
                    DWORD rrva = t[idx].rva;
                    BYTE* rp = base + rrva;
                    write_byte(rp, t[idx].newB, rrva, t[idx].newB);
                }
                HeapFree(GetProcessHeap(), 0, revertedIdx);
            }
            return 0;
        }
    }

    // Success - free rollback list / Успех - освободить список отката
    if (revertedIdx) HeapFree(GetProcessHeap(), 0, revertedIdx);
    return 1;
}

/*******************************************************************************
 * CONVENIENCE WRAPPERS (BACKWARD COMPATIBLE)
 * УДОБНЫЕ ОБЁРТКИ (ОБРАТНО СОВМЕСТИМЫЕ)
 * 
 * These wrappers automatically use current process EXE as base module.
 * Provided for backward compatibility and convenience.
 * 
 * Эти обёртки автоматически используют EXE текущего процесса как базовый модуль.
 * Предоставлены для обратной совместимости и удобства.
 ******************************************************************************/

/*******************************************************************************
 * patcher_apply_rva_table
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Convenience wrapper for applying patches to current process EXE.
 * Удобная обёртка для применения патчей к EXE текущего процесса.
 * 
 * PARAMETERS / ПАРАМЕТРЫ:
 * t - Array of patch byte structures / Массив структур байтов патчей
 * n - Number of patches / Количество патчей
 * 
 * RETURNS / ВОЗВРАЩАЕТ:
 * 1 if all patches applied successfully, 0 on failure
 * 1 если все патчи применены успешно, 0 при ошибке
 * 
 * USAGE / ИСПОЛЬЗОВАНИЕ:
 * Most common case - patching winamp.exe from plugin.
 * Самый распространённый случай - патчинг winamp.exe из плагина.
 ******************************************************************************/
int patcher_apply_rva_table(const PatchByte* t, int n)
{
    BYTE* base = patcher_get_module_baseA(NULL);  // NULL = winamp.exe
    if (!base) return 0;
    return patcher_apply_table_base(base, t, n);
}

/*******************************************************************************
 * patcher_revert_rva_table
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Convenience wrapper for reverting patches from current process EXE.
 * Удобная обёртка для отката патчей из EXE текущего процесса.
 * 
 * PARAMETERS / ПАРАМЕТРЫ:
 * t - Array of patch byte structures / Массив структур байтов патчей
 * n - Number of patches / Количество патчей
 * 
 * RETURNS / ВОЗВРАЩАЕТ:
 * 1 if all patches reverted successfully, 0 on failure
 * 1 если все патчи откачены успешно, 0 при ошибке
 * 
 * USAGE / ИСПОЛЬЗОВАНИЕ:
 * Called during plugin cleanup to restore original code.
 * Вызывается во время очистки плагина для восстановления оригинального кода.
 ******************************************************************************/
int patcher_revert_rva_table(const PatchByte* t, int n)
{
    BYTE* base = patcher_get_module_baseA(NULL);  // NULL = winamp.exe
    if (!base) return 0;
    return patcher_revert_table_base(base, t, n);
}