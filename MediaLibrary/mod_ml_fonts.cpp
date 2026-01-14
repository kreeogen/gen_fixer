/*******************************************************************************
 * WINAMP MEDIA LIBRARY AND PLAYLIST SMOOTH FONTS MODULE
 * МОДУЛЬ СГЛАЖЕННЫХ ШРИФТОВ ДЛЯ ПЛЕЙЛИСТА И БИБЛИОТЕКИ WINAMP
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * This module replaces bitmap fonts in Winamp's Media Library and Playlist
 * Editor with smooth, anti-aliased TrueType fonts. It improves text rendering
 * quality on modern displays while maintaining compatibility with classic skins.
 * 
 * Этот модуль заменяет растровые шрифты в библиотеке Winamp и редакторе
 * плейлистов на сглаженные TrueType-шрифты с антиалиасингом. Улучшает качество
 * отрисовки текста на современных дисплеях, сохраняя совместимость с классическими скинами.
 * 
 * HOW IT WORKS / КАК ЭТО РАБОТАЕТ:
 * 
 * 1. FONT INTERCEPTION / ПЕРЕХВАТ ШРИФТОВ:
 *    - Subclasses all controls in Media Library and Playlist Editor windows
 *    - Intercepts WM_SETFONT messages to replace fonts
 *    - Optional IAT hooking of SelectObject to catch GDI font selections
 * 
 *    - Создаёт subclass всех контролов в окнах библиотеки и редактора плейлистов
 *    - Перехватывает сообщения WM_SETFONT для замены шрифтов
 *    - Опциональный перехват SelectObject через IAT для захвата выбора шрифтов GDI
 * 
 * 2. FONT REPLACEMENT / ЗАМЕНА ШРИФТОВ:
 *    - Reads skin font preferences (face, size, charset) from Winamp
 *    - Converts bitmap fonts (MS Sans Serif, System) to vector fonts (Microsoft Sans Serif)
 *    - Applies ClearType or antialiased quality
 *    - Caches replacement fonts to avoid recreation
 * 
 *    - Читает настройки шрифтов скина (гарнитура, размер, кодировка) из Winamp
 *    - Преобразует растровые шрифты (MS Sans Serif, System) в векторные (Microsoft Sans Serif)
 *    - Применяет качество ClearType или antialiased
 *    - Кеширует заменённые шрифты для избежания пересоздания
 * 
 * 3. SPECIAL CONTROL HANDLING / СПЕЦИАЛЬНАЯ ОБРАБОТКА КОНТРОЛОВ:
 *    TreeView: Automatically adjusts item height to fit new fonts
 *    ListView: Forces relayout after font change (especially critical on Win9x)
 * 
 *    TreeView: Автоматически корректирует высоту элементов под новые шрифты
 *    ListView: Принудительно перестраивает макет после смены шрифта (критично на Win9x)
 * 
 * 4. WIN9X COMPATIBILITY / СОВМЕСТИМОСТЬ С WIN9X:
 *    - Uses parent window subclassing to catch child creation (no CBT hook)
 *    - Implements aggressive ListView relayout (view type toggle)
 *    - Polls for font resets that occur during tab/page switches
 *    - Uses skin parameter polling instead of IPC_SKIN_CHANGED (unreliable on 9x)
 * 
 *    - Использует subclassing родительских окон для перехвата создания детей (нет CBT hook)
 *    - Реализует агрессивную перестройку ListView (переключение типа вида)
 *    - Опрашивает сбросы шрифтов, происходящие при переключении вкладок/страниц
 *    - Использует опрос параметров скина вместо IPC_SKIN_CHANGED (ненадёжен на 9x)
 * 
 * CONTEXT SYSTEM / СИСТЕМА КОНТЕКСТОВ:
 * CTX_LIB (1) - Media Library windows use skin-defined fonts
 * CTX_PL  (2) - Playlist Editor uses its own font (learned from first observation)
 * 
 * CTX_LIB (1) - Окна библиотеки используют шрифты, определённые скином
 * CTX_PL  (2) - Редактор плейлистов использует собственный шрифт (изучается при первом наблюдении)
 * 
 * PERFORMANCE OPTIMIZATIONS / ОПТИМИЗАЦИИ ПРОИЗВОДИТЕЛЬНОСТИ:
 * - Font cache with 1024 entries and last-access optimization
 * - DC tagging to avoid repeated context lookups
 * - Stamp-based change detection to skip unnecessary updates
 * - Debounced timers to batch multiple change events
 * 
 * - Кеш шрифтов с 1024 записями и оптимизацией последнего доступа
 * - Тегирование DC для избежания повторных поисков контекста
 * - Обнаружение изменений на основе отпечатков для пропуска ненужных обновлений
 * - Антидребезговые таймеры для пакетирования нескольких событий изменений
 * 
 ******************************************************************************/

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tchar.h>
#include <string.h>
#include <commctrl.h>
#pragma comment(lib, "comctl32.lib")

/*******************************************************************************
 * CONSTANT DEFINITIONS
 * ОПРЕДЕЛЕНИЯ КОНСТАНТ
 ******************************************************************************/

// Windows message for theme changes / Сообщение Windows для изменений темы
#ifndef WM_THEMECHANGED
#define WM_THEMECHANGED 0x031A
#endif

// Font quality values / Значения качества шрифтов
#ifndef CLEARTYPE_QUALITY
#define CLEARTYPE_QUALITY 5  // ClearType anti-aliasing (best for LCDs) / Антиалиасинг ClearType (лучше для LCD)
#endif
#ifndef ANTIALIASED_QUALITY
#define ANTIALIASED_QUALITY 4  // Standard anti-aliasing / Стандартный антиалиасинг
#endif

// PE/DOS signature for module validation / Сигнатура PE/DOS для проверки модуля
#ifndef IMAGE_DOS_SIGNATURE
#define IMAGE_DOS_SIGNATURE 0x5A4D  // "MZ" signature
#endif

// Winamp IPC messages / IPC-сообщения Winamp
#ifndef WM_WA_IPC
#define WM_WA_IPC WM_USER  // Base message for Winamp IPC / Базовое сообщение для Winamp IPC
#endif
#ifndef IPC_GET_GENSKINBITMAP
#define IPC_GET_GENSKINBITMAP 503  // Get skin font parameters / Получить параметры шрифта скина
#endif
#ifndef IPC_GETSKIN
#define IPC_GETSKIN 201  // Get skin directory / Получить каталог скина
#endif
#ifndef IPC_SKIN_CHANGED
#define IPC_SKIN_CHANGED 3018  // Notification: skin was changed / Уведомление: скин был изменён
#endif

// Feature flags for IAT hooking / Флаги функций для перехвата IAT
#ifndef ENABLE_IAT_HOOK_ON_NT
#define ENABLE_IAT_HOOK_ON_NT 1  // Enable IAT hooks on Windows NT/2000/XP+ / Включить хуки IAT на Windows NT/2000/XP+
#endif
#ifndef ENABLE_IAT_HOOK_ON_9X
#define ENABLE_IAT_HOOK_ON_9X 0  // Disable on Win9x (causes crashes on exit) / Отключить на Win9x (вызывает краши при выходе)
#endif

// Enumeration context flag / Флаг контекста перечисления
#define ENUM_CTX_CONFIRMED 1  // EnumWnd called with pre-confirmed context / EnumWnd вызвана с предварительно подтверждённым контекстом

// Window context types / Типы контекстов окон
#define CTX_LIB  1  // Media Library context / Контекст библиотеки
#define CTX_PL   2  // Playlist Editor context / Контекст редактора плейлистов

/*******************************************************************************
 * EXTERNAL FUNCTIONS
 * ВНЕШНИЕ ФУНКЦИИ
 ******************************************************************************/

// IAT patching function (defined elsewhere) / Функция патчинга IAT (определена в другом месте)
extern "C" BOOL IAT_PatchByName(HMODULE, const char*, const char*, void*, void**);

// Get module instance for current DLL / Получить экземпляр модуля для текущей DLL
extern "C" IMAGE_DOS_HEADER __ImageBase;
#define HINST_THIS ((HINSTANCE)&__ImageBase)

/*******************************************************************************
 * GLOBAL STATE
 * ГЛОБАЛЬНОЕ СОСТОЯНИЕ
 ******************************************************************************/

// Critical section for thread-safe access / Критическая секция для потокобезопасного доступа
static CRITICAL_SECTION g_cs;
static BOOL  g_cs_inited = FALSE;  // TRUE if critical section initialized / TRUE если критическая секция инициализирована
static BOOL  g_init = FALSE;       // TRUE if module initialized / TRUE если модуль инициализирован
static volatile LONG g_shuttingDown = 0;  // Atomic flag: shutting down / Атомарный флаг: завершение работы
static DWORD g_pid = 0;            // Current process ID / ID текущего процесса

// OS detection flags / Флаги определения ОС
static BOOL g_isNT = FALSE;   // Windows NT/2000/XP+ / Windows NT/2000/XP+
static BOOL g_is9x = FALSE;   // Windows 95/98/ME / Windows 95/98/ME
static BOOL g_allowIatHook = FALSE;  // IAT hooking enabled / Перехват IAT включён
static BYTE g_forceQuality = DEFAULT_QUALITY;  // Font quality to apply / Качество шрифта для применения

// Default font face / Гарнитура шрифта по умолчанию
static const char kFaceDef[] = "Microsoft Sans Serif";

/*******************************************************************************
 * CRITICAL SECTION HELPERS
 * ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ КРИТИЧЕСКОЙ СЕКЦИИ
 ******************************************************************************/

static void Lock()   { if(g_cs_inited) EnterCriticalSection(&g_cs); }
static void Unlock() { if(g_cs_inited) LeaveCriticalSection(&g_cs); }

/*******************************************************************************
 * Hash
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * FNV-1a hash function for strings. Used to create unique stamps for font
 * configurations and skin parameters.
 * 
 * Хеш-функция FNV-1a для строк. Используется для создания уникальных отпечатков
 * для конфигураций шрифтов и параметров скинов.
 * 
 * ALGORITHM / АЛГОРИТМ:
 * FNV-1a: hash = (hash XOR byte) * FNV_prime
 * Known for good distribution and fast computation.
 * 
 * FNV-1a: хеш = (хеш XOR байт) * FNV_prime
 * Известна хорошим распределением и быстрым вычислением.
 ******************************************************************************/
static DWORD Hash(const char* s) {
    DWORD h = 2166136261u;  // FNV offset basis
    while(s && *s) { 
        h ^= (BYTE)(*s++); 
        h *= 16777619u;  // FNV prime
    }
    return h ? h : 1;  // Never return 0 / Никогда не возвращать 0
}

/*******************************************************************************
 * IsValidPtr
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Safely checks if a pointer is readable using structured exception handling.
 * 
 * Безопасно проверяет, доступен ли указатель для чтения, используя структурную
 * обработку исключений.
 ******************************************************************************/
static BOOL IsValidPtr(const void* p) {
    if(!p) return FALSE;
    __try { 
        volatile char c = *((const char*)p); 
        (void)c; 
    } __except(1) { 
        return FALSE; 
    }
    return TRUE;
}

/*******************************************************************************
 * IsWindowInThisProcess
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Checks if a window belongs to the current process.
 * 
 * Проверяет, принадлежит ли окно текущему процессу.
 ******************************************************************************/
static BOOL IsWindowInThisProcess(HWND h) {
    if(!h) return FALSE;
    DWORD pid = 0; 
    GetWindowThreadProcessId(h, &pid);
    return (pid == GetCurrentProcessId());
}

/*******************************************************************************
 * FONT CACHE SYSTEM
 * СИСТЕМА КЕША ШРИФТОВ
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Caches replacement fonts to avoid recreating them on every SelectObject call.
 * Uses a simple linear search with last-access optimization.
 * 
 * Кеширует заменённые шрифты для избежания пересоздания при каждом вызове SelectObject.
 * Использует простой линейный поиск с оптимизацией последнего доступа.
 * 
 * STRUCTURE / СТРУКТУРА:
 * - Separate caches for each context (CTX_LIB, CTX_PL)
 * - Maps original HFONT > replacement HFONT
 * - Last-access cache (c_orig/c_repl) for O(1) repeat lookups
 * 
 * - Отдельные кеши для каждого контекста (CTX_LIB, CTX_PL)
 * - Сопоставляет оригинальный HFONT > заменённый HFONT
 * - Кеш последнего доступа (c_orig/c_repl) для повторных поисков O(1)
 ******************************************************************************/

