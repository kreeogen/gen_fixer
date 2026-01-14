/*******************************************************************************
 * mod_unicode_tags_fix.cpp
 * 
 * WINAMP MP3 ID3 TAGS UNICODE/MOJIBAKE FIX MODULE WITH VIRTUAL FILE OVERLAY
 * МОДУЛЬ ИСПРАВЛЕНИЯ Unicode/mojibake В ID3-ТЕГАХ MP3 С ВИРТУАЛЬНЫМ НАЛОЖЕНИЕМ
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * This module fixes character encoding issues (mojibake) in MP3 ID3 tags by
 * creating a virtual overlay of the file with corrected tags. When Winamp's
 * in_mp3.dll plugin reads an MP3 file, it sees a modified version with fixed
 * encoding without actually modifying the file on disk until tags are saved.
 * 
 * Данный модуль исправляет проблемы кодировки символов (mojibake) в ID3-тегах
 * MP3, создавая виртуальное наложение файла с исправленными тегами. Когда
 * плагин in_mp3.dll Winamp читает MP3-файл, он видит модифицированную версию
 * с исправленной кодировкой без фактической модификации файла на диске до
 * сохранения тегов.
 * 
 * HOW IT WORKS / КАК ЭТО РАБОТАЕТ:
 * 
 * PHASE 1 - FILE OPEN / ФАЗА 1 - ОТКРЫТИЕ ФАЙЛА:
 * When in_mp3.dll opens an MP3 file, we intercept CreateFileA, read the
 * original ID3 tag, build a corrected version in memory, and create a virtual
 * overlay context.
 * 
 * Когда in_mp3.dll открывает MP3-файл, мы перехватываем CreateFileA, читаем
 * оригинальный ID3-тег, строим исправленную версию в памяти и создаём контекст
 * виртуального наложения.
 * 
 * PHASE 2 - FILE READ / ФАЗА 2 - ЧТЕНИЕ ФАЙЛА:
 * When in_mp3.dll reads from the file, we serve the corrected tag from memory
 * for tag area reads, and real audio data for audio area reads.
 * 
 * Когда in_mp3.dll читает из файла, мы возвращаем исправленный тег из памяти
 * для чтения области тега и реальные аудио данные для чтения области аудио.
 * 
 * PHASE 3 - TAG SAVE / ФАЗА 3 - СОХРАНЕНИЕ ТЕГОВ:
 * When user edits tags, we intercept the file move operation, combine edited
 * fields from temp file with preserved frames from original, and write the
 * final MP3.
 * 
 * Когда пользователь редактирует теги, мы перехватываем операцию перемещения,
 * объединяем отредактированные поля из временного файла с сохранёнными фреймами
 * из оригинала и записываем финальный MP3.
 * 
 ******************************************************************************/

// Minimize Windows header inclusion for faster compilation
// Минимизируем включение заголовков Windows для быстрой компиляции
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <process.h>    // For _beginthreadex / Для _beginthreadex
#include <stdio.h>      // For sprintf / Для sprintf
#include <stdarg.h>     // For variadic functions / Для вариативных функций
#include <string.h>     // For string operations / Для строковых операций

// Custom unicode decryption engine - contains encoding fix algorithms
// Собственный движок расшифровки unicode - содержит алгоритмы исправления кодировки
#include "..\\Decrypt Engine\\unicode_decrypt_engine.h"
#include "mod_unicode_tags_fix.h"

/*******************************************************************************
 * CONFIGURATION CONSTANTS / КОНСТАНТЫ КОНФИГУРАЦИИ
 ******************************************************************************/

// Maximum number of simultaneously open MP3 files we can track
// Максимальное количество одновременно открытых MP3-файлов, которые мы можем отслеживать
#define MAX_OPEN_MP3S 32

// Sanity limit for tag size to prevent memory exhaustion (50MB)
// Ограничение размера тега для предотвращения истощения памяти (50МБ)
#define TAG_SANITY_LIMIT (50*1024*1024)

// Buffer sizes for wide character and ANSI strings
// Размеры буферов для широких символов и ANSI-строк
#define WBUF_MAX 2048
#define ABUF_MAX 2048

// Array size macro for compile-time array length calculation
// Макрос размера массива для вычисления длины массива во время компиляции
#ifndef ARRAYSIZE
#define ARRAYSIZE(a) (sizeof(a)/sizeof((a)[0]))
#endif

/*******************************************************************************
 * DYNAMIC BUFFER UTILITIES / УТИЛИТЫ ДИНАМИЧЕСКИХ БУФЕРОВ
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Provides growable byte buffers for building ID3 tags dynamically.
 * Предоставляет расширяемые байтовые буферы для динамического построения ID3-тегов.
 ******************************************************************************/

/*******************************************************************************
 * DynBuf - Dynamic Buffer Structure / Структура динамического буфера
 * 
 * FIELDS / ПОЛЯ:
 * data     - Pointer to buffer memory / Указатель на память буфера
 * size     - Current used size in bytes / Текущий использованный размер в байтах
 * capacity - Total allocated capacity / Общая выделенная ёмкость
 ******************************************************************************/
typedef struct { 
    BYTE* data;      // Buffer memory / Память буфера
    DWORD size;      // Used bytes / Использованные байты
    DWORD capacity;  // Allocated bytes / Выделенные байты
} DynBuf;

/*******************************************************************************
 * DB_Init - Initialize Dynamic Buffer / Инициализация динамического буфера
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Initializes a dynamic buffer with specified initial capacity.
 * Инициализирует динамический буфер с указанной начальной ёмкостью.
 * 
 * PARAMETERS / ПАРАМЕТРЫ:
 * d       - Pointer to DynBuf structure / Указатель на структуру DynBuf
 * initCap - Initial capacity (0 = default 256) / Начальная ёмкость (0 = по умолчанию 256)
 * 
 * RETURNS / ВОЗВРАЩАЕТ:
 * TRUE on success / TRUE при успехе
 * FALSE on failure / FALSE при неудаче
 ******************************************************************************/
static BOOL DB_Init(DynBuf* d, DWORD initCap) {
    if (!d) return FALSE;  // Validate parameter / Проверка параметра
    
    // Initialize structure / Инициализация структуры
    d->data = NULL; 
    d->size = 0; 
    d->capacity = 0;
    
    // Set default capacity if not specified / Установка ёмкости по умолчанию, если не указана
    if (initCap == 0) initCap = 256;
    
    // Sanity check / Проверка разумности
    if (initCap > TAG_SANITY_LIMIT) return FALSE;
    
    // Allocate initial buffer / Выделение начального буфера
    d->data = (BYTE*)HeapAlloc(GetProcessHeap(), 0, initCap);
    if (!d->data) return FALSE;
    
    d->capacity = initCap;
    return TRUE;
}

/*******************************************************************************
 * DB_Free - Free Dynamic Buffer / Освобождение динамического буфера
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Frees a dynamic buffer and resets its state.
 * Освобождает динамический буфер и сбрасывает его состояние.
 ******************************************************************************/
static void DB_Free(DynBuf* d) {
    // Free allocated memory / Освобождение выделенной памяти
    if (d->data) 
        HeapFree(GetProcessHeap(), 0, d->data);
    
    // Reset structure / Сброс структуры
    d->data = NULL; 
    d->size = 0; 
    d->capacity = 0;
}

/*******************************************************************************
 * DB_Append - Append Data to Dynamic Buffer / Добавление данных в динамический буфер
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Appends data to buffer, growing it if necessary.
 * Добавляет данные в буфер, расширяя его при необходимости.
 * 
 * PARAMETERS / ПАРАМЕТРЫ:
 * d   - Dynamic buffer / Динамический буфер
 * src - Source data / Исходные данные
 * len - Length in bytes / Длина в байтах
 * 
 * RETURNS / ВОЗВРАЩАЕТ:
 * TRUE on success / TRUE при успехе
 * FALSE on failure / FALSE при неудаче
 * 
 * GROWTH STRATEGY / СТРАТЕГИЯ РОСТА:
 * Doubles capacity until reaching required size.
 * Удваивает ёмкость до достижения требуемого размера.
 ******************************************************************************/
static BOOL DB_Append(DynBuf* d, const void* src, DWORD len) {
    if (!d || !d->data) return FALSE;  // Validate parameters / Проверка параметров
    if (len == 0) return TRUE;  // Nothing to append / Нечего добавлять

    // Prevent overflow and unbounded growth
    // Предотвращение переполнения и неограниченного роста
    if (d->size > TAG_SANITY_LIMIT) return FALSE;
    if (len > TAG_SANITY_LIMIT - d->size) return FALSE; 
    
    DWORD need = d->size + len;  // Calculate required size / Вычисление требуемого размера

    // Grow buffer if needed / Расширение буфера при необходимости
    if (need > d->capacity) {
        // Start with current capacity or 256 / Начать с текущей ёмкости или 256
        DWORD newCap = d->capacity ? d->capacity : 256;
        
        // Double capacity until we reach needed size
        // Удваивать ёмкость до достижения нужного размера
        while (newCap < need) {
            DWORD next = newCap << 1;  // Double / Удвоить
            if (next <= newCap) {      // Overflow check / Проверка переполнения
                newCap = need; 
                break; 
            }
            newCap = next;
        }
        
        // Apply sanity limit / Применение ограничения
        if (newCap > TAG_SANITY_LIMIT) newCap = need;
        if (newCap > TAG_SANITY_LIMIT) return FALSE;

        // Reallocate buffer / Перевыделение буфера
        BYTE* newData = (BYTE*)HeapReAlloc(GetProcessHeap(), 0, d->data, newCap);
        if (!newData) return FALSE;
        
        d->data = newData; 
        d->capacity = newCap;
    }

    // Copy data and update size / Копирование данных и обновление размера
    memcpy(d->data + d->size, src, len);
    d->size = need;
    return TRUE;
}

/*******************************************************************************
 * DB_AppendChar - Append Single Character / Добавление одного символа
 * 
 * Convenience function to append a single character.
 * Удобная функция для добавления одного символа.
 ******************************************************************************/
static BOOL DB_AppendChar(DynBuf* d, char c) { 
    return DB_Append(d, &c, 1); 
}

