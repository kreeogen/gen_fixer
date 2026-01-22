/*******************************************************************************
 * mod_save_id3v2.cpp
 * 
 * MP3 ID3v2 TAG SAVE FIX MODULE
 * МОДУЛЬ ИСПРАВЛЕНИЯ СОХРАНЕНИЯ ID3v2 ТЕГОВ MP3
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Fixes ID3v2 tag corruption issues when saving MP3 tags in Winamp.
 * Intercepts file operations during tag save and rebuilds tags with proper
 * encoding, preserving URL frames and unhandled metadata.
 * 
 * Исправляет проблемы повреждения ID3v2 тегов при сохранении MP3 тегов в Winamp.
 * Перехватывает файловые операции во время сохранения тегов и перестраивает теги
 * с правильной кодировкой, сохраняя URL-фреймы и необработанные метаданные.
 * 
 * HOW IT WORKS / КАК ЭТО РАБОТАЕТ:
 * 1. Patches in_mp3.dll's Import Address Table to hook MoveFileA/MoveFileExA
 * 2. Detects when Winamp saves MP3 tags by monitoring file moves
 * 3. Creates backup map to track temporary files during save process
 * 4. Rebuilds ID3v2 tags from scratch with proper encoding
 * 5. Preserves URL frames and unhandled metadata from original file
 * 6. Supports both ID3v2.2 and ID3v2.3/2.4 tag formats
 * 
 * 1. Патчит таблицу импорта in_mp3.dll для перехвата MoveFileA/MoveFileExA
 * 2. Определяет, когда Winamp сохраняет MP3 теги, отслеживая перемещения файлов
 * 3. Создаёт карту резервных копий для отслеживания временных файлов
 * 4. Перестраивает ID3v2 теги с нуля с правильной кодировкой
 * 5. Сохраняет URL-фреймы и необработанные метаданные из оригинального файла
 * 6. Поддерживает форматы тегов ID3v2.2 и ID3v2.3/2.4
 * 
 * FEATURES / ВОЗМОЖНОСТИ:
 * - Automatic tag version detection (ID3v2.2/2.3/2.4)
 * - Preserves URL frames (WXXX, WCOM, etc.)
 * - Keeps unhandled custom frames from original tags
 * - Safe backup mechanism to prevent data loss
 * - Thread-safe backup map with automatic cleanup
 * - Atomic file operations for crash safety
 * 
 * - Автоматическое определение версии тегов (ID3v2.2/2.3/2.4)
 * - Сохранение URL-фреймов (WXXX, WCOM и т.д.)
 * - Сохранение необработанных пользовательских фреймов из оригинальных тегов
 * - Безопасный механизм резервного копирования для предотвращения потери данных
 * - Потокобезопасная карта резервных копий с автоматической очисткой
 * - Атомарные файловые операции для защиты от крашей
 * 
 * COMPATIBILITY / СОВМЕСТИМОСТЬ:
 * - Winamp 2.95 and compatible versions
 * - in_mp3.dll input plugin
 * - ID3v2.2, ID3v2.3, ID3v2.4 tag formats
 * - Windows 98 through Windows 11
 * 
 * - Winamp 2.95 и совместимые версии
 * - Входной плагин in_mp3.dll
 * - Форматы тегов ID3v2.2, ID3v2.3, ID3v2.4
 * - Windows 98 до Windows 11
 * 
 ******************************************************************************/

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <process.h>
#include <stdio.h>
#include <string.h>

#include "..\\Decrypt Engine\\unicode_decrypt_engine.h"
#include "mod_save_id3v2.h"

/*******************************************************************************
 * CONFIGURATION CONSTANTS
 * КОНСТАНТЫ КОНФИГУРАЦИИ
 ******************************************************************************/

// Maximum tag size to prevent processing malformed/malicious files
// Максимальный размер тега для предотвращения обработки повреждённых/вредоносных файлов
#define TAG_SANITY_LIMIT (50*1024*1024)  // 50 MB

// Buffer sizes for Unicode and ANSI strings
// Размеры буферов для Unicode и ANSI строк
#define WBUF_MAX 2048  // Wide character buffer / Буфер широких символов
#define ABUF_MAX 2048  // ANSI character buffer / Буфер ANSI символов

/*******************************************************************************
 * GLOBAL STATE
 * ГЛОБАЛЬНОЕ СОСТОЯНИЕ
 ******************************************************************************/

// Hook readiness flag (0=not ready, 1=ready)
// Флаг готовности хуков (0=не готов, 1=готов)
static volatile LONG g_hooksReady = 0;

// Thread handle for SaveThread / Хэндл потока для SaveThread
static HANDLE g_hThread = NULL;
static HANDLE g_hStopEvent = NULL;

/*******************************************************************************
 * DYNAMIC BUFFER STRUCTURE
 * СТРУКТУРА ДИНАМИЧЕСКОГО БУФЕРА
 * 
 * Used for building ID3v2 tags incrementally.
 * Automatically grows as data is appended.
 * 
 * Используется для инкрементального построения ID3v2 тегов.
 * Автоматически растёт при добавлении данных.
 * 
 * FIELDS / ПОЛЯ:
 * data     - Pointer to buffer memory / Указатель на память буфера
 * size     - Current data size in bytes / Текущий размер данных в байтах
 * capacity - Total allocated capacity / Общая выделенная ёмкость
 ******************************************************************************/
typedef struct { 
    BYTE* data;      // Buffer data / Данные буфера
    DWORD size;      // Current size / Текущий размер
    DWORD capacity;  // Allocated capacity / Выделенная ёмкость
} SaveDynBuf;

/*******************************************************************************
 * DB_Init
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Initializes a dynamic buffer with specified initial capacity.
 * Инициализирует динамический буфер с указанной начальной ёмкостью.
 * 
 * PARAMETERS / ПАРАМЕТРЫ:
 * d   - Pointer to SaveDynBuf structure / Указатель на структуру SaveDynBuf
 * cap - Initial capacity in bytes (0 = use default 256)
 *       Начальная ёмкость в байтах (0 = использовать по умолчанию 256)
 * 
 * RETURNS / ВОЗВРАЩАЕТ:
 * TRUE if initialization successful, FALSE on allocation failure
 * TRUE если инициализация успешна, FALSE при ошибке выделения памяти
 * 
 * NOTES / ПРИМЕЧАНИЯ:
 * Buffer must be freed with DB_Free when no longer needed.
 * Буфер должен быть освобождён через DB_Free когда больше не нужен.
 ******************************************************************************/
static BOOL DB_Init(SaveDynBuf* d, DWORD cap) {
    if (!d) return FALSE;
    
    // Initialize all fields to zero / Инициализировать все поля нулями
    d->data = NULL; 
    d->size = 0; 
    d->capacity = 0;
    
    // Use default capacity if not specified / Использовать ёмкость по умолчанию если не указана
    if (cap == 0) cap = 256;
    
    // Allocate initial buffer / Выделить начальный буфер
    d->data = (BYTE*)HeapAlloc(GetProcessHeap(), 0, cap);
    if (!d->data) return FALSE;
    
    d->capacity = cap;
    return TRUE;
}

/*******************************************************************************
 * DB_Free
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Frees memory allocated for dynamic buffer.
 * Освобождает память, выделенную для динамического буфера.
 * 
 * PARAMETERS / ПАРАМЕТРЫ:
 * d - Pointer to SaveDynBuf structure / Указатель на структуру SaveDynBuf
 * 
 * NOTES / ПРИМЕЧАНИЯ:
 * Safe to call multiple times - sets data pointer to NULL after freeing.
 * Безопасно вызывать много раз - устанавливает указатель данных в NULL после освобождения.
 ******************************************************************************/
static void DB_Free(SaveDynBuf* d) { 
    if (d->data) HeapFree(GetProcessHeap(), 0, d->data);
    d->data = NULL;
}

/*******************************************************************************
 * DB_Append
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Appends data to dynamic buffer, automatically growing if needed.
 * Добавляет данные в динамический буфер, автоматически увеличивая при необходимости.
 * 
 * PARAMETERS / ПАРАМЕТРЫ:
 * d   - Pointer to SaveDynBuf structure / Указатель на структуру SaveDynBuf
 * src - Source data to append / Исходные данные для добавления
 * len - Length of data in bytes / Длина данных в байтах
 * 
 * RETURNS / ВОЗВРАЩАЕТ:
 * TRUE if append successful, FALSE on allocation failure
 * TRUE если добавление успешно, FALSE при ошибке выделения памяти
 * 
 * ALGORITHM / АЛГОРИТМ:
 * 1. Calculate required size (current + new)
 * 2. If exceeds capacity, grow buffer (double until sufficient)
 * 3. Copy new data to end of buffer
 * 4. Update size
 * 
 * 1. Вычислить требуемый размер (текущий + новый)
 * 2. Если превышает ёмкость, увеличить буфер (удваивать пока не хватит)
 * 3. Скопировать новые данные в конец буфера
 * 4. Обновить размер
 ******************************************************************************/
