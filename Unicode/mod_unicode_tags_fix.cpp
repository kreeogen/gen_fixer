/*******************************************************************************
 * mod_unicode_tags_fix.cpp
 * 
 * UNICODE ID3 TAGS VIRTUALIZATION MODULE
 * МОДУЛЬ ВИРТУАЛИЗАЦИИ UNICODE ID3 ТЕГОВ
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Fixes Unicode ID3 tag reading in Winamp by virtualizing MP3 files on-the-fly.
 * Intercepts file operations and replaces ID3 tags with properly encoded versions.
 * 
 * Исправляет чтение Unicode ID3 тегов в Winamp виртуализацией MP3 файлов на лету.
 * Перехватывает файловые операции и заменяет ID3 теги правильно закодированными версиями.
 * 
 * PROBLEM BEING FIXED / ИСПРАВЛЯЕМАЯ ПРОБЛЕМА:
 * Winamp's in_mp3.dll has issues reading certain Unicode ID3 tags:
 * - Cyrillic characters may display as garbage
 * - Encoding detection fails for some tag formats
 * - Tag structure may be malformed
 * 
 * in_mp3.dll Winamp имеет проблемы с чтением некоторых Unicode ID3 тегов:
 * - Кириллические символы могут отображаться как мусор
 * - Обнаружение кодировки терпит неудачу для некоторых форматов тегов
 * - Структура тегов может быть искажена
 * 
 * HOW IT WORKS / КАК ЭТО РАБОТАЕТ:
 * 1. Hooks KERNEL32 file operations in in_mp3.dll via IAT patching
 * 2. When MP3 file is opened for reading:
 *    a. Reads existing ID3 tags using decrypt engine
 *    b. Rebuilds tags in proper format
 *    c. Creates virtual file context
 * 3. On subsequent reads:
 *    a. If reading tag area: returns rebuilt tag data
 *    b. If reading audio data: redirects to actual file with offset adjustment
 * 4. Virtual file appears to have correct tags without modifying disk
 * 
 * 1. Перехватывает файловые операции KERNEL32 в in_mp3.dll через IAT патчинг
 * 2. Когда MP3 файл открывается для чтения:
 *    a. Читает существующие ID3 теги используя движок расшифровки
 *    b. Перестраивает теги в правильном формате
 *    c. Создаёт контекст виртуального файла
 * 3. При последующих чтениях:
 *    a. Если чтение области тегов: возвращает перестроенные данные тегов
 *    b. Если чтение аудио данных: перенаправляет к реальному файлу с корректировкой смещения
 * 4. Виртуальный файл выглядит как имеющий правильные теги без изменения диска
 * 
 * KEY TECHNIQUES / КЛЮЧЕВЫЕ ТЕХНИКИ:
 * - IAT (Import Address Table) hooking
 * - File system virtualization
 * - Context pooling for multiple open files
 * - Thread-safe reference counting
 * - Lazy initialization with thread synchronization
 * 
 * - IAT (Import Address Table) перехват
 * - Виртуализация файловой системы
 * - Пулинг контекстов для нескольких открытых файлов
 * - Потокобезопасный подсчёт ссылок
 * - Ленивая инициализация с синхронизацией потоков
 * 
 * ADVANTAGES OF THIS APPROACH / ПРЕИМУЩЕСТВА ЭТОГО ПОДХОДА:
 * - No disk modifications (non-destructive)
 * - Works transparently with in_mp3.dll
 * - Handles multiple open files simultaneously
 * - No performance impact on files without problematic tags
 * 
 * - Нет изменений на диске (неразрушающий)
 * - Работает прозрачно с in_mp3.dll
 * - Обрабатывает несколько открытых файлов одновременно
 * - Нет влияния на производительность для файлов без проблемных тегов
 * 
 * THREAD SAFETY / ПОТОКОБЕЗОПАСНОСТЬ:
 * - Uses critical section for context pool access
 * - Interlocked operations for reference counting
 * - Careful handling of context lifecycle during closure
 * 
 * - Использует критическую секцию для доступа к пулу контекстов
 * - Interlocked операции для подсчёта ссылок
 * - Осторожная обработка жизненного цикла контекста при закрытии
 * 
 * COMPATIBILITY / СОВМЕСТИМОСТЬ:
 * - Winamp 2.95 and compatible versions
 * - Works with in_mp3.dll
 * - Windows 98 through Windows 11
 * 
 * - Winamp 2.95 и совместимые версии
 * - Работает с in_mp3.dll
 * - Windows 98 до Windows 11
 * 
 ******************************************************************************/

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <process.h>      // For _beginthreadex / Для _beginthreadex
#include <stdio.h>
#include <string.h>

// Unicode decrypt engine - provides ID3 tag reading/writing functions
// Движок расшифровки Unicode - предоставляет функции чтения/записи ID3 тегов
#include "..\\Decrypt Engine\\unicode_decrypt_engine.h"
#include "mod_unicode_tags_fix.h"

// MP3 Save Fix module - handles tag writing
// Модуль исправления сохранения MP3 - обрабатывает запись тегов
#include "..\Modules\mod_save_id3v2.h"

/*******************************************************************************
 * CONFIGURATION & LIMITS
 * КОНФИГУРАЦИЯ И ОГРАНИЧЕНИЯ
 ******************************************************************************/

/*******************************************************************************
 * TAG_SANITY_LIMIT
 * 
 * Maximum allowed ID3 tag size (50 MB).
 * Prevents memory allocation issues with corrupted/malicious tags.
 * 
 * Максимальный допустимый размер ID3 тега (50 МБ).
 * Предотвращает проблемы выделения памяти с повреждёнными/вредоносными тегами.
 ******************************************************************************/
#define TAG_SANITY_LIMIT (50*1024*1024)

/*******************************************************************************
 * WBUF_MAX
 * 
 * Maximum buffer size for wide character strings.
 * Used for tag field values (artist, title, album, etc.).
 * 
 * Максимальный размер буфера для wide character строк.
 * Используется для значений полей тегов (исполнитель, название, альбом и т.д.).
 ******************************************************************************/
#define WBUF_MAX 2048

/*******************************************************************************
 * MAX_OPEN_MP3S
 * 
 * Maximum number of simultaneously virtualized MP3 files.
 * Context pool size.
 * 
 * Максимальное количество одновременно виртуализированных MP3 файлов.
 * Размер пула контекстов.
 ******************************************************************************/
#define MAX_OPEN_MP3S 32

/*******************************************************************************
 * GLOBAL STATE
 * ГЛОБАЛЬНОЕ СОСТОЯНИЕ
 ******************************************************************************/

/*******************************************************************************
 * g_hooksReady
 * 
 * Hook installation status:
 * 0 = Not initialized
 * 1 = Hooks installed and active
 * 
 * Uses volatile and interlocked operations for thread safety.
 * 
 * Статус установки хуков:
 * 0 = Не инициализировано
 * 1 = Хуки установлены и активны
 * 
 * Использует volatile и interlocked операции для потокобезопасности.
 ******************************************************************************/
static volatile LONG g_hooksReady = 0;

/*******************************************************************************
 * g_hInMp3
 * 
 * Handle to in_mp3.dll module.
 * Used for IAT patching.
 * 
 * Дескриптор модуля in_mp3.dll.
 * Используется для IAT патчинга.
 ******************************************************************************/
static HMODULE g_hInMp3 = NULL;

/*******************************************************************************
 * DYNAMIC BUFFER UTILITY
 * УТИЛИТА ДИНАМИЧЕСКОГО БУФЕРА
 * 
 * Simple growable byte buffer for building ID3 tags.
 * Простой растущий байтовый буфер для построения ID3 тегов.
 ******************************************************************************/