#define CACHE_SIZE 1024  // Maximum cached font pairs per context / Максимум пар кешированных шрифтов на контекст

// Font mapping entry / Запись сопоставления шрифтов
struct FontMap { 
    HFONT o;  // Original font / Оригинальный шрифт
    HFONT r;  // Replacement font / Заменённый шрифт
};

// Font cache structure / Структура кеша шрифтов
struct FontCache {
    FontMap map[CACHE_SIZE];  // Font mapping table / Таблица сопоставления шрифтов
    UINT used;                // Number of entries used / Количество использованных записей
    HFONT c_orig, c_repl;     // Last-access cache / Кеш последнего доступа
};

// Three caches: [0]=unused, [1]=CTX_LIB, [2]=CTX_PL
// Три кеша: [0]=не используется, [1]=CTX_LIB, [2]=CTX_PL
static FontCache g_caches[3];

// Global font tracking for cleanup / Глобальное отслеживание шрифтов для очистки
static HFONT g_tracked[4096];  // All created replacement fonts / Все созданные заменённые шрифты
static UINT  g_tracked_cnt = 0;  // Count of tracked fonts / Количество отслеживаемых шрифтов

/*******************************************************************************
 * TrackFont
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Adds a replacement font to the global tracking array for cleanup on exit.
 * 
 * Добавляет заменённый шрифт в глобальный массив отслеживания для очистки при выходе.
 ******************************************************************************/
static BOOL TrackFont(HFONT f) {
    if(!f) return FALSE;
    Lock();
    BOOL ok = (g_tracked_cnt < 4096);
    if(ok) g_tracked[g_tracked_cnt++] = f;
    Unlock();
    return ok;
}

/*******************************************************************************
 * ResetCache
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Clears a font cache when skin parameters change. Doesn't delete font objects
 * (they're tracked globally and deleted on module exit).
 * 
 * Очищает кеш шрифтов при изменении параметров скина. Не удаляет объекты
 * шрифтов (они отслеживаются глобально и удаляются при выходе из модуля).
 ******************************************************************************/
static void ResetCache(int ctx) {
    Lock();
    g_caches[ctx].used = 0;
    g_caches[ctx].c_orig = g_caches[ctx].c_repl = NULL;
    Unlock();
}

/*******************************************************************************
 * Cache_Find
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Looks up a replacement font in the cache. Uses last-access optimization
 * for repeated lookups of the same font.
 * 
 * Ищет заменённый шрифт в кеше. Использует оптимизацию последнего доступа
 * для повторных поисков того же шрифта.
 * 
 * RETURNS / ВОЗВРАЩАЕТ:
 * Replacement font handle if found, NULL otherwise
 * Дескриптор заменённого шрифта, если найден, NULL иначе
 ******************************************************************************/
static HFONT Cache_Find(int ctx, HFONT o) {
    if(!o) return NULL;
    Lock();
    FontCache* C = &g_caches[ctx];
    
    // Fast path: check last-access cache / Быстрый путь: проверить кеш последнего доступа
    if(o == C->c_orig) { 
        HFONT r = C->c_repl; 
        Unlock(); 
        return r; 
    }

    // Linear search / Линейный поиск
    HFONT r = NULL;
    for(UINT i=0; i<C->used; i++) {
        if(C->map[i].o == o) {
            r = C->map[i].r;
            // Update last-access cache / Обновить кеш последнего доступа
            C->c_orig = o; 
            C->c_repl = r;
            break;
        }
    }
    Unlock();
    return r;
}

/*******************************************************************************
 * Cache_Add
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Adds a new font mapping to the cache.
 * 
 * Добавляет новое сопоставление шрифтов в кеш.
 ******************************************************************************/
static void Cache_Add(int ctx, HFONT o, HFONT r) {
    if(!o || !r) return;
    Lock();
    FontCache* C = &g_caches[ctx];
    if(C->used < CACHE_SIZE) {
        C->map[C->used].o = o;
        C->map[C->used].r = r;
        C->used++;
        // Set last-access cache / Установить кеш последнего доступа
        C->c_orig = o; 
        C->c_repl = r;
    }
    Unlock();
}

/*******************************************************************************
 * InvalidateFont
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Invalidates last-access cache entries for a font. Called when a DC is deleted.
 * 
 * Инвалидирует записи кеша последнего доступа для шрифта. Вызывается при удалении DC.
 ******************************************************************************/
static void InvalidateFont(HFONT f) {
    if(!f) return;
    Lock();
    for(int i=1; i<=2; i++) {
        if(g_caches[i].c_orig == f || g_caches[i].c_repl == f) {
            g_caches[i].c_orig = g_caches[i].c_repl = NULL;
        }
    }
    Unlock();
}

/*******************************************************************************
 * DC TAGGING SYSTEM (IAT HOOK MODE ONLY)
 * СИСТЕМА ТЕГИРОВАНИЯ DC (ТОЛЬКО В РЕЖИМЕ ПЕРЕХВАТА IAT)
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * When IAT hooking is enabled, we need to know which context (CTX_LIB/CTX_PL)
 * a DC belongs to so we can select the correct replacement font in SelectObject.
 * 
 * Когда перехват IAT включён, нам нужно знать, какому контексту (CTX_LIB/CTX_PL)
 * принадлежит DC, чтобы выбрать правильный заменённый шрифт в SelectObject.
 * 
 * METHOD / МЕТОД:
 * - Tag DCs when created (BeginPaint, GetDC, etc.) with window context
 * - Use TLS to propagate context through CreateCompatibleDC
 * - Look up context in SelectObject hook
 * 
 * - Тегировать DC при создании (BeginPaint, GetDC и т.д.) с контекстом окна
 * - Использовать TLS для распространения контекста через CreateCompatibleDC
 * - Искать контекст в хуке SelectObject
 ******************************************************************************/

#define DC_MAP_SIZE 1024  // Maximum tracked DCs / Максимум отслеживаемых DC

// DC tag entry / Запись тега DC
struct DcTag { 
    HDC dc;     // DC handle / Дескриптор DC
    BYTE ctx;   // Context (CTX_LIB or CTX_PL) / Контекст (CTX_LIB или CTX_PL)
};

static DcTag g_dctags[DC_MAP_SIZE];  // DC tag table / Таблица тегов DC
static UINT  g_dc_cnt = 0;           // Number of tagged DCs / Количество помеченных DC
static DWORD g_tls_ctx = 0xFFFFFFFF; // TLS slot for context propagation / Слот TLS для распространения контекста

/*******************************************************************************
 * GetDcCtx / SetDcCtx
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Gets/sets the context tag for a DC.
 * 
 * Получает/устанавливает тег контекста для DC.
 ******************************************************************************/
static BYTE GetDcCtx(HDC dc) {
    if(!dc) return 0;
    Lock();
    BYTE ctx = 0;
    for(UINT i=0; i<g_dc_cnt; i++) {
        if(g_dctags[i].dc == dc) { 
            ctx = g_dctags[i].ctx; 
            break; 
        }
    }
    Unlock();
    return ctx;
}

static void SetDcCtx(HDC dc, BYTE ctx) {
    if(!dc) return;
    Lock();
    
    // Find existing entry / Найти существующую запись
    int idx = -1;
    for(UINT i=0; i<g_dc_cnt; i++) 
        if(g_dctags[i].dc == dc) { 
            idx = (int)i; 
            break; 
        }

    if(idx != -1) {
        // Update or remove existing entry / Обновить или удалить существующую запись
        if(ctx) 
            g_dctags[idx].ctx = ctx;
        else { 
            // Remove by swapping with last / Удалить, поменяв с последним
            g_dctags[idx] = g_dctags[--g_dc_cnt]; 
        }
    } else if(ctx && g_dc_cnt < DC_MAP_SIZE) {
        // Add new entry / Добавить новую запись
        g_dctags[g_dc_cnt].dc = dc;
        g_dctags[g_dc_cnt].ctx = ctx;
        g_dc_cnt++;
    }
    Unlock();
}

/*******************************************************************************
 * TLS Context Functions
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Thread-local storage for context propagation through CreateCompatibleDC.
 * 
 * Локальное хранилище потока для распространения контекста через CreateCompatibleDC.
 ******************************************************************************/
static BYTE GetTlsCtx() { 
    return (g_tls_ctx != 0xFFFFFFFF) ? (BYTE)(UINT_PTR)TlsGetValue(g_tls_ctx) : 0; 
}

static void SetTlsCtx(BYTE c) { 
    if(g_tls_ctx != 0xFFFFFFFF) 
        TlsSetValue(g_tls_ctx, (LPVOID)(UINT_PTR)c); 
}

/*******************************************************************************
 * ResolveDcCtx
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Resolves context for a DC. First checks DC tag, then falls back to TLS.
 * If found in TLS, tags the DC for future lookups.
 * 
 * Определяет контекст для DC. Сначала проверяет тег DC, затем использует TLS.
 * Если найден в TLS, помечает DC для будущих поисков.
 ******************************************************************************/
static BYTE ResolveDcCtx(HDC dc) {
    BYTE c = GetDcCtx(dc);
    if(!c) { 
        c = GetTlsCtx(); 
        if(c) SetDcCtx(dc, c); 
    }
    return c;
}

/*******************************************************************************
 * WINDOW CONTEXT DETECTION
 * ОПРЕДЕЛЕНИЕ КОНТЕКСТА ОКНА
 ******************************************************************************/

/*******************************************************************************
 * IsFileDlg
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Detects file dialogs (Open/Save) which should not have fonts replaced.
 * 
 * Обнаруживает файловые диалоги (Открыть/Сохранить), в которых не следует заменять шрифты.
 ******************************************************************************/
static BOOL IsFileDlg(HWND w) {
    char c[64]; 
    GetClassNameA(w, c, 63);
    
    // Check for dialog window class / Проверить класс окна диалога
    if(!lstrcmpA(c,"#32770")) {
        // Top-level dialog or contains shell view / Диалог верхнего уровня или содержит shell view
        if(!(GetWindowLongA(w, GWL_STYLE) & WS_CHILD)) return TRUE;
        if(FindWindowExA(w, 0, "SHELLDLL_DefView", 0)) return TRUE;
    }
    
    // DirectUI or shell view classes / Классы DirectUI или shell view
    return (!lstrcmpA(c,"DirectUIHWND") || !lstrcmpA(c,"SHELLDLL_DefView"));
}

/*******************************************************************************
 * GetWinCtx
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Determines the context (CTX_LIB or CTX_PL) of a window by walking up the
 * parent chain looking for known Winamp window classes.
 * 
 * Определяет контекст (CTX_LIB или CTX_PL) окна, поднимаясь по цепочке
 * родителей в поисках известных классов окон Winamp.
 * 
 * RETURNS / ВОЗВРАЩАЕТ:
 * CTX_PL  - Playlist Editor window / Окно редактора плейлистов
 * CTX_LIB - Media Library window / Окно библиотеки
 * 0       - Not a Winamp window or file dialog / Не окно Winamp или файловый диалог
 ******************************************************************************/
static BYTE GetWinCtx(HWND w) {
    while(w) {
        // Exclude file dialogs / Исключить файловые диалоги
        if(IsFileDlg(w)) return 0;
        
        char c[64]; 
        GetClassNameA(w, c, 63);
        
        // Check for Playlist Editor windows / Проверить окна редактора плейлистов
        if(!lstrcmpA(c,"Winamp PE") || !lstrcmpA(c,"WinampLibraryPE")) 
            return CTX_PL;
        
        // Check for Media Library windows / Проверить окна библиотеки
        if(!lstrcmpA(c,"Winamp Gen") || !lstrcmpA(c,"WinampLibrary")) 
            return CTX_LIB;
        
        // Exclude main window and equalizer / Исключить главное окно и эквалайзер
        if(!lstrcmpA(c,"Winamp v1.x") || !lstrcmpA(c,"Winamp EQ")) 
            return 0;
        
        w = GetParent(w);
    }
    return 0;
}

