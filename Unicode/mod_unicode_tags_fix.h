/*******************************************************************************
 * mod_unicode_tags_fix.h
 * 
 * UNICODE ID3 TAGS VIRTUALIZATION MODULE - HEADER
 * МОДУЛЬ ВИРТУАЛИЗАЦИИ UNICODE ID3 ТЕГОВ - ЗАГОЛОВОК
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Public API for Unicode ID3 tags fix module.
 * Provides functions to enable/disable file system virtualization for MP3 tags.
 * 
 * Публичный API для модуля исправления Unicode ID3 тегов.
 * Предоставляет функции для включения/выключения виртуализации файловой системы для MP3 тегов.
 * 
 * WHAT THIS MODULE DOES / ЧТО ДЕЛАЕТ ЭТОТ МОДУЛЬ:
 * - Intercepts file operations in in_mp3.dll
 * - Rebuilds ID3 tags with proper Unicode encoding
 * - Virtualizes MP3 files to appear with correct tags
 * - No disk modifications (non-destructive)
 * 
 * - Перехватывает файловые операции в in_mp3.dll
 * - Перестраивает ID3 теги с правильной Unicode кодировкой
 * - Виртуализирует MP3 файлы чтобы они выглядели с правильными тегами
 * - Нет модификаций диска (неразрушающий)
 * 
 * KEY FEATURES / КЛЮЧЕВЫЕ ВОЗМОЖНОСТИ:
 * - Fixes Cyrillic and other Unicode characters in tags
 * - Works transparently with Winamp
 * - Automatically enables save fix module
 * - Thread-safe operation
 * 
 * - Исправляет кириллицу и другие Unicode символы в тегах
 * - Работает прозрачно с Winamp
 * - Автоматически включает модуль исправления сохранения
 * - Потокобезопасная работа
 * 
 * USAGE / ИСПОЛЬЗОВАНИЕ:
 * 1. Call MP3_TagsFix_Init() during plugin initialization
 * 2. Module runs in background, hooking file operations
 * 3. Call MP3_TagsFix_Quit() during plugin cleanup
 * 
 * 1. Вызвать MP3_TagsFix_Init() во время инициализации плагина
 * 2. Модуль работает в фоне, перехватывая файловые операции
 * 3. Вызвать MP3_TagsFix_Quit() во время очистки плагина
 * 
 * INTEGRATION / ИНТЕГРАЦИЯ:
 * This module automatically integrates with mod_save_id3v2 module.
 * When initialized, it also initializes the save fix module.
 * 
 * Этот модуль автоматически интегрируется с модулем mod_save_id3v2.
 * При инициализации он также инициализирует модуль исправления сохранения.
 * 
 ******************************************************************************/

#ifndef MOD_UNICODE_TAGS_FIX_H
#define MOD_UNICODE_TAGS_FIX_H

#include <windows.h>

/*******************************************************************************
 * EXTERN C LINKAGE
 * ВНЕШНЯЯ СВЯЗЬ C
 * 
 * Ensures C linkage for C++ compatibility.
 * Обеспечивает C связь для совместимости с C++.
 ******************************************************************************/
