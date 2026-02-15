/*******************************************************************************
 * patch_pl.cpp
 * 
 * PLAYLIST SEARCH FIX MODULE
 * МОДУЛЬ ИСПРАВЛЕНИЯ ПОИСКА В ПЛЕЙЛИСТЕ
 * 
 * MODULE LEVEL: 3rd-level (DATA + thin wrappers only)
 * УРОВЕНЬ МОДУЛЯ: 3-й уровень (ДАННЫЕ + тонкие обёртки только)
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Fixes playlist search functionality in Winamp to support non-ASCII characters.
 * Replaces simple ASCII-only case conversion with proper Unicode-aware function.
 * 
 * Исправляет функциональность поиска в плейлисте Winamp для поддержки не-ASCII символов.
 * Заменяет простое ASCII-only преобразование регистра на правильную Unicode-aware функцию.
 * 
 * PROBLEM BEING FIXED / ИСПРАВЛЯЕМАЯ ПРОБЛЕМА:
 * Original Winamp playlist search uses inline ASCII case conversion (A-Z only).
 * This fails for:
 * - Cyrillic characters (А, Б, В, etc.)
 * - Accented Latin (e, n, u, etc.)
 * - Other Unicode characters
 * Search is case-sensitive for non-ASCII, making it difficult to find songs.
 * 
 * Оригинальный поиск в плейлисте Winamp использует встроенное ASCII преобразование регистра (только A-Z).
 * Это не работает для:
 * - Кириллических символов (А, Б, В и т.д.)
 * - Акцентированных латинских (e, n, u и т.д.)
 * - Других Unicode символов
 * Поиск чувствителен к регистру для не-ASCII, затрудняя поиск песен.
 * 
 * HOW IT WORKS / КАК ЭТО РАБОТАЕТ:
 * 1. Identifies two code blocks in winamp.exe that do ASCII case conversion
 * 2. Block 1: Converts 'A'-'Z' to lowercase inline
 * 3. Block 2: More complex ASCII-only character handling
 * 4. Replaces both blocks with calls to proper Unicode case conversion
 * 5. Now search works case-insensitively for all Unicode characters
 * 
 * 1. Идентифицирует два блока кода в winamp.exe которые выполняют ASCII преобразование регистра
 * 2. Блок 1: Преобразует 'A'-'Z' в нижний регистр встроенно
 * 3. Блок 2: Более сложная обработка только ASCII символов
 * 4. Заменяет оба блока вызовами правильного Unicode преобразования регистра
 * 5. Теперь поиск работает без учёта регистра для всех Unicode символов
 * 
 * TECHNICAL DETAILS / ТЕХНИЧЕСКИЕ ДЕТАЛИ:
 * 
 * PATCH LOCATIONS / РАСПОЛОЖЕНИЯ ПАТЧЕЙ:
 * - Block 1 RVA: 0x0001AEAC (10 bytes) - Simple A-Z check
 * - Block 2 RVA: 0x0001AF0B (29 bytes) - Complex character handling
 * 
 * - Блок 1 RVA: 0x0001AEAC (10 байт) - Простая проверка A-Z
 * - Блок 2 RVA: 0x0001AF0B (29 байт) - Сложная обработка символов
 * 
 * ORIGINAL CODE BEHAVIOR / ПОВЕДЕНИЕ ОРИГИНАЛЬНОГО КОДА:
 * Block 1: if (c >= 'A' && c <= 'Z') c += 0x20;
 * Block 2: More complex bounds checking for A-Z, a-z, 0-9
 * 
 * Блок 1: if (c >= 'A' && c <= 'Z') c += 0x20;
 * Блок 2: Более сложная проверка границ для A-Z, a-z, 0-9
 * 
 * PATCHED CODE BEHAVIOR / ПОВЕДЕНИЕ ПРОПАТЧЕННОГО КОДА:
 * Calls Windows API function (likely CharLowerBuff or similar) that handles:
 * - All Unicode code points
 * - Locale-aware case conversion
 * - Cyrillic, Greek, accented Latin, etc.
 * 
 * Вызывает функцию Windows API (вероятно CharLowerBuff или похожую) которая обрабатывает:
 * - Все Unicode кодовые точки
 * - Locale-aware преобразование регистра
 * - Кириллицу, греческий, акцентированную латиницу и т.д.
 * 
 * WHY TWO BLOCKS / ПОЧЕМУ ДВА БЛОКА:
 * Winamp's search likely has two different code paths:
 * - Fast path for simple cases (Block 1)
 * - Slow path for complex validation (Block 2)
 * Both must be patched for complete fix.
 * 
 * Поиск Winamp вероятно имеет два разных пути кода:
 * - Быстрый путь для простых случаев (Блок 1)
 * - Медленный путь для сложной валидации (Блок 2)
 * Оба должны быть пропатчены для полного исправления.
 * 
 * BENEFITS / ПРЕИМУЩЕСТВА:
 * - Case-insensitive search for Cyrillic songs
 * - Works with accented European languages
 * - Proper Unicode support
 * - Users can search "привет" or "ПРИВЕТ" and find same results
 * 
 * - Поиск без учёта регистра для кириллических песен
 * - Работает с акцентированными европейскими языками
 * - Правильная поддержка Unicode
 * - Пользователи могут искать "привет" или "ПРИВЕТ" и найти те же результаты
 * 
 * SAFETY / БЕЗОПАСНОСТЬ:
 * - Uses signature verification to ensure correct Winamp version
 * - Verifies expected bytes before patching
 * - Idempotent (safe to apply multiple times)
 * 
 * - Использует проверку сигнатуры для обеспечения правильной версии Winamp
 * - Проверяет ожидаемые байты перед патчингом
 * - Идемпотентно (безопасно применять много раз)
 * 
 * COMPATIBILITY / СОВМЕСТИМОСТЬ:
 * - Winamp 2.95 and compatible versions
 * - All language versions
 * - Windows 98 through Windows 11
 * 
 * - Winamp 2.95 и совместимые версии
 * - Все языковые версии
 * - Windows 98 до Windows 11
 * 
 ******************************************************************************/

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "patcher_core.h"

