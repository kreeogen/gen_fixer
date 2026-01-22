/*******************************************************************************
 * patch_mb_skin.cpp
 * 
 * SKIN-TRIGGERED MINIBROWSER BLOCKING PATCH
 * ПАТЧ БЛОКИРОВКИ МИНИ-БРАУЗЕРА ЗАПУСКАЕМОГО СКИНОМ
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Prevents Winamp skins from opening minibrowser window via mb.ini commands.
 * Blocks skin-triggered minibrowser activation while allowing manual opening.
 * 
 * Предотвращает открытие окна мини-браузера скинами Winamp через команды mb.ini.
 * Блокирует активацию мини-браузера скином, но разрешает ручное открытие.
 * 
 * BACKGROUND / ПРЕДЫСТОРИЯ:
 * Some Winamp skins include special mb.ini file with instructions to auto-open
 * minibrowser window. This can be unwanted behavior, especially if user wants
 * to disable minibrowser or control when it appears.
 * 
 * Некоторые скины Winamp включают специальный файл mb.ini с инструкциями для
 * авто-открытия окна мини-браузера. Это может быть нежелательное поведение,
 * особенно если пользователь хочет отключить мини-браузер или контролировать когда он появляется.
 * 
 * HOW IT WORKS / КАК ЭТО РАБОТАЕТ:
 * 1. Patches byte at RVA 0x0004E964 in winamp.exe
 * 2. Changes 0x62 to 0x64 (modifies mb.ini parsing logic)
 * 3. Winamp ignores mb.ini directives to open minibrowser
 * 4. Skin loads normally but doesn't auto-trigger minibrowser
 * 
 * 1. Патчит байт по RVA 0x0004E964 в winamp.exe
 * 2. Меняет 0x62 на 0x64 (изменяет логику парсинга mb.ini)
 * 3. Winamp игнорирует директивы mb.ini для открытия мини-браузера
 * 4. Скин загружается нормально но не авто-запускает мини-браузер
 * 
 * TECHNICAL DETAILS / ТЕХНИЧЕСКИЕ ДЕТАЛИ:
 * 
 * PATCH LOCATION / РАСПОЛОЖЕНИЕ ПАТЧА:
 * - Target: winamp.exe
 * - RVA: 0x0004E964 (mb.ini processing code)
 * - Original: 0x62 (character 'b' in ASCII)
 * - Patched: 0x64 (character 'd' in ASCII)
 * - Effect: Changes string comparison for mb.ini lookup
 * 
 * - Цель: winamp.exe
 * - RVA: 0x0004E964 (код обработки mb.ini)
 * - Оригинал: 0x62 (символ 'b' в ASCII)
 * - Пропатчено: 0x64 (символ 'd' в ASCII)
 * - Эффект: Изменяет сравнение строк для поиска mb.ini
 * 
 * HOW PATCH WORKS / КАК РАБОТАЕТ ПАТЧ:
 * The byte 0x62 is ASCII character 'b' (part of "mb.ini" string).
 * Changing to 0x64 (character 'd') makes Winamp look for "md.ini" instead.
 * Since "md.ini" doesn't exist in skins, directives are never found.
 * This effectively disables skin-triggered minibrowser opening.
 * 
 * Байт 0x62 - это ASCII символ 'b' (часть строки "mb.ini").
 * Изменение на 0x64 (символ 'd') заставляет Winamp искать "md.ini" вместо этого.
 * Так как "md.ini" не существует в скинах, директивы никогда не найдутся.
 * Это эффективно отключает открытие мини-браузера скином.
 * 
 * WHAT REMAINS WORKING / ЧТО ОСТАЁТСЯ РАБОТАЮЩИМ:
 * - Manual minibrowser opening (via menu or hotkey)
 * - All other skin features
 * - Skin graphics and layout
 * - Other skin .ini configuration files
 * 
 * - Ручное открытие мини-браузера (через меню или горячую клавишу)
 * - Все остальные функции скина
 * - Графика и компоновка скина
 * - Другие .ini файлы конфигурации скина
 * 
 * USE CASES / СЛУЧАИ ИСПОЛЬЗОВАНИЯ:
 * 1. User wants to use skin but not have minibrowser auto-open
 * 2. Combination with patch_mb.cpp to fully disable minibrowser
 * 3. Controlling minibrowser behavior without modifying skin files
 * 4. Preventing unexpected windows from appearing on skin load
 * 
 * 1. Пользователь хочет использовать скин но не иметь авто-открытие мини-браузера
 * 2. Комбинация с patch_mb.cpp для полного отключения мини-браузера
 * 3. Контроль поведения мини-браузера без изменения файлов скина
 * 4. Предотвращение неожиданного появления окон при загрузке скина
 * 
 * COMPATIBILITY / СОВМЕСТИМОСТЬ:
 * - Winamp 2.95 and compatible versions
 * - All skin types (classic and modern)
 * - Windows 98 through Windows 11
 * - Safe with other patches
 * 
 * - Winamp 2.95 и совместимые версии
 * - Все типы скинов (классические и современные)
 * - Windows 98 до Windows 11
 * - Безопасно с другими патчами
 * 
 * INTERACTION WITH OTHER PATCHES / ВЗАИМОДЕЙСТВИЕ С ДРУГИМИ ПАТЧАМИ:
 * Works well with patch_mb.cpp for complete minibrowser control:
 * - patch_mb.cpp: Blocks minibrowser creation + hides menu
 * - patch_mb_skin.cpp: Blocks skin-triggered opening
 * Together: Complete minibrowser disabling solution
 * 
 * Хорошо работает с patch_mb.cpp для полного контроля мини-браузера:
 * - patch_mb.cpp: Блокирует создание мини-браузера + скрывает меню
 * - patch_mb_skin.cpp: Блокирует открытие скином
 * Вместе: Полное решение отключения мини-браузера
 * 
 ******************************************************************************/

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "patcher_core.h"