/*******************************************************************************
 * SKIN PARAMETERS AND FONT CONFIGURATION
 * ПАРАМЕТРЫ СКИНА И КОНФИГУРАЦИЯ ШРИФТОВ
 ******************************************************************************/

// Skin parameters (from Winamp IPC) / Параметры скина (из Winamp IPC)
static char  g_skin_dir[MAX_PATH];         // Current skin directory / Текущий каталог скина
static DWORD g_skin_hash = 0;              // Hash of skin directory / Хеш каталога скина
static char  g_skin_face[LF_FACESIZE];     // Skin font face / Гарнитура шрифта скина
static int   g_skin_h = 0;                 // Skin font height / Высота шрифта скина
static int   g_skin_cs = DEFAULT_CHARSET;  // Skin font charset / Кодировка шрифта скина
static DWORD g_stamp = 0;                  // Combined stamp for change detection / Объединённый отпечаток для обнаружения изменений

// Playlist Editor parameters (learned from observation) / Параметры редактора плейлистов (изучаются из наблюдения)
static char  g_pl_face[LF_FACESIZE];       // PE font face / Гарнитура шрифта PE
static int   g_pl_h = 0;                   // PE font height / Высота шрифта PE
static int   g_pl_cs = DEFAULT_CHARSET;    // PE font charset / Кодировка шрифта PE
static DWORD g_pl_stamp = 0;               // PE stamp for change detection / Отпечаток PE для обнаружения изменений

// Winamp window hooks / Хуки окна Winamp
static HWND   g_hWA = 0;      // Winamp main window / Главное окно Winamp
static WNDPROC g_oldWA = 0;   // Original window procedure / Оригинальная процедура окна
static HHOOK  g_hkCBT = 0;    // CBT hook (NT only) / Хук CBT (только NT)

// Skin polling stamp (Win9x only) / Отпечаток опроса скина (только Win9x)
static DWORD g_skinPollStamp = 0;

// Forward declarations / Предварительные объявления
static void ScheduleUpdate();
static void StartScan_Arm();

/*******************************************************************************
 * UpdateSkinData
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Queries Winamp for current skin parameters (directory, font face, size, charset).
 * If any parameter changed, resets font caches.
 * 
 * Запрашивает у Winamp текущие параметры скина (каталог, гарнитура шрифта, размер, кодировка).
 * Если какой-либо параметр изменился, сбрасывает кеши шрифтов.
 * 
 * CRITICAL / КРИТИЧНО:
 * Uses __try/__except to protect against crashes if Winamp window is invalid.
 * Использует __try/__except для защиты от крашей, если окно Winamp недопустимо.
 ******************************************************************************/
static void UpdateSkinData() {
    if(!g_hWA) return;

    // Get skin directory / Получить каталог скина
    char dir[MAX_PATH] = {0};
    __try { 
        SendMessageA(g_hWA, WM_WA_IPC, (WPARAM)dir, IPC_GETSKIN); 
    } __except(1) { 
        dir[0]=0; 
    }
    Lock(); 
    lstrcpynA(g_skin_dir, dir, MAX_PATH); 
    Unlock();

    // Check if skin directory changed / Проверить, изменился ли каталог скина
    DWORD h = Hash(dir);
    Lock();
    if(h != g_skin_hash) { 
        g_skin_hash = h; 
        ResetCache(CTX_LIB); 
        ResetCache(CTX_PL); 
    }
    Unlock();

    // Get skin font parameters / Получить параметры шрифта скина
    __try {
        // IPC_GET_GENSKINBITMAP returns different values based on first parameter:
        // wParam=1: font face name
        // wParam=2: charset
        // wParam=3: height
        // IPC_GET_GENSKINBITMAP возвращает разные значения в зависимости от первого параметра:
        // wParam=1: имя гарнитуры шрифта
        // wParam=2: кодировка
        // wParam=3: высота
        
        char* p = (char*)SendMessageA(g_hWA, WM_WA_IPC, 1, IPC_GET_GENSKINBITMAP);
        if(p && IsValidPtr(p) && *p) {
            char face[LF_FACESIZE]; 
            face[0]=0;
            lstrcpynA(face, p, LF_FACESIZE);

            int cs = (int)SendMessageA(g_hWA, WM_WA_IPC, 2, IPC_GET_GENSKINBITMAP);
            int ht = (int)SendMessageA(g_hWA, WM_WA_IPC, 3, IPC_GET_GENSKINBITMAP);
            
            // Sanity checks / Проверки разумности
            if(cs<0||cs>255) cs = DEFAULT_CHARSET;
            if(ht<1||ht>72) ht = 11;

            // Check if any parameter changed / Проверить, изменился ли какой-либо параметр
            Lock();
            if(lstrcmpA(g_skin_face, face) || g_skin_h!=ht || g_skin_cs!=cs) {
                lstrcpynA(g_skin_face, face, LF_FACESIZE);
                g_skin_h=ht; 
                g_skin_cs=cs;
                ResetCache(CTX_LIB);
            }
            Unlock();
        }
    } __except(1) {}
}

/*******************************************************************************
 * UpdatePlData
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Learns Playlist Editor font from first observation. The PE uses its own font
 * which may differ from the skin font.
 * 
 * Изучает шрифт редактора плейлистов при первом наблюдении. PE использует собственный
 * шрифт, который может отличаться от шрифта скина.
 * 
 * NOTE / ПРИМЕЧАНИЕ:
 * Only updates if font is not italic or underlined (those are special styles).
 * Обновляет только если шрифт не курсивный и не подчёркнутый (это специальные стили).
 ******************************************************************************/
static void UpdatePlData(const LOGFONTA* lf) {
    if(!lf || lf->lfItalic || lf->lfUnderline) return;

    const char* f = lf->lfFaceName[0] ? lf->lfFaceName : kFaceDef;
    int h = (lf->lfHeight < 0) ? -lf->lfHeight : lf->lfHeight;
    int c = (BYTE)lf->lfCharSet;
    
    // Create stamp from parameters / Создать отпечаток из параметров
    DWORD s = Hash(f) ^ (DWORD)h ^ ((DWORD)c<<24);

    BOOL diff = FALSE;
    Lock();
    if(s != g_pl_stamp) {
        lstrcpynA(g_pl_face, f, LF_FACESIZE);
        g_pl_h = h; 
        g_pl_cs = c; 
        g_pl_stamp = s;
        diff = TRUE;
    }
    Unlock();

    // Schedule update if changed / Запланировать обновление, если изменилось
    if(diff) ScheduleUpdate();
}

/*******************************************************************************
 * GetParams
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Gets the desired font parameters for a given context.
 * 
 * Получает желаемые параметры шрифта для данного контекста.
 * 
 * PARAMETERS / ПАРАМЕТРЫ:
 * ctx - Context (CTX_LIB or CTX_PL) / Контекст (CTX_LIB или CTX_PL)
 * f   - Output: font face / Выход: гарнитура шрифта
 * h   - Output: font height / Выход: высота шрифта
 * c   - Output: charset / Выход: кодировка
 ******************************************************************************/
static void GetParams(int ctx, char* f, int* h, int* c) {
    Lock();
    if(ctx == CTX_PL && g_pl_stamp) {
        // Use learned PE font / Использовать изученный шрифт PE
        lstrcpynA(f, g_pl_face, LF_FACESIZE); 
        *h = g_pl_h; 
        *c = g_pl_cs;
    } else if(g_skin_face[0]) {
        // Use skin font / Использовать шрифт скина
        lstrcpynA(f, g_skin_face, LF_FACESIZE); 
        *h = g_skin_h; 
        *c = g_skin_cs;
    } else {
        // Fallback to default / Резервный вариант по умолчанию
        lstrcpynA(f, kFaceDef, LF_FACESIZE); 
        *h = 0; 
        *c = DEFAULT_CHARSET;
    }
    Unlock();
}

/*******************************************************************************
 * VectorizeFace
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Converts bitmap fonts to their vector equivalents for better rendering.
 * 
 * Преобразует растровые шрифты в их векторные эквиваленты для лучшей отрисовки.
 * 
 * CONVERSIONS / ПРЕОБРАЗОВАНИЯ:
 * MS Sans Serif > Microsoft Sans Serif (vector, scalable)
 * System        > Microsoft Sans Serif
 * MS Serif      > Times New Roman
 * Fixedsys      > Lucida Console
 * Terminal      > Lucida Console
 * 
 * RETURNS / ВОЗВРАЩАЕТ:
 * TRUE if font was replaced, FALSE if left unchanged
 * TRUE если шрифт был заменён, FALSE если оставлен без изменений
 ******************************************************************************/
static BOOL VectorizeFace(char* face) {
    if(!_stricmp(face, "MS Sans Serif") || !_stricmp(face, "System")) {
        lstrcpynA(face, "Microsoft Sans Serif", LF_FACESIZE);
        return TRUE;
    }
    if(!_stricmp(face, "MS Serif")) {
        lstrcpynA(face, "Times New Roman", LF_FACESIZE);
        return TRUE;
    }
    if(!_stricmp(face, "Fixedsys") || !_stricmp(face, "Terminal")) {
        lstrcpynA(face, "Lucida Console", LF_FACESIZE);
        return TRUE;
    }
    return FALSE;
}

/*******************************************************************************
 * GetReplFont
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Creates a replacement font with corrected parameters. This is the core
 * function that implements the font replacement logic.
 * 
 * Создаёт заменённый шрифт с исправленными параметрами. Это основная функция,
 * реализующая логику замены шрифтов.
 * 
 * PARAMETERS / ПАРАМЕТРЫ:
 * o   - Original font handle / Дескриптор оригинального шрифта
 * ctx - Context (CTX_LIB or CTX_PL) / Контекст (CTX_LIB или CTX_PL)
 * 
 * RETURNS / ВОЗВРАЩАЕТ:
 * Replacement font handle, or NULL if no replacement needed
 * Дескриптор заменённого шрифта или NULL, если замена не требуется
 * 
 * ALGORITHM / АЛГОРИТМ:
 * 1. Check cache for existing replacement
 * 2. Get original font's LOGFONT
 * 3. For PE context: learn font and vectorize face
 * 4. For LIB context: use skin parameters and vectorize
 * 5. Apply quality (ClearType or antialiased)
 * 6. Create new font if parameters differ
 * 7. Add to cache and tracking
 * 
 * 1. Проверить кеш на существующую замену
 * 2. Получить LOGFONT оригинального шрифта
 * 3. Для контекста PE: изучить шрифт и векторизовать гарнитуру
 * 4. Для контекста LIB: использовать параметры скина и векторизовать
 * 5. Применить качество (ClearType или antialiased)
 * 6. Создать новый шрифт, если параметры отличаются
 * 7. Добавить в кеш и отслеживание
 ******************************************************************************/
static HFONT GetReplFont(HFONT o, int ctx) {
    if(!o) return NULL;

    // Check cache first / Сначала проверить кеш
    HFONT cached = Cache_Find(ctx, o);
    if(cached) return cached;

    // Get original font parameters / Получить параметры оригинального шрифта
    LOGFONTA lf;
    if(!GetObjectA(o, sizeof(lf), &lf)) return NULL;

    LOGFONTA lf_new = lf;

    if(ctx == CTX_PL) {
        // Playlist Editor: learn and vectorize / Редактор плейлистов: изучить и векторизовать
        UpdatePlData(&lf_new);
        VectorizeFace(lf_new.lfFaceName);
    } else {
        // Media Library: use skin parameters / Библиотека: использовать параметры скина
        char f[LF_FACESIZE]; 
        int wantH, wantCS;
        GetParams(ctx, f, &wantH, &wantCS);

        // Vectorize face / Векторизовать гарнитуру
        char f_vec[LF_FACESIZE]; 
        lstrcpynA(f_vec, f, LF_FACESIZE);
        VectorizeFace(f_vec);

        // Apply parameters / Применить параметры
        lstrcpynA(lf_new.lfFaceName, f_vec, LF_FACESIZE);
        lf_new.lfCharSet = (BYTE)wantCS;
        if(wantH > 0) lf_new.lfHeight = -wantH;  // Negative = character height / Отрицательное = высота символа
    }

    // Apply quality / Применить качество
    lf_new.lfQuality = g_forceQuality;

    // Check if replacement needed / Проверить, требуется ли замена
    if(!_stricmp(lf.lfFaceName, lf_new.lfFaceName) &&
       (BYTE)lf.lfCharSet == (BYTE)lf_new.lfCharSet &&
       lf.lfHeight == lf_new.lfHeight &&
       (BYTE)lf.lfQuality == (BYTE)lf_new.lfQuality)
    {
        return NULL;  // No change needed / Изменение не требуется
    }

    // Create replacement font / Создать заменённый шрифт
    HFONT r = CreateFontIndirectA(&lf_new);
    if(r) {
        // Track and cache / Отслеживать и кешировать
        if(TrackFont(r)) 
            Cache_Add(ctx, o, r);
        else { 
            DeleteObject(r); 
            r = NULL; 
        }
    }
    return r;
}