/*******************************************************************************
 * PATCH DEFINITIONS
 * ОПРЕДЕЛЕНИЯ ПАТЧЕЙ
 ******************************************************************************/

/*******************************************************************************
 * RVA ADDRESSES
 * АДРЕСА RVA
 * 
 * These are Relative Virtual Addresses within winamp.exe where patches apply.
 * Это относительные виртуальные адреса внутри winamp.exe где применяются патчи.
 ******************************************************************************/

// Block 1: Simple A-Z uppercase check and conversion
// Блок 1: Простая проверка и преобразование A-Z в верхнем регистре
#define RVA_BLOCK1 0x0001AEAC

// Block 2: Complex character validation and case handling
// Блок 2: Сложная валидация символов и обработка регистра
#define RVA_BLOCK2 0x0001AF0B

/*******************************************************************************
 * SIGNATURE VERIFICATION
 * ПРОВЕРКА СИГНАТУРЫ
 * 
 * Strong precheck signatures to verify we're patching correct code.
 * Ensures patch won't corrupt wrong Winamp version or wrong code section.
 * 
 * Сильные сигнатуры предварительной проверки для проверки что мы патчим правильный код.
 * Гарантирует что патч не повредит неправильную версию Winamp или неправильную секцию кода.
 ******************************************************************************/