/*******************************************************************************
 * PATCH DEFINITION
 * ОПРЕДЕЛЕНИЕ ПАТЧА
 ******************************************************************************/

/*******************************************************************************
 * g_mbSkinPatchRva
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Byte patch table to prevent skins from triggering minibrowser via mb.ini.
 * Таблица байтовых патчей для предотвращения запуска мини-браузера скинами через mb.ini.
 * 
 * PATCH DETAILS / ДЕТАЛИ ПАТЧА:
 * RVA 0x0004E964 contains the character 'b' from "mb.ini" string lookup.
 * Original byte 0x62 (ASCII 'b') is part of filename check.
 * Patched byte 0x64 (ASCII 'd') changes lookup to "md.ini".
 * 
 * RVA 0x0004E964 содержит символ 'b' из поиска строки "mb.ini".
 * Оригинальный байт 0x62 (ASCII 'b') - часть проверки имени файла.
 * Пропатченный байт 0x64 (ASCII 'd') изменяет поиск на "md.ini".
 * 
 * WHY THIS WORKS / ПОЧЕМУ ЭТО РАБОТАЕТ:
 * Winamp searches for "mb.ini" in skin directory to read directives.
 * Changing search to "md.ini" (non-existent file) prevents directives from loading.
 * Skin still loads and works normally, but can't auto-open minibrowser.
 * 
 * Winamp ищет "mb.ini" в директории скина для чтения директив.
 * Изменение поиска на "md.ini" (несуществующий файл) предотвращает загрузку директив.
 * Скин всё ещё загружается и работает нормально, но не может авто-открыть мини-браузер.
 * 
 * CLEVER APPROACH / УМНЫЙ ПОДХОД:
 * This is more elegant than patching entire function:
 * - Single byte change (minimal modification)
 * - No function behavior changes (just filename lookup)
 * - Easy to revert
 * - No side effects on other skin features
 * 
 * Это более элегантно чем патчить всю функцию:
 * - Изменение одного байта (минимальная модификация)
 * - Нет изменений поведения функций (только поиск имени файла)
 * - Легко откатить
 * - Нет побочных эффектов на другие функции скина
 ******************************************************************************/