/*******************************************************************************
 * TREEVIEW SUPPORT: FONT + ITEM HEIGHT
 * ПОДДЕРЖКА TREEVIEW: ШРИФТ + ВЫСОТА ЭЛЕМЕНТА
 ******************************************************************************/

// TreeView message constants / Константы сообщений TreeView
#ifndef TV_FIRST
#define TV_FIRST 0x1100
#endif
#ifndef TVM_SETITEMHEIGHT
#define TVM_SETITEMHEIGHT (TV_FIRST + 27)  // Set item height / Установить высоту элемента
#endif
#ifndef TVM_GETITEMHEIGHT
#define TVM_GETITEMHEIGHT (TV_FIRST + 28)  // Get item height / Получить высоту элемента
#endif
#ifndef TVM_GETIMAGELIST
#define TVM_GETIMAGELIST (TV_FIRST + 8)    // Get image list / Получить список изображений
#endif
#ifndef TVSIL_NORMAL
#define TVSIL_NORMAL 0  // Normal image list / Обычный список изображений
#endif
#ifndef TVS_NONEVENHEIGHT
#define TVS_NONEVENHEIGHT 0x4000  // Allow odd heights / Разрешить нечётные высоты
#endif

// Control type enumeration / Перечисление типов контролов
enum { CTRL_NONE=0, CTRL_TV=1, CTRL_LV=2 };

/*******************************************************************************
 * GetCtrlKind
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Determines if a window is a TreeView or ListView control.
 * 
 * Определяет, является ли окно контролом TreeView или ListView.
 ******************************************************************************/
static BYTE GetCtrlKind(HWND h) {
    char c[64]; 
    c[0]=0;
    GetClassNameA(h, c, 63);
    if(!lstrcmpiA(c, "SysTreeView32")) return CTRL_TV;
    if(!lstrcmpiA(c, "SysListView32")) return CTRL_LV;
    return CTRL_NONE;
}

/*******************************************************************************
 * LISTVIEW CONSTANTS AND HELPERS
 * КОНСТАНТЫ И ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ LISTVIEW
 ******************************************************************************/

#ifndef LVM_FIRST
#define LVM_FIRST 0x1000
#endif
#ifndef LVM_GETIMAGELIST
#define LVM_GETIMAGELIST (LVM_FIRST + 2)  // Get image list / Получить список изображений
#endif
#ifndef LVM_SETIMAGELIST
#define LVM_SETIMAGELIST (LVM_FIRST + 3)  // Set image list / Установить список изображений
#endif
#ifndef LVSIL_NORMAL
#define LVSIL_NORMAL 0  // Large icon image list / Список изображений больших иконок
#endif
#ifndef LVSIL_SMALL
#define LVSIL_SMALL 1   // Small icon image list / Список изображений маленьких иконок
#endif
#ifndef LVS_TYPEMASK
#define LVS_TYPEMASK 0x0003  // Mask for view type / Маска для типа вида
#endif
#ifndef LVS_REPORT
#define LVS_REPORT 0x0001    // Report (details) view / Вид отчёта (детали)
#endif
#ifndef LVS_LIST
#define LVS_LIST   0x0003    // List view / Вид списка
#endif

/*******************************************************************************
 * TreeView_GetMinHeightFromImages
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Calculates minimum item height based on TreeView's image list icon size.
 * 
 * Вычисляет минимальную высоту элемента на основе размера иконок в списке
 * изображений TreeView.
 ******************************************************************************/
static int TreeView_GetMinHeightFromImages(HWND hwndTV) {
    int minH = 0;
    HIMAGELIST himg = (HIMAGELIST)SendMessageA(hwndTV, TVM_GETIMAGELIST, (WPARAM)TVSIL_NORMAL, 0);
    if(himg) {
        int cx=0, cy=0;
        if(ImageList_GetIconSize(himg, &cx, &cy)) {
            if(cy > 0) minH = cy + 2;  // Icon height + 2px padding / Высота иконки + 2px отступ
        }
    }
    return minH;
}

/*******************************************************************************
 * CalcFontHeightPx
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Calculates required pixel height for TreeView items based on font metrics.
 * 
 * Вычисляет требуемую высоту в пикселях для элементов TreeView на основе
 * метрик шрифта.
 * 
 * ALGORITHM / АЛГОРИТМ:
 * 1. Get font metrics (tmHeight + tmExternalLeading)
 * 2. Add 6px padding
 * 3. Ensure at least as tall as image list icons
 * 4. Ensure minimum 18px
 * 
 * 1. Получить метрики шрифта (tmHeight + tmExternalLeading)
 * 2. Добавить 6px отступ
 * 3. Обеспечить высоту не меньше иконок списка изображений
 * 4. Обеспечить минимум 18px
 ******************************************************************************/
static int CalcFontHeightPx(HWND hwnd, HFONT hFont) {
    if(!hwnd || !hFont) return 0;
    
    HDC dc = GetDC(hwnd);
    if(!dc) return 0;

    // Select font and get metrics / Выбрать шрифт и получить метрики
    HFONT old = (HFONT)SelectObject(dc, hFont);
    TEXTMETRIC tm; 
    ZeroMemory(&tm, sizeof(tm));
    GetTextMetrics(dc, &tm);
    SelectObject(dc, old);
    ReleaseDC(hwnd, dc);

    // Calculate height with padding / Вычислить высоту с отступом
    int h = tm.tmHeight + tm.tmExternalLeading + 6;
    
    // Ensure tall enough for images / Обеспечить достаточную высоту для изображений
    int minImg = TreeView_GetMinHeightFromImages(hwnd);
    if(h < minImg) h = minImg;
    
    // Absolute minimum / Абсолютный минимум
    if(h < 18) h = 18;
    
    return h;
}

/*******************************************************************************
 * TreeView_EnableNonevenHeight
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Enables TVS_NONEVENHEIGHT style to allow odd item heights (NT only).
 * 
 * Включает стиль TVS_NONEVENHEIGHT для разрешения нечётных высот элементов (только NT).
 ******************************************************************************/
static void TreeView_EnableNonevenHeight(HWND hwndTV) {
    LONG st = GetWindowLongA(hwndTV, GWL_STYLE);
    if(!(st & TVS_NONEVENHEIGHT)) {
        SetWindowLongA(hwndTV, GWL_STYLE, st | TVS_NONEVENHEIGHT);
        // Force style update / Принудительно обновить стиль
        SetWindowPos(hwndTV, NULL, 0,0,0,0,
            SWP_NOMOVE|SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE|SWP_FRAMECHANGED);
    }
}

/*******************************************************************************
 * TreeView_EnsureReplFont
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Ensures TreeView has replacement font applied.
 * 
 * Обеспечивает применение заменённого шрифта к TreeView.
 ******************************************************************************/
static void TreeView_EnsureReplFont(HWND hwndTV, BYTE ctx) {
    if(!hwndTV || !ctx) return;

    // Get current font / Получить текущий шрифт
    HFONT cur = (HFONT)SendMessageA(hwndTV, WM_GETFONT, 0, 0);
    if(!cur) cur = (HFONT)GetStockObject(DEFAULT_GUI_FONT);

    // Get and apply replacement / Получить и применить замену
    HFONT repl = GetReplFont(cur, ctx);
    if(repl) {
        SendMessageA(hwndTV, WM_SETFONT, (WPARAM)repl, FALSE);
        InvalidateRect(hwndTV, NULL, FALSE);
    }
}

/*******************************************************************************
 * TreeView_AutoFixItemHeight
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Automatically adjusts TreeView item height to fit the current font.
 * 
 * Автоматически корректирует высоту элементов TreeView под текущий шрифт.
 * 
 * CRITICAL / КРИТИЧНО:
 * On NT, uses WM_SETREDRAW batching to prevent flicker.
 * On 9x, RDW_NOERASE is not used as it causes issues.
 * 
 * На NT использует пакетирование WM_SETREDRAW для предотвращения мигания.
 * На 9x RDW_NOERASE не используется, так как вызывает проблемы.
 ******************************************************************************/
static void TreeView_AutoFixItemHeight(HWND hwndTV) {
    if(!hwndTV || !IsWindow(hwndTV)) return;

    // Get window context / Получить контекст окна
    BYTE ctx = GetWinCtx(hwndTV);
    if(!ctx) return;

    // Ensure replacement font / Обеспечить заменённый шрифт
    TreeView_EnsureReplFont(hwndTV, ctx);
    TreeView_EnableNonevenHeight(hwndTV);

    // Get current font / Получить текущий шрифт
    HFONT base = (HFONT)SendMessageA(hwndTV, WM_GETFONT, 0, 0);
    if(!base) base = (HFONT)GetStockObject(DEFAULT_GUI_FONT);

    // Calculate desired height / Вычислить желаемую высоту
    int wantH = CalcFontHeightPx(hwndTV, base);
    if(wantH <= 0) return;

    // Get current height / Получить текущую высоту
    int curH  = (int)SendMessageA(hwndTV, TVM_GETITEMHEIGHT, 0, 0);
    if(curH != wantH) {
        // On NT: batch updates with WM_SETREDRAW
        // На NT: пакетировать обновления через WM_SETREDRAW
        if(!g_is9x) SendMessageA(hwndTV, WM_SETREDRAW, FALSE, 0);
        
        SendMessageA(hwndTV, TVM_SETITEMHEIGHT, (WPARAM)wantH, 0);
        
        if(!g_is9x) {
            SendMessageA(hwndTV, WM_SETREDRAW, TRUE, 0);
            // RDW_NOERASE to avoid background flicker
            // RDW_NOERASE для избежания мигания фона
            RedrawWindow(hwndTV, NULL, NULL, RDW_INVALIDATE|RDW_ALLCHILDREN|RDW_NOERASE);
        } else {
            RedrawWindow(hwndTV, NULL, NULL, RDW_INVALIDATE|RDW_ALLCHILDREN);
        }
    }
}

/*******************************************************************************
 * LISTVIEW HELPERS
 * ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ LISTVIEW
 ******************************************************************************/

// Window property keys for ListView / Ключи свойств окна для ListView
#define PROP_LV_STAMP "MLF_LV_ST"  // Font stamp / Отпечаток шрифта
#define PROP_LV_IP    "MLF_LV_IP"  // In-progress flag / Флаг выполнения

/*******************************************************************************
 * MakeDesiredFontStamp
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Creates a stamp that uniquely identifies desired font configuration.
 * Used to detect when ListView needs font update.
 * 
 * Создаёт отпечаток, который уникально идентифицирует желаемую конфигурацию шрифта.
 * Используется для обнаружения, когда ListView нуждается в обновлении шрифта.
 ******************************************************************************/
static DWORD MakeDesiredFontStamp(BYTE ctx) {
    char f[LF_FACESIZE]; 
    int h, cs;
    GetParams(ctx, f, &h, &cs);
    return Hash(f) ^ (DWORD)h ^ ((DWORD)(BYTE)cs<<24) ^ ((DWORD)(BYTE)g_forceQuality<<16);
}