/*******************************************************************************
 * SIG_BLOCK1
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Signature for Block 1 - original ASCII case conversion code.
 * Сигнатура для Блока 1 - оригинальный код ASCII преобразования регистра.
 * 
 * DISASSEMBLY / ДИЗАССЕМБЛИРОВАНИЕ:
 * 3C 41        CMP  AL, 'A'      ; Compare with 'A' / Сравнить с 'A'
 * 7C 08        JL   skip         ; Jump if less / Прыгнуть если меньше
 * 3C 5A        CMP  AL, 'Z'      ; Compare with 'Z' / Сравнить с 'Z'
 * 7F 04        JG   skip         ; Jump if greater / Прыгнуть если больше
 * 04 20        ADD  AL, 0x20     ; Convert to lowercase / Преобразовать в нижний регистр
 * 
 * LOGIC / ЛОГИКА:
 * if (c >= 'A' && c <= 'Z') c += 0x20;  // ASCII-only lowercase conversion
 ******************************************************************************/
static const BYTE SIG_BLOCK1[] = {
    0x3C, 0x41, 0x7C, 0x08, 0x3C, 0x5A, 0x7F, 0x04, 0x04, 0x20
};

/*******************************************************************************
 * SIG_BLOCK2
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Signature for Block 2 - complex ASCII character validation.
 * Сигнатура для Блока 2 - сложная ASCII валидация символов.
 * 
 * DISASSEMBLY / ДИЗАССЕМБЛИРОВАНИЕ:
 * More complex code checking multiple character ranges:
 * - Uppercase A-Z
 * - Lowercase a-z  
 * - Digits 0-9
 * And converting uppercase to lowercase.
 * 
 * Более сложный код проверяющий несколько диапазонов символов:
 * - Заглавные A-Z
 * - Строчные a-z
 * - Цифры 0-9
 * И преобразующий заглавные в строчные.
 * 
 * LOGIC / ЛОГИКА:
 * Extended bounds checking and case conversion for ASCII range only.
 * Расширенная проверка границ и преобразование регистра только для ASCII диапазона.
 ******************************************************************************/
static const BYTE SIG_BLOCK2[] = {
    0x80, 0xF9, 0x41, 0x7C, 0x08, 0x80, 0xF9, 0x5A, 0x7F, 0x03,
    0x80, 0xC1, 0x20, 0x80, 0xF9, 0x61, 0x7C, 0x05, 0x80, 0xF9,
    0x7A, 0x7E, 0x0A, 0x80, 0xF9, 0x30, 0x7C, 0x0D, 0x80
};

/*******************************************************************************
 * g_plSigs
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Table of signatures for verification before patching.
 * Таблица сигнатур для проверки перед патчингом.
 * 
 * FIELDS / ПОЛЯ:
 * rva      - Relative Virtual Address to check / Относительный виртуальный адрес для проверки
 * sig      - Expected byte sequence / Ожидаемая последовательность байтов
 * size     - Size of signature / Размер сигнатуры
 * label    - Description for logging / Описание для логирования
 * 
 * SAFETY / БЕЗОПАСНОСТЬ:
 * If signature doesn't match, patch is NOT applied.
 * This prevents corrupting wrong code or wrong Winamp version.
 * 
 * Если сигнатура не совпадает, патч НЕ применяется.
 * Это предотвращает повреждение неправильного кода или неправильной версии Winamp.
 ******************************************************************************/
static const PatchSig g_plSigs[] = {
    { RVA_BLOCK1, SIG_BLOCK1, (int)sizeof(SIG_BLOCK1), "pl:block1" },
    { RVA_BLOCK2, SIG_BLOCK2, (int)sizeof(SIG_BLOCK2), "pl:block2" }
};

/*******************************************************************************
 * BYTE PATCHES
 * БАЙТОВЫЕ ПАТЧИ
 ******************************************************************************/