static const PatchByte g_mbSkinPatchRva[] =
{
    /* RVA        Expected  Patch  Description / Описание */
    { 0x0004E964,  0x62,    0x64 },  // 'b' -> 'd' (mb.ini -> md.ini lookup / поиск mb.ini -> md.ini)
};

// Safe array size calculation / Безопасное вычисление размера массива
#define ARRAYSIZE_(a) ((int)(sizeof(a) / sizeof((a)[0])))

/*******************************************************************************
 * PUBLIC API
 * ПУБЛИЧНЫЙ API
 ******************************************************************************/

/*******************************************************************************
 * patch_mb_skin_init
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Initializes skin-triggered minibrowser blocking patch.
 * Applies byte patch to prevent mb.ini from triggering minibrowser.
 * 
 * Инициализирует патч блокировки мини-браузера запускаемого скином.
 * Применяет байтовый патч для предотвращения запуска мини-браузера через mb.ini.
 * 
 * RETURNS / ВОЗВРАЩАЕТ:
 * Non-zero (TRUE) if patch applied successfully, 0 on failure
 * Не-ноль (TRUE) если патч применён успешно, 0 при ошибке
 * 
 * IDEMPOTENT BEHAVIOR / ИДЕМПОТЕНТНОЕ ПОВЕДЕНИЕ:
 * Safe to call multiple times. If already patched, patcher core accepts it.
 * This means calling init twice won't cause errors or double-patching.
 * 
 * Безопасно вызывать много раз. Если уже пропатчено, ядро патчера примет это.
 * Это означает что двойной вызов init не вызовет ошибок или двойного патчинга.
 * 
 * WHEN TO CALL / КОГДА ВЫЗЫВАТЬ:
 * Call during plugin initialization, before Winamp loads any skins.
 * Early initialization ensures patch is active before skin processing.
 * 
 * Вызывайте во время инициализации плагина, до загрузки любых скинов в Winamp.
 * Ранняя инициализация гарантирует что патч активен до обработки скинов.
 * 
 * ERROR HANDLING / ОБРАБОТКА ОШИБОК:
 * Returns 0 if:
 * - Winamp.exe not found in memory
 * - RVA address invalid
 * - Expected byte doesn't match (wrong Winamp version)
 * - Memory protection change fails
 * 
 * Возвращает 0 если:
 * - Winamp.exe не найден в памяти
 * - Адрес RVA недопустим
 * - Ожидаемый байт не совпадает (неправильная версия Winamp)
 * - Изменение защиты памяти не удалось
 * 
 * EXAMPLE / ПРИМЕР:
 * ```c
 * int plugin_init() {
 *     if (!patch_mb_skin_init()) {
 *         // Patch failed - log error or continue without patch
 *         MessageBox(NULL, "Failed to block skin mb.ini", "Warning", MB_OK);
 *     }
 *     return 0;
 * }
 * ```
 * 
 * NOTES / ПРИМЕЧАНИЯ:
 * This patch can be used independently or combined with patch_mb.cpp.
 * Together they provide comprehensive minibrowser disabling.
 * 
 * Этот патч может использоваться независимо или в комбинации с patch_mb.cpp.
 * Вместе они обеспечивают комплексное отключение мини-браузера.
 ******************************************************************************/
int patch_mb_skin_init(void)
{
    /*
     * Idempotent apply: if already patched, core accepts it
     * Идемпотентное применение: если уже пропатчено, ядро примет это
     * 
     * patcher_apply_rva_table performs:
     * 1. Resolves winamp.exe base address
     * 2. Calculates absolute address from RVA
     * 3. Verifies expected byte matches
     * 4. Changes memory protection to writable
     * 5. Writes patch byte
     * 6. Restores memory protection
     * 
     * patcher_apply_rva_table выполняет:
     * 1. Определяет базовый адрес winamp.exe
     * 2. Вычисляет абсолютный адрес из RVA
     * 3. Проверяет совпадение ожидаемого байта
     * 4. Изменяет защиту памяти на записываемую
     * 5. Записывает патч-байт
     * 6. Восстанавливает защиту памяти
     */
    return patcher_apply_rva_table(g_mbSkinPatchRva, ARRAYSIZE_(g_mbSkinPatchRva));
}