/*******************************************************************************
 * CORE DATA STRUCTURES / ОСНОВНЫЕ СТРУКТУРЫ ДАННЫХ
 ******************************************************************************/

/*******************************************************************************
 * OverlayCtx - Virtual Overlay Context / Контекст виртуального наложения
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Represents a virtual overlay context for an open MP3 file. Associates a
 * file handle with a corrected in-memory ID3 tag.
 * 
 * Представляет контекст виртуального наложения для открытого MP3-файла.
 * Ассоциирует файловый дескриптор с исправленным ID3-тегом в памяти.
 * 
 * FIELDS / ПОЛЯ:
 * hReal      - File handle seen by in_mp3.dll / Дескриптор, видимый in_mp3.dll
 * hKeep      - Duplicate handle for physical I/O / Дублирующий дескриптор для физического I/O
 * refs       - Reference count (for thread safety) / Счётчик ссылок (для потокобезопасности)
 * closing    - Closing flag / Флаг закрытия
 * newTag     - Corrected ID3v2 tag in memory / Исправленный ID3v2 тег в памяти
 * newTagSz   - Size of corrected tag / Размер исправленного тега
 * oldTagSz   - Size of original tag / Размер оригинального тега
 * virtSz     - Virtual file size / Виртуальный размер файла
 * logicalPos - Current logical read position / Текущая логическая позиция чтения
 * pathA      - File path / Путь к файлу
 * inUse      - Active flag / Флаг активности
 ******************************************************************************/
typedef struct OverlayCtx {
    HANDLE hReal;           // File handle returned to in_mp3.dll / Дескриптор, возвращённый in_mp3.dll
    HANDLE hKeep;           // Duplicate handle for actual I/O / Дублирующий дескриптор для реального I/O
    volatile LONG refs;     // Reference count for thread safety / Счётчик ссылок для потокобезопасности
    BOOL closing;           // Closing flag for deferred cleanup / Флаг закрытия для отложенной очистки
    BYTE* newTag;           // Corrected tag buffer / Буфер исправленного тега
    DWORD newTagSz;         // Corrected tag size / Размер исправленного тега
    DWORD oldTagSz;         // Original tag size / Размер оригинального тега
    DWORD virtSz;           // Virtual file size / Виртуальный размер файла
    DWORD logicalPos;       // Current logical position / Текущая логическая позиция
    char pathA[MAX_PATH];   // File path / Путь к файлу
    BOOL inUse;             // Active flag / Флаг активности
} OverlayCtx;

/*******************************************************************************
 * BackupMap - Backup File Mapping / Отображение резервного файла
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Maps original MP3 file paths to their backup paths. When in_mp3.dll creates
 * a backup before editing, we track it here.
 * 
 * Отображает пути оригинальных MP3-файлов на пути их резервных копий.
 * Когда in_mp3.dll создаёт резервную копию перед редактированием, мы
 * отслеживаем это здесь.
 * 
 * FIELDS / ПОЛЯ:
 * orig  - Original MP3 path / Путь оригинального MP3
 * bak   - Backup path / Путь резервной копии
 * tick  - Creation timestamp (for expiration) / Временная метка создания (для истечения)
 * inUse - Active flag / Флаг активности
 ******************************************************************************/
typedef struct BackupMap {
    char orig[MAX_PATH];  // Original MP3 path / Путь оригинального MP3
    char bak[MAX_PATH];   // Backup path / Путь резервной копии
    DWORD tick;           // Creation timestamp / Временная метка создания
    BOOL inUse;           // Active flag / Флаг активности
} BackupMap;

/*******************************************************************************
 * GLOBAL STATE VARIABLES / ГЛОБАЛЬНЫЕ ПЕРЕМЕННЫЕ СОСТОЯНИЯ
 ******************************************************************************/

// Pool of overlay contexts - fixed size array for efficiency
// Пул контекстов наложения - массив фиксированного размера для эффективности
static OverlayCtx g_ctxPool[MAX_OPEN_MP3S];

// Pool of backup mappings - matches overlay pool size
// Пул отображений резервных копий - соответствует размеру пула наложений
static BackupMap  g_bakMap[MAX_OPEN_MP3S];

// Critical section for thread-safe access to pools
// Критическая секция для потокобезопасного доступа к пулам
static CRITICAL_SECTION g_cs;

// Initialization state flags (atomic access only)
// Флаги состояния инициализации (только атомарный доступ)
static volatile LONG g_csInited = 0;    // 0=not inited, 1=initing, 2=ready
static volatile LONG g_hooksReady = 0;  // 0=not ready, 1=ready

// Handle to in_mp3.dll module (set by HookThread)
// Дескриптор модуля in_mp3.dll (устанавливается HookThread)
static HMODULE g_hInMp3 = NULL;

/*******************************************************************************
 * FUNCTION POINTER TYPEDEFS / ТИПЫ УКАЗАТЕЛЕЙ НА ФУНКЦИИ
 * 
 * These define the signatures of Windows API functions we will intercept.
 * Они определяют сигнатуры функций Windows API, которые мы будем перехватывать.
 ******************************************************************************/

// File I/O functions / Функции файлового ввода-вывода
typedef HANDLE (WINAPI *PFN_CreateFileA)(LPCSTR, DWORD, DWORD, LPSECURITY_ATTRIBUTES, DWORD, DWORD, HANDLE);
typedef BOOL   (WINAPI *PFN_ReadFile)(HANDLE, LPVOID, DWORD, LPDWORD, LPOVERLAPPED);
typedef DWORD  (WINAPI *PFN_SetFilePointer)(HANDLE, LONG, PLONG, DWORD);
typedef DWORD  (WINAPI *PFN_GetFileSize)(HANDLE, LPDWORD);
typedef BOOL   (WINAPI *PFN_CloseHandle)(HANDLE);

// File operation functions / Функции файловых операций
typedef BOOL   (WINAPI *PFN_MoveFileA)(LPCSTR, LPCSTR);
typedef BOOL   (WINAPI *PFN_MoveFileExA)(LPCSTR, LPCSTR, DWORD);

/*******************************************************************************
 * ORIGINAL FUNCTION POINTERS / УКАЗАТЕЛИ НА ОРИГИНАЛЬНЫЕ ФУНКЦИИ
 * 
 * These point to the original Windows API functions in KERNEL32.dll.
 * Они указывают на оригинальные функции Windows API в KERNEL32.dll.
 ******************************************************************************/

static PFN_CreateFileA    Orig_CreateFileA = NULL;
static PFN_ReadFile       Orig_ReadFile = NULL;
static PFN_SetFilePointer Orig_SetFilePointer = NULL;
static PFN_GetFileSize    Orig_GetFileSize = NULL;
static PFN_CloseHandle    Orig_CloseHandle = NULL;
static PFN_MoveFileA      Orig_MoveFileA = NULL;
static PFN_MoveFileExA    Orig_MoveFileExA = NULL;

/*******************************************************************************
 * REAL FUNCTION POINTERS / УКАЗАТЕЛИ НА РЕАЛЬНЫЕ ФУНКЦИИ
 * 
 * These are updated by IAT patching to point back to originals after our hooks.
 * Они обновляются IAT-патчингом для указания обратно на оригиналы после наших перехватов.
 ******************************************************************************/

static PFN_CreateFileA    Real_CreateFileA = NULL;
static PFN_ReadFile       Real_ReadFile = NULL;
static PFN_SetFilePointer Real_SetFilePointer = NULL;
static PFN_GetFileSize    Real_GetFileSize = NULL;
static PFN_CloseHandle    Real_CloseHandle = NULL;
static PFN_MoveFileA      Real_MoveFileA = NULL;
static PFN_MoveFileExA    Real_MoveFileExA = NULL;

/*******************************************************************************
 * FORWARD DECLARATIONS / ОПЕРЕЖАЮЩИЕ ОБЪЯВЛЕНИЯ
 ******************************************************************************/static void BakMap_Set_NoLock(const char* orig, const char* bak);
static BOOL BakMap_Peek_NoLock(const char* orig, char* outBak, int cchOut);
static BOOL HandleTagSave_MoveTempIntoMp3_(const char* tempPath, const char* finalMp3Path);
static void AttachVirtualContext(HANDLE hFile, const char* pathA);
static void FreeCtxStruct_NoLock(OverlayCtx* c);
static OverlayCtx* FindCtxByHandle_NoLock(HANDLE h);
static void Ctx_Lock(void);
static void Ctx_Unlock(void);

/*******************************************************************************
 * HELPER FUNCTIONS / ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ
 ******************************************************************************/

/*******************************************************************************
 * HasExtI_A - Case-Insensitive Extension Check
 *             Проверка расширения без учёта регистра
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Checks if a file path has a specific extension (case-insensitive).
 * Проверяет, имеет ли путь к файлу определённое расширение (без учёта регистра).
 * 
 * PARAMETERS / ПАРАМЕТРЫ:
 * path        - File path / Путь к файлу
 * extLowerDot - Extension with dot, lowercase / Расширение с точкой, строчное
 * 
 * RETURNS / ВОЗВРАЩАЕТ:
 * TRUE if extension matches / TRUE если расширение совпадает
 * FALSE otherwise / FALSE в противном случае
 ******************************************************************************/
static BOOL HasExtI_A(const char* path, const char* extLowerDot) {
    if (!path || !extLowerDot) return FALSE;
    
    // Find last dot in path / Найти последнюю точку в пути
    const char* p = strrchr(path, '.');
    if (!p) return FALSE;
    
    // Compare extension (case-insensitive) / Сравнить расширение (без учёта регистра)
    return (lstrcmpiA(p, extLowerDot) == 0);
}

/*******************************************************************************
 * FileExistsA_ - Check File Existence / Проверка существования файла
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Checks if a file exists and is not a directory.
 * Проверяет, существует ли файл и не является ли он директорией.
 * 
 * RETURNS / ВОЗВРАЩАЕТ:
 * TRUE if file exists / TRUE если файл существует
 * FALSE otherwise / FALSE в противном случае
 ******************************************************************************/