#ifdef __cplusplus
extern "C" {
#endif

/*******************************************************************************
 * CORE API FUNCTIONS
 * ОСНОВНЫЕ ФУНКЦИИ API
 ******************************************************************************/

/*******************************************************************************
 * MP3_TagsFix_Init
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Initializes Unicode tags fix module.
 * Starts background thread that installs IAT hooks in in_mp3.dll.
 * 
 * Инициализирует модуль исправления Unicode тегов.
 * Запускает фоновый поток который устанавливает IAT хуки в in_mp3.dll.
 * 
 * BEHAVIOR / ПОВЕДЕНИЕ:
 * - Starts hook installation thread
 * - Thread waits for in_mp3.dll to load (up to 10 seconds)
 * - Installs hooks for CreateFileA, ReadFile, SetFilePointer, GetFileSize, CloseHandle
 * - Automatically initializes MP3_SaveFix module after hooks installed
 * - Safe to call multiple times (internally guards with static flag)
 * 
 * - Запускает поток установки хуков
 * - Поток ждёт загрузки in_mp3.dll (до 10 секунд)
 * - Устанавливает хуки для CreateFileA, ReadFile, SetFilePointer, GetFileSize, CloseHandle
 * - Автоматически инициализирует модуль MP3_SaveFix после установки хуков
 * - Безопасно вызывать много раз (внутренне защищено статическим флагом)
 * 
 * IMPORTANT / ВАЖНО:
 * This function automatically enables the save fix module!
 * You don't need to call MP3_SaveFix_Init() separately.
 * 
 * Эта функция автоматически включает модуль исправления сохранения!
 * Вам не нужно вызывать MP3_SaveFix_Init() отдельно.
 * 
 * WHEN TO CALL / КОГДА ВЫЗЫВАТЬ:
 * Call during plugin initialization, typically in plugin's init() function.
 * Module will activate when in_mp3.dll loads.
 * 
 * Вызывайте во время инициализации плагина, обычно в функции init() плагина.
 * Модуль активируется когда in_mp3.dll загрузится.
 * 
 * THREAD SAFETY / ПОТОКОБЕЗОПАСНОСТЬ:
 * Thread-safe. Uses static initialization guard.
 * Потокобезопасно. Использует статическую защиту инициализации.
 * 
 * EXAMPLE / ПРИМЕР:
 * ```c
 * int winampInit() {
 *     // Initialize Unicode tags fix
 *     MP3_TagsFix_Init();
 *     
 *     // ... other plugin initialization ...
 *     
 *     return 0;
 * }
 * ```
 * 
 * TECHNICAL DETAILS / ТЕХНИЧЕСКИЕ ДЕТАЛИ:
 * Creates background thread that:
 * 1. Polls for in_mp3.dll loading
 * 2. Gets KERNEL32 function addresses
 * 3. Patches in_mp3.dll's Import Address Table
 * 4. Initializes save fix module (sequentially to avoid VirtualProtect conflicts)
 * 
 * Создаёт фоновый поток который:
 * 1. Опрашивает загрузку in_mp3.dll
 * 2. Получает адреса функций KERNEL32
 * 3. Патчит Import Address Table in_mp3.dll
 * 4. Инициализирует модуль исправления сохранения (последовательно для избежания конфликтов VirtualProtect)
 ******************************************************************************/
void MP3_TagsFix_Init(void);

/*******************************************************************************
 * MP3_TagsFix_Quit
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Cleans up Unicode tags fix module.
 * Disables hooks and frees resources.
 * 
 * Очищает модуль исправления Unicode тегов.
 * Отключает хуки и освобождает ресурсы.
 * 
 * BEHAVIOR / ПОВЕДЕНИЕ:
 * - Disables hooks (marks g_hooksReady as 0)
 * - Calls MP3_SaveFix_Quit() to cleanup save fix module
 * - Frees all active file contexts
 * - Does NOT delete critical section (safety during shutdown)
 * 
 * - Отключает хуки (помечает g_hooksReady как 0)
 * - Вызывает MP3_SaveFix_Quit() для очистки модуля исправления сохранения
 * - Освобождает все активные контексты файлов
 * - НЕ удаляет критическую секцию (безопасность во время завершения)
 * 
 * WHEN TO CALL / КОГДА ВЫЗЫВАТЬ:
 * Call during plugin cleanup, typically in plugin's quit() function.
 * Should be called before plugin DLL is unloaded.
 * 
 * Вызывайте во время очистки плагина, обычно в функции quit() плагина.
 * Должна быть вызвана до выгрузки DLL плагина.
 * 
 * SAFETY DURING SHUTDOWN / БЕЗОПАСНОСТЬ ПРИ ЗАВЕРШЕНИИ:
 * Critical section is intentionally NOT deleted to prevent crashes
 * if Winamp tries to read files during shutdown process.
 * This is a design choice for stability over strict resource cleanup.
 * 
 * Критическая секция намеренно НЕ удаляется для предотвращения крашей
 * если Winamp попытается прочитать файлы во время процесса завершения.
 * Это проектное решение для стабильности вместо строгой очистки ресурсов.
 * 
 * THREAD SAFETY / ПОТОКОБЕЗОПАСНОСТЬ:
 * Thread-safe. Uses critical section for context pool access.
 * Потокобезопасно. Использует критическую секцию для доступа к пулу контекстов.
 * 
 * EXAMPLE / ПРИМЕР:
 * ```c
 * void winampQuit() {
 *     // ... other plugin cleanup ...
 *     
 *     // Cleanup Unicode tags fix
 *     MP3_TagsFix_Quit();
 * }
 * ```
 * 
 * TECHNICAL DETAILS / ТЕХНИЧЕСКИЕ ДЕТАЛИ:
 * - Sets g_hooksReady to 0 (disables hook functions)
 * - Calls MP3_SaveFix_Quit()
 * - Locks context pool and frees all in-use contexts
 * - Leaves critical section initialized (prevents shutdown crashes)
 * 
 * - Устанавливает g_hooksReady в 0 (отключает функции хуков)
 * - Вызывает MP3_SaveFix_Quit()
 * - Блокирует пул контекстов и освобождает все используемые контексты
 * - Оставляет критическую секцию инициализированной (предотвращает краши завершения)
 ******************************************************************************/
void MP3_TagsFix_Quit(void);

/*******************************************************************************
 * ALTERNATE ENTRY POINTS
 * АЛЬТЕРНАТИВНЫЕ ТОЧКИ ВХОДА
 * 
 * These functions provide compatibility with different plugin contexts.
 * Эти функции обеспечивают совместимость с различными контекстами плагинов.
 ******************************************************************************/

/*******************************************************************************
 * MP3_TagsFix_Init_C
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Alternate entry point with HWND parameter.
 * Provided for compatibility with certain plugin initialization contexts.
 * 
 * Альтернативная точка входа с параметром HWND.
 * Предоставлена для совместимости с определёнными контекстами инициализации плагинов.
 * 
 * PARAMETERS / ПАРАМЕТРЫ:
 * hwnd - Window handle (currently unused, accepted for compatibility)
 *        Дескриптор окна (в настоящее время не используется, принимается для совместимости)
 * 
 * BEHAVIOR / ПОВЕДЕНИЕ:
 * Simply calls MP3_TagsFix_Init().
 * The hwnd parameter is ignored.
 * 
 * Просто вызывает MP3_TagsFix_Init().
 * Параметр hwnd игнорируется.
 * 
 * USAGE / ИСПОЛЬЗОВАНИЕ:
 * Use this if your plugin framework requires init function with HWND parameter.
 * Используйте это если ваш фреймворк плагина требует функцию init с параметром HWND.
 ******************************************************************************/
void MP3_TagsFix_Init_C(HWND hwnd);

/*******************************************************************************
 * EFIHook_Init
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Alternate entry point with EFI (External File Interface) naming convention.
 * May be used by external modules or different plugin contexts.
 * 
 * Альтернативная точка входа с соглашением об именовании EFI (External File Interface).
 * Может использоваться внешними модулями или различными контекстами плагинов.
 * 
 * BEHAVIOR / ПОВЕДЕНИЕ:
 * Simply calls MP3_TagsFix_Init().
 * Просто вызывает MP3_TagsFix_Init().
 * 
 * USAGE / ИСПОЛЬЗОВАНИЕ:
 * Use this if you need EFI-style naming convention for compatibility.
 * Используйте это если вам нужно соглашение об именовании в стиле EFI для совместимости.
 ******************************************************************************/
void EFIHook_Init(void);

/*******************************************************************************
 * EFIHook_Quit
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Alternate entry point with EFI (External File Interface) naming convention.
 * Cleanup counterpart to EFIHook_Init().
 * 
 * Альтернативная точка входа с соглашением об именовании EFI (External File Interface).
 * Очистка парная к EFIHook_Init().
 * 
 * BEHAVIOR / ПОВЕДЕНИЕ:
 * Simply calls MP3_TagsFix_Quit().
 * Просто вызывает MP3_TagsFix_Quit().
 * 
 * USAGE / ИСПОЛЬЗОВАНИЕ:
 * Use this if you need EFI-style naming convention for compatibility.
 * Используйте это если вам нужно соглашение об именовании в стиле EFI для совместимости.
 ******************************************************************************/
void EFIHook_Quit(void);

/*******************************************************************************
 * INTEGRATION NOTES
 * ПРИМЕЧАНИЯ ПО ИНТЕГРАЦИИ
 * 
 * AUTOMATIC SAVE FIX ACTIVATION / АВТОМАТИЧЕСКАЯ АКТИВАЦИЯ ИСПРАВЛЕНИЯ СОХРАНЕНИЯ:
 * When you call MP3_TagsFix_Init(), the module automatically initializes
 * the MP3 save fix module (mod_save_id3v2). You don't need to initialize
 * the save fix separately. Both modules work together to provide complete
 * Unicode tag support for reading and writing.
 * 
 * Когда вы вызываете MP3_TagsFix_Init(), модуль автоматически инициализирует
 * модуль исправления сохранения MP3 (mod_save_id3v2). Вам не нужно инициализировать
 * исправление сохранения отдельно. Оба модуля работают вместе для обеспечения полной
 * поддержки Unicode тегов для чтения и записи.
 * 
 * INITIALIZATION SEQUENCE / ПОСЛЕДОВАТЕЛЬНОСТЬ ИНИЦИАЛИЗАЦИИ:
 * 1. MP3_TagsFix_Init() starts background thread
 * 2. Thread waits for in_mp3.dll to load
 * 3. Thread installs read hooks (CreateFile, ReadFile, etc.)
 * 4. Thread calls MP3_SaveFix_Init() to install write hooks
 * 
 * 1. MP3_TagsFix_Init() запускает фоновый поток
 * 2. Поток ждёт загрузки in_mp3.dll
 * 3. Поток устанавливает хуки чтения (CreateFile, ReadFile и т.д.)
 * 4. Поток вызывает MP3_SaveFix_Init() для установки хуков записи
 * 
 * CLEANUP SEQUENCE / ПОСЛЕДОВАТЕЛЬНОСТЬ ОЧИСТКИ:
 * 1. MP3_TagsFix_Quit() disables read hooks
 * 2. Calls MP3_SaveFix_Quit() to disable write hooks
 * 3. Frees all file contexts
 * 
 * 1. MP3_TagsFix_Quit() отключает хуки чтения
 * 2. Вызывает MP3_SaveFix_Quit() для отключения хуков записи
 * 3. Освобождает все контексты файлов
 * 
 * DEPENDENCIES / ЗАВИСИМОСТИ:
 * - Requires unicode_decrypt_engine module for tag reading/writing
 * - Requires mod_save_id3v2 module for save functionality
 * - Operates on in_mp3.dll (Winamp's MP3 input plugin)
 * 
 * - Требует модуль unicode_decrypt_engine для чтения/записи тегов
 * - Требует модуль mod_save_id3v2 для функциональности сохранения
 * - Работает с in_mp3.dll (входным плагином MP3 Winamp)
 * 
 ******************************************************************************/

#ifdef __cplusplus
}
#endif

#endif  /* MOD_UNICODE_TAGS_FIX_H */