/*******************************************************************************
 * ListView_ForceRelayout_9x
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Forces ListView to relayout after font change on Win9x. Win9x ListViews are
 * stubborn and don't automatically recalculate item sizes when font changes.
 * 
 * Принудительно перестраивает ListView после смены шрифта на Win9x. ListView
 * на Win9x упрямы и не пересчитывают размеры элементов автоматически при смене шрифта.
 * 
 * WIN9X SPECIFIC / СПЕЦИФИЧНО ДЛЯ WIN9X:
 * This aggressive approach toggles view type to force full recalculation.
 * On NT, a simple SetWindowPos with SWP_FRAMECHANGED is sufficient.
 * 
 * Этот агрессивный подход переключает тип вида для принудительного полного
 * пересчёта. На NT достаточно простого SetWindowPos с SWP_FRAMECHANGED.
 ******************************************************************************/
static void ListView_ForceRelayout_9x(HWND lv) {
    if(!lv || !IsWindow(lv)) return;
    if(!g_is9x) return;

    // Batch updates / Пакетировать обновления
    SendMessageA(lv, WM_SETREDRAW, FALSE, 0);

    // Re-apply imagelists to trigger internal recalculation
    // Повторно применить списки изображений для запуска внутреннего пересчёта
    HIMAGELIST ilS = (HIMAGELIST)SendMessageA(lv, LVM_GETIMAGELIST, (WPARAM)LVSIL_SMALL, 0);
    if(ilS) SendMessageA(lv, LVM_SETIMAGELIST, (WPARAM)LVSIL_SMALL, (LPARAM)ilS);
    HIMAGELIST ilN = (HIMAGELIST)SendMessageA(lv, LVM_GETIMAGELIST, (WPARAM)LVSIL_NORMAL, 0);
    if(ilN) SendMessageA(lv, LVM_SETIMAGELIST, (WPARAM)LVSIL_NORMAL, (LPARAM)ilN);

    // Toggle view type to force full recalc
    // Переключить тип вида для принудительного полного пересчёта
    LONG st = GetWindowLongA(lv, GWL_STYLE);
    LONG type = st & LVS_TYPEMASK;
    LONG other = (type == LVS_REPORT) ? LVS_LIST : LVS_REPORT;

    SetWindowLongA(lv, GWL_STYLE, (st & ~LVS_TYPEMASK) | other);
    SetWindowLongA(lv, GWL_STYLE, (st & ~LVS_TYPEMASK) | type);

    SetWindowPos(lv, NULL, 0,0,0,0,
        SWP_NOMOVE|SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE|SWP_FRAMECHANGED);

    SendMessageA(lv, WM_SETREDRAW, TRUE, 0);
    InvalidateRect(lv, NULL, FALSE);
    UpdateWindow(lv);
}

/*******************************************************************************
 * ListView_AutoFixFontAndRelayout
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Ensures ListView has correct font and triggers relayout if needed.
 * 
 * Обеспечивает правильный шрифт в ListView и запускает перестройку при необходимости.
 * 
 * PARAMETERS / ПАРАМЕТРЫ:
 * lv            - ListView handle / Дескриптор ListView
 * forceRelayout - Force relayout even if stamp hasn't changed
 *                 Принудительно перестроить, даже если отпечаток не изменился
 * 
 * STAMP SYSTEM / СИСТЕМА ОТПЕЧАТКОВ:
 * Stores desired font stamp in window property. Only relayouts when stamp
 * changes or forceRelayout=TRUE. This prevents excessive relayouts.
 * 
 * Хранит желаемый отпечаток шрифта в свойстве окна. Перестраивает только
 * при изменении отпечатка или forceRelayout=TRUE. Предотвращает чрезмерные перестройки.
 ******************************************************************************/
static void ListView_AutoFixFontAndRelayout(HWND lv, BOOL forceRelayout) {
    if(!lv || !IsWindow(lv)) return;

    BYTE ctx = GetWinCtx(lv);
    if(!ctx) return;

    // Check if font stamp changed / Проверить, изменился ли отпечаток шрифта
    DWORD wantStamp = MakeDesiredFontStamp(ctx);
    DWORD haveStamp = (DWORD)(UINT_PTR)GetPropA(lv, PROP_LV_STAMP);
    BOOL need = (haveStamp != wantStamp);

    if(need) SetPropA(lv, PROP_LV_STAMP, (HANDLE)(UINT_PTR)wantStamp);

    // Ensure replacement font / Обеспечить заменённый шрифт
    HFONT cur = (HFONT)SendMessageA(lv, WM_GETFONT, 0, 0);
    if(!cur) cur = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
    BOOL didFont = FALSE;

    HFONT repl = GetReplFont(cur, ctx);
    if(repl) {
        didFont = TRUE;
        // Guard: prevent recursion from our own WM_SETFONT
        // Защита: предотвратить рекурсию от нашего собственного WM_SETFONT
        SetPropA(lv, PROP_LV_IP, (HANDLE)1);
        SendMessageA(lv, WM_SETFONT, (WPARAM)repl, FALSE);
        RemovePropA(lv, PROP_LV_IP);
    }

    if(g_is9x) {
        // Win9x: aggressive relayout if stamp changed or forced
        // Win9x: агрессивная перестройка, если отпечаток изменился или принудительно
        if(need || forceRelayout || didFont) {
            ListView_ForceRelayout_9x(lv);
        }
    } else {
        // NT: simple frame change usually sufficient
        // NT: простое изменение фрейма обычно достаточно
        if(need) {
            SetWindowPos(lv, NULL, 0,0,0,0,
                SWP_NOMOVE|SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE|SWP_FRAMECHANGED);
        }
        
        if(need || didFont) {
            RedrawWindow(lv, NULL, NULL, RDW_INVALIDATE | RDW_NOERASE);
        }
    }
}

/*******************************************************************************
 * OPTIONAL: IAT HOOKS (DISABLED ON 9X BY DEFAULT)
 * ОПЦИОНАЛЬНЫЕ: ХУКИ IAT (ОТКЛЮЧЕНЫ НА 9X ПО УМОЛЧАНИЮ)
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * When enabled, hooks GDI SelectObject to intercept all font selections.
 * This catches fonts that aren't set via WM_SETFONT.
 * 
 * Когда включено, перехватывает GDI SelectObject для перехвата всех выборов шрифтов.
 * Это ловит шрифты, которые не устанавливаются через WM_SETFONT.
 * 
 * DISABLED ON WIN9X / ОТКЛЮЧЕНО НА WIN9X:
 * Causes crashes on exit due to DLL unload order issues.
 * Вызывает краши при выходе из-за проблем с порядком выгрузки DLL.
 ******************************************************************************/

typedef HGDIOBJ (WINAPI *SELOBJ)(HDC, HGDIOBJ);
static SELOBJ Real_SelectObject = 0;

/*******************************************************************************
 * Hook_SelectObject
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Intercepts SelectObject to replace fonts with smooth versions.
 * 
 * Перехватывает SelectObject для замены шрифтов на сглаженные версии.
 ******************************************************************************/
static HGDIOBJ WINAPI Hook_SelectObject(HDC dc, HGDIOBJ o) {
    if(!Real_SelectObject) return NULL;
    if(!dc) return Real_SelectObject(dc, o);

    // Check if selecting a font / Проверить, выбирается ли шрифт
    if(o && GetObjectType(o) == OBJ_FONT) {
        BYTE ctx = ResolveDcCtx(dc);
        if(ctx) {
            HFONT r = GetReplFont((HFONT)o, ctx);
            if(r) return Real_SelectObject(dc, (HGDIOBJ)r);
        }
    }
    return Real_SelectObject(dc, o);
}

// Paint hooks to tag DCs / Хуки рисования для тегирования DC
typedef HDC (WINAPI *BP)(HWND, LPPAINTSTRUCT); 
static BP Real_BeginPaint = 0;

static HDC WINAPI Hook_BeginPaint(HWND w, LPPAINTSTRUCT p) {
    if(!Real_BeginPaint) return NULL;
    BYTE c = GetWinCtx(w);
    if(c) SetTlsCtx(c);
    HDC dc = Real_BeginPaint(w, p);
    if(dc && c) SetDcCtx(dc, c);
    return dc;
}

typedef BOOL (WINAPI *EP)(HWND, const PAINTSTRUCT*); 
static EP Real_EndPaint = 0;

static BOOL WINAPI Hook_EndPaint(HWND w, const PAINTSTRUCT* p) {
    if(!Real_EndPaint) return FALSE;
    if(p && p->hdc) SetDcCtx(p->hdc, 0);
    SetTlsCtx(0);
    return Real_EndPaint(w, p);
}

// Hook macro for DC functions / Макрос хуков для функций DC
#define HK_D(R, N, A, C) typedef R (WINAPI *P_##N)A; static P_##N Real_##N=0; static R WINAPI Hook_##N A { C }

HK_D(HDC,  GetDC,              (HWND w),                  
    HDC d=Real_GetDC(w); 
    if(d) SetDcCtx(d, GetWinCtx(w)); 
    return d;
)

HK_D(HDC,  GetWindowDC,        (HWND w),                  
    HDC d=Real_GetWindowDC(w); 
    if(d) SetDcCtx(d, GetWinCtx(w)); 
    return d;
)

HK_D(int,  ReleaseDC,          (HWND w, HDC d),           
    if(d) SetDcCtx(d, 0); 
    return Real_ReleaseDC(w, d);
)

HK_D(BOOL, DeleteDC,           (HDC d),                   
    if(d){ 
        HFONT f=(HFONT)GetCurrentObject(d,OBJ_FONT); 
        if(f) InvalidateFont(f); 
        SetDcCtx(d,0); 
    } 
    return Real_DeleteDC(d);
)

HK_D(HDC,  CreateCompatibleDC, (HDC r),                   
    HDC d=Real_CreateCompatibleDC(r); 
    if(d) SetDcCtx(d, r?GetDcCtx(r):GetTlsCtx()); 
    return d;
)

HK_D(HDC,  GetDCEx,            (HWND w, HRGN r, DWORD f),  
    HDC d=Real_GetDCEx(w,r,f); 
    if(d) SetDcCtx(d, GetWinCtx(w)); 
    return d;
)

/*******************************************************************************
 * HookMod
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Patches IAT of a module to install hooks.
 * 
 * Патчит IAT модуля для установки хуков.
 ******************************************************************************/
static void HookMod(HMODULE m) {
    if(!g_allowIatHook) return;
    if(!m) return;
    
    // Validate PE header / Проверить PE-заголовок
    __try { 
        if(((PIMAGE_DOS_HEADER)m)->e_magic != IMAGE_DOS_SIGNATURE) 
            return; 
    } __except(1) { 
        return; 
    }

    // Patch function pointers / Пропатчить указатели функций
    #define P(D,N,H,R) IAT_PatchByName(m, D, N, (void*)H, (void**)&R)
    P("GDI32.dll",  "SelectObject",       Hook_SelectObject,       Real_SelectObject);
    P("USER32.dll", "BeginPaint",         Hook_BeginPaint,         Real_BeginPaint);
    P("USER32.dll", "EndPaint",           Hook_EndPaint,           Real_EndPaint);
    P("USER32.dll", "GetDC",              Hook_GetDC,              Real_GetDC);
    P("USER32.dll", "GetWindowDC",        Hook_GetWindowDC,        Real_GetWindowDC);
    P("USER32.dll", "ReleaseDC",          Hook_ReleaseDC,          Real_ReleaseDC);
    P("USER32.dll", "GetDCEx",            Hook_GetDCEx,            Real_GetDCEx);
    P("GDI32.dll",  "CreateCompatibleDC", Hook_CreateCompatibleDC, Real_CreateCompatibleDC);
    P("GDI32.dll",  "DeleteDC",           Hook_DeleteDC,           Real_DeleteDC);
    #undef P
}

/*******************************************************************************
 * WINDOW SUBCLASSING
 * СУБКЛАССИНГ ОКОН
 ******************************************************************************/

// Window property keys / Ключи свойств окна
#define PROP_TAG   "MLF_TAG"    // Subclass tag / Тег субкласса
#define PROP_OLD   "MLF_OLD"    // Original WNDPROC / Оригинальная WNDPROC
#define PROP_TV_IP "MLF_TV_IP"  // TreeView in-progress flag / Флаг выполнения TreeView
#define PROP_LV_WP "MLF_LV_WP"  // ListView paint guard / Защита рисования ListView