/*******************************************************************************
 * UniDynBuf
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Dynamic byte buffer that grows as needed.
 * Used for assembling ID3 tag data.
 * 
 * Динамический байтовый буфер который растёт по мере необходимости.
 * Используется для сборки данных ID3 тегов.
 * 
 * FIELDS / ПОЛЯ:
 * data     - Pointer to buffer / Указатель на буфер
 * size     - Current used size / Текущий использованный размер
 * capacity - Allocated capacity / Выделенная ёмкость
 ******************************************************************************/
typedef struct { 
    BYTE* data; 
    DWORD size; 
    DWORD capacity; 
} UniDynBuf;

/*******************************************************************************
 * DB_Init
 * 
 * Initializes dynamic buffer with specified capacity.
 * Инициализирует динамический буфер с указанной ёмкостью.
 ******************************************************************************/
static BOOL DB_Init(UniDynBuf* d, DWORD cap) {
    if (!d) return FALSE;
    d->data = NULL;
    d->size = 0;
    d->capacity = 0;
    
    // Allocate initial capacity (default 256 if cap=0) / Выделить начальную ёмкость (по умолчанию 256 если cap=0)
    d->data = (BYTE*)HeapAlloc(GetProcessHeap(), 0, cap ? cap : 256);
    if (!d->data) return FALSE;
    
    d->capacity = cap ? cap : 256;
    return TRUE;
}

/*******************************************************************************
 * DB_Append
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Appends data to buffer, automatically growing if needed.
 * Добавляет данные к буферу, автоматически увеличивая если нужно.
 * 
 * PARAMETERS / ПАРАМЕТРЫ:
 * d   - Buffer to append to / Буфер для добавления
 * src - Source data / Исходные данные
 * len - Length of data / Длина данных
 * 
 * RETURNS / ВОЗВРАЩАЕТ:
 * TRUE if append succeeded, FALSE on memory allocation failure
 * TRUE если добавление удалось, FALSE при ошибке выделения памяти
 * 
 * GROWTH STRATEGY / СТРАТЕГИЯ РОСТА:
 * Doubles capacity each time more space is needed.
 * Удваивает ёмкость каждый раз когда нужно больше места.
 ******************************************************************************/
static BOOL DB_Append(UniDynBuf* d, const void* src, DWORD len) {
    if (!d || !d->data) return FALSE;
    if (len == 0) return TRUE;  // Nothing to append / Нечего добавлять
    
    DWORD need = d->size + len;
    
    // Grow buffer if needed / Увеличить буфер если нужно
    if (need > d->capacity) {
        DWORD newCap = d->capacity ? d->capacity : 256;
        while (newCap < need) newCap *= 2;  // Double until sufficient / Удваивать пока не достаточно
        
        BYTE* n = (BYTE*)HeapReAlloc(GetProcessHeap(), 0, d->data, newCap);
        if (!n) return FALSE;  // Allocation failed / Выделение не удалось
        
        d->data = n;
        d->capacity = newCap;
    }
    
    // Copy data and update size / Скопировать данные и обновить размер
    memcpy(d->data + d->size, src, len);
    d->size = need;
    return TRUE;
}

/*******************************************************************************
 * IAT PATCHER UTILITY
 * УТИЛИТА IAT ПАТЧЕРА
 * 
 * Patches Import Address Table to redirect function calls.
 * Патчит Import Address Table для перенаправления вызовов функций.
 ******************************************************************************/

/*******************************************************************************
 * IAT_Patch
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Patches a single import in module's IAT.
 * Finds imported function by name and replaces its address.
 * 
 * Патчит один импорт в IAT модуля.
 * Находит импортированную функцию по имени и заменяет её адрес.
 * 
 * PARAMETERS / ПАРАМЕТРЫ:
 * hMod    - Module to patch / Модуль для патча
 * dll     - DLL name containing function (e.g., "KERNEL32.dll") / Имя DLL содержащей функцию
 * func    - Function name to patch (e.g., "ReadFile") / Имя функции для патча
 * newFunc - New function address / Новый адрес функции
 * oldFunc - Output: original function address / Вывод: оригинальный адрес функции
 * 
 * RETURNS / ВОЗВРАЩАЕТ:
 * TRUE if patch succeeded, FALSE otherwise
 * TRUE если патч удался, FALSE иначе
 * 
 * ALGORITHM / АЛГОРИТМ:
 * 1. Validate PE structure
 * 2. Find import directory
 * 3. Search for specified DLL
 * 4. Search for specified function name
 * 5. Change memory protection
 * 6. Replace function pointer
 * 7. Restore memory protection
 * 
 * 1. Проверить PE структуру
 * 2. Найти каталог импорта
 * 3. Искать указанную DLL
 * 4. Искать указанное имя функции
 * 5. Изменить защиту памяти
 * 6. Заменить указатель функции
 * 7. Восстановить защиту памяти
 ******************************************************************************/
static BOOL IAT_Patch(HMODULE hMod, const char* dll, const char* func, void* newFunc, void** oldFunc) {
    if (!hMod || !dll || !func || !newFunc) return FALSE;
    
    // Validate DOS header / Проверить DOS заголовок
    PIMAGE_DOS_HEADER dos = (PIMAGE_DOS_HEADER)hMod;
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) return FALSE;
    
    // Validate NT header / Проверить NT заголовок
    PIMAGE_NT_HEADERS nt = (PIMAGE_NT_HEADERS)((BYTE*)hMod + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) return FALSE;
    
    // Get import directory / Получить каталог импорта
    DWORD impDirRVA = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress;
    if (impDirRVA == 0) return FALSE;  // No imports / Нет импортов

    PIMAGE_IMPORT_DESCRIPTOR imp = (PIMAGE_IMPORT_DESCRIPTOR)((BYTE*)hMod + impDirRVA);
    
    // Basic sanity check / Базовая проверка работоспособности
    if ((BYTE*)imp == (BYTE*)hMod) return FALSE;

    // Iterate through import descriptors / Итерировать через дескрипторы импорта
    for (; imp->Name; imp++) {
        const char* name = (const char*)((BYTE*)hMod + imp->Name);
        
        // Check if this is the DLL we're looking for / Проверить, это ли DLL которую мы ищем
        if (lstrcmpiA(name, dll) == 0) {
            // Get thunk arrays / Получить массивы thunk
            PIMAGE_THUNK_DATA oft = (PIMAGE_THUNK_DATA)((BYTE*)hMod + imp->OriginalFirstThunk);
            PIMAGE_THUNK_DATA ft = (PIMAGE_THUNK_DATA)((BYTE*)hMod + imp->FirstThunk);
            
            // Iterate through functions / Итерировать через функции
            for (; oft->u1.Function; oft++, ft++) {
                // Skip ordinal imports / Пропустить импорты по порядковому номеру
                if (IMAGE_SNAP_BY_ORDINAL(oft->u1.Ordinal)) continue;
                
                // Get function name / Получить имя функции
                PIMAGE_IMPORT_BY_NAME pIBN = (PIMAGE_IMPORT_BY_NAME)((BYTE*)hMod + oft->u1.AddressOfData);
                
                // Check if this is the function we're looking for / Проверить, это ли функция которую мы ищем
                if (lstrcmpiA((char*)pIBN->Name, func) == 0) {
                    // Make memory writable / Сделать память записываемой
                    DWORD old; 
                    if (VirtualProtect(&ft->u1.Function, 4, PAGE_EXECUTE_READWRITE, &old)) {
                        // Save original and install hook / Сохранить оригинал и установить хук
                        if (oldFunc) *oldFunc = (void*)ft->u1.Function;
                        ft->u1.Function = (DWORD)newFunc;
                        
                        // Restore protection / Восстановить защиту
                        VirtualProtect(&ft->u1.Function, 4, old, &old);
                        return TRUE;
                    }
                }
            }
        }
    }
    return FALSE;
}

