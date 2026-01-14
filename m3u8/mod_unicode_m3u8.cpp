/*******************************************************************************
 * M3U8 AND UNICODE PLAYLIST LOADER MODULE
 * МОДУЛЬ ЗАГРУЗЧИКА M3U8 И UNICODE ПЛЕЙЛИСТОВ
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Enables Winamp 2.95 to load playlists with Unicode (non-ASCII) filenames
 * and content. Winamp's playlist engine only supports ANSI encoding, causing
 * failures when loading files with Cyrillic, Chinese, or other non-Latin text.
 * 
 * Позволяет Winamp 2.95 загружать плейлисты с Unicode (не-ASCII) именами файлов
 * и содержимым. Движок плейлистов Winamp поддерживает только ANSI кодировку,
 * вызывая сбои при загрузке файлов с кириллицей, китайскими или другими не-латинскими текстами.
 * 
 * 
 * SOLUTION / РЕШЕНИЕ:
 * 1. Hook file I/O functions via IAT patching
 * 2. Detect Unicode playlists (UTF-8, UTF-16LE BOM)
 * 3. Convert to ANSI temporary file
 * 4. Redirect Winamp to load temp file
 * 5. Clean up temp files when closed
 * 
 * 1. Перехватить функции файлового I/O через IAT патчинг
 * 2. Обнаружить Unicode плейлисты (UTF-8, UTF-16LE BOM)
 * 3. Преобразовать во временный ANSI файл
 * 4. Перенаправить Winamp на загрузку временного файла
 * 5. Очистить временные файлы при закрытии
 * 
 * SUPPORTED FORMATS / ПОДДЕРЖИВАЕМЫЕ ФОРМАТЫ:
 * - M3U8 files (UTF-8 encoded M3U)
 * - M3U files with UTF-8 encoding
 * - PLS files with UTF-8 or UTF-16LE encoding
 * 
 * - Файлы M3U8 (M3U в кодировке UTF-8)
 * - Файлы M3U с кодировкой UTF-8
 * - Файлы PLS с кодировкой UTF-8 или UTF-16LE
 * 
 * HOOKED FUNCTIONS / ПЕРЕХВАЧЕННЫЕ ФУНКЦИИ:
 * fopen()                     - M3U8 file reading / Чтение файлов M3U8
 * fclose()                    - Temp file cleanup / Очистка временных файлов
 * _stricmp()                  - M3U/M3U8 extension matching / Сопоставление расширений M3U/M3U8
 * GetPrivateProfileStringA()  - PLS file reading / Чтение файлов PLS
 * GetOpenFileNameA()          - File dialog filter / Фильтр диалога файлов
 * 
 * THREAD SAFETY / ПОТОКОБЕЗОПАСНОСТЬ:
 * Uses TLS (Thread Local Storage) to prevent recursive hook calls.
 * Falls back to global atomic guards if TLS unavailable.
 * Critical sections protect temp file cache.
 * 
 * Использует TLS (Thread Local Storage) для предотвращения рекурсивных вызовов хуков.
 * Использует глобальные атомарные защиты, если TLS недоступен.
 * Критические секции защищают кеш временных файлов.
 * 
 ******************************************************************************/

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commdlg.h>
#include <stdio.h>
#include <string.h>
#include "..\\Decrypt Engine\\unicode_decrypt_engine.h"
#include "..\\SwitchLangUI.h"

/*******************************************************************************
 * CONFIGURATION
 * КОНФИГУРАЦИЯ
 ******************************************************************************/

#define PL_DEBUG 0  // Debug logging (0=off, 1=on) / Отладочное логирование (0=выкл, 1=вкл)
#define PL_SANITY_LIMIT (64u*1024u*1024u)  // 64MB max playlist size / Макс. размер плейлиста 64МБ

// IAT Patcher / Патчер IAT
extern "C" BOOL IAT_PatchByName(HMODULE hMod, const char* importDll, const char* funcName, 
                                void* hookFunc, void** ppOrigFunc);

/*******************************************************************************
 * HELPER FUNCTIONS: EXTENSIONS AND PATHS
 * ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ: РАСШИРЕНИЯ И ПУТИ
 ******************************************************************************/

/*******************************************************************************
 * PL_ToLower
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Simple ASCII lowercase conversion.
 * Простое преобразование ASCII в нижний регистр.
 ******************************************************************************/
static char PL_ToLower(char c) {
    return (c >= 'A' && c <= 'Z') ? (char)(c - 'A' + 'a') : c;
}

/*******************************************************************************
 * PL_CheckExt
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Checks if string ends with given extension (case-insensitive).
 * Проверяет, заканчивается ли строка данным расширением (без учёта регистра).
 ******************************************************************************/
static BOOL PL_CheckExt(const char* s, const char* ext) {
    if (!s || !ext) return FALSE;
    const char* p = s;
    const char* dot = strrchr(s, '.');
    if (dot) p = dot;

    while (*p && *ext) {
        if (PL_ToLower(*p) != PL_ToLower(*ext)) return FALSE;
        p++; ext++;
    }
    return (*p == 0 && *ext == 0);
}

/*******************************************************************************
 * PL_MatchExtLoose
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Matches extension without requiring dot prefix.
 * Сопоставляет расширение без требования префикса точки.
 * 
 * EXAMPLE / ПРИМЕР:
 * PL_MatchExtLoose("file.m3u8", "m3u8") > TRUE
 * PL_MatchExtLoose("file.m3u8", ".m3u8") > FALSE
 ******************************************************************************/
