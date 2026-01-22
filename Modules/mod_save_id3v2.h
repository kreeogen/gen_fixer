/*******************************************************************************
 * mod_save_id3v2.h
 * 
 * MP3 ID3v2 TAG SAVE FIX MODULE - HEADER
 * МОДУЛЬ ИСПРАВЛЕНИЯ СОХРАНЕНИЯ ID3v2 ТЕГОВ MP3 - ЗАГОЛОВОК
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Header file for MP3 ID3v2 tag save fix module.
 * Provides initialization and cleanup functions for tag save interception.
 * 
 * Заголовочный файл для модуля исправления сохранения ID3v2 тегов MP3.
 * Предоставляет функции инициализации и очистки для перехвата сохранения тегов.
 * 
 * USAGE / ИСПОЛЬЗОВАНИЕ:
 * 1. Include this header in your main plugin file
 * 2. Call MP3_SaveFix_Init() during plugin initialization
 * 3. Call MP3_SaveFix_Quit() during plugin cleanup
 * 
 * 1. Включить этот заголовок в главный файл плагина
 * 2. Вызвать MP3_SaveFix_Init() во время инициализации плагина
 * 3. Вызвать MP3_SaveFix_Quit() во время очистки плагина
 * 
 * EXAMPLE / ПРИМЕР:
 * ```c
 * #include "mod_save_id3v2.h"
 * 
 * int plugin_init() {
 *     MP3_SaveFix_Init();  // Enable tag save fix
 *     return 0;
 * }
 * 
 * void plugin_quit() {
 *     MP3_SaveFix_Quit();  // Disable tag save fix
 * }
 * ```
 * 
 * DEPENDENCIES / ЗАВИСИМОСТИ:
 * - windows.h (for Windows API types)
 * - in_mp3.dll (patched at runtime)
 * - kernel32.dll (MoveFileA/MoveFileExA functions)
 * 
 * - windows.h (для типов Windows API)
 * - in_mp3.dll (патчится во время выполнения)
 * - kernel32.dll (функции MoveFileA/MoveFileExA)
 * 
 * COMPATIBILITY / СОВМЕСТИМОСТЬ:
 * - C and C++ compatible (uses extern "C")
 * - Winamp 2.95 and compatible versions
 * - Windows 98 through Windows 11
 * 
 * - Совместимо с C и C++ (использует extern "C")
 * - Winamp 2.95 и совместимые версии
 * - Windows 98 до Windows 11
 * 
 ******************************************************************************/

#ifndef MOD_SAVE_ID3V2_H
#define MOD_SAVE_ID3V2_H

#include <windows.h>

/*******************************************************************************
 * EXTERN C LINKAGE
 * ВНЕШНЯЯ СВЯЗЬ C
 * 
 * Ensures C linkage for C++ compatibility.
 * Allows this header to be used in both C and C++ projects.
 * 
 * Обеспечивает C связь для совместимости с C++.
 * Позволяет использовать этот заголовок как в C, так и в C++ проектах.
 ******************************************************************************/