static BOOL FileExistsA_(const char* path) {
    DWORD a = GetFileAttributesA(path);
    return (a != INVALID_FILE_ATTRIBUTES) && !(a & FILE_ATTRIBUTE_DIRECTORY);
}

/*******************************************************************************
 * PathGetDirA_ - Extract Directory from Path / Извлечение директории из пути
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Extracts the directory portion from a full file path.
 * Извлекает часть директории из полного пути к файлу.
 * 
 * PARAMETERS / ПАРАМЕТРЫ:
 * path   - Input file path / Входной путь к файлу
 * outDir - Output directory path buffer / Буфер для выходного пути директории
 * cchOut - Size of output buffer / Размер выходного буфера
 * 
 * EXAMPLE / ПРИМЕР:
 * Input: "C:\Music\song.mp3" > Output: "C:\Music\"
 ******************************************************************************/
static void PathGetDirA_(const char* path, char* outDir, int cchOut) {
    if (!outDir || cchOut <= 0) return; 
    outDir[0] = 0; 
    if (!path) return;
    
    // Copy path to output / Копировать путь в вывод
    lstrcpynA(outDir, path, cchOut);
    
    // Find last slash or backslash / Найти последнюю косую черту или обратную косую
    char* s1 = strrchr(outDir, '\\'); 
    char* s2 = strrchr(outDir, '/');
    char* s = (s1 > s2) ? s1 : s2; 
    
    if (s) 
        *(s+1) = 0;  // Keep trailing slash / Сохранить косую черту в конце
    else 
        outDir[0] = 0;
}

/*******************************************************************************
 * LooksLikeID3v2FileA_ - Quick ID3v2 Check / Быстрая проверка ID3v2
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Quickly checks if a file starts with "ID3" signature (ID3v2 tag).
 * Быстро проверяет, начинается ли файл с сигнатуры "ID3" (тег ID3v2).
 * 
 * RETURNS / ВОЗВРАЩАЕТ:
 * TRUE if file has ID3v2 tag / TRUE если файл имеет тег ID3v2
 * FALSE otherwise / FALSE в противном случае
 ******************************************************************************/static BOOL LooksLikeID3v2FileA_(const char* path) {
    if (!path) return FALSE;
    
    HANDLE h = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ|FILE_SHARE_WRITE, 0, OPEN_EXISTING, 0, 0);
    if (h == INVALID_HANDLE_VALUE) return FALSE;
    
    BYTE hdr[3] = {0}; 
    DWORD rd = 0;
    BOOL ok = ReadFile(h, hdr, 3, &rd, 0) && rd == 3 && memcmp(hdr, "ID3", 3) == 0;
    
    CloseHandle(h); 
    return ok;
}

static BOOL ReadID3v1Tail_(HANDLE h, BYTE out128[128]) {
    if (!h) return FALSE;
    
    DWORD sz = GetFileSize(h, NULL);
    if (sz == INVALID_FILE_SIZE || sz < 128) return FALSE;
    
    LONG hi = 0;
    if (SetFilePointer(h, -128, &hi, FILE_END) == (DWORD)-1 && GetLastError() != NO_ERROR) 
        return FALSE;
    
    DWORD rd = 0;
    if (!ReadFile(h, out128, 128, &rd, 0) || rd != 128) 
        return FALSE;
    
    return (memcmp(out128, "TAG", 3) == 0);
}

static void PatchID3v1CommentFromWide_(BYTE v1[128], const WCHAR* wComment) {
    if (!v1 || memcmp(v1, "TAG", 3) != 0 || !wComment) return;
    
    BOOL is11 = (v1[125] == 0); 
    int maxc = is11 ? 28 : 30;  
    
    char a[256]; 
    a[0] = 0;
    int n = DECRYPT_WideToACP(wComment, a, (int)sizeof(a));
    if (n < 0) n = 0; 
    if (n > (int)sizeof(a)) n = (int)sizeof(a);
    
    for (int i = 0; i < n; i++) { 
        if (a[i] == '\r' || a[i] == '\n' || a[i] == '\t') 
            a[i] = ' '; 
    }
    
    BYTE* c = v1 + 97; 
    for (int i = 0; i < maxc; i++) 
        c[i] = 0;
    
    int copy = n; 
    if (copy > maxc) copy = maxc; 
    if (copy > 0) memcpy(c, a, copy);
    
    if (is11) v1[125] = 0;
}

/*******************************************************************************
 * CRITICAL SECTION MANAGEMENT / УПРАВЛЕНИЕ КРИТИЧЕСКОЙ СЕКЦИЕЙ
 ******************************************************************************/

/*******************************************************************************
 * CS_Init - Initialize Critical Section / Инициализация критической секции
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Thread-safe initialization of critical section using atomic operations.
 * Потокобезопасная инициализация критической секции с использованием атомарных операций.
 * 
 * ALGORITHM / АЛГОРИТМ:
 * 1. Try to atomically claim initialization (0 -> 1)
 *    Попытаться атомарно захватить инициализацию (0 -> 1)
 * 2. If successful, initialize critical section with exception handling
 *    Если успешно, инициализировать критическую секцию с обработкой исключений
 * 3. Mark as ready (2) when complete
 *    Отметить как готово (2) при завершении
 * 4. If another thread is initializing, wait for completion
 *    Если другой поток инициализирует, подождать завершения
 ******************************************************************************/
static void CS_Init(void) {
    // Try to claim initialization / Попытка захватить инициализацию
    LONG s = InterlockedCompareExchange(&g_csInited, 1, 0);
    if (s == 0) {
        // We won - initialize / Мы победили - инициализируем
        __try {
            InitializeCriticalSection(&g_cs);
            InterlockedExchange(&g_csInited, 2);  // Mark as ready / Отметить как готово
        } __except(EXCEPTION_EXECUTE_HANDLER) {
            // Initialization failed / Инициализация провалилась
            InterlockedExchange(&g_csInited, 0);
        }
        return;
    }
    
    // Already ready / Уже готово
    if (s == 2) return;

    // Wait for initializer thread (with 2 second timeout)
    // Ждать поток-инициализатор (с тайм-аутом 2 секунды)
    DWORD start = GetTickCount();
    while (InterlockedCompareExchange(&g_csInited, 2, 2) != 2) {
        Sleep(0);  // Yield to other threads / Передать управление другим потокам
        
        // Timeout after 2 seconds to prevent deadlock
        // Тайм-аут через 2 секунды для предотвращения взаимной блокировки
        if ((DWORD)(GetTickCount() - start) > 2000) {
            return;  // Give up / Сдаться
        }
    }
}

/*******************************************************************************
 * CS_Done - Cleanup Critical Section / Очистка критической секции
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Thread-safe cleanup of critical section.
 * Потокобезопасная очистка критической секции.
 ******************************************************************************/
static void CS_Done(void) { 
    // Atomically transition from ready (2) to not initialized (0)
    // Атомарный переход из готово (2) в неинициализировано (0)
    if (InterlockedCompareExchange(&g_csInited, 0, 2) == 2) 
        DeleteCriticalSection(&g_cs); 
}

/*******************************************************************************
 * Ctx_Lock / Ctx_Unlock - Context Locking Wrappers
 *                          Обёртки блокировки контекста
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Wrapper functions for entering/leaving critical section with init check.
 * Функции-обёртки для входа/выхода из критической секции с проверкой инициализации.
 * 
 * USAGE / ИСПОЛЬЗОВАНИЕ:
 * Always use in pairs:
 * Всегда использовать парами:
 * 
 * Ctx_Lock();
 * // ... access shared data ... / ... доступ к общим данным ...
 * Ctx_Unlock();
 ******************************************************************************/
static void Ctx_Lock(void) { 
    if (g_csInited != 2) CS_Init();  // Initialize if needed / Инициализировать если нужно
    if (g_csInited == 2) EnterCriticalSection(&g_cs); 
}

static void Ctx_Unlock(void) { 
    if (g_csInited == 2) LeaveCriticalSection(&g_cs); 
}

static OverlayCtx* FindCtxByHandle_NoLock(HANDLE h) { 
    for (int i = 0; i < MAX_OPEN_MP3S; i++) 
        if (g_ctxPool[i].inUse && g_ctxPool[i].hReal == h) 
            return &g_ctxPool[i]; 
    return NULL; 
}

static OverlayCtx* AllocCtx_NoLock(void) { 
    for (int i = 0; i < MAX_OPEN_MP3S; i++) { 
        if (!g_ctxPool[i].inUse) { 
            ZeroMemory(&g_ctxPool[i], sizeof(OverlayCtx)); 
            g_ctxPool[i].inUse = TRUE; 
            return &g_ctxPool[i]; 
        } 
    } 
    return NULL; 
}

static void FreeCtxStruct_NoLock(OverlayCtx* c) {
    if (!c) return;
    
    if (c->newTag) { 
        HeapFree(GetProcessHeap(), 0, c->newTag); 
        c->newTag = NULL; 
    }
    
    if (c->hKeep && c->hKeep != c->hReal) { 
        CloseHandle(c->hKeep); 
    }
    
    c->hKeep = NULL;
    c->refs = 0;
    c->closing = FALSE;
    c->inUse = FALSE;
    c->hReal = NULL;
}

static void BakMap_Set_NoLock(const char* orig, const char* bak) {
    if (!orig || !bak) return;
    
    for (int i = 0; i < MAX_OPEN_MP3S; i++) { 
        if (g_bakMap[i].inUse && lstrcmpiA(g_bakMap[i].orig, orig) == 0) { 
            lstrcpynA(g_bakMap[i].bak, bak, MAX_PATH); 
            g_bakMap[i].tick = GetTickCount(); 
            return; 
        } 
    }
    
    for (int i = 0; i < MAX_OPEN_MP3S; i++) { 
        if (!g_bakMap[i].inUse) { 
            ZeroMemory(&g_bakMap[i], sizeof(BackupMap)); 
            g_bakMap[i].inUse = TRUE; 
            lstrcpynA(g_bakMap[i].orig, orig, MAX_PATH); 
            lstrcpynA(g_bakMap[i].bak, bak, MAX_PATH); 
            g_bakMap[i].tick = GetTickCount(); 
            return; 
        } 
    }
}

