/*******************************************************************************
 * mod_unicode_ml_fix.cpp
 * 
 * WINAMP MEDIA LIBRARY UNICODE/MOJIBAKE FIX MODULE
 * Модуль исправления Unicode/мojибake для медиабиблиотеки Winamp
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * This module fixes character encoding issues (mojibake) in Winamp's Media
 * Library by intercepting IPC messages that retrieve file metadata. It ensures
 * that ID3 tags and other metadata are properly decoded and displayed, even
 * when they contain non-ASCII characters (Cyrillic, Japanese, Chinese, etc.).
 * 
 * Этот модуль исправляет проблемы с кодировкой символов (мojибake) в
 * медиабиблиотеке Winamp, перехватывая IPC-сообщения, которые получают
 * метаданные файлов. Он гарантирует, что ID3-теги и другие метаданные
 * правильно декодируются и отображаются, даже когда они содержат не-ASCII
 * символы (кириллицу, японский, китайский и т.д.).
 * 
 * HOW IT WORKS / КАК ЭТО РАБОТАЕТ:
 * 1. Subclasses the main Winamp window to intercept WM_WA_IPC messages
 * 2. Intercepts two specific IPC commands:
 *    - IPC_GET_EXTENDED_FILE_INFO: Extended metadata queries
 *    - IPC_GET_BASIC_FILE_INFO: Basic file information (title)
 * 3. For supported tags, tries to read directly from ID3 tags first
 * 4. If direct read fails or tag not supported, calls original handler
 * 5. Applies mojibake fix to the result to correct encoding issues
 * 6. Returns corrected metadata to caller
 * 
 * 1. Подменяет главное окно Winamp для перехвата сообщений WM_WA_IPC
 * 2. Перехватывает две конкретные IPC-команды:
 *    - IPC_GET_EXTENDED_FILE_INFO: расширенные запросы метаданных
 *    - IPC_GET_BASIC_FILE_INFO: базовая информация о файле (название)
 * 3. Для поддерживаемых тегов сначала пытается прочитать напрямую из ID3-тегов
 * 4. Если прямое чтение не удаётся или тег не поддерживается, вызывает оригинальный обработчик
 * 5. Применяет исправление mojibake к результату для коррекции проблем кодировки
 * 6. Возвращает исправленные метаданные вызывающей стороне
 * 
 * SUPPORTED TAGS / ПОДДЕРЖИВАЕМЫЕ ТЕГИ:
 * - title, artist, album
 * - comment, comments
 * - genre, track, tracknumber
 * - year, date
 * - albumartist, composer
 * 
 * TECHNICAL DETAILS / ТЕХНИЧЕСКИЕ ДЕТАЛИ:
 * - Uses window procedure subclassing (non-invasive technique)
 * - Thread-safe using critical sections
 * - Atomic flag for activation state
 * - Preserves original functionality for unsupported operations
 * 
 * - Использует подмену оконной процедуры (неинвазивная техника)
 * - Потокобезопасно с использованием критических секций
 * - Атомарный флаг для состояния активации
 * - Сохраняет оригинальную функциональность для неподдерживаемых операций
 * 
 ******************************************************************************/

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "..\SDK\wa_ipc.h"
#include "..\Decrypt Engine\unicode_decrypt_engine.h"

// Compatibility macros for older Windows SDK versions
// Макросы совместимости для старых версий Windows SDK
#ifndef GetWindowLongPtrA
#define GetWindowLongPtrA GetWindowLongA
#endif
#ifndef SetWindowLongPtrA
#define SetWindowLongPtrA SetWindowLongA
#endif

/*******************************************************************************
 * GLOBAL STATE VARIABLES
 * ГЛОБАЛЬНЫЕ ПЕРЕМЕННЫЕ СОСТОЯНИЯ
 ******************************************************************************/

// Critical section for thread-safe access to module state
// Критическая секция для потокобезопасного доступа к состоянию модуля
static CRITICAL_SECTION g_cs;