static BOOL PL_MatchExtLoose(const char* s, const char* extNoDot) {
    if (!s || !extNoDot) return FALSE;
    const char* p = strrchr(s, '.');
    if (p) p++; else p = s;

    while (*p && *extNoDot) {
        if (PL_ToLower(*p) != PL_ToLower(*extNoDot)) return FALSE;
        p++; extNoDot++;
    }
    return (*p == 0 && *extNoDot == 0);
}

/*******************************************************************************
 * File Type Detection
 * Определение типа файла
 ******************************************************************************/
static BOOL IsPlaylistFile(const char* path) {
    return PL_MatchExtLoose(path, "m3u") || 
           PL_MatchExtLoose(path, "m3u8") || 
           PL_MatchExtLoose(path, "pls");
}

static BOOL IsPlsFile(const char* path) {
    return PL_MatchExtLoose(path, "pls");
}

static BOOL IsM3u8File(const char* path) {
    return PL_MatchExtLoose(path, "m3u8");
}

/*******************************************************************************
 * UNICODE TO ANSI CONVERTER
 * КОНВЕРТЕР UNICODE В ANSI
 ******************************************************************************/

/*******************************************************************************
 * ReadFileToBuf
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Reads entire file into memory buffer.
 * Читает весь файл в буфер памяти.
 * 
 * PARAMETERS / ПАРАМЕТРЫ:
 * path   - File path / Путь к файлу
 * outBuf - Output: allocated buffer (caller must free) / Выход: выделенный буфер (вызывающая сторона должна освободить)
 * outSz  - Output: buffer size / Выход: размер буфера
 * 
 * RETURNS / ВОЗВРАЩАЕТ:
 * TRUE on success, FALSE on failure
 * TRUE при успехе, FALSE при неудаче
 * 
 * NOTES / ПРИМЕЧАНИЯ:
 * Enforces PL_SANITY_LIMIT to prevent memory exhaustion.
 * Применяет PL_SANITY_LIMIT для предотвращения исчерпания памяти.
 ******************************************************************************/
static BOOL ReadFileToBuf(const char* path, BYTE** outBuf, DWORD* outSz) {
    if (!outBuf || !outSz || !path) return FALSE;
    *outBuf = NULL; 
    *outSz = 0;

    // Open file / Открыть файл
    HANDLE h = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, 
                          NULL, OPEN_EXISTING, 0, NULL);
    if (h == INVALID_HANDLE_VALUE) return FALSE;

    // Check file size / Проверить размер файла
    DWORD sz = GetFileSize(h, NULL);
    if (sz == 0 || sz == INVALID_FILE_SIZE || sz > PL_SANITY_LIMIT) {
        CloseHandle(h);
        return FALSE;
    }

    // Allocate buffer / Выделить буфер
    BYTE* b = (BYTE*)HeapAlloc(GetProcessHeap(), 0, sz);
    if (!b) {
        CloseHandle(h);
        return FALSE;
    }

    // Read file / Прочитать файл
    DWORD rd = 0;
    BOOL ok = ReadFile(h, b, sz, &rd, NULL);
    CloseHandle(h);

    if (!ok || rd == 0) {
        HeapFree(GetProcessHeap(), 0, b);
        return FALSE;
    }

    *outBuf = b;
    *outSz = rd;
    return TRUE;
}

/*******************************************************************************
 * CreateAnsiTemp
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Converts Unicode playlist file to ANSI temporary file.
 * Преобразует Unicode файл плейлиста во временный ANSI файл.
 * 
 * PARAMETERS / ПАРАМЕТРЫ:
 * srcPath  - Source playlist file path / Путь к исходному файлу плейлиста
 * outTemp  - Output: path to created temp file / Выход: путь к созданному временному файлу
 * forcePls - Force PLS format (unused) / Принудительный формат PLS (не используется)
 * 
 * RETURNS / ВОЗВРАЩАЕТ:
 * TRUE if temp file created successfully
 * TRUE если временный файл создан успешно
 * 
 * ALGORITHM / АЛГОРИТМ:
 * 1. Read source file into buffer
 * 2. Detect encoding:
 *    - UTF-16LE (FF FE BOM) > WideCharToMultiByte
 *    - UTF-8 (EF BB BF BOM or auto-detect) > UTF8 to ANSI conversion
 *    - ANSI > pass through
 * 3. Create temp file in system temp directory
 * 4. Write ANSI data to temp file
 * 5. Return temp file path
 * 
 * 1. Прочитать исходный файл в буфер
 * 2. Определить кодировку:
 *    - UTF-16LE (FF FE BOM) > WideCharToMultiByte
 *    - UTF-8 (EF BB BF BOM или авто-определение) > конвертация UTF8 в ANSI
 *    - ANSI > передать как есть
 * 3. Создать временный файл в системном временном каталоге
 * 4. Записать ANSI данные во временный файл
 * 5. Вернуть путь временного файла
 ******************************************************************************/