static BOOL DB_Append(SaveDynBuf* d, const void* src, DWORD len) {
    if (!d || !d->data) return FALSE;
    if (len == 0) return TRUE;  // Nothing to append / Нечего добавлять
    
    // Calculate required size / Вычислить требуемый размер
    DWORD need = d->size + len;
    
    // Grow buffer if needed / Увеличить буфер при необходимости
    if (need > d->capacity) {
        DWORD newCap = d->capacity ? d->capacity : 256;
        
        // Double capacity until sufficient / Удваивать ёмкость пока не хватит
        while (newCap < need) newCap *= 2;
        
        // Reallocate buffer / Перевыделить буфер
        BYTE* n = (BYTE*)HeapReAlloc(GetProcessHeap(), 0, d->data, newCap);
        if (!n) return FALSE;
        
        d->data = n;
        d->capacity = newCap;
    }
    
    // Append data and update size / Добавить данные и обновить размер
    memcpy(d->data + d->size, src, len);
    d->size = need;
    return TRUE;
}

/*******************************************************************************
 * DB_AppendChar
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Convenience function to append a single character to buffer.
 * Удобная функция для добавления одного символа в буфер.
 * 
 * PARAMETERS / ПАРАМЕТРЫ:
 * d - Pointer to SaveDynBuf structure / Указатель на структуру SaveDynBuf
 * c - Character to append / Символ для добавления
 * 
 * RETURNS / ВОЗВРАЩАЕТ:
 * TRUE if append successful, FALSE on allocation failure
 * TRUE если добавление успешно, FALSE при ошибке выделения памяти
 ******************************************************************************/
static BOOL DB_AppendChar(SaveDynBuf* d, char c) { 
    return DB_Append(d, &c, 1); 
}

/*******************************************************************************
 * FILE UTILITY FUNCTIONS
 * УТИЛИТАРНЫЕ ФУНКЦИИ ФАЙЛОВ
 ******************************************************************************/

/*******************************************************************************
 * HasExtI_A
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Case-insensitive file extension check.
 * Проверка расширения файла без учёта регистра.
 * 
 * PARAMETERS / ПАРАМЕТРЫ:
 * path - File path / Путь к файлу
 * ext  - Extension to check (including dot, e.g., ".mp3")
 *        Расширение для проверки (включая точку, например ".mp3")
 * 
 * RETURNS / ВОЗВРАЩАЕТ:
 * TRUE if file has specified extension, FALSE otherwise
 * TRUE если файл имеет указанное расширение, FALSE иначе
 ******************************************************************************/
static BOOL HasExtI_A(const char* path, const char* ext) {
    if (!path || !ext) return FALSE;
    
    // Find last dot in path / Найти последнюю точку в пути
    const char* p = strrchr(path, '.');
    
    // Compare extension case-insensitively / Сравнить расширение без учёта регистра
    return p && lstrcmpiA(p, ext) == 0;
}

/*******************************************************************************
 * PathGetDirA_
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Extracts directory path from full file path.
 * Извлекает путь к каталогу из полного пути к файлу.
 * 
 * PARAMETERS / ПАРАМЕТРЫ:
 * path - Full file path / Полный путь к файлу
 * out  - Output buffer for directory path / Выходной буфер для пути к каталогу
 * cc   - Size of output buffer / Размер выходного буфера
 * 
 * ALGORITHM / АЛГОРИТМ:
 * 1. Copy path to output buffer
 * 2. Find last backslash or forward slash
 * 3. Truncate after slash to get directory
 * 
 * 1. Скопировать путь в выходной буфер
 * 2. Найти последний обратный или прямой слеш
 * 3. Обрезать после слеша для получения каталога
 * 
 * EXAMPLE / ПРИМЕР:
 * Input:  C:\Music\song.mp3
 * Output: C:\Music\
 ******************************************************************************/
static void PathGetDirA_(const char* path, char* out, int cc) {
    if (!out) return;
    out[0] = 0;
    if (!path) return;
    
    // Copy path to output / Скопировать путь в вывод
    lstrcpynA(out, path, cc);
    
    // Find last path separator / Найти последний разделитель пути
    char* s1 = strrchr(out, '\\');
    char* s2 = strrchr(out, '/');
    char* s = (s1 > s2) ? s1 : s2;
    
    // Truncate after separator / Обрезать после разделителя
    if (s) *(s + 1) = 0;
    else out[0] = 0;
}

/*******************************************************************************
 * FileExistsA_
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Checks if file exists and is not a directory.
 * Проверяет, существует ли файл и не является ли каталогом.
 * 
 * PARAMETERS / ПАРАМЕТРЫ:
 * path - File path to check / Путь к файлу для проверки
 * 
 * RETURNS / ВОЗВРАЩАЕТ:
 * TRUE if file exists and is a regular file, FALSE otherwise
 * TRUE если файл существует и является обычным файлом, FALSE иначе
 ******************************************************************************/
static BOOL FileExistsA_(const char* path) { 
    DWORD a = GetFileAttributesA(path);
    return a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY); 
}

/*******************************************************************************
 * LooksLikeID3v2FileA_
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Quick check if file starts with ID3v2 tag header.
 * Быстрая проверка, начинается ли файл с заголовка ID3v2 тега.
 * 
 * PARAMETERS / ПАРАМЕТРЫ:
 * path - File path to check / Путь к файлу для проверки
 * 
 * RETURNS / ВОЗВРАЩАЕТ:
 * TRUE if file starts with "ID3" signature, FALSE otherwise
 * TRUE если файл начинается с сигнатуры "ID3", FALSE иначе
 * 
 * ALGORITHM / АЛГОРИТМ:
 * 1. Open file for reading
 * 2. Read first 3 bytes
 * 3. Check if bytes match "ID3" signature
 * 
 * 1. Открыть файл для чтения
 * 2. Прочитать первые 3 байта
 * 3. Проверить, соответствуют ли байты сигнатуре "ID3"
 ******************************************************************************/
static BOOL LooksLikeID3v2FileA_(const char* path) {
    // Open file for reading / Открыть файл для чтения
    HANDLE h = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, 0, 0);
    if (h == INVALID_HANDLE_VALUE) return FALSE;
    
    // Read first 3 bytes / Прочитать первые 3 байта
    BYTE b[3];
    DWORD r;
    BOOL ok = ReadFile(h, b, 3, &r, 0) && r == 3 && memcmp(b, "ID3", 3) == 0;
    
    CloseHandle(h);
    return ok;
}

/*******************************************************************************
 * BACKUP MAP SYSTEM
 * СИСТЕМА КАРТЫ РЕЗЕРВНЫХ КОПИЙ
 * 
 * Tracks temporary files created during MP3 tag save operations.
 * Maps original file paths to backup file paths with timestamps.
 * Used to preserve original tags when rebuilding.
 * 
 * Отслеживает временные файлы, созданные во время операций сохранения MP3 тегов.
 * Сопоставляет пути оригинальных файлов с путями резервных копий с временными метками.
 * Используется для сохранения оригинальных тегов при перестройке.
 ******************************************************************************/

/*******************************************************************************
 * SaveBakMap Structure
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Single entry in backup map, tracking one file pair.
 * Один элемент в карте резервных копий, отслеживающий одну пару файлов.
 * 
 * FIELDS / ПОЛЯ:
 * orig  - Original file path / Путь к оригинальному файлу
 * bak   - Backup file path / Путь к резервной копии
 * tick  - Timestamp when entry created (GetTickCount) / Временная метка создания элемента
 * inUse - TRUE if entry is valid, FALSE if slot is free / TRUE если элемент допустим
 ******************************************************************************/
typedef struct { 
    char orig[MAX_PATH];  // Original file path / Путь к оригинальному файлу
    char bak[MAX_PATH];   // Backup file path / Путь к резервной копии
    DWORD tick;           // Timestamp / Временная метка
    BOOL inUse;           // Entry valid flag / Флаг допустимости элемента
} SaveBakMap;

// Backup map pool (32 simultaneous saves maximum)
// Пул карты резервных копий (максимум 32 одновременных сохранения)
static SaveBakMap g_saveBakMap[32];

// Critical section for thread-safe access to backup map
// Критическая секция для потокобезопасного доступа к карте резервных копий
static CRITICAL_SECTION g_saveCS;

// Critical section initialization flag
// Флаг инициализации критической секции
static volatile LONG g_saveCSInit = 0;

/*******************************************************************************
 * SaveCS_Init
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Thread-safe initialization of critical section.
 * Uses interlocked operations to ensure one-time initialization.
 * 
 * Потокобезопасная инициализация критической секции.
 * Использует interlocked операции для обеспечения однократной инициализации.
 * 
 * ALGORITHM / АЛГОРИТМ:
 * 1. Attempt to change init flag from 0 to 1
 * 2. If successful, initialize critical section and set flag to 2
 * 3. If not successful, wait for flag to become 2 (another thread initializing)
 * 
 * 1. Попытаться изменить флаг инициализации с 0 на 1
 * 2. Если успешно, инициализировать критическую секцию и установить флаг в 2
 * 3. Если не успешно, ждать пока флаг станет 2 (другой поток инициализирует)
 ******************************************************************************/