// Pointer to original Winamp window procedure before subclassing
// Указатель на оригинальную оконную процедуру Winamp до подмены
static WNDPROC s_OldProc = NULL;

// Atomic flag indicating if module is active and intercepting messages
// 0 = inactive (pass through all messages)
// 1 = active (intercept and process IPC messages)
// Атомарный флаг, указывающий, активен ли модуль и перехватывает ли сообщения
// 0 = неактивен (пропускать все сообщения)
// 1 = активен (перехватывать и обрабатывать IPC-сообщения)
static volatile LONG g_isActive = 0;

/*******************************************************************************
 * HELPER FUNCTIONS
 * ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ
 ******************************************************************************/

/*******************************************************************************
 * Lock / Unlock
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Wrapper functions for entering and leaving the critical section. These
 * ensure thread-safe access to module state variables.
 * 
 * Функции-обёртки для входа и выхода из критической секции. Они обеспечивают
 * потокобезопасный доступ к переменным состояния модуля.
 * 
 * USAGE / ИСПОЛЬЗОВАНИЕ:
 * Always use Lock()/Unlock() pairs when accessing or modifying s_OldProc
 * or other shared state to prevent race conditions.
 * 
 * Всегда используйте пары Lock()/Unlock() при доступе или изменении s_OldProc
 * или другого общего состояния для предотвращения состояния гонки.
 ******************************************************************************/
static void Lock()   { EnterCriticalSection(&g_cs); }
static void Unlock() { LeaveCriticalSection(&g_cs); }

/*******************************************************************************
 * is_tag_supported
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Checks if a metadata tag name is in the list of tags that this module
 * handles specially (attempts direct ID3 reading before fallback). Uses
 * an efficient single-pass string scanning technique.
 * 
 * Проверяет, находится ли имя тега метаданных в списке тегов, которые
 * этот модуль обрабатывает особым образом (пытается прямое чтение ID3
 * перед резервным вариантом). Использует эффективную технику сканирования
 * строки за один проход.
 * 
 * PARAMETERS / ПАРАМЕТРЫ:
 * k - Tag name to check (e.g., "title", "artist", "album")
 *     Имя тега для проверки (например, "title", "artist", "album")
 * 
 * RETURNS / ВОЗВРАЩАЕТ:
 * BOOL - TRUE if tag is supported (should attempt direct ID3 read)
 *        FALSE if tag is not supported (use standard handler only)
 *        TRUE если тег поддерживается (следует попытаться прямое чтение ID3)
 *        FALSE если тег не поддерживается (использовать только стандартный обработчик)
 * 
 * SUPPORTED TAGS / ПОДДЕРЖИВАЕМЫЕ ТЕГИ:
 * title, artist, album, comment, comments, genre, track, tracknumber,
 * year, date, albumartist, composer
 * 
 * ALGORITHM / АЛГОРИТМ:
 * The function uses a clever embedded string technique where all tag names
 * are stored in a single null-separated string (each tag ends with \0).
 * This allows compact storage and efficient searching.
 * 
 * Функция использует умную технику встроенной строки, где все имена тегов
 * хранятся в одной строке, разделённой нулями (каждый тег заканчивается \0).
 * Это позволяет компактное хранение и эффективный поиск.
 * 
 * EXAMPLE STRING LAYOUT / ПРИМЕР РАСПОЛОЖЕНИЯ СТРОКИ:
 * "title\0artist\0album\0comment\0..."
 *  ^     ^      ^     ^
 *  |     |      |     |__ next tag starts here
 *  |     |      |________ album tag
 *  |     |_______________ artist tag  
 *  |_____________________ title tag
 ******************************************************************************/