static BOOL CreateAnsiTemp(const char* srcPath, char* outTemp, BOOL forcePls) {
    (void)forcePls;

    if (!srcPath || !outTemp) return FALSE;
    outTemp[0] = 0;

    // Read source file / Прочитать исходный файл
    BYTE* raw = NULL; 
    DWORD rSz = 0;
    if (!ReadFileToBuf(srcPath, &raw, &rSz)) return FALSE;

    BYTE* ansi = NULL; 
    DWORD aSz = 0;

    // Detect and convert encoding / Определить и преобразовать кодировку
    // UTF-16LE BOM (FF FE)
    if (rSz >= 2 && raw[0] == 0xFF && raw[1] == 0xFE) {
        int wLen = (int)((rSz - 2) / 2);
        WCHAR* wBuf = (WCHAR*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, 
                                        (wLen + 1) * sizeof(WCHAR));
        if (!wBuf) { 
            HeapFree(GetProcessHeap(), 0, raw); 
            return FALSE; 
        }

        memcpy(wBuf, raw + 2, wLen * sizeof(WCHAR));
        wBuf[wLen] = 0;

        // Convert UTF-16LE to ANSI / Преобразовать UTF-16LE в ANSI
        int need = wLen * 3 + 4;
        ansi = (BYTE*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, need);
        if (ansi) {
            int converted = DECRYPT_WideToACP(wBuf, (char*)ansi, need);
            if (converted > 0) {
                ansi[need - 1] = 0;
                aSz = (DWORD)lstrlenA((char*)ansi);
            }
        }
        HeapFree(GetProcessHeap(), 0, wBuf);
    }
    // UTF-8 or ANSI
    else {
        char* src = (char*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, rSz + 1);
        if (!src) { 
            HeapFree(GetProcessHeap(), 0, raw); 
            return FALSE; 
        }

        memcpy(src, raw, rSz);
        src[rSz] = 0;

        char* start = src;
        
        // Skip UTF-8 BOM (EF BB BF) / Пропустить UTF-8 BOM
        if (rSz >= 3 && 
            (unsigned char)src[0] == 0xEF && 
            (unsigned char)src[1] == 0xBB && 
            (unsigned char)src[2] == 0xBF) {
            start = src + 3;
        }

        // Convert UTF-8 to ANSI / Преобразовать UTF-8 в ANSI
        int need = (int)rSz * 3 + 4;
        ansi = (BYTE*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, need);
        if (ansi) {
            int res = DECRYPT_ToACP_Best(start, (char*)ansi, need);
            if (res > 0) {
                ansi[need - 1] = 0;
                aSz = (DWORD)lstrlenA((char*)ansi);
            }
            else {
                // Fallback: treat as ANSI / Резерв: считать ANSI
                lstrcpynA((char*)ansi, start, need);
                aSz = (DWORD)lstrlenA((char*)ansi);
            }
        }

        HeapFree(GetProcessHeap(), 0, src);
    }

    HeapFree(GetProcessHeap(), 0, raw);

    if (!ansi || aSz == 0) {
        if (ansi) HeapFree(GetProcessHeap(), 0, ansi);
        return FALSE;
    }

    // Trim trailing nulls / Обрезать завершающие нули
    while (aSz > 0 && ansi[aSz - 1] == 0) aSz--;
    if (aSz == 0) {
        HeapFree(GetProcessHeap(), 0, ansi);
        return FALSE;
    }

    // Get temp directory / Получить временный каталог
    char tmpDir[MAX_PATH];
    DWORD tLen = GetTempPathA(MAX_PATH, tmpDir);
    if (tLen == 0 || tLen >= MAX_PATH) {
        HeapFree(GetProcessHeap(), 0, ansi);
        return FALSE;
    }

    // Create unique temp file name / Создать уникальное имя временного файла
    if (!GetTempFileNameA(tmpDir, "waPL", 0, outTemp)) {
        HeapFree(GetProcessHeap(), 0, ansi);
        return FALSE;
    }

    // Write ANSI data to temp file / Записать ANSI данные во временный файл
    HANDLE h = CreateFileA(outTemp, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 
                          FILE_ATTRIBUTE_TEMPORARY | FILE_ATTRIBUTE_HIDDEN, NULL);
    if (h == INVALID_HANDLE_VALUE) {
        DeleteFileA(outTemp);
        outTemp[0] = 0;
        HeapFree(GetProcessHeap(), 0, ansi);
        return FALSE;
    }

    DWORD wr = 0;
    BOOL ok = WriteFile(h, ansi, aSz, &wr, NULL);
    CloseHandle(h);

    HeapFree(GetProcessHeap(), 0, ansi);

    if (!ok || wr != aSz) {
        DeleteFileA(outTemp);
        outTemp[0] = 0;
        return FALSE;
    }

    return TRUE;
}

/*******************************************************************************
 * CACHE MANAGER (TEMPORARY FILE TRACKING)
 * МЕНЕДЖЕР КЕША (ОТСЛЕЖИВАНИЕ ВРЕМЕННЫХ ФАЙЛОВ)
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Tracks temporary ANSI files so they can be deleted after use.
 * Two types of temp files:
 * - M3U8: tracked by FILE* pointer (closed via fclose)
 * - PLS: tracked by original path (read via GetPrivateProfileStringA)
 * 
 * Отслеживает временные ANSI файлы для удаления после использования.
 * Два типа временных файлов:
 * - M3U8: отслеживается по указателю FILE* (закрывается через fclose)
 * - PLS: отслеживается по оригинальному пути (читается через GetPrivateProfileStringA)
 ******************************************************************************/

// M3U8 temp file node (linked to FILE*) / Узел временного файла M3U8 (привязан к FILE*)
typedef struct { 
    FILE* fp;           // File pointer / Указатель на файл
    char path[MAX_PATH]; // Temp file path / Путь временного файла
    void* next;         // Next node / Следующий узел
} TmpNode;