static void SaveCS_Init() {
    LONG prev = InterlockedCompareExchange(&g_saveCSInit, 1, 0);

    if (prev == 2) return;      // already inited
    if (prev == -1) return;     // permanently failed

    if (prev == 1) {
        // someone else is initializing — don't spin forever
        DWORD t0 = GetTickCount();
        while (g_saveCSInit == 1) {
            Sleep(1);
            if ((GetTickCount() - t0) > 3000) { // 3s safety
                InterlockedExchange(&g_saveCSInit, -1);
                return;
            }
        }
        return;
    }

    __try {
        InitializeCriticalSection(&g_saveCS);
        InterlockedExchange(&g_saveCSInit, 2);
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        InterlockedExchange(&g_saveCSInit, -1);
    }
}


/*******************************************************************************
 * Bak_Lock / Bak_Unlock
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Thread-safe locking/unlocking for backup map access.
 * Потокобезопасная блокировка/разблокировка для доступа к карте резервных копий.
 * 
 * NOTES / ПРИМЕЧАНИЯ:
 * Always pair Bak_Lock() with Bak_Unlock() to prevent deadlocks.
 * Всегда связывайте Bak_Lock() с Bak_Unlock() для предотвращения дедлоков.
 ******************************************************************************/
static void Bak_Lock() { 
    if (g_saveCSInit != 2) SaveCS_Init();  // Ensure CS initialized / Убедиться что CS инициализирована
    EnterCriticalSection(&g_saveCS); 
}
static void Bak_Unlock() { 
    LeaveCriticalSection(&g_saveCS); 
}

/*******************************************************************************
 * Bak_Set
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Adds or updates backup mapping in the map.
 * Adds entry to track that 'orig' file has backup at 'bak' path.
 * 
 * Добавляет или обновляет отображение резервной копии в карте.
 * Добавляет элемент для отслеживания что файл 'orig' имеет резервную копию по пути 'bak'.
 * 
 * PARAMETERS / ПАРАМЕТРЫ:
 * orig - Original file path / Путь к оригинальному файлу
 * bak  - Backup file path / Путь к резервной копии
 * 
 * ALGORITHM / АЛГОРИТМ:
 * 1. Search for existing entry with same original path
 * 2. If found, update backup path and timestamp
 * 3. If not found, find free slot and create new entry
 * 
 * 1. Искать существующий элемент с тем же путём оригинала
 * 2. Если найден, обновить путь резервной копии и временную метку
 * 3. Если не найден, найти свободный слот и создать новый элемент
 * 
 * NOTES / ПРИМЕЧАНИЯ:
 * Must be called with backup map locked (between Bak_Lock/Bak_Unlock).
 * Должна вызываться с заблокированной картой резервных копий.
 ******************************************************************************/
static void Bak_Set(const char* orig, const char* bak) {
    // Try to update existing entry / Попытаться обновить существующий элемент
    for (int i = 0; i < 32; i++) {
        if (g_saveBakMap[i].inUse && lstrcmpiA(g_saveBakMap[i].orig, orig) == 0) {
            lstrcpynA(g_saveBakMap[i].bak, bak, MAX_PATH);
            g_saveBakMap[i].tick = GetTickCount();
            return;
        }
    }
    
    // Create new entry in free slot / Создать новый элемент в свободном слоте
    for (int i = 0; i < 32; i++) {
        if (!g_saveBakMap[i].inUse) {
            g_saveBakMap[i].inUse = TRUE;
            lstrcpynA(g_saveBakMap[i].orig, orig, MAX_PATH);
            lstrcpynA(g_saveBakMap[i].bak, bak, MAX_PATH);
            g_saveBakMap[i].tick = GetTickCount();
            return;
        }
    }
}

/*******************************************************************************
 * Bak_Peek
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Retrieves backup path for given original file path.
 * Automatically cleans up expired entries (older than 60 seconds).
 * 
 * Получает путь резервной копии для данного пути оригинального файла.
 * Автоматически очищает просроченные элементы (старше 60 секунд).
 * 
 * PARAMETERS / ПАРАМЕТРЫ:
 * orig - Original file path to look up / Путь к оригинальному файлу для поиска
 * out  - Output buffer for backup path (can be NULL) / Выходной буфер для пути резервной копии
 * 
 * RETURNS / ВОЗВРАЩАЕТ:
 * TRUE if backup found, FALSE otherwise
 * TRUE если резервная копия найдена, FALSE иначе
 * 
 * ALGORITHM / АЛГОРИТМ:
 * 1. Get current tick count
 * 2. Iterate through all entries
 * 3. Expire entries older than 60 seconds
 * 4. If matching entry found, copy backup path and return TRUE
 * 
 * 1. Получить текущий счётчик тиков
 * 2. Перебрать все элементы
 * 3. Пометить как просроченные элементы старше 60 секунд
 * 4. Если найден подходящий элемент, скопировать путь резервной копии и вернуть TRUE
 * 
 * NOTES / ПРИМЕЧАНИЯ:
 * Must be called with backup map locked (between Bak_Lock/Bak_Unlock).
 * 60 second timeout prevents map from filling with stale entries.
 * 
 * Должна вызываться с заблокированной картой резервных копий.
 * Таймаут 60 секунд предотвращает заполнение карты устаревшими элементами.
 ******************************************************************************/
static BOOL Bak_Peek(const char* orig, char* out) {
    DWORD now = GetTickCount();
    
    for (int i = 0; i < 32; i++) {
        if (!g_saveBakMap[i].inUse) continue;
        
        // Expire old entries (60 second timeout) / Пометить как просроченные старые элементы (таймаут 60 секунд)
        if ((now - g_saveBakMap[i].tick) > 60000) {
            g_saveBakMap[i].inUse = FALSE;
            continue;
        }
        
        // Check if path matches / Проверить, совпадает ли путь
        if (lstrcmpiA(g_saveBakMap[i].orig, orig) == 0) {
            if (out) lstrcpynA(out, g_saveBakMap[i].bak, MAX_PATH);
            return TRUE;
        }
    }
    
    return FALSE;
}

/*******************************************************************************
 * IAT_Patch
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Patches Import Address Table (IAT) to redirect function calls.
 * Used to hook MoveFileA/MoveFileExA in in_mp3.dll.
 * 
 * Патчит таблицу адресов импорта (IAT) для перенаправления вызовов функций.
 * Используется для перехвата MoveFileA/MoveFileExA в in_mp3.dll.
 * 
 * PARAMETERS / ПАРАМЕТРЫ:
 * hMod    - Module to patch / Модуль для патча
 * dll     - DLL name containing function to hook / Имя DLL содержащей функцию для перехвата
 * func    - Function name to hook / Имя функции для перехвата
 * newFunc - New function pointer / Указатель на новую функцию
 * oldFunc - Output: original function pointer / Вывод: оригинальный указатель функции
 * 
 * RETURNS / ВОЗВРАЩАЕТ:
 * TRUE if patch successful, FALSE otherwise
 * TRUE если патч успешен, FALSE иначе
 * 
 * ALGORITHM / АЛГОРИТМ:
 * 1. Validate module has PE structure
 * 2. Locate import descriptor for specified DLL
 * 3. Find import thunk for specified function
 * 4. Change memory protection to allow writes
 * 5. Replace function pointer with hook
 * 6. Restore original protection
 * 
 * 1. Проверить что модуль имеет PE структуру
 * 2. Найти дескриптор импорта для указанной DLL
 * 3. Найти thunk импорта для указанной функции
 * 4. Изменить защиту памяти для разрешения записи
 * 5. Заменить указатель функции на хук
 * 6. Восстановить оригинальную защиту
 * 
 * NOTES / ПРИМЕЧАНИЯ:
 * This is a standard IAT hooking technique.
 * Safe for Windows 98 through Windows 11.
 * 
 * Это стандартная техника перехвата IAT.
 * Безопасна для Windows 98 до Windows 11.
 ******************************************************************************/
static BOOL RvaInImage(DWORD rva, DWORD size, DWORD imageSize) {
    if (!rva) return FALSE;
    if (rva >= imageSize) return FALSE;
    if (size > (imageSize - rva)) return FALSE;
    return TRUE;
}

static BOOL PatchThunkDWORD(DWORD* pThunk, DWORD newVal, DWORD* oldVal) {
    DWORD oldProt = 0;
    if (!VirtualProtect(pThunk, sizeof(DWORD), PAGE_READWRITE, &oldProt))
        return FALSE;

    if (oldVal) *oldVal = *pThunk;
    *pThunk = newVal;

    DWORD dummy = 0;
    VirtualProtect(pThunk, sizeof(DWORD), oldProt, &dummy);
    FlushInstructionCache(GetCurrentProcess(), pThunk, sizeof(DWORD));
    return TRUE;
}