/*******************************************************************************
 * VIRTUAL FILE CONTEXT SYSTEM
 * СИСТЕМА КОНТЕКСТА ВИРТУАЛЬНОГО ФАЙЛА
 * 
 * Tracks state of virtualized MP3 files.
 * Each open MP3 file gets a context that describes its virtual state.
 * 
 * Отслеживает состояние виртуализированных MP3 файлов.
 * Каждый открытый MP3 файл получает контекст описывающий его виртуальное состояние.
 ******************************************************************************/

/*******************************************************************************
 * OverlayCtx
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Context for a single virtualized MP3 file.
 * Stores rebuilt tag data and tracks virtual file position.
 * 
 * Контекст для одного виртуализированного MP3 файла.
 * Хранит перестроенные данные тегов и отслеживает позицию виртуального файла.
 * 
 * FIELDS / ПОЛЯ:
 * 
 * hReal (HANDLE) - Original file handle from CreateFile
 *                  Оригинальный дескриптор файла из CreateFile
 * 
 * hKeep (HANDLE) - Duplicate handle for independent seeking
 *                  Дубликат дескриптора для независимого поиска
 *                  
 *                  WHY DUPLICATE / ПОЧЕМУ ДУБЛИКАТ:
 *                  hReal's file pointer may be moved by hooked functions.
 *                  hKeep allows us to seek independently when reading audio data.
 *                  
 *                  Указатель файла hReal может быть перемещён перехваченными функциями.
 *                  hKeep позволяет нам искать независимо при чтении аудио данных.
 * 
 * refs (volatile LONG) - Reference count for thread safety
 *                        Счётчик ссылок для потокобезопасности
 *                        
 *                        Incremented when operation starts, decremented when done.
 *                        Prevents freeing context while operation in progress.
 *                        
 *                        Увеличивается когда операция начинается, уменьшается когда закончена.
 *                        Предотвращает освобождение контекста пока операция выполняется.
 * 
 * closing (BOOL) - Set to TRUE when CloseHandle called
 *                  Устанавливается в TRUE когда вызван CloseHandle
 *                  
 *                  Context is freed when refs reaches 0 and closing is TRUE.
 *                  Контекст освобождается когда refs достигает 0 и closing TRUE.
 * 
 * newTag (BYTE*) - Rebuilt ID3 tag data
 *                  Перестроенные данные ID3 тега
 * 
 * newTagSz (DWORD) - Size of rebuilt tag
 *                    Размер перестроенного тега
 * 
 * oldTagSz (DWORD) - Size of original tag (for offset calculation)
 *                    Размер оригинального тега (для вычисления смещения)
 * 
 * virtSz (DWORD) - Virtual file size (newTagSz + audioSize)
 *                  Виртуальный размер файла (newTagSz + audioSize)
 * 
 * logicalPos (DWORD) - Current virtual file position
 *                      Текущая виртуальная позиция файла
 * 
 * pathA (char[MAX_PATH]) - File path (for debugging)
 *                          Путь к файлу (для отладки)
 * 
 * inUse (BOOL) - TRUE if context slot is allocated
 *                TRUE если слот контекста выделен
 ******************************************************************************/
typedef struct {
    HANDLE hReal;            // Original handle / Оригинальный дескриптор
    HANDLE hKeep;            // Duplicate for seeking / Дубликат для поиска
    volatile LONG refs;      // Reference count / Счётчик ссылок
    BOOL closing;            // Close pending / Ожидание закрытия
    
    BYTE* newTag;            // Rebuilt tag data / Перестроенные данные тега
    DWORD newTagSz;          // New tag size / Размер нового тега
    DWORD oldTagSz;          // Original tag size / Размер оригинального тега
    DWORD virtSz;            // Virtual file size / Виртуальный размер файла
    DWORD logicalPos;        // Current position / Текущая позиция
    
    char pathA[MAX_PATH];    // File path / Путь к файлу
    BOOL inUse;              // Slot in use / Слот используется
} OverlayCtx;

/*******************************************************************************
 * CONTEXT POOL & SYNCHRONIZATION
 * ПУЛ КОНТЕКСТОВ И СИНХРОНИЗАЦИЯ
 ******************************************************************************/

/*******************************************************************************
 * g_ctxPool
 * 
 * Pool of context structures for open files.
 * Fixed-size array, no dynamic allocation.
 * 
 * Пул структур контекстов для открытых файлов.
 * Массив фиксированного размера, без динамического выделения.
 ******************************************************************************/
static OverlayCtx g_ctxPool[MAX_OPEN_MP3S];

/*******************************************************************************
 * g_cs
 * 
 * Critical section protecting context pool access.
 * Must be held when searching, allocating, or freeing contexts.
 * 
 * Критическая секция защищающая доступ к пулу контекстов.
 * Должна удерживаться при поиске, выделении или освобождении контекстов.
 ******************************************************************************/
static CRITICAL_SECTION g_cs;

/*******************************************************************************
 * g_csInited
 * 
 * Critical section initialization status:
 * 0 = Not initialized
 * 1 = Initialization in progress
 * 2 = Fully initialized
 * 
 * Статус инициализации критической секции:
 * 0 = Не инициализирована
 * 1 = Инициализация в процессе
 * 2 = Полностью инициализирована
 ******************************************************************************/
static volatile LONG g_csInited = 0;

/*******************************************************************************
 * CS_Init
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Lazy initialization of critical section with thread safety.
 * Ленивая инициализация критической секции с потокобезопасностью.
 * 
 * ALGORITHM / АЛГОРИТМ:
 * 1. Use InterlockedCompareExchange for atomic check-and-set
 * 2. First thread to see 0 sets to 1 and initializes
 * 3. Other threads spin-wait until initialization complete (state 2)
 * 
 * 1. Использовать InterlockedCompareExchange для атомарной проверки-и-установки
 * 2. Первый поток увидевший 0 устанавливает в 1 и инициализирует
 * 3. Другие потоки ждут вращением пока инициализация не завершится (состояние 2)
 * 
 * THREAD SAFETY / ПОТОКОБЕЗОПАСНОСТЬ:
 * Multiple threads can call this safely. Only one will initialize.
 * Несколько потоков могут вызывать это безопасно. Только один инициализирует.
 ******************************************************************************/
static void CS_Init(void) {
    // Atomic check: if currently 0, set to 1 and we do initialization
    // Атомарная проверка: если сейчас 0, установить в 1 и мы делаем инициализацию
    if (InterlockedCompareExchange(&g_csInited, 1, 0) == 0) {
        InitializeCriticalSection(&g_cs);
        InterlockedExchange(&g_csInited, 2);  // Mark fully initialized / Отметить полностью инициализированной
    } else {
        // Another thread is initializing - spin-wait until done
        // Другой поток инициализирует - ждать вращением пока не сделано
        while(g_csInited != 2) Sleep(0);
    }
}

/*******************************************************************************
 * Ctx_Lock / Ctx_Unlock
 * 
 * Enter/leave critical section with lazy initialization.
 * Войти/выйти из критической секции с ленивой инициализацией.
 ******************************************************************************/