// PLS temp file node (linked to original path) / Узел временного файла PLS (привязан к оригинальному пути)
typedef struct { 
    char orig[MAX_PATH]; // Original file path / Путь оригинального файла
    char tmp[MAX_PATH];  // Temp file path / Путь временного файла
    void* next;          // Next node / Следующий узел
} PlsNode;

static TmpNode* g_HeadM3U = NULL;  // M3U8 temp file list / Список временных файлов M3U8
static PlsNode* g_HeadPLS = NULL;  // PLS temp file list / Список временных файлов PLS

// Critical section state / Состояние критической секции
static CRITICAL_SECTION g_cs;
static LONG g_csState = 0; // 0=uninit, 1=initing, 2=ready, -1=disabled

/*******************************************************************************
 * Lock/Unlock
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Thread-safe critical section with lazy initialization and error handling.
 * Потокобезопасная критическая секция с ленивой инициализацией и обработкой ошибок.
 * 
 * CRITICAL / КРИТИЧНО:
 * Uses atomic operations to ensure only one thread initializes critical section.
 * If initialization fails, disables critical section permanently (g_csState=-1).
 * 
 * Использует атомарные операции для обеспечения инициализации критической секции
 * только одним потоком. При неудаче инициализации отключает критическую секцию
 * навсегда (g_csState=-1).
 ******************************************************************************/
static void Lock(void) {
    if (g_csState == 2) { 
        EnterCriticalSection(&g_cs); 
        return; 
    }
    if (g_csState == -1) return;

    // Lazy initialization with double-checked locking
    // Ленивая инициализация с двойной проверкой блокировки
    for (;;) {
        LONG s = g_csState;
        if (s == 2) { 
            EnterCriticalSection(&g_cs); 
            return; 
        }
        if (s == -1) return;

        if (s == 0) {
            // Try to acquire initialization right / Попытаться получить право инициализации
            if (InterlockedCompareExchange(&g_csState, 1, 0) == 0) {
                BOOL ok = TRUE;
                __try {
                    InitializeCriticalSection(&g_cs);
                }
                __except (EXCEPTION_EXECUTE_HANDLER) {
                    ok = FALSE;
                }

                if (ok) {
                    InterlockedExchange(&g_csState, 2);
                    EnterCriticalSection(&g_cs);
                    return;
                }
                // Initialization failed, disable permanently
                // Инициализация не удалась, отключить навсегда
                InterlockedExchange(&g_csState, -1);
                return;
            }
        }
        Sleep(0);  // Yield to other threads / Уступить другим потокам
    }
}

static void Unlock(void) {
    if (g_csState == 2) 
        LeaveCriticalSection(&g_cs);
}

/*******************************************************************************
 * M3U8 Temp File Tracking
 * Отслеживание временных файлов M3U8
 ******************************************************************************/

// Add M3U8 temp file to tracking list / Добавить временный файл M3U8 в список отслеживания
static void TrackM3U(FILE* fp, const char* path) {
    if (!fp || !path) return;
    TmpNode* n = (TmpNode*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(TmpNode));
    if (!n) return;

    n->fp = fp;
    lstrcpynA(n->path, path, MAX_PATH);

    Lock();
    n->next = g_HeadM3U;
    g_HeadM3U = n;
    Unlock();
}

// Remove M3U8 temp file from tracking list / Удалить временный файл M3U8 из списка отслеживания
static BOOL UntrackM3U(FILE* fp, char* outPath) {
    if (outPath) outPath[0] = 0;
    if (!fp) return FALSE;

    Lock();
    TmpNode** pp = &g_HeadM3U;
    while (*pp) {
        if ((*pp)->fp == fp) {
            TmpNode* d = *pp;
            *pp = (TmpNode*)d->next;
            if (outPath) lstrcpynA(outPath, d->path, MAX_PATH);
            HeapFree(GetProcessHeap(), 0, d);
            Unlock();
            return TRUE;
        }
        pp = (TmpNode**)&((*pp)->next);
    }
    Unlock();
    return FALSE;
}

/*******************************************************************************
 * PLS Temp File Caching
 * Кеширование временных файлов PLS
 ******************************************************************************/

// Get cached PLS temp file / Получить кешированный временный файл PLS
static BOOL GetCachedPLS(const char* orig, char* outTmp) {
    if (!orig || !outTmp) return FALSE;
    outTmp[0] = 0;

    Lock();
    PlsNode* p = g_HeadPLS;
    while (p) {
        if (lstrcmpiA(p->orig, orig) == 0) {
            lstrcpynA(outTmp, p->tmp, MAX_PATH);
            Unlock();
            return TRUE;
        }
        p = (PlsNode*)p->next;
    }
    Unlock();
    return FALSE;
}

// Add PLS temp file to cache / Добавить временный файл PLS в кеш
static void CachePLS(const char* orig, const char* tmp) {
    if (!orig || !tmp) return;

    PlsNode* n = (PlsNode*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(PlsNode));
    if (!n) return;

    lstrcpynA(n->orig, orig, MAX_PATH);
    lstrcpynA(n->tmp, tmp, MAX_PATH);

    Lock();
    n->next = g_HeadPLS;
    g_HeadPLS = n;
    Unlock();
}

/*******************************************************************************
 * ClearAllCache
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Clears all temp file caches and deletes temp files.
 * Called on module shutdown.
 * 
 * Очищает все кеши временных файлов и удаляет временные файлы.
 * Вызывается при завершении модуля.
 ******************************************************************************/