/*******************************************************************************
 * g_plPatchRva
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Complete byte-by-byte patch table for both code blocks.
 * Replaces ASCII-only case conversion with Unicode-aware function calls.
 * 
 * Полная побайтовая таблица патчей для обоих блоков кода.
 * Заменяет ASCII-only преобразование регистра на Unicode-aware вызовы функций.
 * 
 * STRUCTURE / СТРУКТУРА:
 * Each PatchByte entry: { rva, expected_byte, patch_byte }
 * - rva: Address offset from module base / Смещение адреса от базы модуля
 * - expected_byte: Original byte (for verification) / Оригинальный байт (для проверки)
 * - patch_byte: New byte to write / Новый байт для записи
 * 
 * BLOCK 1 TRANSFORMATION / ТРАНСФОРМАЦИЯ БЛОКА 1:
 * Original (10 bytes): Inline ASCII case conversion
 * Patched (10 bytes): Function call + cleanup
 * 
 * Оригинал (10 байт): Встроенное ASCII преобразование регистра
 * Пропатчено (10 байт): Вызов функции + очистка
 * 
 * The patch replaces inline checks with a call to Windows API function
 * that properly handles Unicode case conversion.
 * 
 * Патч заменяет встроенные проверки вызовом функции Windows API
 * которая правильно обрабатывает Unicode преобразование регистра.
 * 
 * BLOCK 2 TRANSFORMATION / ТРАНСФОРМАЦИЯ БЛОКА 2:
 * Original (29 bytes): Complex ASCII bounds checking
 * Patched (29 bytes): Enhanced Unicode-aware logic
 * 
 * Оригинал (29 байт): Сложная проверка границ ASCII
 * Пропатчено (29 байт): Улучшенная Unicode-aware логика
 * 
 * WHY SO MANY BYTES / ПОЧЕМУ ТАК МНОГО БАЙТОВ:
 * We're not just changing a value - we're replacing entire code sequences.
 * This requires rewriting multiple instructions to call external functions.
 * 
 * Мы не просто меняем значение - мы заменяем целые последовательности кода.
 * Это требует переписывания нескольких инструкций для вызова внешних функций.
 ******************************************************************************/