static void Ctx_Lock(void) { 
    if(g_csInited != 2) CS_Init();
    EnterCriticalSection(&g_cs);
}

static void Ctx_Unlock(void) { 
    LeaveCriticalSection(&g_cs);
}

/*******************************************************************************
 * AllocCtx
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Allocates a free context from pool.
 * Выделяет свободный контекст из пула.
 * 
 * RETURNS / ВОЗВРАЩАЕТ:
 * Pointer to context or NULL if pool exhausted
 * Указатель на контекст или NULL если пул исчерпан
 * 
 * MUST BE CALLED WITH LOCK HELD / ДОЛЖНА ВЫЗЫВАТЬСЯ С УДЕРЖИВАЕМОЙ БЛОКИРОВКОЙ
 ******************************************************************************/
static OverlayCtx* AllocCtx() { 
    for(int i = 0; i < MAX_OPEN_MP3S; i++) {
        if(!g_ctxPool[i].inUse) { 
            ZeroMemory(&g_ctxPool[i], sizeof(OverlayCtx));
            g_ctxPool[i].inUse = TRUE;
            return &g_ctxPool[i];
        }
    }
    return NULL;  // Pool exhausted / Пул исчерпан
}

/*******************************************************************************
 * FindCtx
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Finds context by file handle.
 * Находит контекст по дескриптору файла.
 * 
 * PARAMETERS / ПАРАМЕТРЫ:
 * h - File handle to search for / Дескриптор файла для поиска
 * 
 * RETURNS / ВОЗВРАЩАЕТ:
 * Pointer to context or NULL if not found
 * Указатель на контекст или NULL если не найден
 * 
 * IMPORTANT / ВАЖНО:
 * Ignores contexts marked as closing (closing=TRUE).
 * This prevents race conditions during CloseHandle.
 * 
 * Игнорирует контексты помеченные как closing (closing=TRUE).
 * Это предотвращает состояния гонки во время CloseHandle.
 * 
 * MUST BE CALLED WITH LOCK HELD / ДОЛЖНА ВЫЗЫВАТЬСЯ С УДЕРЖИВАЕМОЙ БЛОКИРОВКОЙ
 ******************************************************************************/
static OverlayCtx* FindCtx(HANDLE h) { 
    for(int i = 0; i < MAX_OPEN_MP3S; i++) {
        // CRITICAL: Ignore closing contexts / КРИТИЧНО: Игнорировать закрывающиеся контексты
        if(g_ctxPool[i].inUse && !g_ctxPool[i].closing && g_ctxPool[i].hReal == h) 
            return &g_ctxPool[i];
    }
    return NULL;
}

/*******************************************************************************
 * FreeCtx
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Frees context resources and marks slot as available.
 * Освобождает ресурсы контекста и помечает слот как доступный.
 * 
 * PARAMETERS / ПАРАМЕТРЫ:
 * c - Context to free / Контекст для освобождения
 * 
 * CLEANUP / ОЧИСТКА:
 * - Frees rebuilt tag buffer
 * - Closes duplicate handle (if different from hReal)
 * - Marks slot as free
 * 
 * - Освобождает буфер перестроенного тега
 * - Закрывает дубликат дескриптора (если отличается от hReal)
 * - Помечает слот как свободный
 * 
 * MUST BE CALLED WITH LOCK HELD / ДОЛЖНА ВЫЗЫВАТЬСЯ С УДЕРЖИВАЕМОЙ БЛОКИРОВКОЙ
 ******************************************************************************/
static void FreeCtx(OverlayCtx* c) {
    // Free tag buffer / Освободить буфер тега
    if(c->newTag) HeapFree(GetProcessHeap(), 0, c->newTag);
    
    // Close duplicate handle if different / Закрыть дубликат дескриптора если отличается
    if(c->hKeep && c->hKeep != c->hReal) CloseHandle(c->hKeep);
    
    // Mark slot as free / Отметить слот как свободный
    c->inUse = FALSE;
}

/*******************************************************************************
 * TAG BUILDING FOR VIRTUALIZATION
 * ПОСТРОЕНИЕ ТЕГОВ ДЛЯ ВИРТУАЛИЗАЦИИ
 ******************************************************************************/

/*******************************************************************************
 * BE24Write
 * 
 * Writes 24-bit big-endian value to buffer.
 * Used for ID3v2.2 frame sizes.
 * 
 * Записывает 24-битное big-endian значение в буфер.
 * Используется для размеров кадров ID3v2.2.
 ******************************************************************************/
static void BE24Write(BYTE* p, DWORD v) { 
    p[0] = (BYTE)(v >> 16);
    p[1] = (BYTE)(v >> 8);
    p[2] = (BYTE)v;
}

/*******************************************************************************
 * BuildID3_Virtual
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Reads existing ID3 tags and rebuilds them in proper format.
 * Core function for tag virtualization.
 * 
 * Читает существующие ID3 теги и перестраивает их в правильном формате.
 * Основная функция для виртуализации тегов.
 * 
 * PARAMETERS / ПАРАМЕТРЫ:
 * hFile  - File handle / Дескриптор файла
 * pNewSz - Output: size of rebuilt tag / Вывод: размер перестроенного тега
 * pOldSz - Output: size of original tag / Вывод: размер оригинального тега
 * 
 * RETURNS / ВОЗВРАЩАЕТ:
 * Pointer to rebuilt tag data (heap allocated) or NULL on failure
 * Указатель на перестроенные данные тега (выделены в куче) или NULL при ошибке
 * 
 * ALGORITHM / АЛГОРИТМ:
 * 1. Read all ID3 fields using decrypt engine
 * 2. Create dynamic buffer for new tag
 * 3. Copy ID3 header
 * 4. For each non-empty field:
 *    a. Convert Unicode to ANSI
 *    b. Create ID3v2.3/2.4 frame
 *    c. Append to buffer
 * 5. Update tag size in header
 * 6. Return buffer
 * 
 * 1. Прочитать все ID3 поля используя движок расшифровки
 * 2. Создать динамический буфер для нового тега
 * 3. Скопировать ID3 заголовок
 * 4. Для каждого непустого поля:
 *    a. Преобразовать Unicode в ANSI
 *    b. Создать кадр ID3v2.3/2.4
 *    c. Добавить к буферу
 * 5. Обновить размер тега в заголовке
 * 6. Вернуть буфер
 * 
 * FRAME IDS / ID КАДРОВ:
 * TIT2 - Title / Название
 * TPE1 - Artist / Исполнитель
 * TALB - Album / Альбом
 * TYER - Year / Год
 * TCON - Genre / Жанр
 * TRCK - Track / Трек
 * TCOM - Composer / Композитор
 * TOPE - Original Artist / Оригинальный исполнитель
 * TCOP - Copyright / Авторское право
 * TENC - Encoder / Кодировщик
 ******************************************************************************/