static BOOL is_tag_supported(const char* k)
{
    // Validate input / Проверить ввод
    if (!k || !*k) return FALSE;
    
    // Embedded null-separated string containing all supported tag names
    // Each tag name is followed by a null terminator, double-null at end
    // Встроенная строка с разделителями-нулями, содержащая все поддерживаемые имена тегов
    // Каждое имя тега сопровождается нулевым терминатором, двойной ноль в конце
    const char* p = "title\0artist\0album\0comment\0comments\0genre\0"
                    "track\0tracknumber\0year\0date\0albumartist\0composer\0";
    
    // Iterate through null-separated strings / Перебрать строки, разделённые нулями
    while (*p) {
        // Compare input with current tag (case-insensitive)
        // Сравнить ввод с текущим тегом (без учёта регистра)
        if (lstrcmpiA(k, p) == 0) return TRUE;
        
        // Skip to next tag (advance past current string including null terminator)
        // Перейти к следующему тегу (продвинуться за текущую строку, включая нулевой терминатор)
        while (*p++) ;
    }
    
    return FALSE;  // Tag not found in supported list / Тег не найден в списке поддерживаемых
}

/*******************************************************************************
 * FixAnsiMojibake
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Applies mojibake (character encoding corruption) fix to metadata text.
 * This corrects cases where text was incorrectly encoded or decoded, resulting
 * in garbled characters (especially common with Cyrillic, Japanese, Chinese).
 * 
 * Применяет исправление mojibake (искажения кодировки символов) к тексту
 * метаданных. Это исправляет случаи, когда текст был неправильно закодирован
 * или декодирован, что привело к искажённым символам (особенно часто с
 * кириллицей, японским, китайским).
 * 
 * PARAMETERS / ПАРАМЕТРЫ:
 * metadata - Tag name (e.g., "title", "artist") - used to determine fix method
 *            Имя тега (например, "title", "artist") - используется для определения метода исправления
 * buf      - Buffer containing text to fix (modified in-place)
 *            Буфер, содержащий текст для исправления (изменяется на месте)
 * len      - Buffer length in characters
 *            Длина буфера в символах
 * 
 * NOTES / ПРИМЕЧАНИЯ:
 * - Only processes if buffer is non-empty and metadata name is provided
 * - Modifies buffer in-place (no allocation needed)
 * - Uses DECRYPT_FixTagA from unicode_decrypt_engine
 * 
 * - Обрабатывает только если буфер непустой и предоставлено имя метаданных
 * - Изменяет буфер на месте (выделение памяти не требуется)
 * - Использует DECRYPT_FixTagA из unicode_decrypt_engine
 ******************************************************************************/
static void FixAnsiMojibake(const char* metadata, char* buf, int len)
{
    // Only process if buffer has content and metadata name is provided
    // Обрабатывать только если буфер имеет содержимое и предоставлено имя метаданных
    if (buf && buf[0] && metadata) {
        // Apply encoding fix to the buffer
        // Применить исправление кодировки к буферу
        DECRYPT_FixTagA(metadata, buf, len);
    }
}

/*******************************************************************************
 * WINDOW PROCEDURE (SUBCLASS)
 * ОКОННАЯ ПРОЦЕДУРА (ПОДМЕНА)
 ******************************************************************************/

