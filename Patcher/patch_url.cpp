/*******************************************************************************
 * patch_url.cpp
 * 
 * SKINS SERVER URL REDIRECTION PATCH
 * ПАТЧ ПЕРЕНАПРАВЛЕНИЯ URL СЕРВЕРА СКИНОВ
 * 
 * MODULE LEVEL: 3rd-level (DATA + thin wrappers only)
 * УРОВЕНЬ МОДУЛЯ: 3-й уровень (ДАННЫЕ + тонкие обёртки только)
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Redirects Winamp's built-in skin download URL from original server to alternative.
 * Changes hardcoded URL string in winamp.exe memory.
 * 
 * Перенаправляет встроенный URL загрузки скинов Winamp с оригинального сервера на альтернативный.
 * Изменяет жёстко закодированную строку URL в памяти winamp.exe.
 * 
 * URL TRANSFORMATION / ТРАНСФОРМАЦИЯ URL:
 * Original:  "://www.winamp.com/skins/"
 * Patched:   "s://skins.webamp.org    "
 * 
 * Оригинал:  "://www.winamp.com/skins/"
 * Пропатчено: "s://skins.webamp.org    "
 * 
 * COMPLETE URLS / ПОЛНЫЕ URL:
 * Original:  http://www.winamp.com/skins/
 * Patched:   https://skins.webamp.org
 * 
 * Оригинал:  http://www.winamp.com/skins/
 * Пропатчено: https://skins.webamp.org
 * 
 * HOW IT WORKS / КАК ЭТО РАБОТАЕТ:
 * 1. Locates hardcoded URL string in winamp.exe data section
 * 2. Byte-by-byte replaces characters with new URL
 * 3. Pads with spaces to maintain string length (23 bytes)
 * 4. Winamp now connects to webamp.org instead
 * 
 * 1. Находит жёстко закодированную строку URL в секции данных winamp.exe
 * 2. Побайтово заменяет символы новым URL
 * 3. Дополняет пробелами для сохранения длины строки (23 байта)
 * 4. Winamp теперь подключается к webamp.org вместо этого
 * 
 * 
 ******************************************************************************/

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "patcher_core.h"

/*******************************************************************************
 * URL_RVA_BASE
 * 
 * Base RVA address where URL string section begins.
 * This is the starting point for calculating character offsets.
 * 
 * Базовый адрес RVA где начинается секция строки URL.
 * Это начальная точка для вычисления смещений символов.
 ******************************************************************************/
#define URL_RVA_BASE 0x0004DA1D

/*******************************************************************************
 * g_urlPatchRva
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Byte-by-byte patch table to transform URL string.
 * Each entry changes one character from original to new URL.
 * 
 * Побайтовая таблица патчей для трансформации строки URL.
 * Каждый элемент изменяет один символ с оригинального на новый URL.
 * 
 * OFFSET CALCULATION / ВЫЧИСЛЕНИЕ СМЕЩЕНИЯ:
 * URL_RVA_BASE + (absolute_address - reference_address)
 * Example: URL_RVA_BASE + (0x004CC24 - 0x004CC1D) = URL_RVA_BASE + 7
 * 
 * URL_RVA_BASE + (абсолютный_адрес - адрес_ссылки)
 * Пример: URL_RVA_BASE + (0x004CC24 - 0x004CC1D) = URL_RVA_BASE + 7
 * 
 * CHARACTER MAPPING / СОПОСТАВЛЕНИЕ СИМВОЛОВ:
 * Position  Original  Patched   Meaning / Значение
 * --------  --------  -------   -----------------
 *    +7       ':'       's'     Start of protocol / Начало протокола
 *    +8       '/'       ':'     Protocol separator / Разделитель протокола
 *    +9       '/'       '/'     Protocol separator / Разделитель протокола
 *   +10       'w'       's'     Domain start: www -> skins
 *   +11       'w'       'k'     Domain: www -> skins
 *   +12       'w'       'i'     Domain: www -> skins
 *   +13       '.'       'n'     Domain: . -> n
 *   +14       'w'       's'     Domain: winamp -> skins (end)
 *   +15       'i'       '.'     Domain separator / Разделитель домена
 *   +16       'n'       'w'     Domain: winamp -> webamp
 *   +17       'a'       'e'     Domain: winamp -> webamp
 *   +18       'm'       'b'     Domain: winamp -> webamp
 *   +19       'p'       'a'     Domain: winamp -> webamp
 *   +20       '.'       'm'     Domain: winamp -> webamp (end)
 *   +21       'c'       'p'     TLD: com -> org
 *   +22       'o'       '.'     TLD: com -> org
 *   +23       'm'       'o'     TLD: com -> org
 *   +24       '/'       'r'     TLD: com -> org (end)
 *   +25       's'       'g'     Path removed, padding / Путь удалён, дополнение
 *   +26       'k'     space     Padding / Дополнение
 *   +27       'i'     space     Padding / Дополнение
 *   +28       'n'     space     Padding / Дополнение
 *   +29       's'     space     Padding / Дополнение
 *   +30       '/'     space     Padding / Дополнение (likely null terminator / вероятно нулевой терминатор)
 * 
 * FINAL RESULT / ФИНАЛЬНЫЙ РЕЗУЛЬТАТ:
 * "://www.winamp.com/skins/" -> "s://skins.webamp.org    "
 * http://www.winamp.com/skins/ -> https://skins.webamp.org
 ******************************************************************************/