static BYTE* BuildID3_Virtual(HANDLE hFile, DWORD* pNewSz, DWORD* pOldSz) {
    // Wide character buffers for all tag fields / Wide character буферы для всех полей тегов
    WCHAR wA[WBUF_MAX]={0},     // Artist / Исполнитель
          wT[WBUF_MAX]={0},     // Title / Название
          wAlb[WBUF_MAX]={0},   // Album / Альбом
          wY[WBUF_MAX]={0},     // Year / Год
          wG[WBUF_MAX]={0},     // Genre / Жанр
          wTrk[WBUF_MAX]={0},   // Track / Трек
          wC[WBUF_MAX]={0},     // Comment / Комментарий
          wM[WBUF_MAX]={0},     // Composer / Композитор
          wO[WBUF_MAX]={0},     // Original Artist / Оригинальный исполнитель
          wP[WBUF_MAX]={0},     // Publisher / Издатель
          wU[WBUF_MAX]={0},     // URL / URL
          wE[WBUF_MAX]={0};     // Encoder / Кодировщик
    
    DWORD oldSz;
    
    // Read all fields from existing tag / Прочитать все поля из существующего тега
    if(!ID3_ReadAllFieldsW(hFile, wA, WBUF_MAX, wT, WBUF_MAX, wAlb, WBUF_MAX,
                           wY, WBUF_MAX, wG, WBUF_MAX, wTrk, WBUF_MAX,
                           wC, WBUF_MAX, wM, WBUF_MAX, wO, WBUF_MAX,
                           wP, WBUF_MAX, wU, WBUF_MAX, wE, WBUF_MAX, &oldSz)) 
        return NULL;
    
    if(oldSz == 0) return NULL;  // No tag found / Тег не найден
    if(pOldSz) *pOldSz = oldSz;

    // Create dynamic buffer for new tag / Создать динамический буфер для нового тега
    UniDynBuf db;
    DB_Init(&db, 65536);  // Start with 64KB / Начать с 64КБ
    
    // Read and copy original ID3 header / Прочитать и скопировать оригинальный ID3 заголовок
    BYTE hdr[10] = {0};
    DWORD rd;
    SetFilePointer(hFile, 0, 0, FILE_BEGIN);
    ReadFile(hFile, hdr, 10, &rd, 0);
    DB_Append(&db, hdr, 10);

    // Temporary buffer for ANSI conversion / Временный буфер для ANSI преобразования
    char tmp[WBUF_MAX];
    
    // Field arrays for iteration / Массивы полей для итерации
    const WCHAR* w[] = {wT, wA, wAlb, wY, wG, wTrk, wC, wO, wP, wE};
    const char* i4[] = {"TIT2", "TPE1", "TALB", "TYER", "TCON", "TRCK", "TCOM", "TOPE", "TCOP", "TENC"};
    
    int ver = hdr[3];  // ID3 version / Версия ID3
    BOOL isV22 = (ver == 2);  // ID3v2.2 uses 3-byte frame IDs / ID3v2.2 использует 3-байтовые ID кадров

    // Build frames for each non-empty field / Построить кадры для каждого непустого поля
    for(int i = 0; i < 10; i++) {
        if(w[i][0]) {  // Field has content / Поле имеет содержимое
            // Convert Unicode to ANSI / Преобразовать Unicode в ANSI
            int l = DECRYPT_WideToACP(w[i], tmp, WBUF_MAX);
            if(l > 0) {
                if(isV22) { 
                    // ID3v2.2 omitted for simplicity / ID3v2.2 пропущено для простоты
                } else { 
                    // ID3v2.3/2.4 frame format / Формат кадра ID3v2.3/2.4
                    BYTE h[10] = {0};
                    
                    // Frame ID (4 bytes) / ID кадра (4 байта)
                    memcpy(h, i4[i], 4);
                    
                    // Frame size (4 bytes, big-endian, non-syncsafe) / Размер кадра (4 байта, big-endian, не-syncsafe)
                    DECRYPT_BE32Write(h + 4, l + 1);  // +1 for encoding byte / +1 для байта кодировки
                    
                    // Append frame header / Добавить заголовок кадра
                    DB_Append(&db, h, 10);
                    
                    // Encoding byte (0 = ISO-8859-1) / Байт кодировки (0 = ISO-8859-1)
                    char z = 0;
                    DB_Append(&db, &z, 1);
                    
                    // Append text data / Добавить текстовые данные
                    DB_Append(&db, tmp, l);
                }
            }
        }
    }
    
    // Calculate frame data size / Вычислить размер данных кадров
    DWORD fSz = db.size - 10;  // Total size minus header / Общий размер минус заголовок
    BYTE* out = db.data;
    
    // Update header with correct tag signature / Обновить заголовок правильной сигнатурой тега
    memcpy(out, "ID3", 3);
    out[3] = ver;  // Preserve version / Сохранить версию
    
    // Convert size to syncsafe integer (7 bits per byte) / Преобразовать размер в syncsafe целое (7 бит на байт)
    DWORD ss = DECRYPT_SizeToSyncsafe(fSz);
    out[6] = (BYTE)((ss >> 24) & 0x7F);
    out[7] = (BYTE)((ss >> 16) & 0x7F);
    out[8] = (BYTE)((ss >> 8) & 0x7F);
    out[9] = (BYTE)(ss & 0x7F);
    
    if(pNewSz) *pNewSz = db.size;
    return db.data;  // Caller must free with HeapFree / Вызывающий должен освободить с HeapFree
}

/*******************************************************************************
 * HOOKED FUNCTIONS
 * ПЕРЕХВАЧЕННЫЕ ФУНКЦИИ
 * 
 * These functions intercept file operations in in_mp3.dll.
 * Эти функции перехватывают файловые операции в in_mp3.dll.
 ******************************************************************************/

/*******************************************************************************
 * FUNCTION POINTER TYPES
 * ТИПЫ УКАЗАТЕЛЕЙ ФУНКЦИЙ
 ******************************************************************************/
typedef HANDLE (WINAPI *PFN_CreateFileA)(LPCSTR, DWORD, DWORD, LPSECURITY_ATTRIBUTES, DWORD, DWORD, HANDLE);
typedef BOOL   (WINAPI *PFN_ReadFile)(HANDLE, LPVOID, DWORD, LPDWORD, LPOVERLAPPED);
typedef DWORD  (WINAPI *PFN_SetFilePointer)(HANDLE, LONG, PLONG, DWORD);
typedef DWORD  (WINAPI *PFN_GetFileSize)(HANDLE, LPDWORD);
typedef BOOL   (WINAPI *PFN_CloseHandle)(HANDLE);

/*******************************************************************************
 * ORIGINAL AND REAL FUNCTION POINTERS
 * ОРИГИНАЛЬНЫЕ И РЕАЛЬНЫЕ УКАЗАТЕЛИ ФУНКЦИЙ
 * 
 * Orig_* - Original KERNEL32 functions (for non-MP3 files)
 * Real_* - Function to call after our hook (may be original or another hook)
 * 
 * Orig_* - Оригинальные функции KERNEL32 (для не-MP3 файлов)
 * Real_* - Функция для вызова после нашего хука (может быть оригинальной или другим хуком)
 ******************************************************************************/
static PFN_CreateFileA Orig_CreateFileA = NULL;
static PFN_CreateFileA Real_CreateFileA = NULL;

static PFN_ReadFile Orig_ReadFile = NULL;
static PFN_ReadFile Real_ReadFile = NULL;

static PFN_SetFilePointer Orig_SetFilePointer = NULL;
static PFN_SetFilePointer Real_SetFilePointer = NULL;

static PFN_GetFileSize Orig_GetFileSize = NULL;
static PFN_GetFileSize Real_GetFileSize = NULL;

static PFN_CloseHandle Orig_CloseHandle = NULL;
static PFN_CloseHandle Real_CloseHandle = NULL;