static BOOL IAT_Patch(HMODULE hMod, const char* dll, const char* func, void* newFunc, void** oldFunc) {
    if (!hMod || !dll || !func || !newFunc) return FALSE;

    BYTE* base = (BYTE*)hMod;

    PIMAGE_DOS_HEADER dos = (PIMAGE_DOS_HEADER)base;
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) return FALSE;

    PIMAGE_NT_HEADERS nt = (PIMAGE_NT_HEADERS)(base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) return FALSE;

    DWORD imageSize = nt->OptionalHeader.SizeOfImage;

    IMAGE_DATA_DIRECTORY impDir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    if (impDir.VirtualAddress == 0 || impDir.Size == 0) return FALSE;
    if (!RvaInImage(impDir.VirtualAddress, sizeof(IMAGE_IMPORT_DESCRIPTOR), imageSize)) return FALSE;

    PIMAGE_IMPORT_DESCRIPTOR imp = (PIMAGE_IMPORT_DESCRIPTOR)(base + impDir.VirtualAddress);

    // Prepare fallback target address (for OFT==0 or ordinal cases)
    HMODULE hDll = GetModuleHandleA(dll);
    BOOL needFree = FALSE;
    if (!hDll) {
        hDll = LoadLibraryA(dll);
        if (hDll) needFree = TRUE;
    }
    FARPROC targetProc = hDll ? GetProcAddress(hDll, func) : NULL;

    BOOL patched = FALSE;

    for (; imp->Name; ++imp) {
        if (!RvaInImage(imp->Name, 1, imageSize)) continue;

        const char* dllName = (const char*)(base + imp->Name);
        if (!dllName) continue;
        if (lstrcmpiA(dllName, dll) != 0) continue;

        if (!RvaInImage(imp->FirstThunk, sizeof(IMAGE_THUNK_DATA), imageSize)) break;
        PIMAGE_THUNK_DATA ft = (PIMAGE_THUNK_DATA)(base + imp->FirstThunk);

        PIMAGE_THUNK_DATA oft = NULL;
        if (imp->OriginalFirstThunk && RvaInImage(imp->OriginalFirstThunk, sizeof(IMAGE_THUNK_DATA), imageSize)) {
            oft = (PIMAGE_THUNK_DATA)(base + imp->OriginalFirstThunk);
        }

        if (oft) {
            // Name-based walk
            for (; oft->u1.Function; ++oft, ++ft) {
                if (IMAGE_SNAP_BY_ORDINAL(oft->u1.Ordinal)) {
                    // fallback by address if possible
                    if (targetProc && (FARPROC)ft->u1.Function == targetProc) {
                        DWORD oldVal = 0;
                        if (PatchThunkDWORD((DWORD*)&ft->u1.Function, (DWORD)newFunc, &oldVal)) {
                            if (oldFunc) *oldFunc = (void*)oldVal;
                            patched = TRUE;
                        }
                    }
                    continue;
                }

                DWORD aod = oft->u1.AddressOfData;
                if (!RvaInImage(aod, sizeof(IMAGE_IMPORT_BY_NAME), imageSize)) continue;

                PIMAGE_IMPORT_BY_NAME ibn = (PIMAGE_IMPORT_BY_NAME)(base + aod);
                if (!ibn) continue;

                const char* name = (const char*)ibn->Name;
                if (!name) continue;

                // Minimal safety: name must be in image
                // (we don't scan full string; just avoid totally invalid pointer)
                if (!RvaInImage(aod + (DWORD)((BYTE*)ibn->Name - (BYTE*)ibn), 1, imageSize)) continue;

                if (lstrcmpiA(name, func) == 0) {
                    DWORD oldVal = 0;
                    if (PatchThunkDWORD((DWORD*)&ft->u1.Function, (DWORD)newFunc, &oldVal)) {
                        if (oldFunc) *oldFunc = (void*)oldVal;
                        patched = TRUE;
                    }
                    break;
                }
            }
        } else {
            // OFT==0: patch by address
            if (targetProc) {
                for (; ft->u1.Function; ++ft) {
                    if ((FARPROC)ft->u1.Function == targetProc) {
                        DWORD oldVal = 0;
                        if (PatchThunkDWORD((DWORD*)&ft->u1.Function, (DWORD)newFunc, &oldVal)) {
                            if (oldFunc) *oldFunc = (void*)oldVal;
                            patched = TRUE;
                        }
                        break;
                    }
                }
            }
        }

        break; // processed the target dll descriptor
    }

    if (needFree && hDll) FreeLibrary(hDll);
    return patched;
}

/*******************************************************************************
 * TAG BUILDING HELPERS
 * ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ ПОСТРОЕНИЯ ТЕГОВ
 ******************************************************************************/

/*******************************************************************************
 * BE24Write
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Writes 24-bit big-endian integer to buffer.
 * Used for ID3v2.2 frame sizes.
 * 
 * Записывает 24-битное целое число big-endian в буфер.
 * Используется для размеров фреймов ID3v2.2.
 * 
 * PARAMETERS / ПАРАМЕТРЫ:
 * p - Pointer to 3-byte buffer / Указатель на 3-байтовый буфер
 * v - Value to write / Значение для записи
 ******************************************************************************/
static void BE24Write(BYTE* p, DWORD v) { 
    p[0] = (BYTE)(v >> 16);  // Most significant byte / Старший байт
    p[1] = (BYTE)(v >> 8);   // Middle byte / Средний байт
    p[2] = (BYTE)v;          // Least significant byte / Младший байт
}

/*******************************************************************************
 * IsUrlFrame
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Checks if frame ID represents a URL frame.
 * URL frames start with 'W' in ID3v2.
 * 
 * Проверяет, представляет ли ID фрейма URL-фрейм.
 * URL-фреймы начинаются с 'W' в ID3v2.
 * 
 * PARAMETERS / ПАРАМЕТРЫ:
 * v  - ID3 version (2, 3, or 4) / Версия ID3 (2, 3 или 4)
 * id - Frame ID bytes / Байты ID фрейма
 * 
 * RETURNS / ВОЗВРАЩАЕТ:
 * TRUE if frame is URL frame (WXXX, WCOM, etc.), FALSE otherwise
 * TRUE если фрейм является URL-фреймом (WXXX, WCOM и т.д.), FALSE иначе
 ******************************************************************************/
static BOOL IsUrlFrame(int v, const BYTE* id) { 
    return id[0] == 'W'; 
}

/*******************************************************************************
 * IsHandledFrame
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Checks if frame ID is handled by our tag builder.
 * Handled frames are rebuilt from scratch, others are copied.
 * 
 * Проверяет, обрабатывается ли ID фрейма нашим построителем тегов.
 * Обрабатываемые фреймы перестраиваются с нуля, остальные копируются.
 * 
 * PARAMETERS / ПАРАМЕТРЫ:
 * v  - ID3 version (2, 3, or 4) / Версия ID3 (2, 3 или 4)
 * id - Frame ID bytes / Байты ID фрейма
 * 
 * RETURNS / ВОЗВРАЩАЕТ:
 * TRUE if frame is handled (TIT2, TPE1, etc.), FALSE otherwise
 * TRUE если фрейм обрабатывается (TIT2, TPE1 и т.д.), FALSE иначе
 * 
 * HANDLED FRAMES / ОБРАБАТЫВАЕМЫЕ ФРЕЙМЫ:
 * ID3v2.3/2.4:               ID3v2.2:
 * TIT2 - Title               TT2 - Title / Название
 * TPE1 - Artist              TP1 - Artist / Исполнитель
 * TALB - Album               TAL - Album / Альбом
 * TYER - Year                TYE - Year / Год
 * TCON - Genre               TCO - Genre / Жанр
 * TRCK - Track               TRK - Track / Трек
 * TCOM - Composer            TCM - Composer / Композитор
 * TOPE - Original Artist     TOA - Original Artist / Оригинальный исполнитель
 * TCOP - Copyright           TCR - Copyright / Авторские права
 * TENC - Encoder             TEN - Encoder / Кодировщик
 * COMM - Comment             COM - Comment / Комментарий
 ******************************************************************************/
static BOOL IsHandledFrame(int v, const BYTE* id) {
    // ID3v2.3/2.4 frame IDs (4 characters)
    // ID фреймов ID3v2.3/2.4 (4 символа)
    static const char* h4[] = {"TIT2", "TPE1", "TALB", "TYER", "TCON", "TRCK", "TCOM", "TOPE", "TCOP", "TENC", "COMM"};
    
    // ID3v2.2 frame IDs (3 characters)
    // ID фреймов ID3v2.2 (3 символа)
    static const char* h3[] = {"TT2", "TP1", "TAL", "TYE", "TCO", "TRK", "TCM", "TOA", "TCR", "TEN", "COM"};
    
    int n = 11;  // Number of handled frames / Количество обрабатываемых фреймов
    const char** arr = (v == 2) ? h3 : h4;
    int len = (v == 2) ? 3 : 4;
    
    // Check if frame ID matches any handled frame / Проверить, соответствует ли ID фрейма какому-либо обрабатываемому фрейму
    for (int i = 0; i < n; i++) {
        if (memcmp(id, arr[i], len) == 0) return TRUE;
    }
    
    return FALSE;
}