// Win9x parent subclass properties / Свойства субкласса родителя Win9x
#define PROP_PW_TAG "MLF_PW_T"  // Parent window tag / Тег родительского окна
#define PROP_PW_OLD "MLF_PW_O"  // Parent window old proc / Старая процедура родительского окна

static BOOL CALLBACK EnumWnd(HWND h, LPARAM l); // Forward declaration / Предварительное объявление

/*******************************************************************************
 * IsStdCtrlClassA_
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Checks if class name is a standard control that shouldn't be parent-subclassed.
 * 
 * Проверяет, является ли имя класса стандартным контролом, который не должен быть субклассирован как родитель.
 ******************************************************************************/
static BOOL IsStdCtrlClassA_(const char* c) {
    if(!c || !c[0]) return TRUE;
    if(!lstrcmpiA(c,"SysListView32")) return TRUE;
    if(!lstrcmpiA(c,"SysTreeView32")) return TRUE;
    if(!lstrcmpiA(c,"Button")) return TRUE;
    if(!lstrcmpiA(c,"Static")) return TRUE;
    if(!lstrcmpiA(c,"Edit")) return TRUE;
    if(!lstrcmpiA(c,"ListBox")) return TRUE;
    if(!lstrcmpiA(c,"ComboBox")) return TRUE;
    if(!lstrcmpiA(c,"ComboLBox")) return TRUE;
    if(!lstrcmpiA(c,"ScrollBar")) return TRUE;
    return FALSE;
}

static void EnsureParentSubclass_9x(HWND h);

/*******************************************************************************
 * ParentProc_9x
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Window procedure for parent window subclass on Win9x. Catches WM_PARENTNOTIFY
 * to detect immediate child creation (TreeView/ListView) without CBT hook overhead.
 * 
 * Процедура окна для субкласса родительского окна на Win9x. Ловит WM_PARENTNOTIFY
 * для обнаружения создания непосредственных детей (TreeView/ListView) без накладных расходов CBT hook.
 * 
 * WHY WIN9X ONLY / ПОЧЕМУ ТОЛЬКО WIN9X:
 * On NT, we use CBT hook which is more reliable. On Win9x, CBT hooks can cause
 * crashes on exit, so we use this lighter approach.
 * 
 * На NT мы используем CBT hook, который более надёжен. На Win9x CBT hooks могут
 * вызывать краши при выходе, поэтому используем этот более лёгкий подход.
 ******************************************************************************/
static LRESULT CALLBACK ParentProc_9x(HWND h, UINT m, WPARAM w, LPARAM l) {
    WNDPROC old = (WNDPROC)GetPropA(h, PROP_PW_OLD);
    if(!old) old = DefWindowProcA;

    if(InterlockedExchangeAdd(&g_shuttingDown, 0) != 0) {
        return CallWindowProcA(old, h, m, w, l);
    }

    if(m == WM_NCDESTROY) {
        SetWindowLongA(h, GWL_WNDPROC, (LONG)old);
        RemovePropA(h, PROP_PW_OLD);
        RemovePropA(h, PROP_PW_TAG);
        return CallWindowProcA(old, h, m, w, l);
    }

    LRESULT ret = CallWindowProcA(old, h, m, w, l);

    if(m == WM_PARENTNOTIFY) {
        WORD ev = LOWORD(w);
        if(ev == WM_CREATE) {
            HWND ch = (HWND)l;
            if(ch && IsWindow(ch) && IsWindowInThisProcess(ch)) {
                char c[64]; 
                c[0]=0;
                GetClassNameA(ch, c, 63);

                if(!lstrcmpiA(c, "SysTreeView32") || !lstrcmpiA(c, "SysListView32")) {
                    // Immediately subclass and fix / Немедленно субклассировать и исправить
                    EnumWnd(ch, ENUM_CTX_CONFIRMED);
                    if(!lstrcmpiA(c,"SysTreeView32")) 
                        TreeView_AutoFixItemHeight(ch);
                    else 
                        ListView_AutoFixFontAndRelayout(ch, TRUE);
                } else {
                    // Propagate to containers / Распространить на контейнеры
                    EnsureParentSubclass_9x(ch);
                }
            }
        }
    }

    return ret;
}

/*******************************************************************************
 * EnsureParentSubclass_9x
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Installs parent window subclass on Win9x to catch child control creation.
 * 
 * Устанавливает субкласс родительского окна на Win9x для перехвата создания дочерних контролов.
 ******************************************************************************/
static void EnsureParentSubclass_9x(HWND h) {
    if(!g_is9x) return;
    if(!h || !IsWindow(h)) return;
    if(GetPropA(h, PROP_PW_TAG)) return;
    if(IsFileDlg(h)) return;

    // Only inside Media Library area
    // Только внутри области библиотеки
    if(GetWinCtx(h) != CTX_LIB) return;

    char c[64]; 
    c[0]=0;
    GetClassNameA(h, c, 63);
    if(IsStdCtrlClassA_(c)) return;

    // Install subclass / Установить субкласс
    WNDPROC prev = (WNDPROC)SetWindowLongA(h, GWL_WNDPROC, (LONG)ParentProc_9x);
    if(prev) {
        if(!SetPropA(h, PROP_PW_OLD, (HANDLE)prev) || 
           !SetPropA(h, PROP_PW_TAG, (HANDLE)1)) {
            // Rollback on failure / Откатить при неудаче
            SetWindowLongA(h, GWL_WNDPROC, (LONG)prev);
            RemovePropA(h, PROP_PW_OLD);
            RemovePropA(h, PROP_PW_TAG);
        }
    }
}

/*******************************************************************************
 * Subclass
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Main window procedure for subclassed controls. Handles:
 * - WM_SETFONT: Replaces fonts and updates TreeView/ListView
 * - Theme/style changes: Triggers updates
 * - WM_PAINT: Lazy font application
 * - WM_SHOWWINDOW (9x ListView): Re-applies font after tab/page switches
 * 
 * Основная процедура окна для субклассированных контролов. Обрабатывает:
 * - WM_SETFONT: Заменяет шрифты и обновляет TreeView/ListView
 * - Изменения темы/стиля: Запускает обновления
 * - WM_PAINT: Отложенное применение шрифта
 * - WM_SHOWWINDOW (9x ListView): Повторно применяет шрифт после переключения вкладок/страниц
 ******************************************************************************/
static LRESULT CALLBACK Subclass(HWND h, UINT m, WPARAM w, LPARAM l) {
    WNDPROC old = (WNDPROC)GetPropA(h, PROP_OLD);
    if(!old) old = DefWindowProcA;

    if(InterlockedExchangeAdd(&g_shuttingDown, 0) != 0) {
        return CallWindowProcA(old, h, m, w, l);
    }

    BYTE kind = GetCtrlKind(h);

    // WM_SETFONT: Replace font and update control
    // WM_SETFONT: Заменить шрифт и обновить контрол
    if(m == WM_SETFONT) {
        BYTE c = GetWinCtx(h);
        if(c && w) {
            HFONT r = GetReplFont((HFONT)w, c);
            if(r) w = (WPARAM)r;
        }

        LRESULT ret = CallWindowProcA(old, h, m, w, l);

        if(kind == CTRL_TV) {
            TreeView_AutoFixItemHeight(h);
        } else if(kind == CTRL_LV) {
            if(!GetPropA(h, PROP_LV_IP)) {
                ListView_AutoFixFontAndRelayout(h, FALSE);
            }
        }
        return ret;
    }

    // Handle theme/style changes / Обработать изменения темы/стиля
    if(kind) {
        if(m == WM_THEMECHANGED || m == WM_STYLECHANGED || 
           m == WM_SETTINGCHANGE || m == WM_FONTCHANGE) {
            LRESULT ret = CallWindowProcA(old, h, m, w, l);
            if(kind == CTRL_TV) 
                TreeView_AutoFixItemHeight(h);
            else 
                ListView_AutoFixFontAndRelayout(h, FALSE);
            return ret;
        }

        // Win9x: ListView may reset font on show / Win9x: ListView может сбросить шрифт при показе
        if(g_is9x && kind == CTRL_LV && m == WM_SHOWWINDOW && w) {
            LRESULT ret = CallWindowProcA(old, h, m, w, l);
            if(!GetPropA(h, PROP_LV_IP)) {
                ListView_AutoFixFontAndRelayout(h, TRUE);
            }
            return ret;
        }

        // Lazy font application on first paint / Отложенное применение шрифта при первой отрисовке
        if(m == WM_PAINT) {
            const char* prop = (kind == CTRL_TV) ? PROP_TV_IP : PROP_LV_WP;
            if(!GetPropA(h, prop)) {
                SetPropA(h, prop, (HANDLE)1);
                if(kind == CTRL_TV) 
                    TreeView_AutoFixItemHeight(h);
                else 
                    ListView_AutoFixFontAndRelayout(h, FALSE);
                LRESULT ret = CallWindowProcA(old, h, m, w, l);
                RemovePropA(h, prop);
                return ret;
            }
        }
    }

    // Cleanup on destroy / Очистка при уничтожении
    if(m == WM_NCDESTROY) {
        SetWindowLongA(h, GWL_WNDPROC, (LONG)old);
        RemovePropA(h, PROP_OLD);
        RemovePropA(h, PROP_TAG);
        RemovePropA(h, PROP_TV_IP);
        RemovePropA(h, PROP_LV_IP);
        RemovePropA(h, PROP_LV_WP);
        RemovePropA(h, PROP_LV_STAMP);
        return CallWindowProcA(old, h, m, w, l);
    }

    return CallWindowProcA(old, h, m, w, l);
}

/*******************************************************************************
 * EnumWnd
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * EnumChildWindows callback. Subclasses relevant controls and applies initial
 * font replacement.
 * 
 * Функция обратного вызова EnumChildWindows. Субклассирует релевантные контролы
 * и применяет начальную замену шрифта.
 ******************************************************************************/
static BOOL CALLBACK EnumWnd(HWND h, LPARAM l) {
    if(!IsWindowInThisProcess(h) || IsFileDlg(h)) return TRUE;

    char c[64]; 
    GetClassNameA(h, c, 63);

    // Win9x: parent-subclass containers / Win9x: субклассировать родительские контейнеры
    if(g_is9x && GetWinCtx(h) == CTX_LIB) {
        if(!IsStdCtrlClassA_(c)) {
            EnsureParentSubclass_9x(h);
        }
    }

    // Determine control kind / Определить вид контрола
    BYTE kind = CTRL_NONE;
    if(!lstrcmpiA(c,"SysTreeView32")) kind = CTRL_TV;
    else if(!lstrcmpiA(c,"SysListView32")) kind = CTRL_LV;

    // Skip irrelevant controls / Пропустить нерелевантные контролы
    if(!lstrcmpiA(c,"ListBox") && !lstrcmpiA(c,"Button") && 
       !lstrcmpiA(c,"Static") && !lstrcmpiA(c,"Edit") && !kind)
        return TRUE;

    // Install subclass if not already done / Установить субкласс, если ещё не сделано
    if(!GetPropA(h, PROP_TAG)) {
        if(l != ENUM_CTX_CONFIRMED && !GetWinCtx(h)) return TRUE;

        WNDPROC prev = (WNDPROC)SetWindowLongA(h, GWL_WNDPROC, (LONG)Subclass);
        if(prev) {
            // Atomic subclass installation / Атомарная установка субкласса
            if(!SetPropA(h, PROP_OLD, (HANDLE)prev) || 
               !SetPropA(h, PROP_TAG, (HANDLE)1)) {
                // Rollback on failure / Откатить при неудаче
                SetWindowLongA(h, GWL_WNDPROC, (LONG)prev);
                RemovePropA(h, PROP_OLD);
                RemovePropA(h, PROP_TAG);
                return TRUE;
            }

            // Trigger font replacement / Запустить замену шрифта
            HFONT f = (HFONT)SendMessageA(h, WM_GETFONT, 0, 0);
            if(!f) f = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
            SendMessageA(h, WM_SETFONT, (WPARAM)f, TRUE);

            // Apply special control fixes / Применить исправления для специальных контролов
            if(kind == CTRL_TV) 
                TreeView_AutoFixItemHeight(h);
            else if(kind == CTRL_LV) 
                ListView_AutoFixFontAndRelayout(h, TRUE);
        }
    }
    return TRUE;
}