/*******************************************************************************
 * Hook_CreateFileA
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Intercepts file opens to detect MP3 files and create virtual context.
 * Перехватывает открытия файлов для обнаружения MP3 файлов и создания виртуального контекста.
 * 
 * ALGORITHM / АЛГОРИТМ:
 * 1. Call original CreateFileA
 * 2. If file opened successfully and for reading (not writing):
 *    a. Check if extension is .mp3
 *    b. Try to build virtual tag
 *    c. If successful, create context
 * 3. Return handle
 * 
 * 1. Вызвать оригинальную CreateFileA
 * 2. Если файл открыт успешно и для чтения (не записи):
 *    a. Проверить, расширение ли .mp3
 *    b. Попытаться построить виртуальный тег
 *    c. Если успешно, создать контекст
 * 3. Вернуть дескриптор
 * 
 * NOTES / ПРИМЕЧАНИЯ:
 * Only virtualizes files opened without GENERIC_WRITE.
 * Writing is handled by separate mod_save_id3v2 module.
 * 
 * Виртуализирует только файлы открытые без GENERIC_WRITE.
 * Запись обрабатывается отдельным модулем mod_save_id3v2.
 ******************************************************************************/
static HANDLE WINAPI Hook_CreateFileA(LPCSTR fn, DWORD acc, DWORD shr, 
                                     LPSECURITY_ATTRIBUTES sa, DWORD disp, 
                                     DWORD flg, HANDLE tpl) {
    PFN_CreateFileA p = Real_CreateFileA;
    
    // If hooks not ready, pass through / Если хуки не готовы, пропустить
    if(!p || g_hooksReady != 1) 
        return Orig_CreateFileA(fn, acc, shr, sa, disp, flg, tpl);
    
    // Call original function / Вызвать оригинальную функцию
    HANDLE h = p(fn, acc, shr, sa, disp, flg, tpl);
    
    // Check if file opened successfully and for reading / Проверить, открыт ли файл успешно и для чтения
    if(h != INVALID_HANDLE_VALUE && !(acc & GENERIC_WRITE)) {
        if (fn) {  // Check for NULL pointer / Проверить на NULL указатель
            // Check extension / Проверить расширение
            const char* ext = strrchr(fn, '.');
            if(ext && lstrcmpiA(ext, ".mp3") == 0) {
                // Try to build virtual tag / Попытаться построить виртуальный тег
                BYTE* tag = NULL;
                DWORD nSz, oSz;
                tag = BuildID3_Virtual(h, &nSz, &oSz);
                
                if(tag) {
                    // Create context / Создать контекст
                    Ctx_Lock();
                    OverlayCtx* c = AllocCtx();
                    if(c) {
                        c->hReal = h;
                        
                        // Duplicate handle for independent seeking / Дублировать дескриптор для независимого поиска
                        DuplicateHandle(GetCurrentProcess(), h, 
                                      GetCurrentProcess(), &c->hKeep,
                                      0, FALSE, DUPLICATE_SAME_ACCESS);
                        
                        // Store tag data / Сохранить данные тега
                        c->newTag = tag;
                        c->newTagSz = nSz;
                        c->oldTagSz = oSz;
                        
                        // Calculate virtual file size / Вычислить виртуальный размер файла
                        DWORD hi;
                        DWORD fs = GetFileSize(h, &hi);
                        c->virtSz = nSz + (fs - oSz);  // newTag + audioData
                    } else {
                        // Context allocation failed / Выделение контекста не удалось
                        HeapFree(GetProcessHeap(), 0, tag);
                    }
                    Ctx_Unlock();
                }
            }
        }
    }
    return h;
}

/*******************************************************************************
 * Hook_ReadFile
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Intercepts reads to return virtual tag data or redirected audio data.
 * Перехватывает чтения для возврата данных виртуального тега или перенаправленных аудио данных.
 * 
 * ALGORITHM / АЛГОРИТМ:
 * 1. Find context for handle
 * 2. If context exists and has virtual tag:
 *    a. If reading from tag area (pos < newTagSz):
 *       - Copy from virtual tag buffer
 *       - Update logical position
 *    b. If reading from audio area (pos >= newTagSz):
 *       - Calculate physical position (adjust for tag size difference)
 *       - Seek in duplicate handle
 *       - Read from duplicate handle
 *       - Update logical position
 * 3. If no context, pass through to original function
 * 
 * 1. Найти контекст для дескриптора
 * 2. Если контекст существует и имеет виртуальный тег:
 *    a. Если чтение из области тега (pos < newTagSz):
 *       - Скопировать из буфера виртуального тега
 *       - Обновить логическую позицию
 *    b. Если чтение из области аудио (pos >= newTagSz):
 *       - Вычислить физическую позицию (скорректировать для разницы размеров тега)
 *       - Искать в дубликате дескриптора
 *       - Читать из дубликата дескриптора
 *       - Обновить логическую позицию
 * 3. Если нет контекста, пропустить к оригинальной функции
 * 
 * THREAD SAFETY / ПОТОКОБЕЗОПАСНОСТЬ:
 * Uses reference counting to prevent context from being freed during operation.
 * Использует подсчёт ссылок для предотвращения освобождения контекста во время операции.
 ******************************************************************************/
static BOOL WINAPI Hook_ReadFile(HANDLE h, LPVOID buf, DWORD toRead, LPDWORD pRead, LPOVERLAPPED ovl) {
    PFN_ReadFile p = Real_ReadFile;
    
    // If hooks not ready, pass through / Если хуки не готовы, пропустить
    if(!p || g_hooksReady != 1) 
        return Orig_ReadFile(h, buf, toRead, pRead, ovl);
    
    // Need SetFilePointer for seeking / Нужен SetFilePointer для поиска
    if(!Orig_SetFilePointer) 
        return p(h, buf, toRead, pRead, ovl);
    
    Ctx_Lock();
    OverlayCtx* c = FindCtx(h);
    
    if(c && c->newTag) {
        // Increment reference count / Увеличить счётчик ссылок
        InterlockedIncrement(&c->refs);
        
        /*********************************************************************
         * CASE 1: Reading from virtual tag area
         * СЛУЧАЙ 1: Чтение из области виртуального тега
         *********************************************************************/
        if(c->logicalPos < c->newTagSz) {
            // Calculate how much tag data is available / Вычислить сколько данных тега доступно
            DWORD avail = c->newTagSz - c->logicalPos;
            DWORD chunk = (toRead < avail) ? toRead : avail;
            
            // Copy from virtual tag buffer / Скопировать из буфера виртуального тега
            memcpy(buf, c->newTag + c->logicalPos, chunk);
            c->logicalPos += chunk;
            
            // Check if context should be freed / Проверить, должен ли контекст быть освобождён
            if(InterlockedDecrement(&c->refs) == 0 && c->closing) 
                FreeCtx(c);
            
            Ctx_Unlock();
            if(pRead) *pRead = chunk;
            return TRUE;
        }
        
        /*********************************************************************
         * CASE 2: Reading from audio data area
         * СЛУЧАЙ 2: Чтение из области аудио данных
         *********************************************************************/
        
        // Calculate physical file position / Вычислить физическую позицию файла
        // logical pos = newTagSz + audioOffset
        // physical pos = oldTagSz + audioOffset
        DWORD phys = c->oldTagSz + (c->logicalPos - c->newTagSz);
        
        // Use duplicate handle for seeking / Использовать дубликат дескриптора для поиска
        HANDLE hPh = (c->hKeep) ? c->hKeep : c->hReal;
        
        Ctx_Unlock();
        
        // Seek to physical position / Искать физическую позицию
        Orig_SetFilePointer(hPh, phys, NULL, FILE_BEGIN);
        
        // Read from physical file / Читать из физического файла
        DWORD rd;
        BOOL r = p(hPh, buf, toRead, &rd, ovl);
        
        // Update logical position / Обновить логическую позицию
        Ctx_Lock();
        if(c->inUse && c->newTag) 
            c->logicalPos += rd;
        
        // Check if context should be freed / Проверить, должен ли контекст быть освобождён
        if(InterlockedDecrement(&c->refs) == 0 && c->closing) 
            FreeCtx(c);
        
        Ctx_Unlock();
        if(pRead) *pRead = rd;
        return r;
    }
    
    Ctx_Unlock();
    
    // No context - pass through / Нет контекста - пропустить
    return p(h, buf, toRead, pRead, ovl);
}