/*******************************************************************************
 * patch_mb_skin_quit
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Cleans up skin-triggered minibrowser blocking patch.
 * Reverts byte patch to restore original mb.ini processing.
 * 
 * Очищает патч блокировки мини-браузера запускаемого скином.
 * Откатывает байтовый патч для восстановления оригинальной обработки mb.ini.
 * 
 * IDEMPOTENT BEHAVIOR / ИДЕМПОТЕНТНОЕ ПОВЕДЕНИЕ:
 * Safe to call multiple times. If already at original value, patcher core accepts it.
 * This means calling quit twice won't cause errors.
 * 
 * Безопасно вызывать много раз. Если уже оригинальное значение, ядро патчера примет это.
 * Это означает что двойной вызов quit не вызовет ошибок.
 * 
 * WHEN TO CALL / КОГДА ВЫЗЫВАТЬ:
 * Call during plugin cleanup, before plugin DLL is unloaded.
 * Restoring original byte ensures clean state on plugin removal.
 * 
 * Вызывайте во время очистки плагина, до выгрузки DLL плагина.
 * Восстановление оригинального байта обеспечивает чистое состояние при удалении плагина.
 * 
 * WHAT IT DOES / ЧТО ДЕЛАЕТ:
 * Restores 0x64 back to 0x62 ('d' -> 'b').
 * This re-enables mb.ini processing in skins.
 * After revert, skins can again auto-open minibrowser.
 * 
 * Восстанавливает 0x64 обратно в 0x62 ('d' -> 'b').
 * Это повторно включает обработку mb.ini в скинах.
 * После отката скины снова могут авто-открывать мини-браузер.
 * 
 * WHY REVERT / ПОЧЕМУ ОТКАТЫВАЕМ:
 * Good practice to restore original state when plugin unloads.
 * Prevents conflicts if user reinstalls plugin or uses different version.
 * Leaves Winamp in clean, unmodified state.
 * 
 * Хорошая практика восстанавливать оригинальное состояние при выгрузке плагина.
 * Предотвращает конфликты если пользователь переустановит плагин или использует другую версию.
 * Оставляет Winamp в чистом, немодифицированном состоянии.
 * 
 * EXAMPLE / ПРИМЕР:
 * ```c
 * void plugin_quit() {
 *     patch_mb_skin_quit();  // Restore original behavior
 *     // ... other cleanup ...
 * }
 * ```
 * 
 * NOTES / ПРИМЕЧАНИЯ:
 * Revert happens in reverse order of application (LIFO).
 * If multiple patches applied, they're reverted in opposite order.
 * 
 * Откат происходит в обратном порядке применения (LIFO).
 * Если применено несколько патчей, они откатываются в обратном порядке.
 ******************************************************************************/
void patch_mb_skin_quit(void)
{
    /*
     * Idempotent revert: if already original, core accepts it
     * Идемпотентный откат: если уже оригинал, ядро примет это
     * 
     * patcher_revert_rva_table performs:
     * 1. Resolves winamp.exe base address
     * 2. Calculates absolute address from RVA
     * 3. Verifies patched byte matches (0x64)
     * 4. Changes memory protection to writable
     * 5. Writes original byte (0x62)
     * 6. Restores memory protection
     * 
     * patcher_revert_rva_table выполняет:
     * 1. Определяет базовый адрес winamp.exe
     * 2. Вычисляет абсолютный адрес из RVA
     * 3. Проверяет совпадение пропатченного байта (0x64)
     * 4. Изменяет защиту памяти на записываемую
     * 5. Записывает оригинальный байт (0x62)
     * 6. Восстанавливает защиту памяти
     */
    patcher_revert_rva_table(g_mbSkinPatchRva, ARRAYSIZE_(g_mbSkinPatchRva));
}