/*******************************************************************************
 * CopyFrames
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Copies frames from existing ID3v2 tag to new tag being built.
 * Can filter to copy only URL frames, or exclude URL frames, or exclude handled frames.
 * 
 * Копирует фреймы из существующего ID3v2 тега в новый строящийся тег.
 * Может фильтровать для копирования только URL-фреймов, или исключения URL-фреймов, или исключения обрабатываемых фреймов.
 * 
 * PARAMETERS / ПАРАМЕТРЫ:
 * h          - File handle positioned at tag start / Дескриптор файла, позиционированный на начале тега
 * db         - Dynamic buffer to append frames to / Динамический буфер для добавления фреймов
 * isV22      - TRUE if ID3v2.2, FALSE if ID3v2.3/2.4 / TRUE если ID3v2.2
 * onlyUrl    - TRUE to copy only URL frames / TRUE для копирования только URL-фреймов
 * excludeUrl - TRUE to exclude URL frames / TRUE для исключения URL-фреймов
 * 
 * RETURNS / ВОЗВРАЩАЕТ:
 * TRUE if successful, FALSE on error
 * TRUE если успешно, FALSE при ошибке
 * 
 * ALGORITHM / АЛГОРИТМ:
 * 1. Read tag header to get tag size
 * 2. Iterate through all frames in tag
 * 3. For each frame, check if it should be copied based on filters
 * 4. If copying, append frame header and data to buffer
 * 5. If not copying, skip frame data
 * 
 * 1. Прочитать заголовок тега для получения размера тега
 * 2. Перебрать все фреймы в теге
 * 3. Для каждого фрейма проверить, должен ли он копироваться на основе фильтров
 * 4. Если копируем, добавить заголовок фрейма и данные в буфер
 * 5. Если не копируем, пропустить данные фрейма
 * 
 * NOTES / ПРИМЕЧАНИЯ:
 * This preserves unknown/custom frames that our builder doesn't handle.
 * Это сохраняет неизвестные/пользовательские фреймы, которые наш построитель не обрабатывает.
 ******************************************************************************/
static BOOL CopyFrames(HANDLE h, SaveDynBuf* db, BOOL isV22, BOOL onlyUrl, BOOL excludeUrl) {
    DWORD sz = GetFileSize(h, NULL);
    BYTE hdr[10];
    DWORD rd;
    
    // Read tag header / Прочитать заголовок тега
    SetFilePointer(h, 0, 0, FILE_BEGIN);
    ReadFile(h, hdr, 10, &rd, 0);
    
    // Get version and tag size / Получить версию и размер тега
    int ver = hdr[3];
    DWORD tagSz = DECRYPT_SyncsafeToSize(hdr + 6);
    
    // Sanity check tag size / Проверка размера тега на адекватность
    if (tagSz == 0 || tagSz > TAG_SANITY_LIMIT) return FALSE;
    
    // Position after tag header / Позиционировать после заголовка тега
    SetFilePointer(h, 10, 0, FILE_BEGIN);
    
    int rem = tagSz;  // Remaining bytes in tag / Оставшиеся байты в теге
    
    // Iterate through frames / Перебрать фреймы
    while (rem >= (isV22 ? 6 : 10)) {
        BYTE fh[10];
        int hSz = isV22 ? 6 : 10;  // Frame header size / Размер заголовка фрейма
        
        // Read frame header / Прочитать заголовок фрейма
        ReadFile(h, fh, hSz, &rd, 0);
        
        // Check for padding (zero byte indicates end of frames) / Проверить заполнение (нулевой байт указывает конец фреймов)
        if (fh[0] == 0) break;
        
        // Parse frame size based on version / Разобрать размер фрейма на основе версии
        DWORD fsz = 0;
        if (isV22) {
            // ID3v2.2: 24-bit big-endian / ID3v2.2: 24-битное big-endian
            fsz = ((DWORD)fh[3] << 16) | ((DWORD)fh[4] << 8) | fh[5];
        } else {
            // ID3v2.3/2.4: check if syncsafe or regular / ID3v2.3/2.4: проверить, syncsafe или обычное
            fsz = (ver == 4) ? DECRYPT_SyncsafeToSize(fh + 4) : DECRYPT_BE32Read(fh + 4);
        }
        
        // Determine if frame should be copied / Определить, должен ли фрейм копироваться
        BOOL copy = TRUE;
        if (onlyUrl) {
            // Copy only URL frames / Копировать только URL-фреймы
            copy = IsUrlFrame(isV22 ? 2 : ver, fh);
        } else if (excludeUrl && IsUrlFrame(isV22 ? 2 : ver, fh)) {
            // Exclude URL frames / Исключить URL-фреймы
            copy = FALSE;
        } else if (!onlyUrl && IsHandledFrame(isV22 ? 2 : ver, fh)) {
            // Exclude handled frames (we rebuild these) / Исключить обрабатываемые фреймы (мы перестраиваем их)
            copy = FALSE;
        }
        
        if (copy) {
            // Convert v2.4 syncsafe size to regular for output / Преобразовать syncsafe размер v2.4 в обычный для вывода
            if (!isV22 && ver == 4) {
                DECRYPT_BE32Write(fh + 4, fsz);
            }
            
            // Append frame header / Добавить заголовок фрейма
            DB_Append(db, fh, hSz);
            
            // Append frame data / Добавить данные фрейма
            BYTE tmp[4096];
            DWORD left = fsz;
            while (left > 0) {
                DWORD chk = (left > 4096) ? 4096 : left;
                ReadFile(h, tmp, chk, &rd, 0);
                DB_Append(db, tmp, rd);
                left -= rd;
            }
        } else {
            // Skip frame data / Пропустить данные фрейма
            SetFilePointer(h, fsz, NULL, FILE_CURRENT);
        }
        
        rem -= (hSz + fsz);
    }
    
    return TRUE;
}

/*******************************************************************************
 * BuildID3_Universal
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Builds complete ID3v2 tag from field values and existing tags.
 * Combines new field values with preserved frames from original/temp files.
 * 
 * Строит полный ID3v2 тег из значений полей и существующих тегов.
 * Объединяет новые значения полей с сохранёнными фреймами из оригинальных/временных файлов.
 * 
 * PARAMETERS / ПАРАМЕТРЫ:
 * hBase  - Handle to original file (can be INVALID_HANDLE_VALUE) / Дескриптор оригинального файла
 * hTemp  - Handle to temp file (can be INVALID_HANDLE_VALUE) / Дескриптор временного файла
 * A      - Artist / Исполнитель
 * T      - Title / Название
 * Alb    - Album / Альбом
 * Y      - Year / Год
 * G      - Genre / Жанр
 * Trk    - Track number / Номер трека
 * C      - Composer / Композитор
 * M      - Comment / Комментарий
 * O      - Original Artist / Оригинальный исполнитель
 * P      - Copyright / Авторские права
 * E      - Encoder / Кодировщик
 * pNewSz - Output: size of built tag / Вывод: размер построенного тега
 * isV22  - TRUE to build ID3v2.2, FALSE for ID3v2.3 / TRUE для построения ID3v2.2
 * 
 * RETURNS / ВОЗВРАЩАЕТ:
 * Pointer to allocated tag data (caller must free with HeapFree)
 * NULL on allocation failure
 * 
 * Указатель на выделенные данные тега (вызывающий должен освободить через HeapFree)
 * NULL при ошибке выделения памяти
 * 
 * ALGORITHM / АЛГОРИТМ:
 * 1. Initialize dynamic buffer with placeholder header
 * 2. Add text frames for all provided fields (TIT2, TPE1, etc.)
 * 3. Add comment frame (COMM) if comment provided
 * 4. Copy URL frames from temp file (or base file if no temp)
 * 5. Copy unhandled frames from base file
 * 6. Finalize header with correct tag size
 * 
 * 1. Инициализировать динамический буфер с заполнителем заголовка
 * 2. Добавить текстовые фреймы для всех предоставленных полей (TIT2, TPE1 и т.д.)
 * 3. Добавить фрейм комментария (COMM) если комментарий предоставлен
 * 4. Скопировать URL-фреймы из временного файла (или базового файла если нет временного)
 * 5. Скопировать необработанные фреймы из базового файла
 * 6. Завершить заголовок с правильным размером тега
 * 
 * NOTES / ПРИМЕЧАНИЯ:
 * All text fields use Latin-1 encoding (encoding byte 0).
 * Caller is responsible for freeing returned buffer.
 * 
 * Все текстовые поля используют кодировку Latin-1 (байт кодировки 0).
 * Вызывающий ответственен за освобождение возвращённого буфера.
 ******************************************************************************/