/*******************************************************************************
 * Hook_SetFilePointer
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Intercepts seeks to maintain virtual file position.
 * Перехватывает поиски для поддержания позиции виртуального файла.
 * 
 * ALGORITHM / АЛГОРИТМ:
 * 1. Find context for handle
 * 2. If context exists:
 *    a. Calculate base position (BEGIN=0, CURRENT=logicalPos, END=virtSz)
 *    b. Add movement offset
 *    c. Clamp to valid range [0, virtSz]
 *    d. Update logical position
 * 3. If no context, pass through to original function
 * 
 * 1. Найти контекст для дескриптора
 * 2. Если контекст существует:
 *    a. Вычислить базовую позицию (BEGIN=0, CURRENT=logicalPos, END=virtSz)
 *    b. Добавить смещение движения
 *    c. Ограничить допустимым диапазоном [0, virtSz]
 *    d. Обновить логическую позицию
 * 3. Если нет контекста, пропустить к оригинальной функции
 ******************************************************************************/
static DWORD WINAPI Hook_SetFilePointer(HANDLE h, LONG mv, PLONG hi, DWORD mth) {
    PFN_SetFilePointer p = Real_SetFilePointer;
    
    // If hooks not ready, pass through / Если хуки не готовы, пропустить
    if(!p || g_hooksReady != 1) 
        return Orig_SetFilePointer(h, mv, hi, mth);
    
    Ctx_Lock();
    OverlayCtx* c = FindCtx(h);
    
    if(c && c->newTag) {
        // Calculate base position / Вычислить базовую позицию
        LONGLONG b = 0;
        switch(mth) {
            case FILE_BEGIN:   b = 0;             break;
            case FILE_CURRENT: b = c->logicalPos; break;
            case FILE_END:     b = c->virtSz;     break;
        }
        
        // Calculate new position / Вычислить новую позицию
        LONGLONG s = b + mv;
        
        // Clamp to valid range / Ограничить допустимым диапазоном
        if(s < 0) s = 0;
        if(s > (LONGLONG)c->virtSz) s = c->virtSz;
        
        // Update logical position / Обновить логическую позицию
        c->logicalPos = (DWORD)s;
        
        Ctx_Unlock();
        
        // Clear high DWORD (we only support 32-bit files) / Очистить старший DWORD (поддерживаем только 32-битные файлы)
        if(hi) *hi = 0;
        
        return (DWORD)s;
    }
    
    Ctx_Unlock();
    
    // No context - pass through / Нет контекста - пропустить
    return p(h, mv, hi, mth);
}

/*******************************************************************************
 * Hook_GetFileSize
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Intercepts size queries to return virtual file size.
 * Перехватывает запросы размера для возврата виртуального размера файла.
 * 
 * ALGORITHM / АЛГОРИТМ:
 * 1. Find context for handle
 * 2. If context exists, return virtual size
 * 3. If no context, pass through to original function
 * 
 * 1. Найти контекст для дескриптора
 * 2. Если контекст существует, вернуть виртуальный размер
 * 3. Если нет контекста, пропустить к оригинальной функции
 ******************************************************************************/
static DWORD WINAPI Hook_GetFileSize(HANDLE h, LPDWORD hi) {
    PFN_GetFileSize p = Real_GetFileSize;
    
    // If hooks not ready, pass through / Если хуки не готовы, пропустить
    if(!p || g_hooksReady != 1) 
        return Orig_GetFileSize(h, hi);
    
    Ctx_Lock();
    OverlayCtx* c = FindCtx(h);
    
    if(c && c->newTag) {
        Ctx_Unlock();
        
        // Return virtual size / Вернуть виртуальный размер
        if(hi) *hi = 0;
        return c->virtSz;
    }
    
    Ctx_Unlock();
    
    // No context - pass through / Нет контекста - пропустить
    return p(h, hi);
}

/*******************************************************************************
 * Hook_CloseHandle
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Intercepts handle closes to free context when all operations complete.
 * Перехватывает закрытия дескрипторов для освобождения контекста когда все операции завершены.
 * 
 * ALGORITHM / АЛГОРИТМ:
 * 1. Find context for handle
 * 2. If context exists:
 *    a. Mark as closing
 *    b. If no operations in progress (refs=0), free immediately
 *    c. Otherwise, free will happen when last operation completes
 * 3. Call original CloseHandle
 * 
 * 1. Найти контекст для дескриптора
 * 2. Если контекст существует:
 *    a. Отметить как closing
 *    b. Если нет операций в процессе (refs=0), освободить немедленно
 *    c. Иначе освобождение произойдёт когда последняя операция завершится
 * 3. Вызвать оригинальную CloseHandle
 * 
 * THREAD SAFETY / ПОТОКОБЕЗОПАСНОСТЬ:
 * Uses reference counting to ensure context isn't freed while operations in progress.
 * Использует подсчёт ссылок для гарантии что контекст не освобождается пока операции в процессе.
 ******************************************************************************/
static BOOL WINAPI Hook_CloseHandle(HANDLE h) {
    PFN_CloseHandle p = Real_CloseHandle;
    
    Ctx_Lock();
    OverlayCtx* c = FindCtx(h);
    
    if(c) {
        // Mark as closing / Отметить как closing
        c->closing = TRUE;
        
        // If no operations in progress, free now / Если нет операций в процессе, освободить сейчас
        if(InterlockedCompareExchange(&c->refs, 0, 0) == 0) 
            FreeCtx(c);
        // Otherwise context will be freed by last operation
        // Иначе контекст будет освобождён последней операцией
    }
    
    Ctx_Unlock();
    
    // Call original close / Вызвать оригинальное закрытие
    return p(h);
}

/*******************************************************************************
 * INITIALIZATION
 * ИНИЦИАЛИЗАЦИЯ
 ******************************************************************************/

/*******************************************************************************
 * HookThread
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Background thread that installs IAT hooks after in_mp3.dll loads.
 * Фоновый поток который устанавливает IAT хуки после загрузки in_mp3.dll.
 * 
 * ALGORITHM / АЛГОРИТМ:
 * 1. Wait for in_mp3.dll to load (poll with timeout)
 * 2. Get original KERNEL32 function addresses
 * 3. Install IAT patches in in_mp3.dll
 * 4. Mark hooks as ready
 * 5. Initialize MP3 save fix module (AFTER hooks ready to avoid VirtualProtect conflicts)
 * 
 * 1. Ждать загрузки in_mp3.dll (опрос с тайм-аутом)
 * 2. Получить оригинальные адреса функций KERNEL32
 * 3. Установить IAT патчи в in_mp3.dll
 * 4. Отметить хуки как готовые
 * 5. Инициализировать модуль исправления сохранения MP3 (ПОСЛЕ готовности хуков для избежания конфликтов VirtualProtect)
 * 
 * WHY SEPARATE THREAD / ПОЧЕМУ ОТДЕЛЬНЫЙ ПОТОК:
 * in_mp3.dll may not be loaded when plugin initializes.
 * Background thread waits until it's available.
 * 
 * in_mp3.dll может быть не загружена когда плагин инициализируется.
 * Фоновый поток ждёт пока она не станет доступной.
 * 
 * TIMING / ТАЙМИНГ:
 * - Polls for in_mp3.dll for up to 10 seconds (50 * 200ms)
 * - Additional 500ms delay after finding module
 * - Ensures module is fully initialized before patching
 * 
 * - Опрашивает in_mp3.dll до 10 секунд (50 * 200мс)
 * - Дополнительная задержка 500мс после нахождения модуля
 * - Гарантирует что модуль полностью инициализирован перед патчингом
 ******************************************************************************/