static void ClearAllCache(void) {
    if (g_csState != 2 && g_csState != -1) {
        // Critical section not initialized / Критическая секция не инициализирована
        return;
    }

    Lock();
    
    // Clear M3U8 temp files / Очистить временные файлы M3U8
    while (g_HeadM3U) {
        TmpNode* d = g_HeadM3U;
        g_HeadM3U = (TmpNode*)d->next;
        DeleteFileA(d->path);
        HeapFree(GetProcessHeap(), 0, d);
    }
    
    // Clear PLS temp files / Очистить временные файлы PLS
    while (g_HeadPLS) {
        PlsNode* d = g_HeadPLS;
        g_HeadPLS = (PlsNode*)d->next;
        DeleteFileA(d->tmp);
        HeapFree(GetProcessHeap(), 0, d);
    }
    
    Unlock();

    // Delete critical section / Удалить критическую секцию
    if (g_csState == 2) {
        DeleteCriticalSection(&g_cs);
    }
    g_csState = 0;
}

/*******************************************************************************
 * HOOK IMPLEMENTATIONS
 * РЕАЛИЗАЦИИ ХУКОВ
 ******************************************************************************/

/*******************************************************************************
 * Original Function Pointers
 * Указатели на оригинальные функции
 ******************************************************************************/
typedef FILE* (__cdecl *PFN_fopen)(const char*, const char*);
typedef int   (__cdecl *PFN_fclose)(FILE*);
typedef int   (__cdecl *PFN_stricmp)(const char*, const char*);
typedef DWORD (WINAPI *PFN_GPPSA)(LPCSTR, LPCSTR, LPCSTR, LPSTR, DWORD, LPCSTR);
typedef BOOL  (WINAPI *PFN_GOFNA)(LPOPENFILENAMEA);

static PFN_fopen   Real_fopen   = NULL;
static PFN_fclose  Real_fclose  = NULL;
static PFN_stricmp Real_stricmp = NULL;
static PFN_GPPSA   Real_GPPSA   = NULL;
static PFN_GOFNA   Real_GOFNA   = NULL;

/*******************************************************************************
 * RECURSION GUARDS (TLS-BASED)
 * ЗАЩИТА ОТ РЕКУРСИИ (НА ОСНОВЕ TLS)
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Prevents infinite recursion when our hooks call functions that may trigger
 * the same hooks again. Uses TLS for per-thread tracking.
 * 
 * Предотвращает бесконечную рекурсию, когда наши хуки вызывают функции, которые
 * могут снова запустить те же хуки. Использует TLS для отслеживания на каждый поток.
 * 
 * FALLBACK / РЕЗЕРВ:
 * If TLS allocation fails, uses global atomic counters (less precise but safe).
 * Если выделение TLS не удаётся, использует глобальные атомарные счётчики (менее точно, но безопасно).
 ******************************************************************************/
static DWORD g_tlsFopen = TLS_OUT_OF_INDEXES;  // TLS slot for fopen guard / Слот TLS для защиты fopen
static DWORD g_tlsGPPS  = TLS_OUT_OF_INDEXES;  // TLS slot for GPPSA guard / Слот TLS для защиты GPPSA

// Fallback global guards if TLS not available / Резервные глобальные защиты, если TLS недоступен
static LONG g_guardFopen = 0;
static LONG g_guardGPPS  = 0;

/*******************************************************************************
 * GuardEnter_TlsOrGlobal
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Enters recursion guard using TLS if available, global counter otherwise.
 * Входит в защиту рекурсии, используя TLS если доступен, иначе глобальный счётчик.
 * 
 * RETURNS / ВОЗВРАЩАЕТ:
 * TRUE if guard acquired (safe to proceed)
 * FALSE if already in hook (recursion detected, skip hook)
 ******************************************************************************/
static BOOL GuardEnter_TlsOrGlobal(DWORD tlsIndex, LONG* pGlobal) {
    if (tlsIndex != TLS_OUT_OF_INDEXES) {
        LPVOID v = TlsGetValue(tlsIndex);
        if (v) return FALSE;  // Already in hook / Уже в хуке
        TlsSetValue(tlsIndex, (LPVOID)1);
        return TRUE;
    }
    
    // Global fallback / Глобальный резерв
    if (InterlockedIncrement(pGlobal) != 1) {
        InterlockedDecrement(pGlobal);
        return FALSE;  // Another thread in hook / Другой поток в хуке
    }
    return TRUE;
}

static void GuardLeave_TlsOrGlobal(DWORD tlsIndex, LONG* pGlobal) {
    if (tlsIndex != TLS_OUT_OF_INDEXES) {
        TlsSetValue(tlsIndex, 0);
    } else {
        InterlockedDecrement(pGlobal);
    }
}

/*******************************************************************************
 * Hook_fopen (M3U8 HANDLING)
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Intercepts fopen to detect M3U8 files and redirect to ANSI temp file.
 * Перехватывает fopen для обнаружения M3U8 файлов и перенаправления на ANSI временный файл.
 * 
 * ALGORITHM / АЛГОРИТМ:
 * 1. Check if opening M3U8 file for reading
 * 2. Enter recursion guard
 * 3. Create ANSI temp file
 * 4. Open temp file instead of original
 * 5. Track FILE* pointer for cleanup
 * 6. Return FILE* to temp file
 * 
 * 1. Проверить, открывается ли M3U8 файл для чтения
 * 2. Войти в защиту рекурсии
 * 3. Создать ANSI временный файл
 * 4. Открыть временный файл вместо оригинального
 * 5. Отследить указатель FILE* для очистки
 * 6. Вернуть FILE* на временный файл
 ******************************************************************************/