/*******************************************************************************
 * MLFix_WndProc
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Custom window procedure that intercepts Winamp IPC messages related to
 * metadata retrieval. This is the core of the module - it sits between
 * the caller and Winamp's original handler, correcting encoding issues.
 * 
 * Пользовательская оконная процедура, которая перехватывает IPC-сообщения
 * Winamp, связанные с получением метаданных. Это ядро модуля - оно находится
 * между вызывающей стороной и оригинальным обработчиком Winamp, исправляя
 * проблемы кодировки.
 * 
 * PARAMETERS / ПАРАМЕТРЫ:
 * hwnd   - Handle to window / Дескриптор окна
 * msg    - Message identifier / Идентификатор сообщения
 * wParam - First message parameter / Первый параметр сообщения
 * lParam - Second message parameter / Второй параметр сообщения
 * 
 * RETURNS / ВОЗВРАЩАЕТ:
 * LRESULT - Message processing result / Результат обработки сообщения
 * 
 * INTERCEPTED MESSAGES / ПЕРЕХВАТЫВАЕМЫЕ СООБЩЕНИЯ:
 * 
 * WM_WA_IPC with IPC_GET_EXTENDED_FILE_INFO:
 *   Extended metadata query (artist, album, genre, etc.)
 *   Strategy:
 *   1. Check if tag is in supported list
 *   2. If yes, try direct ID3 read first
 *   3. If direct read succeeds, return that result
 *   4. If direct read fails, call original handler
 *   5. Apply mojibake fix to result
 *   6. Return corrected result
 * 
 *   Расширенный запрос метаданных (исполнитель, альбом, жанр и т.д.)
 *   Стратегия:
 *   1. Проверить, находится ли тег в списке поддерживаемых
 *   2. Если да, сначала попытаться прямое чтение ID3
 *   3. Если прямое чтение успешно, вернуть этот результат
 *   4. Если прямое чтение не удалось, вызвать оригинальный обработчик
 *   5. Применить исправление mojibake к результату
 *   6. Вернуть исправленный результат
 * 
 * WM_WA_IPC with IPC_GET_BASIC_FILE_INFO:
 *   Basic file info query (primarily title)
 *   Strategy:
 *   1. Call original handler first (gets all basic info)
 *   2. For title field specifically:
 *      a. Try direct ID3 read
 *      b. If successful, use that
 *      c. If not, apply mojibake fix to existing title
 *   3. Return result
 * 
 *   Запрос базовой информации о файле (в основном название)
 *   Стратегия:
 *   1. Сначала вызвать оригинальный обработчик (получает всю базовую информацию)
 *   2. Специально для поля названия:
 *      a. Попытаться прямое чтение ID3
 *      b. Если успешно, использовать это
 *      c. Если нет, применить исправление mojibake к существующему названию
 *   3. Вернуть результат
 ******************************************************************************/