/*******************************************************************************
 * APPLY AND REDRAW LOGIC
 * ЛОГИКА ПРИМЕНЕНИЯ И ПЕРЕРИСОВКИ
 ******************************************************************************/

/*******************************************************************************
 * EnumFixProc
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * EnumChildWindows callback to fix special controls (TreeView/ListView).
 * 
 * Функция обратного вызова EnumChildWindows для исправления специальных контролов.
 ******************************************************************************/
static BOOL CALLBACK EnumFixProc(HWND h, LPARAM lParam) {
    BOOL force = (lParam != 0);
    BYTE kind = GetCtrlKind(h);
    if(kind == CTRL_TV) 
        TreeView_AutoFixItemHeight(h);
    else if(kind == CTRL_LV) 
        ListView_AutoFixFontAndRelayout(h, force);
    return TRUE;
}

/*******************************************************************************
 * EnumFixListViewsOnlyProc
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Win9x periodic fix: Re-applies fonts to ListViews that may have been reset
 * during tab/page switches.
 * 
 * Периодическое исправление Win9x: Повторно применяет шрифты к ListView, которые
 * могли быть сброшены во время переключения вкладок/страниц.
 ******************************************************************************/
static BOOL CALLBACK EnumFixListViewsOnlyProc(HWND h, LPARAM lp) {
    (void)lp;
    if(GetCtrlKind(h) == CTRL_LV) {
        ListView_AutoFixFontAndRelayout(h, FALSE);
    }
    return TRUE;
}

/*******************************************************************************
 * Win9x_PollFix_ListViewsInMediaLibrary
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Win9x only: Periodically checks and fixes ListView fonts in Media Library.
 * Called from timer to catch font resets that occur during tab/page switches.
 * 
 * Только Win9x: Периодически проверяет и исправляет шрифты ListView в библиотеке.
 * Вызывается из таймера для перехвата сбросов шрифтов, происходящих при переключении вкладок/страниц.
 ******************************************************************************/
static void Win9x_PollFix_ListViewsInMediaLibrary(void) {
    if(InterlockedExchangeAdd(&g_shuttingDown, 0) != 0) return;

    HWND top = GetTopWindow(NULL);
    while(top) {
        if(IsWindowInThisProcess(top)) {
            BYTE cTop = GetWinCtx(top);
            if(cTop == CTX_LIB) {
                EnumChildWindows(top, EnumFixListViewsOnlyProc, 0);
            }
        }
        top = GetWindow(top, GW_HWNDNEXT);
    }
}

/*******************************************************************************
 * ForceRecalcAllSpecialControls
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Forces recalculation of all TreeView/ListView controls in Winamp windows.
 * 
 * Принудительно пересчитывает все контролы TreeView/ListView в окнах Winamp.
 ******************************************************************************/
static void ForceRecalcAllSpecialControls(BOOL forceListView) {
    HWND top = GetTopWindow(NULL);
    while(top) {
        if(IsWindowInThisProcess(top)) {
            BYTE cTop = GetWinCtx(top);
            if(cTop) 
                EnumChildWindows(top, EnumFixProc, (LPARAM)(forceListView ? 1 : 0));
        }
        top = GetWindow(top, GW_HWNDNEXT);
    }
}

/*******************************************************************************
 * ScanWindowsOnce
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Scans all windows once to ensure subclassing without causing redraw storms.
 * 
 * Сканирует все окна один раз для обеспечения субклассинга без вызывания штормов перерисовки.
 ******************************************************************************/
static void ScanWindowsOnce() {
    HWND h = GetTopWindow(NULL);
    while(h) {
        if(IsWindowInThisProcess(h)) {
            BYTE c = GetWinCtx(h);
            if(c) {
                // Win9x parent subclass / Win9x субкласс родителя
                if(g_is9x && c == CTX_LIB) 
                    EnsureParentSubclass_9x(h);
                
                // IAT hook module / IAT hook модуля
                if(g_allowIatHook) {
                    HMODULE m = (HMODULE)GetWindowLongA(h, GWL_HINSTANCE);
                    if(m) HookMod(m);
                }
                
                // Subclass children / Субклассировать детей
                EnumChildWindows(h, EnumWnd, ENUM_CTX_CONFIRMED);
            }
        }
        h = GetWindow(h, GW_HWNDNEXT);
    }
}

/*******************************************************************************
 * KickRedraw_Minimal
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Triggers minimal redraw of Winamp windows. Uses RDW_NOERASE to avoid flicker.
 * 
 * Запускает минимальную перерисовку окон Winamp. Использует RDW_NOERASE для избежания мигания.
 ******************************************************************************/
static void KickRedraw_Minimal() {
    HWND h = GetTopWindow(NULL);
    while(h) {
        if(IsWindowInThisProcess(h)) {
            BYTE c = GetWinCtx(h);
            if(c && IsWindowVisible(h)) {
                UINT flags = RDW_INVALIDATE | RDW_ALLCHILDREN | RDW_NOERASE;
                RedrawWindow(h, NULL, NULL, flags);
            }
        }
        h = GetWindow(h, GW_HWNDNEXT);
    }
}

/*******************************************************************************
 * Apply
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Main apply function. Updates skin data, detects changes, and triggers
 * necessary updates. This is called on skin changes and periodically.
 * 
 * Основная функция применения. Обновляет данные скина, обнаруживает изменения
 * и запускает необходимые обновления. Вызывается при изменениях скина и периодически.
 * 
 * PARAMETERS / ПАРАМЕТРЫ:
 * forceListViewRelayout - Force ListView relayout even if stamp unchanged
 *                         Принудительно перестроить ListView даже если отпечаток не изменился
 ******************************************************************************/
static void Apply(BOOL forceListViewRelayout) {
    if(InterlockedExchangeAdd(&g_shuttingDown, 0) != 0) return;

    UpdateSkinData();

    // Get current parameters / Получить текущие параметры
    char f[LF_FACESIZE]; 
    int h, cs;
    GetParams(CTX_LIB, f, &h, &cs);

    Lock(); 
    DWORD sh = g_skin_hash; 
    Unlock();
    
    // Create combined stamp / Создать объединённый отпечаток
    DWORD s = Hash(f) ^ (DWORD)h ^ ((DWORD)cs<<24) ^ Hash(g_skin_dir) ^ ((sh<<1)|(sh>>31));

    BOOL diff = (s != g_stamp);
    
    if(diff) { 
        g_stamp = s; 
        ResetCache(CTX_LIB); 
        ResetCache(CTX_PL); 
    }

    if (!diff) {
        forceListViewRelayout = FALSE;
    }

    ScanWindowsOnce();

    if(g_is9x && diff) 
        KickRedraw_Minimal();

    ForceRecalcAllSpecialControls(forceListViewRelayout);
}

/*******************************************************************************
 * TIMER MANAGEMENT
 * УПРАВЛЕНИЕ ТАЙМЕРАМИ
 ******************************************************************************/

// Timer IDs / ID таймеров
#define TMR_MLF_DEBOUNCE   0x4D4C  // Debounce timer for updates / Антидребезговый таймер для обновлений
#define TMR_MLF_STARTSCAN  0x4D4D  // Short follow-up scan timer / Короткий таймер последующего сканирования
#define TMR_MLF_SKINPOLL   0x4D4E  // Skin polling timer (Win9x) / Таймер опроса скина (Win9x)

static int g_followTicks = 0;  // Follow-up scan ticks remaining / Оставшиеся тики последующего сканирования
static volatile LONG g_forceListViewOnce = 0;  // One-time ListView force flag / Одноразовый флаг принудительного ListView

/*******************************************************************************
 * StartScan_Arm
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Arms the short follow-up scan timer. Used to catch late-created controls
 * after skin changes.
 * 
 * Взводит короткий таймер последующего сканирования. Используется для перехвата
 * поздно созданных контролов после изменений скина.
 ******************************************************************************/
static void StartScan_Arm() {
    if(!g_hWA) return;
    // Short window: 8 * 250ms = 2s (instead of 10s storm)
    // Короткое окно: 8 * 250мс = 2с (вместо шторма 10с)
    g_followTicks = g_is9x ? 8 : 1;
    SetTimer(g_hWA, TMR_MLF_STARTSCAN, 250, NULL);
}

/*******************************************************************************
 * ScheduleUpdate
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Schedules a debounced update. Kills existing timer and starts new one.
 * Multiple rapid changes will be batched into single update.
 * 
 * Планирует антидребезговое обновление. Убивает существующий таймер и запускает новый.
 * Множественные быстрые изменения будут пакетированы в одно обновление.
 ******************************************************************************/
static void ScheduleUpdate() {
    if(!g_hWA) return;
    KillTimer(g_hWA, TMR_MLF_DEBOUNCE);
    SetTimer(g_hWA, TMR_MLF_DEBOUNCE, (g_is9x ? 100 : 150), NULL);
}

/*******************************************************************************
 * QuerySkinStampFast
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Quickly queries current skin stamp without full UpdateSkinData overhead.
 * Used by Win9x polling timer to detect skin changes.
 * 
 * Быстро запрашивает текущий отпечаток скина без полных накладных расходов UpdateSkinData.
 * Используется таймером опроса Win9x для обнаружения изменений скина.
 ******************************************************************************/
static DWORD QuerySkinStampFast() {
    if(!g_hWA) return 0;

    // Get skin directory / Получить каталог скина
    char dir[MAX_PATH] = {0};
    __try { 
        SendMessageA(g_hWA, WM_WA_IPC, (WPARAM)dir, IPC_GETSKIN); 
    } __except(1) { 
        dir[0]=0; 
    }
    DWORD hDir = Hash(dir);

    // Get font parameters / Получить параметры шрифта
    char face[LF_FACESIZE] = {0};
    int cs = 0, ht = 0;

    __try {
        char* p = (char*)SendMessageA(g_hWA, WM_WA_IPC, 1, IPC_GET_GENSKINBITMAP);
        if(p && IsValidPtr(p) && *p) {
            lstrcpynA(face, p, LF_FACESIZE);
            cs = (int)SendMessageA(g_hWA, WM_WA_IPC, 2, IPC_GET_GENSKINBITMAP);
            ht = (int)SendMessageA(g_hWA, WM_WA_IPC, 3, IPC_GET_GENSKINBITMAP);
            if(cs<0||cs>255) cs = 0;
            if(ht<0||ht>200) ht = 0;
        }
    } __except(1) {
        face[0]=0; 
        cs=0; 
        ht=0;
    }

    DWORD hFace = Hash(face);
    return hDir ^ (hFace << 1) ^ ((DWORD)(BYTE)cs << 24) ^ (DWORD)(WORD)ht;
}

/*******************************************************************************
 * WINAMP WINDOW PROCEDURE
 * ПРОЦЕДУРА ОКНА WINAMP
 ******************************************************************************/

/*******************************************************************************
 * WAProc
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Subclassed Winamp main window procedure. Handles:
 * - Timers (debounce, follow-up scan, skin polling)
 * - Skin change events
 * - Theme/font/display changes
 * 
 * Процедура субклассированного главного окна Winamp. Обрабатывает:
 * - Таймеры (антидребезг, последующее сканирование, опрос скина)
 * - События смены скина
 * - Изменения темы/шрифта/дисплея
 ******************************************************************************/