static BYTE* BuildID3_Universal(HANDLE hBase, HANDLE hTemp, 
    const WCHAR* A, const WCHAR* T, const WCHAR* Alb, const WCHAR* Y, const WCHAR* G,
    const WCHAR* Trk, const WCHAR* C, const WCHAR* M, const WCHAR* O, const WCHAR* P, const WCHAR* E,
    DWORD* pNewSz, BOOL isV22) 
{
    if (pNewSz) *pNewSz = 0;
    
    // Initialize dynamic buffer / Инициализировать динамический буфер
    SaveDynBuf db;
    if (!DB_Init(&db, 65536)) return NULL;
    
    // Write placeholder header (will be filled later) / Записать заполнитель заголовка (будет заполнен позже)
    BYTE hdr[10] = {0};
    DB_Append(&db, hdr, 10);
    
    // Temporary buffer for ANSI conversion / Временный буфер для преобразования ANSI
    char tmp[ABUF_MAX];
    
    // Arrays of field values and their frame IDs
    // Массивы значений полей и их ID фреймов
    const WCHAR* w[] = {T, A, Alb, Y, G, Trk, C, O, P, E};
    const char* i4[] = {"TIT2", "TPE1", "TALB", "TYER", "TCON", "TRCK", "TCOM", "TOPE", "TCOP", "TENC"};
    const char* i3[] = {"TT2", "TP1", "TAL", "TYE", "TCO", "TRK", "TCM", "TOA", "TCR", "TEN"};
    
    // Add text frames for each non-empty field / Добавить текстовые фреймы для каждого непустого поля
    for (int i = 0; i < 10; i++) {
        if (w[i] && w[i][0]) {
            // Convert Unicode to ANSI / Преобразовать Unicode в ANSI
            int l = DECRYPT_WideToACP(w[i], tmp, ABUF_MAX);
            if (l > 0) {
                if (isV22) {
                    // ID3v2.2: 3-char frame ID + 24-bit size / ID3v2.2: 3-символьный ID фрейма + 24-битный размер
                    BYTE h[6] = {0};
                    memcpy(h, i3[i], 3);
                    BE24Write(h + 3, l + 1);  // +1 for encoding byte / +1 для байта кодировки
                    DB_Append(&db, h, 6);
                    DB_AppendChar(&db, 0);     // Encoding: Latin-1 / Кодировка: Latin-1
                    DB_Append(&db, tmp, l);
                } else {
                    // ID3v2.3: 4-char frame ID + 32-bit size / ID3v2.3: 4-символьный ID фрейма + 32-битный размер
                    BYTE h[10] = {0};
                    memcpy(h, i4[i], 4);
                    DECRYPT_BE32Write(h + 4, l + 1);
                    DB_Append(&db, h, 10);
                    DB_AppendChar(&db, 0);     // Encoding: Latin-1 / Кодировка: Latin-1
                    DB_Append(&db, tmp, l);
                }
            }
        }
    }
    
    // Add comment frame if comment provided / Добавить фрейм комментария если комментарий предоставлен
    if (M && M[0]) {
        int l = DECRYPT_WideToACP(M, tmp, ABUF_MAX);
        if (l > 0) {
            // Comment frame: encoding + language + short desc + text
            // Фрейм комментария: кодировка + язык + короткое описание + текст
            DWORD pSz = 1 + 3 + 1 + l;  // encoding + "eng" + null + comment / кодировка + "eng" + null + комментарий
            
            if (isV22) {
                // ID3v2.2: COM frame / ID3v2.2: фрейм COM
                BYTE h[6] = {'C', 'O', 'M', 0, 0, 0};
                BE24Write(h + 3, pSz);
                DB_Append(&db, h, 6);
            } else {
                // ID3v2.3: COMM frame / ID3v2.3: фрейм COMM
                BYTE h[10] = {'C', 'O', 'M', 'M', 0, 0, 0, 0, 0, 0};
                DECRYPT_BE32Write(h + 4, pSz);
                DB_Append(&db, h, 10);
            }
            
            DB_AppendChar(&db, 0);          // Encoding: Latin-1 / Кодировка: Latin-1
            DB_Append(&db, "eng", 3);       // Language code / Код языка
            DB_AppendChar(&db, 0);          // Short description (empty) / Короткое описание (пустое)
            DB_Append(&db, tmp, l);         // Comment text / Текст комментария
        }
    }
    
    // Copy URL frames from temp file (preferred) or base file
    // Скопировать URL-фреймы из временного файла (предпочтительно) или базового файла
    if (hTemp != INVALID_HANDLE_VALUE) {
        CopyFrames(hTemp, &db, isV22, TRUE, FALSE);  // Only URL frames / Только URL-фреймы
    } else if (hBase != INVALID_HANDLE_VALUE) {
        CopyFrames(hBase, &db, isV22, TRUE, FALSE);
    }
    
    // Copy unhandled frames from base file (preserves custom frames)
    // Скопировать необработанные фреймы из базового файла (сохраняет пользовательские фреймы)
    if (hBase != INVALID_HANDLE_VALUE) {
        CopyFrames(hBase, &db, isV22, FALSE, TRUE);  // Exclude URL frames / Исключить URL-фреймы
    }
    
    // Finalize header / Завершить заголовок
    DWORD fSz = db.size - 10;  // Tag size excluding header / Размер тега без заголовка
    BYTE* out = db.data;
    
    // Write ID3v2 header / Записать заголовок ID3v2
    memcpy(out, "ID3", 3);
    out[3] = isV22 ? 2 : 3;  // Version / Версия
    out[4] = 0;               // Revision / Ревизия
    out[5] = 0;               // Flags / Флаги
    
    // Write tag size as syncsafe integer (4 bytes, 7 bits each)
    // Записать размер тега как syncsafe целое (4 байта, по 7 бит каждый)
    DWORD ss = DECRYPT_SizeToSyncsafe(fSz);
    out[6] = (BYTE)((ss >> 24) & 0x7F);
    out[7] = (BYTE)((ss >> 16) & 0x7F);
    out[8] = (BYTE)((ss >> 8) & 0x7F);
    out[9] = (BYTE)(ss & 0x7F);
    
    if (pNewSz) *pNewSz = db.size;
    return db.data;
}

/*******************************************************************************
 * TAG SAVE HANDLER
 * ОБРАБОТЧИК СОХРАНЕНИЯ ТЕГОВ
 ******************************************************************************/

/*******************************************************************************
 * HandleTagSave
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Main handler for tag save operation. Rebuilds ID3v2 tag with proper encoding.
 * Главный обработчик операции сохранения тегов. Перестраивает ID3v2 тег с правильной кодировкой.
 * 
 * PARAMETERS / ПАРАМЕТРЫ:
 * tempPath  - Path to temporary file with edited tags / Путь к временному файлу с отредактированными тегами
 * finalPath - Path to final MP3 file / Путь к финальному MP3 файлу
 * 
 * RETURNS / ВОЗВРАЩАЕТ:
 * TRUE if save successful, FALSE on error
 * TRUE если сохранение успешно, FALSE при ошибке
 * 
 * ALGORITHM / АЛГОРИТМ:
 * 1. Read all field values from temporary file
 * 2. Find base file (backup or original)
 * 3. Detect ID3v2 version from base file
 * 4. Build new tag using BuildID3_Universal
 * 5. Write new tag + audio data to temporary output file
 * 6. Atomically replace original file with new file
 * 7. Clean up temporary and backup files
 * 
 * 1. Прочитать все значения полей из временного файла
 * 2. Найти базовый файл (резервная копия или оригинал)
 * 3. Определить версию ID3v2 из базового файла
 * 4. Построить новый тег используя BuildID3_Universal
 * 5. Записать новый тег + аудио данные во временный выходной файл
 * 6. Атомарно заменить оригинальный файл новым файлом
 * 7. Очистить временные файлы и резервные копии
 * 
 * NOTES / ПРИМЕЧАНИЯ:
 * Uses atomic file operations to prevent corruption on crash.
 * Preserves original tag version (ID3v2.2 vs ID3v2.3).
 * 
 * Использует атомарные файловые операции для предотвращения повреждения при краше.
 * Сохраняет оригинальную версию тега (ID3v2.2 против ID3v2.3).
 ******************************************************************************/