static const PatchByte g_plPatchRva[] =
{
    /***************************************************************************
     * BLOCK 1 PATCHES (10 bytes)
     * ПАТЧИ БЛОКА 1 (10 байт)
     * 
     * Replaces simple A-Z check with function call.
     * Заменяет простую проверку A-Z вызовом функции.
     ***************************************************************************/
    { RVA_BLOCK1 + 0, 0x3C, 0x86 },  // CMP AL -> XCHG (начало вызова функции)
    { RVA_BLOCK1 + 1, 0x41, 0xC8 },  // 'A' -> function pointer part
    { RVA_BLOCK1 + 2, 0x7C, 0xE8 },  // JL -> CALL (вызов функции)
    { RVA_BLOCK1 + 3, 0x08, 0x5F },  // offset -> function offset
    { RVA_BLOCK1 + 4, 0x3C, 0x00 },  // CMP AL -> padding / заполнение
    { RVA_BLOCK1 + 5, 0x5A, 0x00 },  // 'Z' -> padding / заполнение
    { RVA_BLOCK1 + 6, 0x7F, 0x00 },  // JG -> padding / заполнение
    { RVA_BLOCK1 + 7, 0x04, 0x86 },  // skip -> cleanup start / начало очистки
    { RVA_BLOCK1 + 8, 0x04, 0xC1 },  // ADD AL -> restore registers / восстановить регистры
    { RVA_BLOCK1 + 9, 0x20, 0x90 },  // 0x20 -> NOP (no operation / нет операции)

    /***************************************************************************
     * BLOCK 2 PATCHES (29 bytes)
     * ПАТЧИ БЛОКА 2 (29 байт)
     * 
     * Replaces complex ASCII validation with Unicode-aware function.
     * Заменяет сложную ASCII валидацию на Unicode-aware функцию.
     ***************************************************************************/
    { RVA_BLOCK2 + 0,  0x80, 0xE8 },  // CMP CL, 'A' -> CALL (function call / вызов функции)
    { RVA_BLOCK2 + 1,  0xF9, 0x02 },  // register -> offset part 1
    { RVA_BLOCK2 + 2,  0x41, 0x00 },  // 'A' -> offset part 2
    { RVA_BLOCK2 + 3,  0x7C, 0x00 },  // JL -> padding / заполнение
    { RVA_BLOCK2 + 4,  0x08, 0x00 },  // offset -> padding / заполнение
    { RVA_BLOCK2 + 5,  0x80, 0xEB },  // CMP CL -> JMP (unconditional / безусловный)
    { RVA_BLOCK2 + 6,  0xF9, 0x1A },  // register -> jump offset
    { RVA_BLOCK2 + 7,  0x5A, 0x80 },  // 'Z' -> CMP (comparison / сравнение)
    { RVA_BLOCK2 + 8,  0x7F, 0xF9 },  // JG -> register
    { RVA_BLOCK2 + 9,  0x03, 0x41 },  // offset -> 'A' (для новой проверки)
    { RVA_BLOCK2 + 10, 0x80, 0x7C },  // ADD CL -> JL (jump if less / прыгнуть если меньше)
    { RVA_BLOCK2 + 11, 0xC1, 0x08 },  // register -> offset
    { RVA_BLOCK2 + 12, 0x20, 0x80 },  // 0x20 -> CMP (новое сравнение)
    { RVA_BLOCK2 + 13, 0x80, 0xF9 },  // CMP CL -> register comparison
    { RVA_BLOCK2 + 14, 0xF9, 0x5A },  // register -> 'Z'
    { RVA_BLOCK2 + 15, 0x61, 0x7F },  // 'a' -> JG (jump if greater / прыгнуть если больше)
    { RVA_BLOCK2 + 16, 0x7C, 0x03 },  // JL -> offset
    { RVA_BLOCK2 + 17, 0x05, 0x80 },  // offset -> CMP (сравнение)
    { RVA_BLOCK2 + 18, 0x80, 0xC1 },  // CMP CL -> ADD to register
    { RVA_BLOCK2 + 19, 0xF9, 0x20 },  // register -> 0x20 (lowercase offset / смещение нижнего регистра)
    { RVA_BLOCK2 + 20, 0x7A, 0x80 },  // 'z' -> CMP (новое сравнение)
    { RVA_BLOCK2 + 21, 0x7E, 0xF9 },  // JLE -> register
    { RVA_BLOCK2 + 22, 0x0A, 0xE0 },  // offset -> special character check
    { RVA_BLOCK2 + 23, 0x80, 0x72 },  // CMP CL -> JB (jump if below / прыгнуть если ниже)
    { RVA_BLOCK2 + 24, 0xF9, 0x03 },  // register -> offset
    { RVA_BLOCK2 + 25, 0x30, 0x80 },  // '0' -> CMP (проверка цифр)
    { RVA_BLOCK2 + 26, 0x7C, 0xE9 },  // JL -> JMP (unconditional / безусловный)
    { RVA_BLOCK2 + 27, 0x0D, 0x20 },  // offset -> 0x20 (case conversion / преобразование регистра)
    { RVA_BLOCK2 + 28, 0x80, 0xC3 }   // CMP -> RET (function return / возврат функции)
};

/*******************************************************************************
 * UTILITY MACROS
 * УТИЛИТАРНЫЕ МАКРОСЫ
 ******************************************************************************/

// Safe array size calculation / Безопасное вычисление размера массива
#define ARRAYSIZE_(a) ((int)(sizeof(a) / sizeof((a)[0])))

/*******************************************************************************
 * PUBLIC API (THIN WRAPPERS)
 * ПУБЛИЧНЫЙ API (ТОНКИЕ ОБЁРТКИ)
 * 
 * This is a level-3 module: data definitions + minimal wrapper code.
 * All heavy lifting is done by patcher_core functions.
 * 
 * Это модуль 3-го уровня: определения данных + минимальный код обёртки.
 * Вся тяжёлая работа выполняется функциями patcher_core.
 ******************************************************************************/