static FILE* __cdecl Hook_fopen(const char* path, const char* mode) {
    if (!Real_fopen) return NULL;
    if (!path || !mode) return Real_fopen(path, mode);

    // Hook only for reading M3U8 files / Хук только для чтения M3U8 файлов
    if (mode[0] == 'r' && IsM3u8File(path)) {
        if (!GuardEnter_TlsOrGlobal(g_tlsFopen, &g_guardFopen)) {
            return Real_fopen(path, mode);
        }

        FILE* fp = NULL;
        char tmp[MAX_PATH];
        if (CreateAnsiTemp(path, tmp, FALSE)) {
            fp = Real_fopen(tmp, mode);
            if (fp) {
                TrackM3U(fp, tmp);  // Track for cleanup / Отследить для очистки
            } else {
                DeleteFileA(tmp);
            }
        }

        GuardLeave_TlsOrGlobal(g_tlsFopen, &g_guardFopen);
        if (fp) return fp;
    }

    return Real_fopen(path, mode);
}

/*******************************************************************************
 * Hook_fclose
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Intercepts fclose to delete temp M3U8 files.
 * Перехватывает fclose для удаления временных M3U8 файлов.
 ******************************************************************************/
static int __cdecl Hook_fclose(FILE* fp) {
    if (!Real_fclose) return EOF;
    
    char tmp[MAX_PATH];
    BOOL myFile = UntrackM3U(fp, tmp);  // Check if it's our temp file / Проверить, наш ли это временный файл
    
    int res = Real_fclose(fp);
    
    if (myFile) 
        DeleteFileA(tmp);  // Delete temp file / Удалить временный файл
    
    return res;
}

/*******************************************************************************
 * Hook_stricmp (M3U VS M3U8 MATCHING)
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Makes M3U and M3U8 extensions equivalent for Winamp's extension checks.
 * Winamp checks if file extension matches playlist type; we want both to match.
 * 
 * Делает расширения M3U и M3U8 эквивалентными для проверок расширений Winamp.
 * Winamp проверяет, совпадает ли расширение файла с типом плейлиста; мы хотим, чтобы оба совпадали.
 * 
 * ALGORITHM / АЛГОРИТМ:
 * If both strings are M3U or M3U8 extensions, return 0 (equal).
 * Otherwise, use real _stricmp.
 * 
 * Если обе строки - расширения M3U или M3U8, вернуть 0 (равны).
 * Иначе использовать настоящий _stricmp.
 ******************************************************************************/
static int __cdecl Hook_stricmp(const char* s1, const char* s2) {
    if (!s1 && !s2) return 0;
    if (!s1) return -1;
    if (!s2) return 1;

    // Check if both are M3U/M3U8 / Проверить, оба ли M3U/M3U8
    BOOL a = PL_MatchExtLoose(s1, "m3u") || PL_MatchExtLoose(s1, "m3u8");
    BOOL b = PL_MatchExtLoose(s2, "m3u") || PL_MatchExtLoose(s2, "m3u8");
    if (a && b) return 0;  // Treat as equal / Считать равными

    return Real_stricmp ? Real_stricmp(s1, s2) : lstrcmpiA(s1, s2);
}

/*******************************************************************************
 * Hook_GPPSA (PLS FILE HANDLING)
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Intercepts GetPrivateProfileStringA to handle Unicode PLS files.
 * PLS files use INI format read via GetPrivateProfileStringA.
 * 
 * Перехватывает GetPrivateProfileStringA для обработки Unicode PLS файлов.
 * PLS файлы используют формат INI, читаемый через GetPrivateProfileStringA.
 * 
 * ALGORITHM / АЛГОРИТМ:
 * 1. Check if reading from PLS file
 * 2. Check cache for existing temp file
 * 3. If not cached: create temp file and cache it
 * 4. Redirect GetPrivateProfileStringA to temp file
 * 
 * 1. Проверить, читается ли из PLS файла
 * 2. Проверить кеш на существующий временный файл
 * 3. Если не кешировано: создать временный файл и закешировать его
 * 4. Перенаправить GetPrivateProfileStringA на временный файл
 * 
 * CACHING / КЕШИРОВАНИЕ:
 * PLS files are read multiple times (once per entry), so we cache the temp file
 * to avoid recreating it on each read.
 * 
 * PLS файлы читаются многократно (один раз на запись), поэтому кешируем временный
 * файл, чтобы избежать пересоздания при каждом чтении.
 ******************************************************************************/
static DWORD WINAPI Hook_GPPSA(LPCSTR App, LPCSTR Key, LPCSTR Def, LPSTR Ret, DWORD Sz, LPCSTR File) {
    if (Real_GPPSA && File && IsPlsFile(File)) {
        if (!GuardEnter_TlsOrGlobal(g_tlsGPPS, &g_guardGPPS)) {
            return Real_GPPSA(App, Key, Def, Ret, Sz, File);
        }

        DWORD res = 0;
        char tmp[MAX_PATH];
        
        // Check cache / Проверить кеш
        if (GetCachedPLS(File, tmp)) {
            res = Real_GPPSA(App, Key, Def, Ret, Sz, tmp);
        } else {
            // Create and cache temp file / Создать и закешировать временный файл
            if (CreateAnsiTemp(File, tmp, TRUE)) {
                CachePLS(File, tmp);
                res = Real_GPPSA(App, Key, Def, Ret, Sz, tmp);
            } else {
                res = Real_GPPSA(App, Key, Def, Ret, Sz, File);
            }
        }

        GuardLeave_TlsOrGlobal(g_tlsGPPS, &g_guardGPPS);
        return res;
    }

    return Real_GPPSA ? Real_GPPSA(App, Key, Def, Ret, Sz, File) : 0;
}