static BOOL HandleTagSave(const char* tempPath, const char* finalPath) {
    /***************************************************************************
     * STEP 1: READ EDITED FIELDS FROM TEMP FILE
     * ШАГ 1: ПРОЧИТАТЬ ОТРЕДАКТИРОВАННЫЕ ПОЛЯ ИЗ ВРЕМЕННОГО ФАЙЛА
     ***************************************************************************/
    
    // Allocate buffers for all fields / Выделить буферы для всех полей
    WCHAR wA[WBUF_MAX] = {0},    // Artist / Исполнитель
          wT[WBUF_MAX] = {0},    // Title / Название
          wAlb[WBUF_MAX] = {0},  // Album / Альбом
          wY[WBUF_MAX] = {0},    // Year / Год
          wG[WBUF_MAX] = {0},    // Genre / Жанр
          wTrk[WBUF_MAX] = {0},  // Track / Трек
          wC[WBUF_MAX] = {0},    // Composer / Композитор
          wM[WBUF_MAX] = {0},    // Comment / Комментарий
          wO[WBUF_MAX] = {0},    // Original Artist / Оригинальный исполнитель
          wP[WBUF_MAX] = {0},    // Copyright / Авторские права
          wU[WBUF_MAX] = {0},    // (unused)
          wE[WBUF_MAX] = {0};    // Encoder / Кодировщик
    
    // Open temp file / Открыть временный файл
    HANDLE hTemp = CreateFileA(tempPath, GENERIC_READ, 7, 0, OPEN_EXISTING, 0, 0);
    if (hTemp == INVALID_HANDLE_VALUE) return FALSE;
    
    // Read all fields / Прочитать все поля
    DWORD dum;
    ID3_ReadAllFieldsW(hTemp, wA, WBUF_MAX, wT, WBUF_MAX, wAlb, WBUF_MAX, wY, WBUF_MAX, 
                       wG, WBUF_MAX, wTrk, WBUF_MAX, wC, WBUF_MAX, wM, WBUF_MAX, 
                       wO, WBUF_MAX, wP, WBUF_MAX, wU, WBUF_MAX, wE, WBUF_MAX, &dum);
    
    /***************************************************************************
     * STEP 2: FIND BASE FILE
     * ШАГ 2: НАЙТИ БАЗОВЫЙ ФАЙЛ
     ***************************************************************************/
    
    char bakPath[MAX_PATH] = {0};
    const char* basePath = NULL;
    
    // Check if we have a backup file for this path
    // Проверить, есть ли резервная копия для этого пути
    Bak_Lock();
    if (Bak_Peek(finalPath, bakPath)) {
        if (FileExistsA_(bakPath)) {
            basePath = bakPath;
        }
    }
    Bak_Unlock();
    
    // If no backup, use original file / Если нет резервной копии, использовать оригинальный файл
    if (!basePath && FileExistsA_(finalPath)) {
        basePath = finalPath;
    }
    
    // No base file found - abort / Базовый файл не найден - прервать
    if (!basePath) {
        CloseHandle(hTemp);
        return FALSE;
    }
    
    // Open base file / Открыть базовый файл
    HANDLE hBase = CreateFileA(basePath, GENERIC_READ, 7, 0, OPEN_EXISTING, 0, 0);
    if (hBase == INVALID_HANDLE_VALUE) {
        CloseHandle(hTemp);
        return FALSE;
    }
    
    /***************************************************************************
     * STEP 3: DETECT ID3V2 VERSION
     * ШАГ 3: ОПРЕДЕЛИТЬ ВЕРСИЮ ID3V2
     ***************************************************************************/
    
    int ver = 3;  // Default to ID3v2.3 / По умолчанию ID3v2.3
    BYTE h3[10];
    
    SetFilePointer(hBase, 0, 0, FILE_BEGIN);
    if (ReadFile(hBase, h3, 10, &dum, 0) && memcmp(h3, "ID3", 3) == 0) {
        ver = h3[3];  // Extract version from header / Извлечь версию из заголовка
    }
    SetFilePointer(hBase, 0, 0, FILE_BEGIN);

    /***************************************************************************
     * STEP 4: BUILD NEW TAG
     * ШАГ 4: ПОСТРОИТЬ НОВЫЙ ТЕГ
     ***************************************************************************/
    
    DWORD newSz;
    BYTE* newTag = BuildID3_Universal(hBase, hTemp, wA, wT, wAlb, wY, wG, wTrk, wC, wM, wO, wP, wE, &newSz, (ver == 2));
    
    /***************************************************************************
     * STEP 5: WRITE NEW FILE
     * ШАГ 5: ЗАПИСАТЬ НОВЫЙ ФАЙЛ
     ***************************************************************************/
    
    // Get output directory / Получить выходной каталог
    char outPath[MAX_PATH];
    PathGetDirA_(finalPath, outPath, MAX_PATH);
    
    // Create temporary output file / Создать временный выходной файл
    char tmpF[MAX_PATH];
    GetTempFileNameA(outPath, "sv", 0, tmpF);
    
    HANDLE hOut = CreateFileA(tmpF, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, 0, 0);
    if (hOut != INVALID_HANDLE_VALUE && newTag) {
        // Write new tag / Записать новый тег
        WriteFile(hOut, newTag, newSz, &dum, 0);
        
        // Copy audio data from base file / Скопировать аудио данные из базового файла
        DWORD oldSz;
        ID3_ReadAllFieldsW(hBase, NULL, 0, NULL, 0, NULL, 0, NULL, 0, NULL, 0, 
                          NULL, 0, NULL, 0, NULL, 0, NULL, 0, NULL, 0, NULL, 0, 
                          NULL, 0, &oldSz);
        
        // Position after old tag / Позиционировать после старого тега
        SetFilePointer(hBase, oldSz, 0, FILE_BEGIN);
        
        // Copy audio in chunks / Копировать аудио по частям
        BYTE cp[4096];
        while (ReadFile(hBase, cp, 4096, &dum, 0) && dum > 0) {
            WriteFile(hOut, cp, dum, &dum, 0);
        }
        
        CloseHandle(hOut);
        
        /***************************************************************************
         * STEP 6: ATOMIC FILE SWAP
         * ШАГ 6: АТОМАРНАЯ ЗАМЕНА ФАЙЛА
         ***************************************************************************/
        
        // Delete original and move new file in place
        // Удалить оригинал и переместить новый файл на место
        DeleteFileA(finalPath);
        MoveFileA(tmpF, finalPath);
        
        // Clean up backup if it exists / Очистить резервную копию если существует
        if (bakPath[0]) {
            DeleteFileA(bakPath);
        }
    }
    
    /***************************************************************************
     * CLEANUP
     * ОЧИСТКА
     ***************************************************************************/
    
    if (newTag) HeapFree(GetProcessHeap(), 0, newTag);
    CloseHandle(hBase);
    CloseHandle(hTemp);
    DeleteFileA(tempPath);
    
    return TRUE;
}

/*******************************************************************************
 * FILE OPERATION HOOKS
 * ХУКИ ФАЙЛОВЫХ ОПЕРАЦИЙ
 ******************************************************************************/

// Function pointer types for hooked functions
// Типы указателей функций для перехваченных функций
typedef BOOL (WINAPI *PFN_MoveFileA)(LPCSTR, LPCSTR);
typedef BOOL (WINAPI *PFN_MoveFileExA)(LPCSTR, LPCSTR, DWORD);

// Original function pointers (saved during patching)
// Указатели на оригинальные функции (сохранённые во время патча)
static PFN_MoveFileA Orig_MoveFileA = NULL;
static PFN_MoveFileExA Orig_MoveFileExA = NULL;

// Real function pointers (may be hooked by other code)
// Реальные указатели функций (могут быть перехвачены другим кодом)
static PFN_MoveFileA Real_MoveFileA = NULL;
static PFN_MoveFileExA Real_MoveFileExA = NULL;

/*******************************************************************************
 * Hook_MoveFileA
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Hook for MoveFileA to intercept MP3 tag save operations.
 * Detects when Winamp saves tags and rebuilds them with proper encoding.
 * 
 * Хук для MoveFileA для перехвата операций сохранения MP3 тегов.
 * Определяет, когда Winamp сохраняет теги и перестраивает их с правильной кодировкой.
 * 
 * PARAMETERS / ПАРАМЕТРЫ:
 * src - Source file path / Путь к исходному файлу
 * dst - Destination file path / Путь к файлу назначения
 * 
 * RETURNS / ВОЗВРАЩАЕТ:
 * TRUE if file moved successfully, FALSE otherwise
 * TRUE если файл перемещён успешно, FALSE иначе
 * 
 * ALGORITHM / АЛГОРИТМ:
 * 1. Check if hooks are ready
 * 2. Detect backup scenario: MP3 -> non-MP3 (store in backup map)
 * 3. Detect save scenario: temp -> MP3 (process tag save)
 * 4. If tag save detected, rebuild tag and return
 * 5. Otherwise, call original MoveFileA
 * 
 * 1. Проверить, готовы ли хуки
 * 2. Определить сценарий резервного копирования: MP3 -> не-MP3 (сохранить в карте резервных копий)
 * 3. Определить сценарий сохранения: временный -> MP3 (обработать сохранение тегов)
 * 4. Если обнаружено сохранение тегов, перестроить тег и вернуться
 * 5. Иначе, вызвать оригинальную MoveFileA
 * 
 * NOTES / ПРИМЕЧАНИЯ:
 * This hook is what enables the entire tag fix functionality.
 * It intercepts Winamp's tag save process and rebuilds tags correctly.
 * 
 * Этот хук - то, что обеспечивает всю функциональность исправления тегов.
 * Он перехватывает процесс сохранения тегов Winamp и перестраивает теги правильно.
 ******************************************************************************/