static BOOL BakMap_Peek_NoLock(const char* orig, char* outBak, int cchOut) {
    if (!orig) return FALSE; 
    
    DWORD now = GetTickCount();
    for (int i = 0; i < MAX_OPEN_MP3S; i++) {
        if (!g_bakMap[i].inUse) continue;
        
        if ((now - g_bakMap[i].tick) > 60000) { 
            g_bakMap[i].inUse = FALSE; 
            continue; 
        }
        
        if (lstrcmpiA(g_bakMap[i].orig, orig) == 0) { 
            if (outBak && cchOut > 0) 
                lstrcpynA(outBak, g_bakMap[i].bak, cchOut); 
            return TRUE; 
        }
    }
    return FALSE;
}

static void TrimRightSpacesAndZeroA_(char* s) {
    if (!s) return; 
    int n = (int)lstrlenA(s);
    while (n > 0) { 
        char c = s[n-1]; 
        if (c == 0 || c == ' ' || c == '\t' || c == '\r' || c == '\n') { 
            s[n-1] = 0; 
            n--; 
        } else break; 
    }
}

static void ReadID3v1CommentToWide_(const BYTE v1[128], WCHAR* out, int cchOut, BOOL* pHasTag) {
    if (pHasTag) *pHasTag = FALSE; 
    if (!out || cchOut <= 0) return; 
    out[0] = 0;
    
    if (!v1 || memcmp(v1, "TAG", 3) != 0) return;
    if (pHasTag) *pHasTag = TRUE;
    
    BOOL is11 = (v1[125] == 0); 
    int maxc = is11 ? 28 : 30;
    
    char a[64]; 
    ZeroMemory(a, sizeof(a)); 
    memcpy(a, v1 + 97, maxc); 
    a[maxc] = 0; 
    TrimRightSpacesAndZeroA_(a);
    
    if (a[0]) { 
        MultiByteToWideChar(CP_ACP, 0, a, -1, out, cchOut); 
        out[cchOut-1] = 0; 
    }
}