static LRESULT CALLBACK MLFix_WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    // Get original window procedure (thread-safe read of cached pointer)
    // Получить оригинальную оконную процедуру (потокобезопасное чтение кэшированного указателя)
    WNDPROC old = s_OldProc;
    if (!old) return DefWindowProcA(hwnd, msg, wParam, lParam);
    
    // If module is not active, pass through to original handler
    // Если модуль неактивен, передать оригинальному обработчику
    if (g_isActive != 1) return CallWindowProcA(old, hwnd, msg, wParam, lParam);

    // Check if this is a Winamp IPC message
    // Проверить, является ли это IPC-сообщением Winamp
    if (msg == WM_WA_IPC)
    {
        /*******************************************************************
         * HANDLE: IPC_GET_EXTENDED_FILE_INFO
         * Extended metadata query (artist, album, genre, year, etc.)
         * 
         * ОБРАБОТКА: IPC_GET_EXTENDED_FILE_INFO
         * Расширенный запрос метаданных (исполнитель, альбом, жанр, год и т.д.)
         *******************************************************************/
        if (lParam == IPC_GET_EXTENDED_FILE_INFO && wParam) 
        {
            // Cast wParam to extended file info structure pointer
            // Преобразовать wParam в указатель на структуру расширенной информации о файле
            extendedFileInfoStruct* e = (extendedFileInfoStruct*)wParam;
            
            // Validate structure fields / Проверить поля структуры
            if (e->filename && e->metadata && e->ret && e->retlen > 1) 
            {
                // STRATEGY 1: Try direct ID3 read for supported tags
                // СТРАТЕГИЯ 1: Попытаться прямое чтение ID3 для поддерживаемых тегов
                
                // Check if this tag is in our supported list
                // Проверить, находится ли этот тег в нашем списке поддерживаемых
                if (is_tag_supported(e->metadata)) {
                    char tmp[1024]; 
                    tmp[0] = 0;
                    
                    // Try to read directly from ID3 tags
                    // Попытаться прочитать напрямую из ID3-тегов
                    if (ID3_ReadFieldA(e->filename, e->metadata, tmp, sizeof(tmp)) && tmp[0]) {
                        // Direct read succeeded - copy to output buffer and return
                        // Прямое чтение успешно - копировать в выходной буфер и вернуться
                        lstrcpynA(e->ret, tmp, e->retlen);
                        return 1;  // Success / Успех
                    }
                }
                
                // STRATEGY 2: Direct read failed or tag not supported
                // Call original handler and apply mojibake fix to result
                // СТРАТЕГИЯ 2: Прямое чтение не удалось или тег не поддерживается
                // Вызвать оригинальный обработчик и применить исправление mojibake к результату
                
                // Call original Winamp handler / Вызвать оригинальный обработчик Winamp
                LRESULT r = CallWindowProcA(old, hwnd, msg, wParam, lParam);
                
                // Apply encoding fix to the result / Применить исправление кодировки к результату
                FixAnsiMojibake(e->metadata, e->ret, e->retlen);
                
                return r;
            }
        }
        
        /*******************************************************************
         * HANDLE: IPC_GET_BASIC_FILE_INFO
         * Basic file information query (title, length, etc.)
         * 
         * ОБРАБОТКА: IPC_GET_BASIC_FILE_INFO
         * Запрос базовой информации о файле (название, длина и т.д.)
         *******************************************************************/
        else if (lParam == IPC_GET_BASIC_FILE_INFO && wParam) 
        {
            // Cast wParam to basic file info structure pointer
            // Преобразовать wParam в указатель на структуру базовой информации о файле
            basicFileInfoStruct* b = (basicFileInfoStruct*)wParam;
            
            // Call original handler first to populate all basic info fields
            // This gets title, length, and other basic properties
            // Сначала вызвать оригинальный обработчик для заполнения всех полей базовой информации
            // Это получает название, длину и другие базовые свойства
            LRESULT r = CallWindowProcA(old, hwnd, msg, wParam, lParam);

            // Now specifically handle the title field
            // Теперь специально обработать поле названия
            if (b->filename && b->title && b->titlelen > 1) {
                char tmp[512]; 
                tmp[0] = 0;
                
                // Try to read title directly from ID3 tags
                // Попытаться прочитать название напрямую из ID3-тегов
                if (ID3_ReadFieldA(b->filename, "title", tmp, sizeof(tmp)) && tmp[0]) {
                    // Direct read succeeded - use this title
                    // Прямое чтение успешно - использовать это название
                    lstrcpynA(b->title, tmp, b->titlelen);
                } else {
                    // Direct read failed - apply mojibake fix to existing title
                    // Прямое чтение не удалось - применить исправление mojibake к существующему названию
                    FixAnsiMojibake("title", b->title, b->titlelen);
                }
            }
            
            return r;
        }
    }

    // For all other messages, pass through to original handler
    // Для всех остальных сообщений передать оригинальному обработчику
    return CallWindowProcA(old, hwnd, msg, wParam, lParam);
}

/*******************************************************************************
 * EXPORTED FUNCTIONS
 * ЭКСПОРТИРУЕМЫЕ ФУНКЦИИ
 ******************************************************************************/