BOOL WINAPI Hook_MoveFileA(LPCSTR src, LPCSTR dst) {
    // Get real function pointer / Получить реальный указатель функции
    PFN_MoveFileA p = Real_MoveFileA;
    
    // If hooks not ready, use fallback / Если хуки не готовы, использовать запасной вариант
    if (!p || g_hooksReady != 1) {
        return Orig_MoveFileA ? Orig_MoveFileA(src, dst) : FALSE;
    }
    
    if (src && dst) {
        /***************************************************************************
         * BACKUP DETECTION
         * ОПРЕДЕЛЕНИЕ РЕЗЕРВНОГО КОПИРОВАНИЯ
         * 
         * When Winamp starts tag edit, it moves:
         * Когда Winamp начинает редактирование тегов, он перемещает:
         *   original.mp3 -> temp_backup_file
         ***************************************************************************/
        if (HasExtI_A(src, ".mp3") && !HasExtI_A(dst, ".mp3")) {
            // This is backup operation - store in map
            // Это операция резервного копирования - сохранить в карте
            Bak_Lock();
            Bak_Set(src, dst);
            Bak_Unlock();
        }
        
        /***************************************************************************
         * SAVE DETECTION
         * ОПРЕДЕЛЕНИЕ СОХРАНЕНИЯ
         * 
         * When Winamp finishes tag edit, it moves:
         * Когда Winamp завершает редактирование тегов, он перемещает:
         *   temp_edited_file -> original.mp3
         ***************************************************************************/
        if (HasExtI_A(dst, ".mp3") && lstrcmpiA(src, dst) != 0) {
            char bak[MAX_PATH] = {0};
            BOOL isBak = FALSE;
            
            // Check if we have backup for this file / Проверить, есть ли резервная копия для этого файла
            Bak_Lock();
            isBak = Bak_Peek(dst, bak);
            Bak_Unlock();
            
            // If we have backup and source is ID3v2 file, process tag save
            // Если есть резервная копия и источник - файл ID3v2, обработать сохранение тегов
            if (isBak && LooksLikeID3v2FileA_(src)) {
                if (HandleTagSave(src, dst)) {
                    return TRUE;  // We handled it - don't call original / Мы обработали - не вызывать оригинал
                }
            }
        }
    }
    
    // Default: call original function / По умолчанию: вызвать оригинальную функцию
    return p(src, dst);
}

/*******************************************************************************
 * Hook_MoveFileExA
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Hook for MoveFileExA. Currently just wraps Hook_MoveFileA.
 * Хук для MoveFileExA. В настоящее время просто обёртка для Hook_MoveFileA.
 * 
 * PARAMETERS / ПАРАМЕТРЫ:
 * src - Source file path / Путь к исходному файлу
 * dst - Destination file path / Путь к файлу назначения
 * f   - Move flags (currently ignored) / Флаги перемещения (в настоящее время игнорируются)
 * 
 * RETURNS / ВОЗВРАЩАЕТ:
 * TRUE if file moved successfully, FALSE otherwise
 * TRUE если файл перемещён успешно, FALSE иначе
 ******************************************************************************/
BOOL WINAPI Hook_MoveFileExA(LPCSTR src, LPCSTR dst, DWORD f) {
    return Hook_MoveFileA(src, dst);  // Simple wrapper / Простая обёртка
}

/*******************************************************************************
 * INITIALIZATION AND EXPORTS
 * ИНИЦИАЛИЗАЦИЯ И ЭКСПОРТЫ
 ******************************************************************************/

/*******************************************************************************
 * SaveThread
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Background thread that waits for in_mp3.dll to load, then patches it.
 * Фоновый поток, который ожидает загрузки in_mp3.dll, затем патчит его.
 * 
 * ALGORITHM / АЛГОРИТМ:
 * 1. Wait for in_mp3.dll to load (up to 10 seconds)
 * 2. Wait additional 500ms for stabilization
 * 3. Get kernel32 function addresses
 * 4. Patch in_mp3.dll's IAT to hook MoveFileA/MoveFileExA
 * 5. Mark hooks as ready
 * 
 * 1. Ждать загрузки in_mp3.dll (до 10 секунд)
 * 2. Ждать дополнительные 500мс для стабилизации
 * 3. Получить адреса функций kernel32
 * 4. Пропатчить IAT in_mp3.dll для перехвата MoveFileA/MoveFileExA
 * 5. Отметить хуки как готовые
 * 
 * RETURNS / ВОЗВРАЩАЕТ:
 * 0 on completion (return value unused)
 * 0 при завершении (возвращаемое значение не используется)
 * 
 * NOTES / ПРИМЕЧАНИЯ:
 * Runs in separate thread to avoid blocking plugin initialization.
 * Uses polling to detect in_mp3.dll load.
 * 
 * Работает в отдельном потоке, чтобы избежать блокировки инициализации плагина.
 * Использует опрос для определения загрузки in_mp3.dll.
 ******************************************************************************/
static unsigned __stdcall SaveThread(void*) {
    // quick exit if stop requested
    if (g_hStopEvent && WaitForSingleObject(g_hStopEvent, 0) == WAIT_OBJECT_0)
        return 0;

    HMODULE h = NULL;

    // wait up to ~10s for in_mp3.dll, but cancellable
    for (int i = 0; i < 100; i++) {
        if (g_hStopEvent && WaitForSingleObject(g_hStopEvent, 0) == WAIT_OBJECT_0)
            return 0;

        h = GetModuleHandleA("in_mp3.dll");
        if (h) break;
        Sleep(100);
    }
    if (!h) return 0;

    // stabilization delay (cancellable)
    for (int j = 0; j < 60; j++) {
        if (g_hStopEvent && WaitForSingleObject(g_hStopEvent, 0) == WAIT_OBJECT_0)
            return 0;
        Sleep(10);
    }

    HMODULE k = GetModuleHandleA("KERNEL32.dll");
    if (!k) return 0;

    Orig_MoveFileA   = (PFN_MoveFileA)  GetProcAddress(k, "MoveFileA");
    Orig_MoveFileExA = (PFN_MoveFileExA)GetProcAddress(k, "MoveFileExA");
    if (!Orig_MoveFileA || !Orig_MoveFileExA) return 0;

    Real_MoveFileA   = Orig_MoveFileA;
    Real_MoveFileExA = Orig_MoveFileExA;

    // patch IAT; allow "at least one succeeded"
    BOOL ok1 = IAT_Patch(h, "KERNEL32.dll", "MoveFileA",   (void*)Hook_MoveFileA,   (void**)&Real_MoveFileA);
    BOOL ok2 = IAT_Patch(h, "KERNEL32.dll", "MoveFileExA", (void*)Hook_MoveFileExA, (void**)&Real_MoveFileExA);

    if (ok1 || ok2) {
        InterlockedExchange(&g_hooksReady, 1);
    } else {
        InterlockedExchange(&g_hooksReady, 0);
    }
    return 0;
}

/*******************************************************************************
 * MP3_SaveFix_Init
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Initializes the MP3 save fix module by starting background thread.
 * Инициализирует модуль исправления сохранения MP3, запуская фоновый поток.
 * 
 * NOTES / ПРИМЕЧАНИЯ:
 * Safe to call multiple times - only initializes once.
 * Background thread handles actual hooking after delay.
 * 
 * Безопасно вызывать много раз - инициализирует только один раз.
 * Фоновый поток обрабатывает фактический перехват после задержки.
 ******************************************************************************/
void MP3_SaveFix_Init(void) {
    static BOOL inited = FALSE;
    if (inited) return;
    inited = TRUE;

    InterlockedExchange(&g_hooksReady, 0);

    if (!g_hStopEvent) {
        g_hStopEvent = CreateEvent(NULL, TRUE, FALSE, NULL); // manual reset
        if (!g_hStopEvent) {
            inited = FALSE;
            return;
        }
    } else {
        ResetEvent(g_hStopEvent);
    }

    unsigned threadID = 0;
    g_hThread = (HANDLE)_beginthreadex(NULL, 0, SaveThread, NULL, 0, &threadID);
    if (!g_hThread) {
        // allow retry
        inited = FALSE;
        SetEvent(g_hStopEvent);
        return;
    }
}


/*******************************************************************************
 * MP3_SaveFix_Quit
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Cleans up the MP3 save fix module by marking hooks as inactive.
 * Очищает модуль исправления сохранения MP3, отмечая хуки как неактивные.
 * 
 * NOTES / ПРИМЕЧАНИЯ:
 * Does not unhook functions - just disables hook logic.
 * This prevents issues if in_mp3.dll is still loaded.
 * 
 * Не отцепляет функции - просто отключает логику хуков.
 * Это предотвращает проблемы, если in_mp3.dll всё ещё загружен.
 ******************************************************************************/
void MP3_SaveFix_Quit(void) { 
    // 1. Сообщаем потоку, что пора закругляться (опционально, можно добавить глобальный флаг g_stopThread)
    InterlockedExchange(&g_hooksReady, 0);
    
    // 2. Ждем завершения потока, если он жив
    if (g_hThread) {
        // Ждем максимум 2 секунды, чтобы не зависнуть намертво
        if (WaitForSingleObject(g_hThread, 2000) == WAIT_TIMEOUT) {
            // Если поток завис - принудительно убиваем (нежелательно, но лучше, чем краш)
            TerminateThread(g_hThread, 0); 
        }
        CloseHandle(g_hThread);
        g_hThread = NULL;
    }
}