static BOOL LooksLikeFrameHeader2_(int ver, const BYTE* p, int rem) {
    int need = (ver == 2) ? 6 : 10; 
    if (!p || rem < need) return FALSE; 
    
    if (p[0] == 0) return TRUE;
    
    int idlen = (ver == 2) ? 3 : 4; 
    for (int i = 0; i < idlen; i++) { 
        BYTE c = p[i]; 
        if (!((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9'))) 
            return FALSE; 
    }
    return TRUE;
}

static void SkipExtHeaderGuess_(int ver, BYTE tagFlags, BYTE** pp, int* prem) {
    if (!pp || !prem) return; 
    BYTE* p = *pp; 
    int rem = *prem; 
    if (!p || rem < 4) return; 
    
    if (!(tagFlags & 0x40)) return;
    
    DWORD extSzRaw = 0;
    if (ver == 3) 
        extSzRaw = DECRYPT_BE32Read(p); 
    else if (ver == 4) 
        extSzRaw = DECRYPT_SyncsafeToSize(p); 
    else 
        return;
    
    int cand1 = (ver == 3) ? ((int)extSzRaw + 4) : (int)extSzRaw;
    int cand2 = (ver == 3) ? (int)extSzRaw : ((int)extSzRaw + 4);
    
    int best = 0; 
    int bestScore = -2147483647;
    int cands[2] = { cand1, cand2 };
    
    for (int i = 0; i < 2; i++) {
        int s = cands[i]; 
        if (s <= 0 || s > rem) continue;
        
        int score = 0; 
        
        if (LooksLikeFrameHeader2_(ver, p + s, rem - s)) 
            score += 100000; 
        
        score -= s;
        
        if (score > bestScore) { 
            bestScore = score; 
            best = s; 
        }
    }
    
    if (best > 0 && best <= rem) { 
        *pp = p + best; 
        *prem = rem - best; 
    }
}

static BOOL ReadTempCommentFromID3_LastW_(HANDLE hFile, WCHAR* out, int cchOut, BOOL* pFramePresent) {
    if (pFramePresent) *pFramePresent = FALSE; 
    if (!out || cchOut <= 0) return FALSE; 
    out[0] = 0;
    
    if (!hFile || hFile == INVALID_HANDLE_VALUE) return FALSE;
    
    BYTE hdr[10] = {0}; 
    DWORD rd = 0;
    SetFilePointer(hFile, 0, 0, FILE_BEGIN);
    if (!ReadFile(hFile, hdr, 10, &rd, 0) || rd != 10 || memcmp(hdr, "ID3", 3) != 0) { 
        SetFilePointer(hFile, 0, 0, FILE_BEGIN); 
        return FALSE; 
    }
    
    int ver = hdr[3]; 
    BYTE tagFlags = hdr[5];
    DWORD tagSz = DECRYPT_SyncsafeToSize(hdr + 6);
    
    if (tagSz == 0 || tagSz > TAG_SANITY_LIMIT) { 
        SetFilePointer(hFile, 0, 0, FILE_BEGIN); 
        return FALSE; 
    }
    
    BYTE* buf = (BYTE*)HeapAlloc(GetProcessHeap(), 0, tagSz);
    if (!buf) { 
        SetFilePointer(hFile, 0, 0, FILE_BEGIN); 
        return FALSE; 
    }
    
    BOOL seen = FALSE; 
    BOOL preferEmptyDesc = FALSE; 
    BYTE* parseBuf = buf; 
    int parseLen = (int)tagSz; 
    BYTE* uns = NULL;
    
    if (ReadFile(hFile, buf, tagSz, &rd, 0) && rd == tagSz) {
        if (tagFlags & 0x80) {
            uns = (BYTE*)HeapAlloc(GetProcessHeap(), 0, tagSz);
            if (uns) {
                int outLen = 0; 
                for (DWORD i = 0; i < tagSz; i++) { 
                    BYTE b = buf[i]; 
                    uns[outLen++] = b; 
                    if (b == 0xFF && (i + 1) < tagSz && buf[i + 1] == 0x00) 
                        i++; 
                }
                parseBuf = uns; 
                parseLen = outLen;
            }
        }
        
        BYTE* p = parseBuf; 
        int rem = parseLen;
        
        SkipExtHeaderGuess_(ver, tagFlags, &p, &rem);
        
        while (rem >= 10) {
            if (p[0] == 0) break;
            
            DWORD fsz = 0; 
            DWORD total = 0;
            if (ver == 2) { 
                if (rem < 6) break; 
                fsz = ((DWORD)p[3] << 16) | ((DWORD)p[4] << 8) | (DWORD)p[5]; 
                total = 6 + fsz; 
            } else { 
                fsz = (ver == 4) ? DECRYPT_SyncsafeToSize(p + 4) : DECRYPT_BE32Read(p + 4); 
                total = 10 + fsz; 
            }
            
            if (fsz == 0 || (DWORD)rem < total) break;
            
            BOOL isComm = (ver==2) ? (memcmp(p,"COM",3)==0) : (memcmp(p,"COMM",4)==0);
            if (isComm) {
                if (pFramePresent) *pFramePresent = TRUE;
                
                const BYTE* pay = p + ((ver==2) ? 6 : 10); 
                int psz = (int)fsz; 
                BYTE enc = (psz > 0) ? pay[0] : 0;
                
                BOOL descEmpty = FALSE;
                if (psz >= 5) { 
                    int idx = 4; 
                    if (enc == 0 || enc == 3) 
                        descEmpty = (idx < psz && pay[idx] == 0); 
                    else 
                        descEmpty = (idx + 1 < psz && pay[idx] == 0 && pay[idx + 1] == 0); 
                }
                
                WCHAR tmp[WBUF_MAX]; 
                tmp[0] = 0; 
                ID3_DecodeCOMM_PayloadToWide(pay, psz, tmp, WBUF_MAX);
                
                if (descEmpty || !preferEmptyDesc) { 
                    lstrcpynW(out, tmp, cchOut); 
                    out[cchOut - 1] = 0; 
                    if (descEmpty) preferEmptyDesc = TRUE; 
                }
                seen = TRUE;
            }
            
            p += total; 
            rem -= (int)total;
        }
    }
    
    if (uns) HeapFree(GetProcessHeap(), 0, uns); 
    HeapFree(GetProcessHeap(), 0, buf);
    SetFilePointer(hFile, 0, 0, FILE_BEGIN);
    return seen;
}

static BOOL IsHandledFrameID(int ver, const BYTE* id) {
    static const char* h4[] = { 
        "TIT2","TPE1","TALB","TYER","TCON","TRCK",
        "TCOM","TOPE","TCOP","TENC","COMM" 
    };
    
    static const char* h3[] = { 
        "TT2","TP1","TAL","TYE","TCO","TRK",
        "TCM","TOA","TCR","TEN","COM" 
    };
    
    if (ver == 2) { 
        for (int i=0; i<11; i++) 
            if (memcmp(id, h3[i], 3)==0) return TRUE; 
    } else { 
        for (int i=0; i<11; i++) 
            if (memcmp(id, h4[i], 4)==0) return TRUE; 
    }
    return FALSE;
}

static BOOL IsUrlFrameID(int ver, const BYTE* id) { 
    return (id[0] == 'W'); 
}

static BOOL FileCopyChunk_(HANDLE hSrc, DynBuf* db, DWORD bytes) {
    BYTE tmp[4096];
    while (bytes > 0) {
        DWORD toRead = (bytes > sizeof(tmp)) ? sizeof(tmp) : bytes;
        DWORD rd = 0;
        if (!ReadFile(hSrc, tmp, toRead, &rd, 0) || rd != toRead) 
            return FALSE;
        if (!DB_Append(db, tmp, rd)) 
            return FALSE;
        bytes -= rd;
    }
    return TRUE;
}

static BOOL CopyFrames_Universal(HANDLE hFile, DynBuf* db, BOOL isV22, BOOL onlyUrl, BOOL excludeUrl) {
    if (!hFile || !db) return FALSE;
    
    DWORD fileSz = GetFileSize(hFile, NULL);
    BYTE hdr[10]; 
    DWORD rd;
    
    SetFilePointer(hFile, 0, 0, FILE_BEGIN);
    if (!ReadFile(hFile, hdr, 10, &rd, 0) || rd != 10 || memcmp(hdr, "ID3", 3) != 0) 
        return FALSE;
    
    int srcVer = hdr[3];
    
    if (isV22 && srcVer != 2) return FALSE;
    if (!isV22 && (srcVer < 3 || srcVer > 4)) return FALSE;

    DWORD tagSz = DECRYPT_SyncsafeToSize(hdr + 6);
    if (tagSz == 0 || tagSz > TAG_SANITY_LIMIT || (fileSz != INVALID_FILE_SIZE && fileSz < (10+tagSz))) 
        return FALSE;

    BOOL ok = FALSE;
    
    SetFilePointer(hFile, 10, NULL, FILE_BEGIN);
    int rem = (int)tagSz;

    if (!isV22) {
        if (srcVer == 4 && (hdr[5] & 0x10) && rem >= 10) 
            rem -= 10;
        
        if (hdr[5] & 0x40) {
            BYTE eh[4]; 
            if (ReadFile(hFile, eh, 4, &rd, 0) && rd==4) {
                DWORD extSz = (srcVer == 3) ? DECRYPT_BE32Read(eh) : DECRYPT_SyncsafeToSize(eh);
                int skip = (srcVer == 3) ? 0 : -4; 
                skip += (int)extSz;
                if (skip > 0) { 
                    SetFilePointer(hFile, skip, NULL, FILE_CURRENT); 
                    rem -= (4 + skip); 
                } else 
                    rem -= 4;
            } else 
                rem = 0;
        }
    }

    while (rem >= (isV22 ? 6 : 10)) {
        BYTE fh[10]; 
        int headSz = isV22 ? 6 : 10;
        
        if (!ReadFile(hFile, fh, headSz, &rd, 0) || rd != (DWORD)headSz) break;
        
        if (fh[0] == 0) break;

        DWORD fsz = 0;
        if (isV22) 
            fsz = ((DWORD)fh[3] << 16) | ((DWORD)fh[4] << 8) | (DWORD)fh[5];
        else       
            fsz = (srcVer == 4) ? DECRYPT_SyncsafeToSize(fh + 4) : DECRYPT_BE32Read(fh + 4);

        if (fsz == 0 || (DWORD)rem < headSz + fsz) break;

        BOOL isUrl = IsUrlFrameID(isV22 ? 2 : srcVer, fh);
        BOOL isHandled = IsHandledFrameID(isV22 ? 2 : srcVer, fh);

        BOOL shouldCopy = FALSE;
        if (onlyUrl) {
            if (isUrl) shouldCopy = TRUE;
        } else {
            if (!isHandled) {
                if (excludeUrl && isUrl) 
                    shouldCopy = FALSE;  
                else 
                    shouldCopy = TRUE;
            }
        }

        if (shouldCopy) {
            if (!isV22 && srcVer == 4) 
                DECRYPT_BE32Write(fh + 4, fsz);
            
            if (!DB_Append(db, fh, headSz)) return FALSE;
            
            if (!FileCopyChunk_(hFile, db, fsz)) return FALSE;
        } else {
            SetFilePointer(hFile, fsz, NULL, FILE_CURRENT);
        }
        
        rem -= (headSz + (int)fsz);
    }
    
    ok = TRUE;
    return ok;
}

static void BE24Write_(BYTE* p, DWORD v) { 
    p[0]=(BYTE)((v>>16)&0xFF); 
    p[1]=(BYTE)((v>>8)&0xFF); 
    p[2]=(BYTE)(v&0xFF); 
}

static BYTE* BuildID3_Universal(HANDLE hBase, HANDLE hTemp, 
    const WCHAR* A, const WCHAR* T, const WCHAR* Alb, const WCHAR* Y, const WCHAR* G,
    const WCHAR* Trk, const WCHAR* C, const WCHAR* M, const WCHAR* O, const WCHAR* P, const WCHAR* E,
    DWORD* pNewSz, BOOL isV22, BOOL isView) 
{
    if (pNewSz) *pNewSz = 0;

    DynBuf db;
    if (!DB_Init(&db, 64*1024)) return NULL;

    {
        BYTE hdr10[10] = {0};
        if (!DB_Append(&db, hdr10, 10)) { 
            DB_Free(&db); 
            return NULL; 
        }
    }

    char tempAnsi[ABUF_MAX];

#define DB_TRY(x) do { if (!(x)) goto fail; } while (0)

    const WCHAR* vals[] = { T, A, Alb, Y, G, Trk, C, O, P, E };
    const char* ids4[]  = { "TIT2","TPE1","TALB","TYER","TCON","TRCK","TCOM","TOPE","TCOP","TENC" };  
    const char* ids3[]  = { "TT2", "TP1", "TAL", "TYE", "TCO", "TRK", "TCM", "TOA", "TCR", "TEN" };   

    for (int i = 0; i < 10; ++i) {
        if (vals[i] && vals[i][0]) {
            int len = DECRYPT_WideToACP(vals[i], tempAnsi, ABUF_MAX);
            if (len > 0) {
                if (isV22) {
                    BYTE h[6] = {0};
                    memcpy(h, ids3[i], 3);
                    BE24Write_(h + 3, 1 + (DWORD)len);  
                    DB_TRY(DB_Append(&db, h, 6));
                    DB_TRY(DB_AppendChar(&db, 0));      
                    DB_TRY(DB_Append(&db, tempAnsi, (DWORD)len));
                } else {
                    BYTE h[10] = {0};
                    memcpy(h, ids4[i], 4);
                    DECRYPT_BE32Write(h + 4, 1 + (DWORD)len);
                    DB_TRY(DB_Append(&db, h, 10));
                    DB_TRY(DB_AppendChar(&db, 0));      
                    DB_TRY(DB_Append(&db, tempAnsi, (DWORD)len));
                }
            }
        }
    }

    if (M && M[0]) {
        int len = DECRYPT_WideToACP(M, tempAnsi, ABUF_MAX);
        if (len > 0) {
            DWORD paySz = (DWORD)(1 + 3 + 1 + len + (isView ? 1 : 0));
            
            if (isV22) {
                BYTE h[6] = {'C','O','M',0,0,0};
                BE24Write_(h + 3, paySz);
                DB_TRY(DB_Append(&db, h, 6));
            } else {
                BYTE h[10] = {'C','O','M','M',0,0,0,0,0,0};
                DECRYPT_BE32Write(h + 4, paySz);
                DB_TRY(DB_Append(&db, h, 10));
            }
            
            DB_TRY(DB_AppendChar(&db, 0));           
            DB_TRY(DB_Append(&db, "eng", 3));        
            DB_TRY(DB_AppendChar(&db, 0));           
            DB_TRY(DB_Append(&db, tempAnsi, (DWORD)len));
            
            if (isView) DB_TRY(DB_AppendChar(&db, 0));
        }
    }

    if (hTemp && hTemp != INVALID_HANDLE_VALUE) {
        if (!CopyFrames_Universal(hTemp, &db, isV22, TRUE, FALSE)) 
            goto fail;
    } else if (hBase && hBase != INVALID_HANDLE_VALUE) {
        if (!CopyFrames_Universal(hBase, &db, isV22, TRUE, FALSE)) 
            goto fail;
    }

    if (hBase && hBase != INVALID_HANDLE_VALUE) {
        if (!CopyFrames_Universal(hBase, &db, isV22, FALSE, TRUE)) 
            goto fail;
    }

    if (db.size < 10) goto fail;
    {
        DWORD framesSz = db.size - 10;
        BYTE* out = db.data;

        out[0] = 'I'; out[1] = 'D'; out[2] = '3';
        
        out[3] = (BYTE)(isV22 ? 2 : 3);  
        out[4] = 0;                       
        
        out[5] = 0;  

        DWORD ss = DECRYPT_SizeToSyncsafe(framesSz);
        out[6] = (BYTE)((ss >> 24) & 0x7F);
        out[7] = (BYTE)((ss >> 16) & 0x7F);
        out[8] = (BYTE)((ss >>  8) & 0x7F);
        out[9] = (BYTE)( ss        & 0x7F);
    }

    {
        BYTE* shrink = (BYTE*)HeapReAlloc(GetProcessHeap(), 0, db.data, db.size);
        if (shrink) db.data = shrink;
    }

    if (pNewSz) *pNewSz = db.size;
    return db.data;

fail:
    DB_Free(&db);
    return NULL;

#undef DB_TRY
}

static BOOL CreateVirtualView_Optimized(HANDLE hFile, BYTE** pTagBuf, DWORD* pNewTagSz, DWORD* pOldTagSz) {
    if (pTagBuf) *pTagBuf = NULL; 
    *pNewTagSz=0; 
    *pOldTagSz=0;
    
    WCHAR wA[WBUF_MAX]={0}, wT[WBUF_MAX]={0}, wAlb[WBUF_MAX]={0};
    WCHAR wY[WBUF_MAX]={0}, wG[WBUF_MAX]={0}, wTrk[WBUF_MAX]={0};
    WCHAR wC[WBUF_MAX]={0}, wM[WBUF_MAX]={0}, wO[WBUF_MAX]={0};
    WCHAR wP[WBUF_MAX]={0}, wU[WBUF_MAX]={0}, wE[WBUF_MAX]={0};
    DWORD oldTagSz = 0;
    
    if (!ID3_ReadAllFieldsW(hFile, wA, WBUF_MAX, wT, WBUF_MAX, wAlb, WBUF_MAX, 
                            wY, WBUF_MAX, wG, WBUF_MAX, wTrk, WBUF_MAX, 
                            wC, WBUF_MAX, wM, WBUF_MAX, wO, WBUF_MAX, 
                            wP, WBUF_MAX, wU, WBUF_MAX, wE, WBUF_MAX, &oldTagSz)) {
        if (oldTagSz == 0) return FALSE; 
    }
    
    int srcVer = 0; 
    { 
        BYTE hdr10[10] = {0}; 
        DWORD rd10 = 0; 
        SetFilePointer(hFile, 0, 0, FILE_BEGIN); 
        if (ReadFile(hFile, hdr10, 10, &rd10, 0) && rd10 == 10 && memcmp(hdr10, "ID3", 3) == 0) 
            srcVer = hdr10[3]; 
        SetFilePointer(hFile, 0, 0, FILE_BEGIN); 
    }
    
    DWORD newTagSz = 0; 
    BYTE* newTag = BuildID3_Universal(hFile, NULL, wA, wT, wAlb, wY, wG, wTrk, 
                                      wC, wM, wO, wP, wE, &newTagSz, (srcVer==2), TRUE);
    if (!newTag) return FALSE;
    
    *pTagBuf = newTag; 
    *pNewTagSz = newTagSz; 
    *pOldTagSz = oldTagSz;
    return TRUE;
}

static void AttachVirtualContext(HANDLE hFile, const char* pathA) {
    if (hFile == INVALID_HANDLE_VALUE || !HasExtI_A(pathA, ".mp3")) return;
    
    BYTE* tag = NULL; 
    DWORD newSz = 0; 
    DWORD oldSz = 0;
    if (!CreateVirtualView_Optimized(hFile, &tag, &newSz, &oldSz)) return;
    
    SetFilePointer(hFile, 0, NULL, FILE_BEGIN);
    
    Ctx_Lock(); 
    OverlayCtx* ctx = AllocCtx_NoLock();
    if (ctx) {
        ctx->hReal = hFile; 
        ctx->hKeep = NULL; 
        ctx->refs = 0; 
        ctx->closing = FALSE;
        
        DuplicateHandle(GetCurrentProcess(), hFile, GetCurrentProcess(), 
                       &ctx->hKeep, 0, FALSE, DUPLICATE_SAME_ACCESS);
        
        if (!ctx->hKeep) { 
            HeapFree(GetProcessHeap(), 0, tag); 
            ZeroMemory(ctx, sizeof(OverlayCtx)); 
            ctx->inUse = FALSE; 
            Ctx_Unlock(); 
            return; 
        }
        
        ctx->newTag = tag; 
        ctx->newTagSz = newSz; 
        ctx->oldTagSz = oldSz;
        
        DWORD high=0; 
        DWORD fileSize = GetFileSize(hFile, &high);
        if (fileSize != INVALID_FILE_SIZE) 
            ctx->virtSz = newSz + (fileSize - oldSz); 
        else 
            ctx->virtSz = newSz;
        
        ctx->logicalPos = 0; 
        lstrcpynA(ctx->pathA, pathA, MAX_PATH);
    } else { 
        HeapFree(GetProcessHeap(), 0, tag); 
    }
    Ctx_Unlock();
}


static BOOL ReadEditedFieldsFromTemp_(const char* tempPath, 
    WCHAR* wA, int cchA, WCHAR* wT, int cchT, WCHAR* wAlb, int cchAlb, 
    WCHAR* wY, int cchY, WCHAR* wG, int cchG, WCHAR* wTrk, int cchTrk, 
    WCHAR* wC, int cchC, WCHAR* wM, int cchM, WCHAR* wO, int cchO, 
    WCHAR* wP, int cchP, WCHAR* wU, int cchU, WCHAR* wE, int cchE)
{
    if (wA) wA[0]=0; if (wT) wT[0]=0; if (wAlb) wAlb[0]=0; 
    if (wY) wY[0]=0; if (wG) wG[0]=0; if (wTrk) wTrk[0]=0; 
    if (wC) wC[0]=0; if (wM) wM[0]=0; if (wO) wO[0]=0; 
    if (wP) wP[0]=0; if (wU) wU[0]=0; if (wE) wE[0]=0;
    
    HANDLE h = CreateFileA(tempPath, GENERIC_READ, FILE_SHARE_READ|FILE_SHARE_WRITE, 
                          0, OPEN_EXISTING, 0, 0);
    if (h == INVALID_HANDLE_VALUE) return FALSE;
    
    DWORD dummyOld = 0;
    BOOL ok = ID3_ReadAllFieldsW(h, wA, cchA, wT, cchT, wAlb, cchAlb, 
                                 wY, cchY, wG, cchG, wTrk, cchTrk, 
                                 wC, cchC, wM, cchM, wO, cchO, 
                                 wP, cchP, wU, cchU, wE, cchE, &dummyOld);
    
    CloseHandle(h); 
    return ok;
}

static BOOL GetOldID3v2Size_(HANDLE hBase, DWORD* pOldTagSz) {
    if (pOldTagSz) *pOldTagSz = 0; 
    if (!hBase || hBase == INVALID_HANDLE_VALUE) return FALSE;
    
    return ID3_ReadAllFieldsW(hBase, NULL,0, NULL,0, NULL,0, NULL,0, NULL,0, 
                             NULL,0, NULL,0, NULL,0, NULL,0, NULL,0, 
                             NULL,0, NULL,0, pOldTagSz);
}

static BOOL WriteNewFileFromBase_(HANDLE hBase, HANDLE hTemp, 
    const BYTE* id3v1Override, BOOL hasId3v1Override, const char* finalPath, 
    const WCHAR* wA, const WCHAR* wT, const WCHAR* wAlb, const WCHAR* wY, 
    const WCHAR* wG, const WCHAR* wTrk, const WCHAR* wC, const WCHAR* wM, 
    const WCHAR* wO, const WCHAR* wP, const WCHAR* wU, const WCHAR* wE)
{
    if (!hBase || hBase == INVALID_HANDLE_VALUE || !finalPath) return FALSE;
    
    DWORD oldTagSz = 0; 
    GetOldID3v2Size_(hBase, &oldTagSz);
    
    int baseVer = 0; 
    { 
        BYTE hdr10[10] = {0}; 
        DWORD rd10 = 0; 
        SetFilePointer(hBase, 0, 0, FILE_BEGIN); 
        if (ReadFile(hBase, hdr10, 10, &rd10, 0) && rd10 == 10 && memcmp(hdr10, "ID3", 3) == 0) 
            baseVer = hdr10[3]; 
        SetFilePointer(hBase, 0, 0, FILE_BEGIN); 
    }
    
    DWORD newTagSz = 0; 
    BYTE* newTag = BuildID3_Universal(hBase, hTemp, wA,wT,wAlb,wY,wG,wTrk,wC,wM,wO,wP,wE, 
                                      &newTagSz, (baseVer==2), FALSE);
    if (!newTag) return FALSE;

    DWORD fileSz = GetFileSize(hBase, NULL); 
    if (oldTagSz > fileSz) oldTagSz = 0;
    
    BYTE id3v1[128]; 
    BOOL hasV1 = ReadID3v1Tail_(hBase, id3v1);
    DWORD tailLen = hasV1 ? 128 : 0; 
    DWORD audioSz = 0; 
    if (fileSz >= oldTagSz + tailLen) 
        audioSz = fileSz - oldTagSz - tailLen;
    
    char dir[MAX_PATH] = {0}; 
    PathGetDirA_(finalPath, dir, MAX_PATH); 
    char tmpPath[MAX_PATH] = {0};
    
    if (dir[0]) 
        if (!GetTempFileNameA(dir, "gft", 0, tmpPath)) 
            tmpPath[0] = 0;
    
    if (!tmpPath[0]) { 
        char sysTmp[MAX_PATH] = {0}; 
        GetTempPathA(MAX_PATH, sysTmp); 
        GetTempFileNameA(sysTmp, "gft", 0, tmpPath); 
    }
    
    if (!tmpPath[0]) { 
        HeapFree(GetProcessHeap(),0,newTag); 
        return FALSE; 
    }
    
    HANDLE hOut = CreateFileA(tmpPath, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, 
                             FILE_ATTRIBUTE_NORMAL, 0);
    if (hOut == INVALID_HANDLE_VALUE) { 
        HeapFree(GetProcessHeap(),0,newTag); 
        return FALSE; 
    }
    
    DWORD wr = 0; 
    BOOL ok = WriteFile(hOut, newTag, newTagSz, &wr, 0) && wr == newTagSz;
    HeapFree(GetProcessHeap(), 0, newTag);
    
    if (!ok) { 
        CloseHandle(hOut); 
        DeleteFileA(tmpPath); 
        return FALSE; 
    }
    
    SetFilePointer(hBase, (LONG)oldTagSz, 0, FILE_BEGIN);
    BYTE buf[64*1024]; 
    DWORD left = audioSz;
    
    while (left > 0) {
        DWORD to = (left > sizeof(buf)) ? sizeof(buf) : left; 
        DWORD rd = 0;
        if (!ReadFile(hBase, buf, to, &rd, 0) || rd == 0) { 
            ok = FALSE; 
            break; 
        }
        if (!WriteFile(hOut, buf, rd, &wr, 0) || wr != rd) { 
            ok = FALSE; 
            break; 
        }
        left -= rd;
    }
    
    if (ok && (hasId3v1Override || hasV1)) {
        const BYTE* tailSrc = (hasId3v1Override && id3v1Override) ? id3v1Override : id3v1;
        BYTE tailBuf[128]; 
        memcpy(tailBuf, tailSrc, 128);
        
        if (wM && wM[0]) 
            PatchID3v1CommentFromWide_(tailBuf, wM);
        
        if (!WriteFile(hOut, tailBuf, 128, &wr, 0) || wr != 128) 
            ok = FALSE;
    }
    
    CloseHandle(hOut);
    
    if (!ok) { 
        DeleteFileA(tmpPath); 
        return FALSE; 
    }
    
    DeleteFileA(finalPath);
    ok = MoveFileA(tmpPath, finalPath);
    
    if (!ok) {
        HMODULE k32 = GetModuleHandleA("KERNEL32.dll");
        if (k32) { 
            typedef BOOL (WINAPI *PFN_MoveFileExA)(LPCSTR,LPCSTR,DWORD); 
            PFN_MoveFileExA p = (PFN_MoveFileExA)GetProcAddress(k32, "MoveFileExA"); 
            if (p) ok = p(tmpPath, finalPath, 1); 
        }
    }
    
    if (!ok) DeleteFileA(tmpPath);
    return ok;
}

static BOOL HandleTagSave_MoveTempIntoMp3_(const char* tempPath, const char* finalMp3Path) {
    WCHAR wA[WBUF_MAX]={0}, wT[WBUF_MAX]={0}, wAlb[WBUF_MAX]={0};
    WCHAR wY[WBUF_MAX]={0}, wG[WBUF_MAX]={0}, wTrk[WBUF_MAX]={0};
    WCHAR wC[WBUF_MAX]={0}, wM[WBUF_MAX]={0}, wO[WBUF_MAX]={0};
    WCHAR wP[WBUF_MAX]={0}, wU[WBUF_MAX]={0}, wE[WBUF_MAX]={0};
    
    if (!ReadEditedFieldsFromTemp_(tempPath, wA,WBUF_MAX, wT,WBUF_MAX, wAlb,WBUF_MAX, 
                                   wY,WBUF_MAX, wG,WBUF_MAX, wTrk,WBUF_MAX, 
                                   wC,WBUF_MAX, wM,WBUF_MAX, wO,WBUF_MAX, 
                                   wP,WBUF_MAX, wU,WBUF_MAX, wE,WBUF_MAX)) 
        return FALSE;
    
    const char* basePath = NULL; 
    char bak[MAX_PATH] = {0};
    Ctx_Lock(); 
    if (BakMap_Peek_NoLock(finalMp3Path, bak, MAX_PATH) && FileExistsA_(bak)) 
        basePath = bak; 
    Ctx_Unlock();
    
    if (!basePath && FileExistsA_(finalMp3Path)) 
        basePath = finalMp3Path;
    
    if (!basePath) return FALSE;
    
    HANDLE hBase = CreateFileA(basePath, GENERIC_READ, FILE_SHARE_READ|FILE_SHARE_WRITE, 
                              0, OPEN_EXISTING, 0, 0);
    if (hBase == INVALID_HANDLE_VALUE) return FALSE;
    
    HANDLE hTemp = CreateFileA(tempPath, GENERIC_READ, FILE_SHARE_READ|FILE_SHARE_WRITE, 
                              0, OPEN_EXISTING, 0, 0);
    
    BYTE v1Temp[128]; 
    BOOL hasV1Temp = FALSE;
    if (hTemp != INVALID_HANDLE_VALUE) 
        hasV1Temp = ReadID3v1Tail_(hTemp, v1Temp);
    
    WCHAR wM_final[WBUF_MAX]; 
    wM_final[0] = 0; 
    BOOL tempCommPresent = FALSE;
    
    if (hTemp != INVALID_HANDLE_VALUE) 
        ReadTempCommentFromID3_LastW_(hTemp, wM_final, WBUF_MAX, &tempCommPresent);
    
    if (!tempCommPresent) {
        BOOL hasV1Tag = FALSE;
        if (hasV1Temp) 
            ReadID3v1CommentToWide_(v1Temp, wM_final, WBUF_MAX, &hasV1Tag);
        
        if (!hasV1Tag) wM_final[0] = 0;
        if (!wM_final[0] && wM && wM[0]) 
            lstrcpynW(wM_final, wM, WBUF_MAX);
        
        if (!wM_final[0]) {
            WCHAR wBaseM[WBUF_MAX]; 
            wBaseM[0] = 0; 
            BOOL baseCommPresent = FALSE;
            ReadTempCommentFromID3_LastW_(hBase, wBaseM, WBUF_MAX, &baseCommPresent);
            if (baseCommPresent) { 
                lstrcpynW(wM_final, wBaseM, WBUF_MAX); 
                wM_final[WBUF_MAX-1]=0; 
            }
        }
    }
    
    BOOL ok = WriteNewFileFromBase_(hBase, (hTemp != INVALID_HANDLE_VALUE) ? hTemp : NULL, 
                                   hasV1Temp ? v1Temp : NULL, hasV1Temp, finalMp3Path, 
                                   wA,wT,wAlb,wY,wG,wTrk,wC,wM_final,wO,wP,wU,wE);
    
    if (hTemp != INVALID_HANDLE_VALUE) CloseHandle(hTemp);
    CloseHandle(hBase);
    
    if (ok && bak[0] && FileExistsA_(bak)) 
        DeleteFileA(bak);
    
    if (ok) 
        DeleteFileA(tempPath);
    
    return ok;
}

static HANDLE WINAPI Hook_CreateFileA(LPCSTR fn, DWORD access, DWORD share, 
                                     LPSECURITY_ATTRIBUTES sa, DWORD disp, 
                                     DWORD flags, HANDLE tpl) {
    PFN_CreateFileA pReal = Orig_CreateFileA; 
    if (!pReal || g_hooksReady != 1) 
        return pReal ? pReal(fn, access, share, sa, disp, flags, tpl) : INVALID_HANDLE_VALUE;
    
    HANDLE h = pReal(fn, access, share, sa, disp, flags, tpl);
    
    if (h != INVALID_HANDLE_VALUE && !(access & GENERIC_WRITE)) 
        AttachVirtualContext(h, fn);
    
    return h;
}

static BOOL WINAPI Hook_ReadFile(HANDLE hFile, LPVOID buf, DWORD toRead, 
                                LPDWORD pRead, LPOVERLAPPED ovl) {
    PFN_ReadFile pReal = Orig_ReadFile;
    if (!pReal || g_hooksReady != 1) 
        return pReal ? pReal(hFile, buf, toRead, pRead, ovl) : FALSE;
    if (!Orig_SetFilePointer) 
        return pReal(hFile, buf, toRead, pRead, ovl);

    OverlayCtx* c = NULL;
    DWORD total = 0;

    Ctx_Lock();
    c = FindCtxByHandle_NoLock(hFile);
    if (c && c->newTag && !c->closing) {
        InterlockedIncrement(&c->refs);

        if (c->logicalPos < c->newTagSz) {
            DWORD avail = c->newTagSz - c->logicalPos;
            DWORD chunk = (toRead < avail) ? toRead : avail;
            memcpy(buf, c->newTag + c->logicalPos, chunk);
            c->logicalPos += chunk;
            total += chunk;

            if (total == toRead) {
                if (InterlockedDecrement(&c->refs) == 0 && c->closing) 
                    FreeCtxStruct_NoLock(c);
                Ctx_Unlock();
                if (pRead) *pRead = total;
                return TRUE;
            }
            toRead -= chunk;
            buf = (BYTE*)buf + chunk;
        }

        DWORD posSnap = c->logicalPos;
        DWORD physOffset = c->oldTagSz + (posSnap - c->newTagSz);
        HANDLE hPhys = (c->hKeep) ? c->hKeep : c->hReal;
        Ctx_Unlock();

        Orig_SetFilePointer(hPhys, physOffset, NULL, FILE_BEGIN);

        DWORD chunkRead = 0;
        BOOL res = pReal(hPhys, buf, toRead, &chunkRead, ovl);
        total += chunkRead;

        Ctx_Lock();
        if (c && c->inUse && c->newTag && c->logicalPos == posSnap) {
            c->logicalPos = posSnap + chunkRead;
        }
        if (c) {
            if (InterlockedDecrement(&c->refs) == 0 && c->closing) 
                FreeCtxStruct_NoLock(c);
        }
        Ctx_Unlock();

        if (pRead) *pRead = total;
        return res;
    }
    Ctx_Unlock();
    
    return pReal(hFile, buf, toRead, pRead, ovl);
}

static DWORD WINAPI Hook_SetFilePointer(HANDLE hFile, LONG move, PLONG hi, DWORD method) {
    PFN_SetFilePointer pReal = Orig_SetFilePointer; 
    if (!pReal || g_hooksReady != 1) 
        return pReal ? pReal(hFile, move, hi, method) : (DWORD)-1;
    
    OverlayCtx* c = NULL; 
    Ctx_Lock(); 
    c = FindCtxByHandle_NoLock(hFile);
    
    if (c && c->newTag) {
        LONGLONG base = 0;
        switch (method) { 
            case FILE_BEGIN: base = 0; break; 
            case FILE_CURRENT: base = (LONG)c->logicalPos; break; 
            case FILE_END: base = (LONG)c->virtSz; break; 
            default: base = (LONG)c->logicalPos; break; 
        }
        
        LONGLONG sum = base + move; 
        if (sum < 0) sum = 0; 
        if (sum > (LONGLONG)c->virtSz) sum = c->virtSz;
        
        c->logicalPos = (DWORD)sum; 
        Ctx_Unlock(); 
        if (hi) *hi = 0; 
        return (DWORD)sum;
    }
    Ctx_Unlock(); 
    
    return pReal(hFile, move, hi, method);
}

static DWORD WINAPI Hook_GetFileSize(HANDLE hFile, LPDWORD pHigh) {
    PFN_GetFileSize pReal = Orig_GetFileSize; 
    if (!pReal || g_hooksReady != 1) 
        return pReal ? pReal(hFile, pHigh) : INVALID_FILE_SIZE;
    
    OverlayCtx* c = NULL; 
    Ctx_Lock(); 
    c = FindCtxByHandle_NoLock(hFile);
    
    if (c && c->newTag) { 
        if (pHigh) *pHigh = 0; 
        DWORD sz = c->virtSz; 
        Ctx_Unlock(); 
        return sz; 
    }
    Ctx_Unlock(); 
    
    return pReal(hFile, pHigh);
}

static BOOL WINAPI Hook_CloseHandle(HANDLE h) {
    PFN_CloseHandle pReal = Orig_CloseHandle; 
    if (!pReal) return FALSE;

    Ctx_Lock();
    OverlayCtx* c = FindCtxByHandle_NoLock(h);
    if (c) {
        c->closing = TRUE;
        
        if (InterlockedCompareExchange(&c->refs, 0, 0) == 0) {
            FreeCtxStruct_NoLock(c);
        }
    }
    Ctx_Unlock();

    return pReal(h);
}

static BOOL WINAPI Hook_MoveFileA(LPCSTR src, LPCSTR dst) {
    PFN_MoveFileA pReal = Orig_MoveFileA; 
    if (!pReal || g_hooksReady != 1) 
        return pReal ? pReal(src, dst) : FALSE;
    
    if (src && dst) {
        if (HasExtI_A(src, ".mp3") && !HasExtI_A(dst, ".mp3")) { 
            Ctx_Lock(); 
            BakMap_Set_NoLock(src, dst); 
            Ctx_Unlock(); 
            return pReal(src, dst); 
        }
        
        if (HasExtI_A(dst, ".mp3") && lstrcmpiA(src, dst) != 0) {
            char bak[MAX_PATH] = {0}; 
            BOOL knownBackup = FALSE; 
            Ctx_Lock(); 
            knownBackup = BakMap_Peek_NoLock(dst, bak, MAX_PATH); 
            Ctx_Unlock();
            
            if (knownBackup && LooksLikeID3v2FileA_(src)) { 
                if (HandleTagSave_MoveTempIntoMp3_(src, dst)) 
                    return TRUE; 
            }
        }
    }
    
    return pReal(src, dst);
}

static BOOL WINAPI Hook_MoveFileExA(LPCSTR src, LPCSTR dst, DWORD flags) {
    PFN_MoveFileExA pReal = Orig_MoveFileExA; 
    if (!pReal || g_hooksReady != 1) 
        return pReal ? pReal(src, dst, flags) : FALSE;
    
    if (src && dst) {
        if (HasExtI_A(src, ".mp3") && !HasExtI_A(dst, ".mp3")) { 
            Ctx_Lock(); 
            BakMap_Set_NoLock(src, dst); 
            Ctx_Unlock(); 
            return pReal(src, dst, flags); 
        }
        
        if (HasExtI_A(dst, ".mp3") && lstrcmpiA(src, dst) != 0) {
            char bak[MAX_PATH] = {0}; 
            BOOL knownBackup = FALSE; 
            Ctx_Lock(); 
            knownBackup = BakMap_Peek_NoLock(dst, bak, MAX_PATH); 
            Ctx_Unlock();
            
            if (knownBackup && LooksLikeID3v2FileA_(src)) { 
                if (HandleTagSave_MoveTempIntoMp3_(src, dst)) 
                    return TRUE; 
            }
        }
    }
    
    return pReal(src, dst, flags);
}

/*******************************************************************************
 * INITIALIZATION AND CLEANUP / ИНИЦИАЛИЗАЦИЯ И ОЧИСТКА
 ******************************************************************************/

/*******************************************************************************
 * HookThread - Background Hook Installation Thread
 *              Фоновый поток установки перехватов
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Worker thread that waits for in_mp3.dll to load, then installs IAT hooks.
 * Рабочий поток, который ждёт загрузки in_mp3.dll, затем устанавливает IAT-перехваты.
 * 
 * PROCESS / ПРОЦЕСС:
 * 1. Initialize critical section / Инициализировать критическую секцию
 * 2. Wait for in_mp3.dll to load / Ждать загрузки in_mp3.dll
 * 3. Wait 500ms for plugin initialization / Ждать 500мс для инициализации плагина
 * 4. Get original function pointers from KERNEL32.dll
 *    Получить указатели на оригинальные функции из KERNEL32.dll
 * 5. Patch in_mp3.dll's IAT / Пропатчить IAT in_mp3.dll
 * 6. Mark hooks as ready / Отметить перехваты как готовые
 ******************************************************************************/
static unsigned __stdcall HookThread(void*) {
    // Initialize critical section / Инициализация критической секции
    CS_Init();
    
    // Wait for in_mp3.dll to load / Ждать загрузки in_mp3.dll
    for (;;) { 
        g_hInMp3 = GetModuleHandleA("in_mp3.dll"); 
        if (g_hInMp3) break; 
        Sleep(200);  // Check every 200ms / Проверять каждые 200мс
    }
    
    // Wait for plugin to initialize / Ждать инициализации плагина
    Sleep(500);
    
    // Get KERNEL32.dll module / Получить модуль KERNEL32.dll
    HMODULE hK32 = GetModuleHandleA("KERNEL32.dll");
    if (!hK32) return 0;
    
    // Get original function pointers / Получить указатели на оригинальные функции
    Orig_CreateFileA = (PFN_CreateFileA)GetProcAddress(hK32, "CreateFileA"); 
    Orig_ReadFile = (PFN_ReadFile)GetProcAddress(hK32, "ReadFile"); 
    Orig_SetFilePointer = (PFN_SetFilePointer)GetProcAddress(hK32, "SetFilePointer"); 
    Orig_GetFileSize = (PFN_GetFileSize)GetProcAddress(hK32, "GetFileSize"); 
    Orig_CloseHandle = (PFN_CloseHandle)GetProcAddress(hK32, "CloseHandle"); 
    Orig_MoveFileA = (PFN_MoveFileA)GetProcAddress(hK32, "MoveFileA"); 
    Orig_MoveFileExA = (PFN_MoveFileExA)GetProcAddress(hK32, "MoveFileExA");
    
    // Validate required functions / Проверить необходимые функции
    if (!Orig_CreateFileA || !Orig_ReadFile || !Orig_SetFilePointer || 
        !Orig_CloseHandle || !Orig_MoveFileA) 
        return 0;
    
    // Initialize Real pointers / Инициализировать Real указатели
    Real_CreateFileA = Orig_CreateFileA; 
    Real_ReadFile = Orig_ReadFile; 
    Real_SetFilePointer = Orig_SetFilePointer; 
    Real_GetFileSize = Orig_GetFileSize; 
    Real_CloseHandle = Orig_CloseHandle; 
    Real_MoveFileA = Orig_MoveFileA; 
    Real_MoveFileExA = Orig_MoveFileExA;
    
    // Patch in_mp3.dll's IAT / Пропатчить IAT in_mp3.dll
    IAT_PatchByName(g_hInMp3, "KERNEL32.dll", "CreateFileA", 
                   (void*)Hook_CreateFileA, (void**)&Real_CreateFileA);
    IAT_PatchByName(g_hInMp3, "KERNEL32.dll", "ReadFile", 
                   (void*)Hook_ReadFile, (void**)&Real_ReadFile);
    IAT_PatchByName(g_hInMp3, "KERNEL32.dll", "SetFilePointer", 
                   (void*)Hook_SetFilePointer, (void**)&Real_SetFilePointer);
    if (Orig_GetFileSize) 
        IAT_PatchByName(g_hInMp3, "KERNEL32.dll", "GetFileSize", 
                       (void*)Hook_GetFileSize, (void**)&Real_GetFileSize);
    IAT_PatchByName(g_hInMp3, "KERNEL32.dll", "CloseHandle", 
                   (void*)Hook_CloseHandle, (void**)&Real_CloseHandle);
    IAT_PatchByName(g_hInMp3, "KERNEL32.dll", "MoveFileA", 
                   (void*)Hook_MoveFileA, (void**)&Real_MoveFileA);
    if (Orig_MoveFileExA) 
        IAT_PatchByName(g_hInMp3, "KERNEL32.dll", "MoveFileExA", 
                       (void*)Hook_MoveFileExA, (void**)&Real_MoveFileExA);
    
    // Mark hooks as ready / Отметить перехваты как готовые
    InterlockedExchange(&g_hooksReady, 1);
    return 0;
}

/*******************************************************************************
 * EXPORTED PUBLIC API FUNCTIONS / ЭКСПОРТИРУЕМЫЕ ПУБЛИЧНЫЕ ФУНКЦИИ API
 ******************************************************************************/

/*******************************************************************************
 * MP3_TagsFix_Init - Initialize Module / Инициализация модуля
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Initializes the MP3 tags fix module. Creates a background thread that
 * waits for in_mp3.dll to load and installs hooks.
 * 
 * Инициализирует модуль исправления MP3-тегов. Создаёт фоновый поток,
 * который ждёт загрузки in_mp3.dll и устанавливает перехваты.
 * 
 * NOTES / ПРИМЕЧАНИЯ:
 * Safe to call multiple times (only installs once).
 * Безопасно вызывать несколько раз (устанавливается только один раз).
 ******************************************************************************/
extern "C" __declspec(dllexport) void MP3_TagsFix_Init(void) { 
    static BOOL installed = FALSE;  // Installation flag / Флаг установки
    if (!installed) { 
        installed = TRUE; 
        unsigned tid = 0; 
        // Create background hook thread / Создать фоновый поток перехвата
        _beginthreadex(NULL, 0, HookThread, NULL, 0, &tid); 
    } 
}

/*******************************************************************************
 * MP3_TagsFix_Quit - Cleanup Module / Очистка модуля
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Cleans up the MP3 tags fix module. Disables hooks and frees all contexts.
 * Очищает модуль исправления MP3-тегов. Отключает перехваты и освобождает все контексты.
 * 
 * CRITICAL / КРИТИЧНО:
 * Must free all overlay contexts and their associated resources.
 * Должен освободить все контексты наложения и связанные с ними ресурсы.
 ******************************************************************************/
extern "C" __declspec(dllexport) void MP3_TagsFix_Quit(void) { 
    // Disable hooks / Отключить перехваты
    InterlockedExchange(&g_hooksReady, 0); 
    
    // Free all contexts / Освободить все контексты
    Ctx_Lock(); 
    for (int i = 0; i < MAX_OPEN_MP3S; i++) { 
        if (g_ctxPool[i].inUse) 
            FreeCtxStruct_NoLock(&g_ctxPool[i]); 
        g_bakMap[i].inUse = FALSE; 
    } 
    Ctx_Unlock(); 
    
    // Cleanup critical section / Очистить критическую секцию
    CS_Done(); 
}

/*******************************************************************************
 * ALTERNATIVE ENTRY POINTS / АЛЬТЕРНАТИВНЫЕ ТОЧКИ ВХОДА
 * 
 * These are aliases provided for compatibility with different calling conventions.
 * Это псевдонимы, предоставленные для совместимости с различными соглашениями о вызовах.
 ******************************************************************************/

// Entry point with HWND parameter (unused) / Точка входа с параметром HWND (неиспользуемым)
extern "C" __declspec(dllexport) void MP3_TagsFix_Init_C(HWND) { 
    MP3_TagsFix_Init(); 
}

// Alternative naming for Extended File Info hook / Альтернативное именование для перехвата Extended File Info
extern "C" __declspec(dllexport) void EFIHook_Init(void) { 
    MP3_TagsFix_Init(); 
}

extern "C" __declspec(dllexport) void EFIHook_Quit(void) { 
    MP3_TagsFix_Quit(); 
}