/*******************************************************************************
 * ML_UnicodeFix_Init
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Initializes the Media Library Unicode fix module by subclassing the main
 * Winamp window. After initialization, the module will intercept metadata
 * IPC messages and apply encoding fixes.
 * 
 * Инициализирует модуль исправления Unicode медиабиблиотеки, подменяя главное
 * окно Winamp. После инициализации модуль будет перехватывать IPC-сообщения
 * метаданных и применять исправления кодировки.
 * 
 * PARAMETERS / ПАРАМЕТРЫ:
 * hwndWinamp - Handle to main Winamp window
 *              Дескриптор главного окна Winamp
 * 
 * INITIALIZATION PROCESS / ПРОЦЕСС ИНИЦИАЛИЗАЦИИ:
 * 1. Validate window handle
 * 2. Initialize critical section for thread safety
 * 3. Enter critical section (lock)
 * 4. Check if already initialized (idempotency)
 * 5. Get current window procedure
 * 6. Verify it's not already our procedure
 * 7. Save original procedure pointer
 * 8. Install our custom procedure (subclass)
 * 9. If successful, set active flag
 * 10. If failed, clear saved pointer
 * 11. Exit critical section (unlock)
 * 
 * 1. Проверить дескриптор окна
 * 2. Инициализировать критическую секцию для потокобезопасности
 * 3. Войти в критическую секцию (заблокировать)
 * 4. Проверить, уже инициализирован (идемпотентность)
 * 5. Получить текущую оконную процедуру
 * 6. Проверить, что это ещё не наша процедура
 * 7. Сохранить указатель на оригинальную процедуру
 * 8. Установить нашу пользовательскую процедуру (подмену)
 * 9. Если успешно, установить флаг активности
 * 10. Если не удалось, очистить сохранённый указатель
 * 11. Выйти из критической секции (разблокировать)
 * 
 * SAFETY FEATURES / ФУНКЦИИ БЕЗОПАСНОСТИ:
 * - Validates window handle before proceeding
 * - Uses critical section to prevent race conditions
 * - Checks for duplicate initialization
 * - Atomic flag setting for activation state
 * - Verifies we're not subclassing our own procedure
 * 
 * - Проверяет дескриптор окна перед продолжением
 * - Использует критическую секцию для предотвращения состояния гонки
 * - Проверяет на дублирующую инициализацию
 * - Атомарная установка флага для состояния активации
 * - Проверяет, что мы не подменяем нашу собственную процедуру
 ******************************************************************************/
void ML_UnicodeFix_Init(HWND hwndWinamp)
{
    // Validate window handle / Проверить дескриптор окна
    if (!IsWindow(hwndWinamp)) return;

    // Initialize critical section for thread-safe operations
    // Инициализировать критическую секцию для потокобезопасных операций
    InitializeCriticalSection(&g_cs);
    
    // Enter critical section (lock) / Войти в критическую секцию (заблокировать)
    Lock();

    // Only initialize if not already done (idempotency check)
    // Инициализировать только если ещё не выполнено (проверка идемпотентности)
    if (!s_OldProc) {
        // Get current window procedure / Получить текущую оконную процедуру
        WNDPROC old = (WNDPROC)GetWindowLongPtrA(hwndWinamp, GWLP_WNDPROC);
        
        // Verify we got a valid procedure and it's not already ours
        // Проверить, что получили допустимую процедуру, и это ещё не наша
        if (old && old != MLFix_WndProc) {
            // Save original procedure pointer
            // Сохранить указатель на оригинальную процедуру
            s_OldProc = old;
            
            // Install our custom window procedure (subclass the window)
            // Установить нашу пользовательскую оконную процедуру (подменить окно)
            if (SetWindowLongPtrA(hwndWinamp, GWLP_WNDPROC, (LONG_PTR)MLFix_WndProc)) {
                // Subclassing succeeded - activate the module
                // Uses atomic operation for thread-safe flag setting
                // Подмена успешна - активировать модуль
                // Использует атомарную операцию для потокобезопасной установки флага
                InterlockedExchange(&g_isActive, 1);
            } else {
                // Subclassing failed - clear saved pointer to indicate failure
                // Подмена не удалась - очистить сохранённый указатель для обозначения неудачи
                s_OldProc = NULL;
            }
        }
    }
    
    // Exit critical section (unlock) / Выйти из критической секции (разблокировать)
    Unlock();
}