/*******************************************************************************
 * Hook_GOFNA (FILE DIALOG FILTER)
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Modifies playlist file dialogs to include M3U8 in filter.
 * Изменяет диалоги файлов плейлистов для включения M3U8 в фильтр.
 * 
 * ALGORITHM / АЛГОРИТМ:
 * 1. Detect if dialog is for playlists (by title or filter)
 * 2. Replace filter with custom one including M3U8
 * 3. Adjust filter index if needed
 * 
 * 1. Определить, является ли диалог для плейлистов (по заголовку или фильтру)
 * 2. Заменить фильтр на пользовательский, включающий M3U8
 * 3. Скорректировать индекс фильтра при необходимости
 * 
 * FILTER FORMAT / ФОРМАТ ФИЛЬТРА:
 * String with double-null termination:
 * "All Playlists\0*.m3u;*.m3u8;*.pls\0M3U Files\0*.m3u;*.m3u8\0PLS Files\0*.pls\0\0"
 * 
 * Строка с двойным null завершением:
 * "Все плейлисты\0*.m3u;*.m3u8;*.pls\0M3U файлы\0*.m3u;*.m3u8\0PLS файлы\0*.pls\0\0"
 ******************************************************************************/
static char g_Filter[512] = { 0 };  // Custom filter string / Пользовательская строка фильтра

static BOOL WINAPI Hook_GOFNA(LPOPENFILENAMEA ofn) {
    if (ofn && Real_GOFNA) {
        BOOL isPL = FALSE;
        
        // Detect playlist dialog / Определить диалог плейлистов
        if (ofn->lpstrTitle && 
            (strstr(ofn->lpstrTitle, "Playlist") || strstr(ofn->lpstrTitle, "лейлист"))) 
            isPL = TRUE;
        if (!isPL && ofn->lpstrFilter && 
            (strstr(ofn->lpstrFilter, "M3U") || strstr(ofn->lpstrFilter, "m3u"))) 
            isPL = TRUE;

        if (isPL) {
            // Build custom filter once / Построить пользовательский фильтр один раз
            if (!g_Filter[0]) {
                char* p = g_Filter;
                
                // All Playlists / Все плейлисты
                lstrcpyA(p, ALL_PL);      
                p += lstrlenA(p) + 1; 
                lstrcpyA(p, "*.m3u;*.m3u8;*.pls"); 
                p += lstrlenA(p) + 1;
                
                // M3U Files / M3U файлы
                lstrcpyA(p, M3U_FILES);   
                p += lstrlenA(p) + 1; 
                lstrcpyA(p, "*.m3u;*.m3u8");      
                p += lstrlenA(p) + 1;
                
                // PLS Files / PLS файлы
                lstrcpyA(p, PLS_FILES);   
                p += lstrlenA(p) + 1; 
                lstrcpyA(p, "*.pls");              
                p += lstrlenA(p) + 1;
                
                *p = 0;  // Double-null termination / Двойное null завершение
            }
            
            ofn->lpstrFilter = g_Filter;
            if (ofn->nFilterIndex < 1 || ofn->nFilterIndex > 3) 
                ofn->nFilterIndex = 1;
        }
    }
    return Real_GOFNA ? Real_GOFNA(ofn) : FALSE;
}

/*******************************************************************************
 * INSTALLATION LOGIC
 * ЛОГИКА УСТАНОВКИ
 ******************************************************************************/

/*******************************************************************************
 * PatchModule
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Patches IAT of a module to install all hooks.
 * Патчит IAT модуля для установки всех хуков.
 * 
 * PARAMETERS / ПАРАМЕТРЫ:
 * hMod - Module to patch (exe or dll) / Модуль для патчинга (exe или dll)
 * 
 * HOOKS INSTALLED / УСТАНОВЛЕННЫЕ ХУКИ:
 * 1. CRT functions (MSVCRT.dll): fopen, fclose, _stricmp
 * 2. Kernel32: GetPrivateProfileStringA
 * 3. ComDlg32: GetOpenFileNameA
 * 
 * 1. CRT функции (MSVCRT.dll): fopen, fclose, _stricmp
 * 2. Kernel32: GetPrivateProfileStringA
 * 3. ComDlg32: GetOpenFileNameA
 * 
 * NOTES / ПРИМЕЧАНИЯ:
 * Tries multiple CRT DLL names for compatibility (MSVCRT, MSVCR71, MSVCR70).
 * Пробует несколько имён CRT DLL для совместимости (MSVCRT, MSVCR71, MSVCR70).
 ******************************************************************************/