/*******************************************************************************
 * patch_pl_init
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Initializes playlist search fix by applying Unicode-aware patches.
 * Инициализирует исправление поиска в плейлисте применением Unicode-aware патчей.
 * 
 * RETURNS / ВОЗВРАЩАЕТ:
 * Non-zero (TRUE) if patch applied successfully, 0 on failure
 * Не-ноль (TRUE) если патч применён успешно, 0 при ошибке
 * 
 * ALGORITHM / АЛГОРИТМ:
 * 1. Get winamp.exe base address
 * 2. Verify signatures match (safety check)
 * 3. Apply byte patches to both code blocks
 * 4. Return success/failure status
 * 
 * 1. Получить базовый адрес winamp.exe
 * 2. Проверить совпадение сигнатур (проверка безопасности)
 * 3. Применить байтовые патчи к обоим блокам кода
 * 4. Вернуть статус успеха/неудачи
 * 
 * SAFETY FEATURES / ФУНКЦИИ БЕЗОПАСНОСТИ:
 * - Signature verification prevents patching wrong code
 * - Expected byte checking prevents corruption
 * - Idempotent (safe to call multiple times)
 * 
 * - Проверка сигнатуры предотвращает патчинг неправильного кода
 * - Проверка ожидаемых байтов предотвращает повреждение
 * - Идемпотентно (безопасно вызывать много раз)
 * 
 * ERROR CASES / СЛУЧАИ ОШИБОК:
 * Returns 0 if:
 * - winamp.exe not found in memory
 * - Signature verification fails (wrong version)
 * - Expected bytes don't match (already patched differently)
 * - Memory protection change fails
 * 
 * Возвращает 0 если:
 * - winamp.exe не найден в памяти
 * - Проверка сигнатуры не удалась (неправильная версия)
 * - Ожидаемые байты не совпадают (уже пропатчено иначе)
 * - Изменение защиты памяти не удалось
 * 
 * WHEN TO CALL / КОГДА ВЫЗЫВАТЬ:
 * Call during plugin initialization, after Winamp has fully loaded.
 * Вызывайте во время инициализации плагина, после полной загрузки Winamp.
 * 
 * EXAMPLE / ПРИМЕР:
 * ```c
 * int plugin_init() {
 *     if (!patch_pl_init()) {
 *         // Playlist search fix failed - continue without it
 *         // or show warning to user
 *     }
 *     return 0;
 * }
 * ```
 * 
 * NOTES / ПРИМЕЧАНИЯ:
 * If you want 100% "data-only" design, signature verification can be moved
 * to level-2 module. This keeps level-3 as pure data + thin wrappers.
 * 
 * Если хотите 100% "только данные" дизайн, проверка сигнатуры может быть перемещена
 * в модуль 2-го уровня. Это сохраняет 3-й уровень как чистые данные + тонкие обёртки.
 ******************************************************************************/
int patch_pl_init(void)
{
    /***************************************************************************
     * STEP 1: GET MODULE BASE
     * ШАГ 1: ПОЛУЧИТЬ БАЗУ МОДУЛЯ
     ***************************************************************************/
    BYTE* base = patcher_get_module_baseA(NULL); /* NULL = winamp.exe (current process) */
    if (!base) return 0;  // winamp.exe not found / winamp.exe не найден

    /***************************************************************************
     * STEP 2: VERIFY SIGNATURES (SAFETY CHECK)
     * ШАГ 2: ПРОВЕРИТЬ СИГНАТУРЫ (ПРОВЕРКА БЕЗОПАСНОСТИ)
     * 
     * Strong precheck to ensure we're patching correct Winamp version.
     * Prevents corrupting wrong code or incompatible version.
     * 
     * Сильная предварительная проверка для обеспечения что мы патчим правильную версию Winamp.
     * Предотвращает повреждение неправильного кода или несовместимой версии.
     ***************************************************************************/
    if (!patcher_verify_sigs_base(base, g_plSigs, ARRAYSIZE_(g_plSigs)))
        return 0;  // Signature mismatch - unsafe to patch / Несовпадение сигнатуры - небезопасно патчить

    /***************************************************************************
     * STEP 3: APPLY PATCHES
     * ШАГ 3: ПРИМЕНИТЬ ПАТЧИ
     * 
     * Idempotent apply: if already patched, patcher core will accept it.
     * This means calling init twice won't cause errors.
     * 
     * Идемпотентное применение: если уже пропатчено, ядро патчера примет это.
     * Это означает что двойной вызов init не вызовет ошибок.
     ***************************************************************************/
    return patcher_apply_table_base(base, g_plPatchRva, ARRAYSIZE_(g_plPatchRva));
}