static LRESULT CALLBACK WAProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    if(m == WM_TIMER) {
        // Debounce timer / Антидребезговый таймер
        if(w == TMR_MLF_DEBOUNCE) {
            KillTimer(h, TMR_MLF_DEBOUNCE);
            BOOL forceLV = (InterlockedExchange(&g_forceListViewOnce, 0) != 0);
            Apply(forceLV);
            StartScan_Arm();  // Short follow-up / Короткое последующее сканирование
            return 0;
        }
        
        // Follow-up scan timer / Таймер последующего сканирования
        if(w == TMR_MLF_STARTSCAN) {
            if(g_followTicks <= 0) {
                KillTimer(h, TMR_MLF_STARTSCAN);
                return 0;
            }
            g_followTicks--;

            // No redraw storms: only rescan+recalc
            // Никаких штормов перерисовки: только пересканировать+пересчитать
            ScanWindowsOnce();
            ForceRecalcAllSpecialControls(FALSE);
            return 0;
        }
        
        // Skin polling timer (Win9x only) / Таймер опроса скина (только Win9x)
        if(w == TMR_MLF_SKINPOLL) {
            DWORD s = QuerySkinStampFast();
            if(s && s != g_skinPollStamp) {
                g_skinPollStamp = s;
                // One-time stronger ListView refresh / Одноразовое более сильное обновление ListView
                InterlockedExchange(&g_forceListViewOnce, 1);
                ScheduleUpdate();
            }

            // Win9x: periodic ListView font fix / Win9x: периодическое исправление шрифта ListView
            if(g_is9x) {
                static DWORD s_last = 0;
                DWORD now = GetTickCount();
                if((DWORD)(now - s_last) >= 200) {  // ~5 Hz
                    s_last = now;
                    Win9x_PollFix_ListViewsInMediaLibrary();
                }
            }
            return 0;
        }
    }

    // Skin change events / События смены скина
    if((m==WM_DISPLAYCHANGE && !w && !l) ||
       (m==WM_WA_IPC && l==IPC_SKIN_CHANGED) ||
       m==WM_SETTINGCHANGE || m==WM_FONTCHANGE || 
       m==WM_THEMECHANGED || m==WM_SYSCOLORCHANGE)
    {
        KillTimer(h, TMR_MLF_DEBOUNCE);
        InterlockedExchange(&g_forceListViewOnce, 1);
        Apply(TRUE);
        StartScan_Arm();
    }

    // Cleanup on destroy / Очистка при уничтожении
    if(m == WM_NCDESTROY) {
        WNDPROC old = g_oldWA;

        KillTimer(h, TMR_MLF_DEBOUNCE);
        KillTimer(h, TMR_MLF_STARTSCAN);
        KillTimer(h, TMR_MLF_SKINPOLL);

        if(old) SetWindowLongA(h, GWL_WNDPROC, (LONG)old);
        g_oldWA = 0;
        g_hWA = 0;

        return CallWindowProcA(old ? old : DefWindowProcA, h, m, w, l);
    }

    return CallWindowProcA(g_oldWA ? g_oldWA : DefWindowProcA, h, m, w, l);
}

/*******************************************************************************
 * HELPER FUNCTIONS
 * ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ
 ******************************************************************************/

/*******************************************************************************
 * HookWindowChildren
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Hooks IAT and enumerates children for a window.
 * 
 * Хукает IAT и перечисляет детей для окна.
 ******************************************************************************/
static void HookWindowChildren(HWND h) {
    if(!h) return;
    if(g_allowIatHook) {
        HMODULE m = (HMODULE)GetWindowLongA(h, GWL_HINSTANCE);
        if(m) HookMod(m);
    }
    EnumChildWindows(h, EnumWnd, ENUM_CTX_CONFIRMED);
}

/*******************************************************************************
 * CBTProc
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * CBT hook procedure (NT only). Catches window creation to immediately
 * subclass Media Library and Playlist Editor windows and their controls.
 * 
 * Процедура CBT hook (только NT). Перехватывает создание окон для немедленного
 * субклассирования окон библиотеки и редактора плейлистов и их контролов.
 * 
 * NOT USED ON WIN9X / НЕ ИСПОЛЬЗУЕТСЯ НА WIN9X:
 * CBT hooks cause crashes on exit on Win9x. Use parent subclassing instead.
 * CBT hooks вызывают краши при выходе на Win9x. Использовать субклассинг родителя вместо этого.
 ******************************************************************************/
static LRESULT CALLBACK CBTProc(int n, WPARAM w, LPARAM l) {
    if(InterlockedExchangeAdd(&g_shuttingDown, 0) != 0) {
        return CallNextHookEx(g_hkCBT, n, w, l);
    }

    if(n == HCBT_CREATEWND) {
        HWND hw = (HWND)w;
        char c[64]; 
        c[0]=0;
        GetClassNameA(hw, c, 63);

        // Check for Media Library or Playlist Editor windows
        // Проверить окна библиотеки или редактора плейлистов
        if(!lstrcmpA(c,"Winamp Gen") || !lstrcmpA(c,"WinampLibrary") ||
           !lstrcmpA(c,"Winamp PE")  || !lstrcmpA(c,"WinampLibraryPE"))
        {
            HookWindowChildren(hw);
        }
        else if(!lstrcmpiA(c, "SysTreeView32") || !lstrcmpiA(c, "SysListView32")) {
            // Immediately subclass and fix special controls
            // Немедленно субклассировать и исправить специальные контролы
            EnumWnd(hw, ENUM_CTX_CONFIRMED);
            if(!lstrcmpiA(c,"SysTreeView32")) 
                TreeView_AutoFixItemHeight(hw);
            else 
                ListView_AutoFixFontAndRelayout(hw, TRUE);
        }
    }
    return CallNextHookEx(g_hkCBT, n, w, l);
}

/*******************************************************************************
 * INITIALIZATION AND CLEANUP
 * ИНИЦИАЛИЗАЦИЯ И ОЧИСТКА
 ******************************************************************************/

/*******************************************************************************
 * DetectOS
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Detects OS version and sets appropriate flags and defaults.
 * 
 * Определяет версию ОС и устанавливает соответствующие флаги и значения по умолчанию.
 ******************************************************************************/
static void DetectOS() {
    OSVERSIONINFO os; 
    ZeroMemory(&os, sizeof(os));
    os.dwOSVersionInfoSize = sizeof(os);
    GetVersionEx(&os);

    g_isNT = (os.dwPlatformId == VER_PLATFORM_WIN32_NT);
    g_is9x = (os.dwPlatformId == VER_PLATFORM_WIN32_WINDOWS);

    // Win98: no cleartype, safe default / Win98: нет cleartype, безопасное значение по умолчанию
    g_forceQuality = g_is9x ? DEFAULT_QUALITY : CLEARTYPE_QUALITY;

    // IAT hook settings / Настройки перехвата IAT
    if(g_isNT) 
        g_allowIatHook = (ENABLE_IAT_HOOK_ON_NT ? TRUE : FALSE);
    else if(g_is9x) 
        g_allowIatHook = (ENABLE_IAT_HOOK_ON_9X ? TRUE : FALSE);
    else 
        g_allowIatHook = FALSE;
}

/*******************************************************************************
 * ML_SmoothFonts_Init
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Initializes the module. Called once on DLL load.
 * 
 * Инициализирует модуль. Вызывается один раз при загрузке DLL.
 * 
 * PROCESS / ПРОЦЕСС:
 * 1. Detect OS version
 * 2. Initialize critical section
 * 3. Allocate TLS slot (if IAT hooking enabled)
 * 4. Find and subclass Winamp main window
 * 5. Install CBT hook (NT only)
 * 6. Update skin data
 * 7. Hook main executable
 * 8. Initial apply and scan
 * 9. Start skin polling timer (Win9x only)
 * 
 * 1. Определить версию ОС
 * 2. Инициализировать критическую секцию
 * 3. Выделить слот TLS (если перехват IAT включён)
 * 4. Найти и субклассировать главное окно Winamp
 * 5. Установить CBT hook (только NT)
 * 6. Обновить данные скина
 * 7. Хукнуть главный исполняемый файл
 * 8. Начальное применение и сканирование
 * 9. Запустить таймер опроса скина (только Win9x)
 ******************************************************************************/
extern "C" void ML_SmoothFonts_Init() {
    if(g_init) return;

    DetectOS();
    g_pid = GetCurrentProcessId();

    // Initialize critical section / Инициализировать критическую секцию
    __try { 
        InitializeCriticalSection(&g_cs); 
        g_cs_inited = TRUE; 
    } __except(1) { 
        g_cs_inited = FALSE; 
        return; 
    }

    // Allocate TLS slot / Выделить слот TLS
    if(g_allowIatHook) 
        g_tls_ctx = TlsAlloc();
    else 
        g_tls_ctx = 0xFFFFFFFF;

    // Find Winamp window / Найти окно Winamp
    g_hWA = FindWindowA("Winamp v1.x", NULL);
    if(g_hWA) {
        DWORD p=0; 
        GetWindowThreadProcessId(g_hWA, &p);
        if(p == g_pid) {
            // Subclass Winamp window / Субклассировать окно Winamp
            g_oldWA = (WNDPROC)SetWindowLongA(g_hWA, GWL_WNDPROC, (LONG)WAProc);

            // Install CBT hook (NT only) / Установить CBT hook (только NT)
            if(g_isNT) {
                g_hkCBT = SetWindowsHookExA(WH_CBT, CBTProc, HINST_THIS, 
                                           GetWindowThreadProcessId(g_hWA, NULL));
            } else {
                g_hkCBT = 0;  // Win9x: no CBT hook / Win9x: нет CBT hook
            }
        }
    }

    UpdateSkinData();
    if(g_allowIatHook) 
        HookMod(GetModuleHandle(NULL));

    // Apply now (no storm), then short follow-up scan
    // Применить сейчас (без шторма), затем короткое последующее сканирование
    Apply(TRUE);
    StartScan_Arm();

    // Win9x: skin polling timer / Win9x: таймер опроса скина
    if(g_is9x && g_hWA) {
        g_skinPollStamp = QuerySkinStampFast();
        SetTimer(g_hWA, TMR_MLF_SKINPOLL, 100, NULL);
    }

    g_init = TRUE;
}

/*******************************************************************************
 * ML_SmoothFonts_Quit
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Cleans up the module. Called on DLL unload.
 * 
 * Очищает модуль. Вызывается при выгрузке DLL.
 * 
 * CRITICAL / КРИТИЧНО:
 * Sets shutdown flag first to prevent new operations during cleanup.
 * Устанавливает флаг завершения сначала для предотвращения новых операций во время очистки.
 ******************************************************************************/
extern "C" void ML_SmoothFonts_Quit() {
    if(!g_init) return;

    // Signal shutdown / Сигнализировать завершение
    InterlockedExchange(&g_shuttingDown, 1);

    // Kill timers / Убить таймеры
    if(g_hWA) {
        KillTimer(g_hWA, TMR_MLF_DEBOUNCE);
        KillTimer(g_hWA, TMR_MLF_STARTSCAN);
        KillTimer(g_hWA, TMR_MLF_SKINPOLL);
    }

    // Unhook / Снять хук
    if(g_hkCBT) { 
        UnhookWindowsHookEx(g_hkCBT); 
        g_hkCBT = 0; 
    }

    // Unsubclass Winamp window / Снять субкласс окна Winamp
    if(g_hWA && g_oldWA) {
        SetWindowLongA(g_hWA, GWL_WNDPROC, (LONG)g_oldWA);
        g_oldWA = 0;
        g_hWA = 0;
    }

    // Delete all tracked fonts / Удалить все отслеживаемые шрифты
    Lock();
    for(UINT i=0; i<g_tracked_cnt; i++) 
        DeleteObject(g_tracked[i]);
    g_tracked_cnt = 0;
    Unlock();

    // Free TLS / Освободить TLS
    if(g_tls_ctx != 0xFFFFFFFF) { 
        TlsFree(g_tls_ctx); 
        g_tls_ctx = 0xFFFFFFFF; 
    }

    // Delete critical section / Удалить критическую секцию
    DeleteCriticalSection(&g_cs);
    g_cs_inited = FALSE;
    g_init = FALSE;
}

/*******************************************************************************
 * EXPORTED ALIASES
 * ЭКСПОРТИРУЕМЫЕ ПСЕВДОНИМЫ
 * 
 * These are alternative entry points for compatibility.
 * Это альтернативные точки входа для совместимости.
 ******************************************************************************/

extern "C" void PeFontNoAA_Init()  { ML_SmoothFonts_Init(); }
extern "C" void PeFontNoAA_Quit() { ML_SmoothFonts_Quit(); }