static unsigned __stdcall HookThread(void*) {
    // Wait for in_mp3.dll to load / Ждать загрузки in_mp3.dll
    HMODULE h = NULL;
    for(int i = 0; i < 50; i++) {
        h = GetModuleHandleA("in_mp3.dll");
        if(h) break;
        Sleep(200);
    }
    if(!h) return 0;  // Timeout - in_mp3.dll not found / Тайм-аут - in_mp3.dll не найдена
    
    Sleep(500);  // Additional delay for module initialization / Дополнительная задержка для инициализации модуля
    
    // Get KERNEL32 handle / Получить дескриптор KERNEL32
    HMODULE k = GetModuleHandleA("KERNEL32.dll");
    
    // Get original function addresses / Получить оригинальные адреса функций
    Orig_CreateFileA = (PFN_CreateFileA)GetProcAddress(k, "CreateFileA");
    Orig_ReadFile = (PFN_ReadFile)GetProcAddress(k, "ReadFile");
    Orig_SetFilePointer = (PFN_SetFilePointer)GetProcAddress(k, "SetFilePointer");
    Orig_GetFileSize = (PFN_GetFileSize)GetProcAddress(k, "GetFileSize");
    Orig_CloseHandle = (PFN_CloseHandle)GetProcAddress(k, "CloseHandle");
    
    // Initialize Real_* pointers (will be updated by IAT_Patch) / Инициализировать Real_* указатели (будут обновлены IAT_Patch)
    Real_CreateFileA = Orig_CreateFileA;
    Real_ReadFile = Orig_ReadFile;
    Real_SetFilePointer = Orig_SetFilePointer;
    Real_GetFileSize = Orig_GetFileSize;
    Real_CloseHandle = Orig_CloseHandle;

    // Install IAT patches / Установить IAT патчи
    IAT_Patch(h, "KERNEL32.dll", "CreateFileA", (void*)Hook_CreateFileA, (void**)&Real_CreateFileA);
    IAT_Patch(h, "KERNEL32.dll", "ReadFile", (void*)Hook_ReadFile, (void**)&Real_ReadFile);
    IAT_Patch(h, "KERNEL32.dll", "SetFilePointer", (void*)Hook_SetFilePointer, (void**)&Real_SetFilePointer);
    if(Orig_GetFileSize) 
        IAT_Patch(h, "KERNEL32.dll", "GetFileSize", (void*)Hook_GetFileSize, (void**)&Real_GetFileSize);
    IAT_Patch(h, "KERNEL32.dll", "CloseHandle", (void*)Hook_CloseHandle, (void**)&Real_CloseHandle);

    // Mark hooks as ready / Отметить хуки как готовые
    InterlockedExchange(&g_hooksReady, 1);
    
    /***************************************************************************
     * CRITICAL: Initialize save fix AFTER hooks are ready
     * КРИТИЧНО: Инициализировать исправление сохранения ПОСЛЕ готовности хуков
     * 
     * This prevents thread race conditions in VirtualProtect.
     * Both modules patch in_mp3.dll's IAT, so we serialize the operations.
     * 
     * Это предотвращает состояния гонки потоков в VirtualProtect.
     * Оба модуля патчат IAT in_mp3.dll, поэтому мы сериализуем операции.
     ***************************************************************************/
    MP3_SaveFix_Init();
    
    return 0;
}

/*******************************************************************************
 * PUBLIC API
 * ПУБЛИЧНЫЙ API
 ******************************************************************************/

/*******************************************************************************
 * MP3_TagsFix_Init
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Initializes Unicode tags fix by starting hook installation thread.
 * Инициализирует исправление Unicode тегов запуском потока установки хуков.
 * 
 * THREAD SAFETY / ПОТОКОБЕЗОПАСНОСТЬ:
 * Uses static flag to ensure only one initialization.
 * Использует статический флаг для обеспечения только одной инициализации.
 ******************************************************************************/
void MP3_TagsFix_Init(void) {
    static BOOL i = FALSE;
    if(!i) {
        i = TRUE;
        unsigned t;
        // Start hook installation thread / Запустить поток установки хуков
        _beginthreadex(NULL, 0, HookThread, NULL, 0, &t);
    }
}

/*******************************************************************************
 * MP3_TagsFix_Quit
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Cleans up Unicode tags fix module.
 * Очищает модуль исправления Unicode тегов.
 * 
 * CLEANUP / ОЧИСТКА:
 * 1. Mark hooks as inactive
 * 2. Cleanup save fix module
 * 3. Free all context resources
 * 4. DO NOT delete critical section (prevents crashes during shutdown)
 * 
 * 1. Отметить хуки как неактивные
 * 2. Очистить модуль исправления сохранения
 * 3. Освободить все ресурсы контекстов
 * 4. НЕ удалять критическую секцию (предотвращает краши во время завершения)
 * 
 * WHY NOT DELETE CRITICAL SECTION / ПОЧЕМУ НЕ УДАЛЯТЬ КРИТИЧЕСКУЮ СЕКЦИЮ:
 * Winamp may still try to read files during shutdown.
 * Deleting CS could cause crash if another thread tries to lock.
 * Небольшая утечка ресурса лучше чем краш.
 * 
 * Winamp может всё ещё пытаться читать файлы во время завершения.
 * Удаление CS может вызвать краш если другой поток попытается заблокировать.
 * Small resource leak is better than crash.
 ******************************************************************************/
void MP3_TagsFix_Quit(void) {
    // Disable hooks / Отключить хуки
    InterlockedExchange(&g_hooksReady, 0);
    
    // Cleanup save fix module / Очистить модуль исправления сохранения
    MP3_SaveFix_Quit();
    
    // Free all contexts / Освободить все контексты
    Ctx_Lock();
    for(int i = 0; i < MAX_OPEN_MP3S; i++) {
        if(g_ctxPool[i].inUse) 
            FreeCtx(&g_ctxPool[i]);
    }
    Ctx_Unlock();
    
    /***************************************************************************
     * SAFETY: DO NOT delete critical section
     * БЕЗОПАСНОСТЬ: НЕ удалять критическую секцию
     * 
     * This prevents crashes if Winamp tries to read during shutdown.
     * Это предотвращает краши если Winamp попытается прочитать во время завершения.
     ***************************************************************************/
    // if(g_csInited == 2) DeleteCriticalSection(&g_cs);
    // g_csInited = 0;
}

/*******************************************************************************
 * ALTERNATE ENTRY POINTS
 * АЛЬТЕРНАТИВНЫЕ ТОЧКИ ВХОДА
 * 
 * Compatibility functions for different initialization contexts.
 * Функции совместимости для различных контекстов инициализации.
 ******************************************************************************/

void MP3_TagsFix_Init_C(HWND) { MP3_TagsFix_Init(); }
void EFIHook_Init(void) { MP3_TagsFix_Init(); }
void EFIHook_Quit(void) { MP3_TagsFix_Quit(); }