static const PatchByte g_urlPatchRva[] =
{
    /* Position +7: ':' -> 's' (protocol: http -> https) */
    { URL_RVA_BASE + (0x004CC24 - 0x004CC1D), 0x3A, 0x73 },
    
    /* Position +8: '/' -> ':' (protocol separator) */
    { URL_RVA_BASE + (0x004CC25 - 0x004CC1D), 0x2F, 0x3A },
    
    /* Position +9: '/' stays '/' (protocol separator) */
    { URL_RVA_BASE + (0x004CC27 - 0x004CC1D), 0x77, 0x2F },
    
    /* Position +10: 'w' -> 's' (www -> skins start) */
    { URL_RVA_BASE + (0x004CC28 - 0x004CC1D), 0x77, 0x73 },
    
    /* Position +11: 'w' -> 'k' (www -> skins) */
    { URL_RVA_BASE + (0x004CC29 - 0x004CC1D), 0x77, 0x6B },
    
    /* Position +12: 'w' -> 'i' (www -> skins) */
    { URL_RVA_BASE + (0x004CC2A - 0x004CC1D), 0x2E, 0x69 },
    
    /* Position +13: '.' -> 'n' (www. -> skins) */
    { URL_RVA_BASE + (0x004CC2B - 0x004CC1D), 0x77, 0x6E },
    
    /* Position +14: 'w' -> 's' (winamp -> skins end) */
    { URL_RVA_BASE + (0x004CC2C - 0x004CC1D), 0x69, 0x73 },
    
    /* Position +15: 'i' -> '.' (winamp -> .webamp separator) */
    { URL_RVA_BASE + (0x004CC2D - 0x004CC1D), 0x6E, 0x2E },
    
    /* Position +16: 'n' -> 'w' (winamp -> webamp) */
    { URL_RVA_BASE + (0x004CC2E - 0x004CC1D), 0x61, 0x77 },
    
    /* Position +17: 'a' -> 'e' (winamp -> webamp) */
    { URL_RVA_BASE + (0x004CC2F - 0x004CC1D), 0x6D, 0x65 },
    
    /* Position +18: 'm' -> 'b' (winamp -> webamp) */
    { URL_RVA_BASE + (0x004CC30 - 0x004CC1D), 0x70, 0x62 },
    
    /* Position +19: 'p' -> 'a' (winamp -> webamp) */
    { URL_RVA_BASE + (0x004CC31 - 0x004CC1D), 0x2E, 0x61 },
    
    /* Position +20: '.' -> 'm' (winamp. -> webamp) */
    { URL_RVA_BASE + (0x004CC32 - 0x004CC1D), 0x63, 0x6D },
    
    /* Position +21: 'c' -> 'p' (.com -> .org start) */
    { URL_RVA_BASE + (0x004CC33 - 0x004CC1D), 0x6F, 0x70 },
    
    /* Position +22: 'o' -> '.' (.com -> .org) */
    { URL_RVA_BASE + (0x004CC34 - 0x004CC1D), 0x6D, 0x2E },
    
    /* Position +23: 'm' -> 'o' (.com -> .org) */
    { URL_RVA_BASE + (0x004CC35 - 0x004CC1D), 0x2F, 0x6F },
    
    /* Position +24: '/' -> 'r' (.com/ -> .org) */
    { URL_RVA_BASE + (0x004CC36 - 0x004CC1D), 0x73, 0x72 },
    
    /* Position +25: 's' -> 'g' (skins -> org end) */
    { URL_RVA_BASE + (0x004CC37 - 0x004CC1D), 0x6B, 0x67 },
    
    /* Position +26: 'i' -> space (padding start / начало дополнения) */
    { URL_RVA_BASE + (0x004CC38 - 0x004CC1D), 0x69, 0x20 },
    
    /* Position +27: 'n' -> space (padding) */
    { URL_RVA_BASE + (0x004CC39 - 0x004CC1D), 0x6E, 0x20 },
    
    /* Position +28: 's' -> space (padding) */
    { URL_RVA_BASE + (0x004CC3A - 0x004CC1D), 0x73, 0x20 },
    
    /* Position +29: '/' -> space (padding end / конец дополнения) */
    { URL_RVA_BASE + (0x004CC3B - 0x004CC1D), 0x2F, 0x20 },
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
 * patch_url_init
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Initializes URL redirection by patching hardcoded skin server address.
 * Инициализирует перенаправление URL патчингом жёстко закодированного адреса сервера скинов.
 * 
 * RETURNS / ВОЗВРАЩАЕТ:
 * Non-zero (TRUE) if patch applied successfully, 0 on failure
 * Не-ноль (TRUE) если патч применён успешно, 0 при ошибке
 * 
 * WHAT IT DOES / ЧТО ДЕЛАЕТ:
 * 1. Locates winamp.exe in memory
 * 2. Finds hardcoded URL string at RVA offset
 * 3. Changes each character from original to new URL
 * 4. Winamp now points to webamp.org for skins
 * 
 * 1. Находит winamp.exe в памяти
 * 2. Находит жёстко закодированную строку URL по смещению RVA
 * 3. Меняет каждый символ с оригинального на новый URL
 * 4. Winamp теперь указывает на webamp.org для скинов
 * 
 * IDEMPOTENT BEHAVIOR / ИДЕМПОТЕНТНОЕ ПОВЕДЕНИЕ:
 * Safe to call multiple times. If already patched, patcher core accepts it.
 * Безопасно вызывать много раз. Если уже пропатчено, ядро патчера примет это.
 * 
 * ERROR CASES / СЛУЧАИ ОШИБОК:
 * Returns 0 if:
 * - winamp.exe not found in memory
 * - RVA address invalid
 * - Expected bytes don't match (wrong version or already patched differently)
 * - Memory protection change fails
 * 
 * Возвращает 0 если:
 * - winamp.exe не найден в памяти
 * - Адрес RVA недопустим
 * - Ожидаемые байты не совпадают (неправильная версия или уже пропатчено иначе)
 * - Изменение защиты памяти не удалось
 * 
 * WHEN TO CALL / КОГДА ВЫЗЫВАТЬ:
 * Call during plugin initialization.
 * Safe to call early, as it patches data section (not code).
 * 
 * Вызывайте во время инициализации плагина.
 * Безопасно вызывать рано, так как патчит секцию данных (не код).
 * 
 * EXAMPLE / ПРИМЕР:
 * ```c
 * int plugin_init() {
 *     if (!patch_url_init()) {
 *         // URL redirect failed - skins may not download
 *         // but Winamp still works normally
 *     }
 *     return 0;
 * }
 * ```
 * 
 * NOTES / ПРИМЕЧАНИЯ:
 * This patch only affects skin browser URL, not all network features.
 * Other Winamp URLs may still point to original servers.
 * 
 * Этот патч влияет только на URL браузера скинов, не все сетевые функции.
 * Другие URL Winamp могут всё ещё указывать на оригинальные серверы.
 ******************************************************************************/
int patch_url_init(void)
{
    /*
     * Idempotent apply: if already patched, core will accept it
     * Идемпотентное применение: если уже пропатчено, ядро примет это
     * 
     * patcher_apply_rva_table performs:
     * 1. Resolves winamp.exe base address
     * 2. For each byte in table:
     *    - Calculates absolute address from RVA
     *    - Verifies expected byte matches
     *    - Changes memory protection to writable
     *    - Writes patch byte
     *    - Restores memory protection
     * 3. Returns success if all patches applied
     * 
     * patcher_apply_rva_table выполняет:
     * 1. Определяет базовый адрес winamp.exe
     * 2. Для каждого байта в таблице:
     *    - Вычисляет абсолютный адрес из RVA
     *    - Проверяет совпадение ожидаемого байта
     *    - Изменяет защиту памяти на записываемую
     *    - Записывает патч-байт
     *    - Восстанавливает защиту памяти
     * 3. Возвращает успех если все патчи применены
     */
    return patcher_apply_rva_table(g_urlPatchRva, ARRAYSIZE_(g_urlPatchRva));
}

/*******************************************************************************
 * patch_url_quit
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Cleans up URL redirection by restoring original skin server address.
 * Очищает перенаправление URL восстановлением оригинального адреса сервера скинов.
 * 
 * WHAT IT DOES / ЧТО ДЕЛАЕТ:
 * Reverts all character changes back to original URL.
 * Winamp will point to www.winamp.com again (may be offline).
 * 
 * Откатывает все изменения символов обратно к оригинальному URL.
 * Winamp будет указывать на www.winamp.com снова (может быть оффлайн).
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
 * 
 * Хорошая практика восстанавливать оригинальное состояние при выгрузке плагина.
 * Оставляет Winamp в чистом, немодифицированном состоянии.
 * 
 * EXAMPLE / ПРИМЕР:
 * ```c
 * void plugin_quit() {
 *     patch_url_quit();  // Restore original URL
 *     // ... other cleanup ...
 * }
 * ```
 * 
 * NOTES / ПРИМЕЧАНИЯ:
 * After calling this, skin browser will use original (possibly offline) URL.
 * Users may lose ability to download skins until plugin is re-enabled.
 * 
 * После вызова этого, браузер скинов будет использовать оригинальный (возможно оффлайн) URL.
 * Пользователи могут потерять возможность загружать скины пока плагин не включён повторно.
 ******************************************************************************/
void patch_url_quit(void)
{
    /*
     * Idempotent revert: if already original, core will accept it
     * Идемпотентный откат: если уже оригинал, ядро примет это
     * 
     * patcher_revert_rva_table performs:
     * 1. Resolves winamp.exe base address
     * 2. For each byte in table:
     *    - Calculates absolute address from RVA
     *    - Verifies patched byte matches
     *    - Changes memory protection to writable
     *    - Writes original byte
     *    - Restores memory protection
     * 3. Returns when all patches reverted
     * 
     * patcher_revert_rva_table выполняет:
     * 1. Определяет базовый адрес winamp.exe
     * 2. Для каждого байта в таблице:
     *    - Вычисляет абсолютный адрес из RVA
     *    - Проверяет совпадение пропатченного байта
     *    - Изменяет защиту памяти на записываемую
     *    - Записывает оригинальный байт
     *    - Восстанавливает защиту памяти
     * 3. Возвращается когда все патчи откачены
     */
    patcher_revert_rva_table(g_urlPatchRva, ARRAYSIZE_(g_urlPatchRva));
}