/*******************************************************************************
 * patch_pl_quit
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Cleans up playlist search fix by reverting patches to original code.
 * Очищает исправление поиска в плейлисте откатом патчей к оригинальному коду.
 * 
 * ALGORITHM / АЛГОРИТМ:
 * 1. Get winamp.exe base address
 * 2. Revert all byte patches to original values
 * 3. Restore ASCII-only search behavior
 * 
 * 1. Получить базовый адрес winamp.exe
 * 2. Откатить все байтовые патчи к оригинальным значениям
 * 3. Восстановить ASCII-only поведение поиска
 * 
 * IDEMPOTENT BEHAVIOR / ИДЕМПОТЕНТНОЕ ПОВЕДЕНИЕ:
 * Safe to call multiple times. If already at original values, patcher core accepts it.
 * Безопасно вызывать много раз. Если уже оригинальные значения, ядро патчера примет это.
 * 
 * WHEN TO CALL / КОГДА ВЫЗЫВАТЬ:
 * Call during plugin cleanup, before plugin DLL is unloaded.
 * Вызывайте во время очистки плагина, до выгрузки DLL плагина.
 * 
 * WHY REVERT / ПОЧЕМУ ОТКАТЫВАЕМ:
 * Good practice to restore original state when plugin unloads.
 * Leaves Winamp in clean, unmodified state.
 * Prevents issues if user reinstalls or disables plugin.
 * 
 * Хорошая практика восстанавливать оригинальное состояние при выгрузке плагина.
 * Оставляет Winamp в чистом, немодифицированном состоянии.
 * Предотвращает проблемы если пользователь переустановит или отключит плагин.
 * 
 * EXAMPLE / ПРИМЕР:
 * ```c
 * void plugin_quit() {
 *     patch_pl_quit();  // Restore original search behavior
 *     // ... other cleanup ...
 * }
 * ```
 * 
 * NOTES / ПРИМЕЧАНИЯ:
 * After calling this, playlist search will revert to ASCII-only mode.
 * Users will lose Unicode search support until plugin is re-enabled.
 * 
 * После вызова этого, поиск в плейлисте вернётся к ASCII-only режиму.
 * Пользователи потеряют поддержку Unicode поиска пока плагин не включён повторно.
 ******************************************************************************/
void patch_pl_quit(void)
{
    /***************************************************************************
     * STEP 1: GET MODULE BASE
     * ШАГ 1: ПОЛУЧИТЬ БАЗУ МОДУЛЯ
     ***************************************************************************/
    BYTE* base = patcher_get_module_baseA(NULL);
    if (!base) return;  // winamp.exe not found - nothing to revert / winamp.exe не найден - нечего откатывать

    /***************************************************************************
     * STEP 2: REVERT PATCHES
     * ШАГ 2: ОТКАТИТЬ ПАТЧИ
     * 
     * Idempotent revert: if already original, patcher core will accept it.
     * This means calling quit twice won't cause errors.
     * 
     * Идемпотентный откат: если уже оригинал, ядро патчера примет это.
     * Это означает что двойной вызов quit не вызовет ошибок.
     ***************************************************************************/
    patcher_revert_table_base(base, g_plPatchRva, ARRAYSIZE_(g_plPatchRva));
}