static void PatchModule(HMODULE hMod) {
    if (!hMod) return;

    // 1. CRT Hooks (fopen, fclose, _stricmp) / CRT хуки
    const char* crts[] = { "MSVCRT.dll", "msvcrt.dll", "MSVCR71.dll", "MSVCR70.dll" };
    for (int i = 0; i < 4; i++) {
        HMODULE hC = GetModuleHandleA(crts[i]);
        if (!hC) continue;

        // Get original function pointers / Получить оригинальные указатели функций
        if (!Real_fopen)   Real_fopen   = (PFN_fopen)GetProcAddress(hC, "fopen");
        if (!Real_fclose)  Real_fclose  = (PFN_fclose)GetProcAddress(hC, "fclose");
        if (!Real_stricmp) Real_stricmp = (PFN_stricmp)GetProcAddress(hC, "_stricmp");

        // Patch IAT / Пропатчить IAT
        if (Real_fopen)   IAT_PatchByName(hMod, crts[i], "fopen",   (void*)Hook_fopen,   NULL);
        if (Real_fclose)  IAT_PatchByName(hMod, crts[i], "fclose",  (void*)Hook_fclose,  NULL);
        if (Real_stricmp) IAT_PatchByName(hMod, crts[i], "_stricmp", (void*)Hook_stricmp, NULL);

        // Some CRTs export as "stricmp" without underscore / Некоторые CRT экспортируют как "stricmp" без подчёркивания
        if (Real_stricmp) IAT_PatchByName(hMod, crts[i], "stricmp", (void*)Hook_stricmp, NULL);
    }

    // 2. Kernel32 Hooks (GetPrivateProfileStringA) / Хуки Kernel32
    if (!Real_GPPSA) {
        HMODULE hK = GetModuleHandleA("KERNEL32.dll");
        if (hK) Real_GPPSA = (PFN_GPPSA)GetProcAddress(hK, "GetPrivateProfileStringA");
    }
    if (Real_GPPSA) {
        IAT_PatchByName(hMod, "KERNEL32.dll", "GetPrivateProfileStringA", (void*)Hook_GPPSA, NULL);
    }

    // 3. ComDlg32 Hooks (GetOpenFileNameA) / Хуки ComDlg32
    if (!Real_GOFNA) {
        HMODULE hD = GetModuleHandleA("COMDLG32.dll");
        if (hD) Real_GOFNA = (PFN_GOFNA)GetProcAddress(hD, "GetOpenFileNameA");
    }
    if (Real_GOFNA) {
        IAT_PatchByName(hMod, "COMDLG32.dll", "GetOpenFileNameA", (void*)Hook_GOFNA, NULL);
    }
}

/*******************************************************************************
 * InitTimer
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Timer callback that waits for gen_ml.dll to load, then patches it.
 * Media Library (gen_ml.dll) may not be loaded immediately.
 * 
 * Функция обратного вызова таймера, которая ждёт загрузки gen_ml.dll, затем патчит её.
 * Библиотека (gen_ml.dll) может быть не загружена сразу.
 ******************************************************************************/
static VOID CALLBACK InitTimer(HWND, UINT, UINT_PTR id, DWORD) {
    HMODULE hML = GetModuleHandleA("gen_ml.dll");
    if (hML) {
        PatchModule(hML);
        KillTimer(NULL, id);
    }
}

/*******************************************************************************
 * PUBLIC API
 * ПУБЛИЧНЫЙ API
 ******************************************************************************/

/*******************************************************************************
 * m3u8_Loader_Init
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Initializes the M3U8 loader module. Allocates TLS slots and installs hooks.
 * Инициализирует модуль загрузчика M3U8. Выделяет слоты TLS и устанавливает хуки.
 ******************************************************************************/
extern "C" void m3u8_Loader_Init(void) {
    // Allocate TLS slots for recursion guards / Выделить слоты TLS для защиты от рекурсии
    if (g_tlsFopen == TLS_OUT_OF_INDEXES) {
        g_tlsFopen = TlsAlloc();
        if (g_tlsFopen == TLS_OUT_OF_INDEXES) {
            // Fallback to global guard / Резерв на глобальную защиту
        }
    }
    if (g_tlsGPPS == TLS_OUT_OF_INDEXES) {
        g_tlsGPPS = TlsAlloc();
        if (g_tlsGPPS == TLS_OUT_OF_INDEXES) {
            // Fallback to global guard / Резерв на глобальную защиту
        }
    }

    // Patch main executable / Пропатчить главный исполняемый файл
    PatchModule(GetModuleHandle(NULL));

    // Wait for gen_ml.dll to load / Ждать загрузки gen_ml.dll
    SetTimer(NULL, 0, 500, InitTimer);
}

/*******************************************************************************
 * m3u8_Loader_Quit
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Cleans up the M3U8 loader module. Deletes all temp files and frees TLS.
 * Очищает модуль загрузчика M3U8. Удаляет все временные файлы и освобождает TLS.
 * 
 * CRITICAL / КРИТИЧНО:
 * Must be called before module unload to prevent temp file leaks.
 * Должна быть вызвана перед выгрузкой модуля для предотвращения утечек временных файлов.
 ******************************************************************************/
extern "C" void m3u8_Loader_Quit(void) {
    // Clear all temp files / Очистить все временные файлы
    ClearAllCache();

    // Free TLS slots / Освободить слоты TLS
    if (g_tlsFopen != TLS_OUT_OF_INDEXES) {
        TlsFree(g_tlsFopen);
        g_tlsFopen = TLS_OUT_OF_INDEXES;
    }
    if (g_tlsGPPS != TLS_OUT_OF_INDEXES) {
        TlsFree(g_tlsGPPS);
        g_tlsGPPS = TLS_OUT_OF_INDEXES;
    }
}