/*******************************************************************************
 * ML_UnicodeFix_Quit
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Cleans up the Media Library Unicode fix module by unsubclassing the main
 * Winamp window and restoring the original window procedure. Must be called
 * during Winamp shutdown to prevent crashes.
 * 
 * Очищает модуль исправления Unicode медиабиблиотеки, снимая подмену с
 * главного окна Winamp и восстанавливая оригинальную оконную процедуру.
 * Должна быть вызвана при завершении Winamp для предотвращения крашей.
 * 
 * PARAMETERS / ПАРАМЕТРЫ:
 * hwndWinamp - Handle to main Winamp window
 *              Дескриптор главного окна Winamp
 * 
 * CLEANUP PROCESS / ПРОЦЕСС ОЧИСТКИ:
 * 1. Validate window handle
 * 2. Enter critical section (lock)
 * 3. Check if we actually subclassed the window
 * 4. Deactivate module (atomic flag clear)
 * 5. Get current window procedure
 * 6. Verify it's still our procedure (safety check)
 * 7. If yes, restore original procedure
 * 8. Exit critical section (unlock)
 * 9. Delete critical section object
 * 
 * 1. Проверить дескриптор окна
 * 2. Войти в критическую секцию (заблокировать)
 * 3. Проверить, действительно ли мы подменили окно
 * 4. Деактивировать модуль (атомарная очистка флага)
 * 5. Получить текущую оконную процедуру
 * 6. Проверить, что это всё ещё наша процедура (проверка безопасности)
 * 7. Если да, восстановить оригинальную процедуру
 * 8. Выйти из критической секции (разблокировать)
 * 9. Удалить объект критической секции
 * 
 * IMPORTANCE / ВАЖНОСТЬ:
 * CRITICAL: This function must be called before module unload to prevent
 * crashes. Failing to restore the window procedure will cause Winamp to
 * call unloaded code when processing messages.
 * 
 * КРИТИЧНО: Эта функция должна быть вызвана до выгрузки модуля для
 * предотвращения крашей. Неспособность восстановить оконную процедуру
 * приведёт к тому, что Winamp попытается вызвать выгруженный код при
 * обработке сообщений.
 * 
 * SAFETY FEATURES / ФУНКЦИИ БЕЗОПАСНОСТИ:
 * - Validates window handle before proceeding
 * - Deactivates module before restoration (prevents message processing)
 * - Verifies current procedure is ours before restoration
 * - Uses atomic operation to clear active flag
 * - Properly deletes critical section
 * 
 * - Проверяет дескриптор окна перед продолжением
 * - Деактивирует модуль перед восстановлением (предотвращает обработку сообщений)
 * - Проверяет, что текущая процедура наша, перед восстановлением
 * - Использует атомарную операцию для очистки флага активности
 * - Правильно удаляет критическую секцию
 ******************************************************************************/
void ML_UnicodeFix_Quit(HWND hwndWinamp)
{
    // Validate window handle / Проверить дескриптор окна
    if (!IsWindow(hwndWinamp)) return;

    // Enter critical section (lock) / Войти в критическую секцию (заблокировать)
    Lock();
    
    // Check if we actually subclassed the window
    // Проверить, действительно ли мы подменили окно
    if (s_OldProc) {
        // STEP 1: Deactivate module first (prevent further message processing)
        // This is done atomically for thread safety
        // ШАГ 1: Сначала деактивировать модуль (предотвратить дальнейшую обработку сообщений)
        // Это делается атомарно для потокобезопасности
        InterlockedExchange(&g_isActive, 0);
        
        // STEP 2: Restore original window procedure if we're still the current one
        // ШАГ 2: Восстановить оригинальную оконную процедуру, если мы всё ещё текущая
        
        // Get current window procedure / Получить текущую оконную процедуру
        WNDPROC cur = (WNDPROC)GetWindowLongPtrA(hwndWinamp, GWLP_WNDPROC);
        
        // Safety check: only restore if current procedure is ours
        // This prevents restoring over someone else's subclass
        // Проверка безопасности: восстанавливать только если текущая процедура наша
        // Это предотвращает восстановление поверх чужой подмены
        if (cur == MLFix_WndProc) {
            SetWindowLongPtrA(hwndWinamp, GWLP_WNDPROC, (LONG_PTR)s_OldProc);
        }
        
        // Note: s_OldProc is intentionally not cleared here to allow
        // detection of whether we were ever initialized
        // Примечание: s_OldProc намеренно не очищается здесь, чтобы позволить
        // обнаружение, были ли мы когда-либо инициализированы
    }
    
    // Exit critical section (unlock) / Выйти из критической секции (разблокировать)
    Unlock();
    
    // Clean up critical section object (must be done outside the lock)
    // Очистить объект критической секции (должно быть выполнено вне блокировки)
    DeleteCriticalSection(&g_cs);
}