#ifdef __cplusplus
extern "C" {
#endif

/*******************************************************************************
 * EXPORTED FUNCTIONS
 * ЭКСПОРТИРУЕМЫЕ ФУНКЦИИ
 ******************************************************************************/

/*******************************************************************************
 * MP3_SaveFix_Init
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Initializes the MP3 tag save fix module.
 * Starts background thread that patches in_mp3.dll to intercept tag save operations.
 * 
 * Инициализирует модуль исправления сохранения MP3 тегов.
 * Запускает фоновый поток, который патчит in_mp3.dll для перехвата операций сохранения тегов.
 * 
 * WHEN TO CALL / КОГДА ВЫЗЫВАТЬ:
 * Call this during your plugin's initialization phase.
 * Safe to call multiple times - only initializes once.
 * 
 * Вызывайте во время фазы инициализации вашего плагина.
 * Безопасно вызывать много раз - инициализирует только один раз.
 * 
 * WHAT IT DOES / ЧТО ДЕЛАЕТ:
 * 1. Starts background thread that waits for in_mp3.dll to load
 * 2. Thread patches in_mp3.dll's Import Address Table
 * 3. Hooks MoveFileA/MoveFileExA to intercept tag save operations
 * 4. Enables automatic tag rebuilding with proper encoding
 * 
 * 1. Запускает фоновый поток, который ожидает загрузки in_mp3.dll
 * 2. Поток патчит таблицу адресов импорта in_mp3.dll
 * 3. Перехватывает MoveFileA/MoveFileExA для перехвата операций сохранения тегов
 * 4. Включает автоматическое перестроение тегов с правильной кодировкой
 * 
 * THREAD SAFETY / ПОТОКОБЕЗОПАСНОСТЬ:
 * Thread-safe. Can be called from any thread.
 * Uses internal synchronization to prevent race conditions.
 * 
 * Потокобезопасна. Может вызываться из любого потока.
 * Использует внутреннюю синхронизацию для предотвращения гонок.
 * 
 * PERFORMANCE / ПРОИЗВОДИТЕЛЬНОСТЬ:
 * Returns immediately - patching happens in background thread.
 * Minimal performance impact - only active during tag save operations.
 * 
 * Возвращается немедленно - патчинг происходит в фоновом потоке.
 * Минимальное влияние на производительность - активен только во время операций сохранения тегов.
 * 
 * NOTES / ПРИМЕЧАНИЯ:
 * This function ONLY fixes tag WRITING/SAVING.
 * It does not affect tag reading operations.
 * Call this if you need to fix tag write operations but not tag reading.
 * 
 * Эта функция исправляет ТОЛЬКО ЗАПИСЬ/СОХРАНЕНИЕ тегов.
 * Она не влияет на операции чтения тегов.
 * Вызывайте её, если нужно исправить операции записи тегов, но не чтение.
 * 
 * EXAMPLE / ПРИМЕР:
 * ```c
 * // In your plugin initialization
 * int init() {
 *     MP3_SaveFix_Init();  // Enable tag save fix
 *     // ... other initialization ...
 *     return 0;
 * }
 * ```
 ******************************************************************************/
void MP3_SaveFix_Init(void);

/*******************************************************************************
 * MP3_SaveFix_Quit
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Cleans up the MP3 tag save fix module.
 * Disables tag save interception and marks hooks as inactive.
 * 
 * Очищает модуль исправления сохранения MP3 тегов.
 * Отключает перехват сохранения тегов и отмечает хуки как неактивные.
 * 
 * WHEN TO CALL / КОГДА ВЫЗЫВАТЬ:
 * Call this during your plugin's cleanup phase, before unloading.
 * Safe to call multiple times.
 * Safe to call even if MP3_SaveFix_Init was never called.
 * 
 * Вызывайте во время фазы очистки вашего плагина, перед выгрузкой.
 * Безопасно вызывать много раз.
 * Безопасно вызывать даже если MP3_SaveFix_Init никогда не вызывалась.
 * 
 * WHAT IT DOES / ЧТО ДЕЛАЕТ:
 * 1. Marks hooks as inactive (g_hooksReady = 0)
 * 2. Hook functions will pass through to original implementations
 * 3. Does NOT unhook functions (to avoid issues with in_mp3.dll)
 * 
 * 1. Отмечает хуки как неактивные (g_hooksReady = 0)
 * 2. Функции хуков будут передавать вызовы оригинальным реализациям
 * 3. НЕ отцепляет функции (чтобы избежать проблем с in_mp3.dll)
 * 
 * THREAD SAFETY / ПОТОКОБЕЗОПАСНОСТЬ:
 * Thread-safe. Can be called from any thread.
 * Uses interlocked operations to ensure atomic state change.
 * 
 * Потокобезопасна. Может вызываться из любого потока.
 * Использует interlocked операции для обеспечения атомарного изменения состояния.
 * 
 * PERFORMANCE / ПРОИЗВОДИТЕЛЬНОСТЬ:
 * Returns immediately - very fast operation.
 * Just sets a flag, no complex cleanup needed.
 * 
 * Возвращается немедленно - очень быстрая операция.
 * Просто устанавливает флаг, сложная очистка не требуется.
 * 
 * NOTES / ПРИМЕЧАНИЯ:
 * CRITICAL: Must be called before plugin DLL is unloaded.
 * If not called, hooked functions may crash when in_mp3.dll calls them.
 * 
 * КРИТИЧНО: Должна быть вызвана перед выгрузкой DLL плагина.
 * Если не вызвана, перехваченные функции могут крашнуться когда in_mp3.dll их вызовет.
 * 
 * WHY NOT UNHOOK / ПОЧЕМУ НЕ ОТЦЕПЛЯЕМ:
 * Unhooking IAT is risky if in_mp3.dll is still loaded and might call functions.
 * Setting flag to 0 safely disables hook logic while leaving IAT entries intact.
 * 
 * Отцепление IAT рискованно если in_mp3.dll всё ещё загружен и может вызывать функции.
 * Установка флага в 0 безопасно отключает логику хука, оставляя записи IAT нетронутыми.
 * 
 * EXAMPLE / ПРИМЕР:
 * ```c
 * // In your plugin cleanup
 * void quit() {
 *     MP3_SaveFix_Quit();  // Disable tag save fix
 *     // ... other cleanup ...
 * }
 * ```
 ******************************************************************************/
void MP3_SaveFix_Quit(void);

/*******************************************************************************
 * END OF EXTERN C BLOCK
 * КОНЕЦ БЛОКА EXTERN C
 ******************************************************************************/
#ifdef __cplusplus
}
#endif

#endif  // MOD_SAVE_ID3V2_H