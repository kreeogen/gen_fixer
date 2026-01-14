/*******************************************************************************
 * mod_unicode_stream_fix.cpp (v3.1 Stable & Fixed)
 * 
 * SHOUTCAST/ICECAST STREAM METADATA UNICODE/MOJIBAKE FIX MODULE
 * Модуль исправления Unicode/mojibake для метаданных потоков SHOUTcast/Icecast
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * This module fixes character encoding issues (mojibake) in streaming audio
 * metadata from SHOUTcast and Icecast servers. It intercepts Winsock API calls
 * to process HTTP response headers and ICY metadata in real-time, correcting
 * encoding issues before the data reaches Winamp's audio plugins.
 * 
 * Этот модуль исправляет проблемы с кодировкой символов (mojibake) в метаданных
 * потокового аудио с серверов SHOUTcast и Icecast. Он перехватывает вызовы
 * API Winsock для обработки HTTP-заголовков ответов и ICY-метаданных в реальном
 * времени, исправляя проблемы кодировки до того, как данные попадут в аудио-плагины Winamp.
 * 
 * FEATURES / ВОЗМОЖНОСТИ:
 * 1. Correct typedef order (fixes build errors on older compilers)
 * 2. Aggressive socket tracking (fixes "missing metadata" and mojibake issues)
 * 3. Full WSARecv support (handles both recv and WSARecv API calls)
 * 4. Thread-safe implementation with critical sections
 * 5. Compatible with Visual Studio 2003 and Win9x/NT/2000/XP/Vista/7+
 * 
 * 1. Правильный порядок typedef (исправляет ошибки сборки на старых компиляторах)
 * 2. Агрессивное отслеживание сокетов (исправляет проблемы "отсутствующих метаданных" и mojibake)
 * 3. Полная поддержка WSARecv (обрабатывает как recv, так и WSARecv API)
 * 4. Потокобезопасная реализация с критическими секциями
 * 5. Совместимо с Visual Studio 2003 и Win9x/NT/2000/XP/Vista/7+
 * 
 * HOW IT WORKS / КАК ЭТО РАБОТАЕТ:
 * 1. Patches Import Address Table (IAT) of Winsock DLLs (WSOCK32.dll, WS2_32.dll)
 * 2. Redirects socket functions (send, recv, WSASend, WSARecv, closesocket)
 * 3. Tracks active streaming sockets in a context pool
 * 4. For each socket, parses HTTP headers to find icy-metaint value
 * 5. Uses metaint to separate audio data from metadata chunks
 * 6. Applies encoding fixes to both HTTP headers and ICY metadata
 * 7. Returns corrected data to Winamp
 * 
 * 1. Патчит таблицу адресов импорта (IAT) DLL Winsock (WSOCK32.dll, WS2_32.dll)
 * 2. Перенаправляет функции сокетов (send, recv, WSASend, WSARecv, closesocket)
 * 3. Отслеживает активные потоковые сокеты в пуле контекстов
 * 4. Для каждого сокета парсит HTTP-заголовки для поиска значения icy-metaint
 * 5. Использует metaint для разделения аудиоданных и блоков метаданных
 * 6. Применяет исправления кодировки как к HTTP-заголовкам, так и к ICY-метаданным
 * 7. Возвращает исправленные данные в Winamp
 * 
 * PROTOCOL DETAILS / ДЕТАЛИ ПРОТОКОЛА:
 * SHOUTcast/Icecast use a modified HTTP protocol with inline metadata:
 * - Client sends: Icy-MetaData: 1 header
 * - Server responds with: icy-metaint: N header (N = bytes between metadata)
 * - Stream format: [N bytes audio][1 byte length][length*16 bytes metadata][repeat]
 * - Metadata format: key='value';key='value'; (e.g., StreamTitle='Song - Artist';)
 * 
 * SHOUTcast/Icecast использует модифицированный HTTP-протокол со встроенными метаданными:
 * - Клиент отправляет: заголовок Icy-MetaData: 1
 * - Сервер отвечает: заголовок icy-metaint: N (N = байты между метаданными)
 * - Формат потока: [N байт аудио][1 байт длины][длина*16 байт метаданных][повтор]
 * - Формат метаданных: key='value';key='value'; (например, StreamTitle='Song - Artist';)
 * 
 * SUPPORTED HEADERS / ПОДДЕРЖИВАЕМЫЕ ЗАГОЛОВКИ:
 * - icy-name, ice-name, x-audiocast-name (station name)
 * - icy-genre, ice-genre, x-audiocast-genre (genre)
 * - icy-url, ice-url, x-audiocast-url (station URL)
 * - icy-metaint (metadata interval)
 * 
 * SUPPORTED METADATA / ПОДДЕРЖИВАЕМЫЕ МЕТАДАННЫЕ:
 * - StreamTitle (current song/show title)
 * - StreamUrl (optional URL)
 * 
 ******************************************************************************/

// SHOUTcast/Icecast fixer: icy-* / ICY- (StreamTitle/StreamUrl)
// Features:
// Особенности:
// 1. Correct typedef order (Fixes build errors).
//    Правильный порядок typedef (исправляет ошибки сборки).
// 2. Aggressive socket tracking (Fixes "missing metadata" / mojibake).
//    Агрессивное отслеживание сокетов (исправляет "отсутствующие метаданные" / mojibake).
// 3. Full WSARecv support.
//    Полная поддержка WSARecv.
// 4. Thread-safe & VS2003 compatible.
//    Потокобезопасно и совместимо с VS2003.

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <process.h>
#include <string.h>
#include <tlhelp32.h>
#include "..\Decrypt Engine\unicode_decrypt_engine.h"

// Array size macro for compile-time array length calculation
// Макрос размера массива для вычисления длины массива во время компиляции
#ifndef ARRAYSIZE
#define ARRAYSIZE(a) (sizeof(a)/sizeof((a)[0]))
#endif

/*******************************************************************************
 * CONFIGURATION CONSTANTS
 * КОНСТАНТЫ КОНФИГУРАЦИИ
 ******************************************************************************/

// Maximum size for temporary buffers (4KB)
// Максимальный размер для временных буферов (4КБ)
#define ABUF_MAX      4096

// Maximum size for HTTP header scanning (16KB - enough for most headers)
// Максимальный размер для сканирования HTTP-заголовков (16КБ - достаточно для большинства заголовков)
#define HDR_SCAN_MAX  16384

// Maximum number of simultaneous streaming sockets to track
// Increased pool size for safety (typical usage: 1-2 streams)
// Максимальное количество одновременных потоковых сокетов для отслеживания
// Увеличенный размер пула для безопасности (типичное использование: 1-2 потока)
#define MAX_STREAMS   8

/*******************************************************************************
 * WINSOCK TYPE DEFINITIONS
 * ОПРЕДЕЛЕНИЯ ТИПОВ WINSOCK
 * 
 * IMPORTANT / ВАЖНО:
 * These type definitions are placed at the top of the file to fix build
 * errors on older compilers (VS2003). The order matters - function pointer
 * typedefs must come after the basic type definitions they reference.
 * 
 * Эти определения типов размещены в начале файла для исправления ошибок
 * сборки на старых компиляторах (VS2003). Порядок важен - typedef указателей
 * на функции должен идти после определений базовых типов, на которые они ссылаются.
 ******************************************************************************/

// Socket handle type (UINT_PTR for 32/64-bit compatibility)
// Тип дескриптора сокета (UINT_PTR для совместимости 32/64-бит)
typedef UINT_PTR SOCKET;

// FAR pointer modifier (legacy from 16-bit Windows, now empty)
// Модификатор указателя FAR (наследие от 16-битной Windows, теперь пустой)
#ifndef FAR
#define FAR
#endif

// Unsigned long type (4 bytes)
// Тип unsigned long (4 байта)
typedef unsigned long u_long;

// Winsock scatter-gather buffer structure
// Used for WSASend/WSARecv to send/receive data in multiple buffers
// Структура буфера scatter-gather для Winsock
// Используется для WSASend/WSARecv для отправки/получения данных в нескольких буферах
typedef struct _WSABUF { 
    u_long len;      // Length of buffer / Длина буфера
    char FAR* buf;   // Pointer to buffer / Указатель на буфер
} WSABUF, *LPWSABUF;

// Overlapped I/O structure (opaque pointer)
// Used for asynchronous socket operations
// Структура перекрывающегося ввода-вывода (непрозрачный указатель)
// Используется для асинхронных операций сокетов
typedef void* LPWSAOVERLAPPED;

// Completion routine callback type for asynchronous operations
// Тип функции обратного вызова для завершения асинхронных операций
typedef void (CALLBACK *LPWSAOVERLAPPED_COMPLETION_ROUTINE)(DWORD, DWORD, LPWSAOVERLAPPED, DWORD);

// Function pointer types for Winsock API functions we will intercept
// Типы указателей на функции для функций Winsock API, которые мы будем перехватывать

// send: Synchronous data send / Синхронная отправка данных
typedef int (WINAPI *PFN_send)(SOCKET, const char*, int, int);

// WSASend: Asynchronous/scatter-gather send / Асинхронная/scatter-gather отправка
typedef int (WINAPI *PFN_WSASend)(SOCKET, LPWSABUF, DWORD, LPDWORD, DWORD, LPWSAOVERLAPPED, LPWSAOVERLAPPED_COMPLETION_ROUTINE);

// recv: Synchronous data receive / Синхронное получение данных
typedef int (WINAPI *PFN_recv)(SOCKET, char*, int, int);

// WSARecv: Asynchronous/scatter-gather receive / Асинхронное/scatter-gather получение
typedef int (WINAPI *PFN_WSARecv)(SOCKET, LPWSABUF, DWORD, LPDWORD, LPDWORD, LPWSAOVERLAPPED, LPWSAOVERLAPPED_COMPLETION_ROUTINE);

// closesocket: Close socket / Закрыть сокет
typedef int (WINAPI *PFN_closesocket)(SOCKET);

/*******************************************************************************
 * SOCKET CONTEXT STRUCTURE AND POOL
 * СТРУКТУРА КОНТЕКСТА СОКЕТА И ПУЛ
 ******************************************************************************/

/*******************************************************************************
 * SockCtx Structure
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Maintains state for a single streaming socket. Tracks HTTP header parsing,
 * icy-metaint value, and current position in the audio/metadata stream.
 * 
 * Поддерживает состояние для одного потокового сокета. Отслеживает парсинг
 * HTTP-заголовков, значение icy-metaint и текущую позицию в потоке аудио/метаданных.
 * 
 * FIELDS / ПОЛЯ:
 * s           - Socket handle / Дескриптор сокета
 * inUse       - TRUE if this context is active / TRUE если этот контекст активен
 * headerDone  - TRUE when HTTP header fully received and parsed
 *               TRUE когда HTTP-заголовок полностью получен и разобран
 * hdrLen      - Current length of accumulated header data
 *               Текущая длина накопленных данных заголовка
 * hdrBuf      - Buffer for accumulating header data across multiple recv calls
 *               Буфер для накопления данных заголовка через несколько вызовов recv
 * metaInt     - icy-metaint value: bytes of audio between metadata chunks
 *               значение icy-metaint: байты аудио между блоками метаданных
 * leftAudio   - Bytes remaining in current audio chunk before next metadata
 *               Байты, оставшиеся в текущем блоке аудио перед следующими метаданными
 * needLenByte - TRUE if next byte should be metadata length byte
 *               TRUE если следующий байт должен быть байтом длины метаданных
 * metaLeft    - Bytes remaining in current metadata chunk
 *               Байты, оставшиеся в текущем блоке метаданных
 ******************************************************************************/
typedef struct SockCtx {
    SOCKET s;                      // Socket handle / Дескриптор сокета
    BOOL   inUse;                  // Context active flag / Флаг активности контекста
    int    headerDone;             // Header parsing complete / Парсинг заголовка завершён
    int    hdrLen;                 // Accumulated header length / Накопленная длина заголовка
    char   hdrBuf[HDR_SCAN_MAX];   // Header accumulation buffer / Буфер накопления заголовка
    DWORD  metaInt;                // Metadata interval (icy-metaint) / Интервал метаданных (icy-metaint)
    DWORD  leftAudio;              // Remaining audio bytes / Оставшиеся байты аудио
    int    needLenByte;            // Expecting metadata length byte / Ожидание байта длины метаданных
    DWORD  metaLeft;               // Remaining metadata bytes / Оставшиеся байты метаданных
} SockCtx;

// Pool of socket contexts (zero-initialized by default in .bss section)
// Пул контекстов сокетов (инициализирован нулями по умолчанию в секции .bss)
static SockCtx g_ctxPool[MAX_STREAMS];

// Critical section for thread-safe access to context pool
// Критическая секция для потокобезопасного доступа к пулу контекстов
static CRITICAL_SECTION g_cs;

// Initialization state of critical section (0=not inited, 1=initing, 2=ready)
// Uses atomic operations for thread-safe initialization
// Состояние инициализации критической секции (0=не инициализирована, 1=инициализируется, 2=готова)
// Использует атомарные операции для потокобезопасной инициализации
static volatile LONG g_csInited = 0;

/*******************************************************************************
 * CRITICAL SECTION MANAGEMENT
 * УПРАВЛЕНИЕ КРИТИЧЕСКОЙ СЕКЦИЕЙ
 ******************************************************************************/

/*******************************************************************************
 * CS_Init
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Thread-safe initialization of the critical section using double-checked
 * locking pattern with atomic operations.
 * 
 * Потокобезопасная инициализация критической секции с использованием паттерна
 * двойной проверки блокировки с атомарными операциями.
 * 
 * ALGORITHM / АЛГОРИТМ:
 * 1. Try to atomically change state from 0 (not inited) to 1 (initing)
 * 2. If successful (we're first thread):
 *    - Initialize the critical section
 *    - Change state to 2 (ready)
 * 3. If not successful (another thread is initing):
 *    - Spin-wait until state becomes 2 (ready)
 * 
 * 1. Попытаться атомарно изменить состояние с 0 (не инициализирована) на 1 (инициализируется)
 * 2. Если успешно (мы первый поток):
 *    - Инициализировать критическую секцию
 *    - Изменить состояние на 2 (готова)
 * 3. Если не успешно (другой поток инициализирует):
 *    - Крутиться в ожидании, пока состояние не станет 2 (готова)
 ******************************************************************************/
static void CS_Init(void) {
    // Try to claim initialization (atomic compare-and-swap: if 0, set to 1)
    // Попытаться захватить инициализацию (атомарное сравнение и замена: если 0, установить в 1)
    if (InterlockedCompareExchange(&g_csInited, 1, 0) == 0) {
        // We won the race - initialize the critical section
        // Мы выиграли гонку - инициализировать критическую секцию
        InitializeCriticalSection(&g_cs);
        
        // Mark as ready (atomic set to 2)
        // Отметить как готовую (атомарная установка в 2)
        InterlockedExchange(&g_csInited, 2);
    } else {
        // Another thread is initializing - wait until ready
        // Другой поток инициализирует - ждать до готовности
        while (InterlockedCompareExchange(&g_csInited, 2, 2) != 2) 
            Sleep(1);  // Brief sleep to avoid busy-waiting / Короткий сон для избежания активного ожидания
    }
}

/*******************************************************************************
 * CS_Done
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Thread-safe cleanup of the critical section. Only deletes if currently
 * in ready state.
 * 
 * Потокобезопасная очистка критической секции. Удаляет только если в данный
 * момент в готовом состоянии.
 ******************************************************************************/
static void CS_Done(void) {
    // Atomically change from 2 (ready) to 0 (not inited) and delete if successful
    // Атомарно изменить с 2 (готова) на 0 (не инициализирована) и удалить, если успешно
    if (InterlockedCompareExchange(&g_csInited, 0, 2) == 2)
        DeleteCriticalSection(&g_cs);
}

/*******************************************************************************
 * Lock / Unlock
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Wrapper functions for entering/leaving critical section with safety check.
 * Only attempts to lock if critical section is ready (state == 2).
 * 
 * Функции-обёртки для входа/выхода из критической секции с проверкой безопасности.
 * Пытается заблокировать только если критическая секция готова (состояние == 2).
 ******************************************************************************/
static void Lock(void)   { 
    if (g_csInited == 2) 
        EnterCriticalSection(&g_cs); 
}

static void Unlock(void) { 
    if (g_csInited == 2) 
        LeaveCriticalSection(&g_cs); 
}

/*******************************************************************************
 * SOCKET CONTEXT POOL MANAGEMENT
 * УПРАВЛЕНИЕ ПУЛОМ КОНТЕКСТОВ СОКЕТОВ
 ******************************************************************************/

/*******************************************************************************
 * Sock_Find
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Searches the context pool for a context associated with the given socket.
 * 
 * Ищет в пуле контекстов контекст, связанный с данным сокетом.
 * 
 * PARAMETERS / ПАРАМЕТРЫ:
 * s - Socket handle to search for / Дескриптор сокета для поиска
 * 
 * RETURNS / ВОЗВРАЩАЕТ:
 * Pointer to SockCtx if found, NULL if not found
 * Указатель на SockCtx если найден, NULL если не найден
 * 
 * THREAD SAFETY / ПОТОКОБЕЗОПАСНОСТЬ:
 * Caller must hold Lock() before calling this function!
 * Вызывающая сторона должна удерживать Lock() перед вызовом этой функции!
 ******************************************************************************/
static SockCtx* Sock_Find(SOCKET s) {
    // Note: Caller must hold Lock!
    // Примечание: вызывающая сторона должна удерживать Lock!
    
    // Linear search through pool / Линейный поиск по пулу
    for (int i = 0; i < MAX_STREAMS; ++i) {
        if (g_ctxPool[i].inUse && g_ctxPool[i].s == s) 
            return &g_ctxPool[i];
    }
    return NULL;  // Not found / Не найден
}

/*******************************************************************************
 * Sock_Add
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Adds a new socket to the context pool, or returns existing context if
 * socket is already tracked. Creates and initializes a new context if needed.
 * 
 * Добавляет новый сокет в пул контекстов или возвращает существующий контекст,
 * если сокет уже отслеживается. Создаёт и инициализирует новый контекст при необходимости.
 * 
 * PARAMETERS / ПАРАМЕТРЫ:
 * s - Socket handle to add / Дескриптор сокета для добавления
 * 
 * RETURNS / ВОЗВРАЩАЕТ:
 * Pointer to SockCtx (new or existing), or NULL if pool is full
 * Указатель на SockCtx (новый или существующий), или NULL если пул заполнен
 * 
 * THREAD SAFETY / ПОТОКОБЕЗОПАСНОСТЬ:
 * Handles its own locking internally.
 * Обрабатывает собственную блокировку внутренне.
 ******************************************************************************/
static SockCtx* Sock_Add(SOCKET s) {
    Lock();
    
    // Check if already exists / Проверить, уже существует ли
    SockCtx* c = Sock_Find(s);
    if (c) { 
        Unlock(); 
        return c;  // Already exists / Уже существует
    }

    // Find first free slot in pool / Найти первый свободный слот в пуле
    for (int i = 0; i < MAX_STREAMS; ++i) {
        if (!g_ctxPool[i].inUse) {
            c = &g_ctxPool[i];
            
            // Reset/initialize context state / Сбросить/инициализировать состояние контекста
            c->s = s;
            c->inUse = TRUE;
            c->headerDone = 0;
            c->hdrLen = 0;
            c->metaInt = 0;
            c->leftAudio = 0;
            c->needLenByte = 1;
            c->metaLeft = 0;
            
            Unlock();
            return c;
        }
    }
    
    // Pool is full - return NULL / Пул заполнен - вернуть NULL
    Unlock();
    return NULL;
}

/*******************************************************************************
 * Sock_Del
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Removes a socket from the context pool by marking its context as not in use.
 * 
 * Удаляет сокет из пула контекстов, отмечая его контекст как не используемый.
 * 
 * PARAMETERS / ПАРАМЕТРЫ:
 * s - Socket handle to remove / Дескриптор сокета для удаления
 * 
 * THREAD SAFETY / ПОТОКОБЕЗОПАСНОСТЬ:
 * Handles its own locking internally.
 * Обрабатывает собственную блокировку внутренне.
 ******************************************************************************/
static void Sock_Del(SOCKET s) {
    Lock();
    
    // Find and mark as free / Найти и отметить как свободный
    for (int i = 0; i < MAX_STREAMS; ++i) {
        if (g_ctxPool[i].inUse && g_ctxPool[i].s == s) {
            g_ctxPool[i].inUse = FALSE;  // Mark as free / Отметить как свободный
            break;
        }
    }
    
    Unlock();
}

/*******************************************************************************
 * REAL API FUNCTION POINTERS
 * УКАЗАТЕЛИ НА РЕАЛЬНЫЕ ФУНКЦИИ API
 * 
 * These pointers store the addresses of the original Winsock functions before
 * we patch them. They are used to call the real implementation after we've
 * processed the data.
 * 
 * Эти указатели хранят адреса оригинальных функций Winsock до того, как мы
 * их пропатчили. Они используются для вызова реальной реализации после того,
 * как мы обработали данные.
 ******************************************************************************/

static PFN_send         Real_send         = NULL;  // Original send function / Оригинальная функция send
static PFN_WSASend      Real_WSASend      = NULL;  // Original WSASend function / Оригинальная функция WSASend
static PFN_recv         Real_recv         = NULL;  // Original recv function / Оригинальная функция recv
static PFN_WSARecv      Real_WSARecv      = NULL;  // Original WSARecv function / Оригинальная функция WSARecv
static PFN_closesocket  Real_closesocket  = NULL;  // Original closesocket function / Оригинальная функция closesocket

/*******************************************************************************
 * HEADER/METADATA PARSING AND FIXING HELPERS
 * ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ ПАРСИНГА И ИСПРАВЛЕНИЯ ЗАГОЛОВКОВ/МЕТАДАННЫХ
 ******************************************************************************/

/*******************************************************************************
 * key_eq_ci
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Case-insensitive string comparison for checking header keys. Compares the
 * beginning of string 's' with the key string.
 * 
 * Сравнение строк без учёта регистра для проверки ключей заголовков. Сравнивает
 * начало строки 's' со строкой ключа.
 * 
 * PARAMETERS / ПАРАМЕТРЫ:
 * s   - String to check / Строка для проверки
 * key - Key string to match / Строка ключа для сопоставления
 * 
 * RETURNS / ВОЗВРАЩАЕТ:
 * TRUE if 's' starts with 'key' (case-insensitive), FALSE otherwise
 * TRUE если 's' начинается с 'key' (без учёта регистра), FALSE иначе
 ******************************************************************************/
static BOOL key_eq_ci(const char* s, const char* key) {
    while (*key) {
        char a = *s++, b = *key++;
        
        // Convert to lowercase for comparison / Преобразовать в нижний регистр для сравнения
        if (a >= 'A' && a <= 'Z') a = (char)(a + 'a' - 'A');
        if (b >= 'A' && b <= 'Z') b = (char)(b + 'a' - 'A');
        
        if (a != b) return FALSE;
    }
    return TRUE;
}

/*******************************************************************************
 * FixHeaderValueInplace
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Fixes encoding of a single HTTP header line in-place. Identifies known
 * headers that commonly have encoding issues (station name, genre, URL) and
 * applies mojibake fix to their values.
 * 
 * Исправляет кодировку одной строки HTTP-заголовка на месте. Идентифицирует
 * известные заголовки, которые обычно имеют проблемы с кодировкой (название
 * станции, жанр, URL) и применяет исправление mojibake к их значениям.
 * 
 * PARAMETERS / ПАРАМЕТРЫ:
 * lineStart - Pointer to start of header line
 *             Указатель на начало строки заголовка
 * lineEnd   - Pointer to end of header line
 *             Указатель на конец строки заголовка
 * 
 * PROCESS / ПРОЦЕСС:
 * 1. Find colon separator
 * 2. Check if header key matches known problematic headers
 * 3. If match, extract value after colon
 * 4. Apply encoding fix
 * 5. Write fixed value back (in-place)
 * 
 * 1. Найти разделитель двоеточие
 * 2. Проверить, соответствует ли ключ заголовка известным проблемным заголовкам
 * 3. Если совпадение, извлечь значение после двоеточия
 * 4. Применить исправление кодировки
 * 5. Записать исправленное значение обратно (на месте)
 ******************************************************************************/
static void FixHeaderValueInplace(char* lineStart, char* lineEnd) {
    // List of headers that commonly have encoding issues
    // Список заголовков, которые обычно имеют проблемы с кодировкой
    static const char* keys[] = {
        "icy-name:", "ice-name:", "x-audiocast-name:",      // Station name / Название станции
        "icy-genre:", "ice-genre:", "x-audiocast-genre:",   // Genre / Жанр
        "icy-url:", "ice-url:", "x-audiocast-url:"          // Station URL / URL станции
    };
    
    // Find colon separator / Найти разделитель двоеточие
    char* colon = NULL;
    for (char* p = lineStart; p < lineEnd; ++p) {
        if (*p == ':') { colon = p; break; }
    }
    if (!colon) return;  // No colon found / Двоеточие не найдено

    // Check if this header matches any known problematic headers
    // Проверить, соответствует ли этот заголовок любому известному проблемному заголовку
    BOOL match = FALSE;
    for (int i = 0; i < ARRAYSIZE(keys); i++) {
        int kl = (int)lstrlenA(keys[i]);
        if ((lineEnd - lineStart) >= kl && key_eq_ci(lineStart, keys[i])) {
            match = TRUE; 
            break;
        }
    }
    if (!match) return;  // Not a header we care about / Не заголовок, который нас интересует

    // Extract value (skip whitespace after colon)
    // Извлечь значение (пропустить пробелы после двоеточия)
    const char* vbeg = colon + 1;
    while (*vbeg == ' ' || *vbeg == '\t') ++vbeg;
    
    int vlen = (int)(lineEnd - (char*)vbeg);
    if (vlen <= 0) return;  // Empty value / Пустое значение

    // Copy value to temporary buffer / Копировать значение во временный буфер
    char val[ABUF_MAX] = {0};
    int take = (vlen < ABUF_MAX - 1) ? vlen : (ABUF_MAX - 1);
    CopyMemory(val, vbeg, take);
    val[take] = 0;

    // Apply encoding fix / Применить исправление кодировки
    char fixed[ABUF_MAX] = {0};
    if (DECRYPT_ToACP_Best(val, fixed, ABUF_MAX) > 0) {
        // Write fixed value back (in-place)
        // Записать исправленное значение обратно (на месте)
        int ulen = (int)lstrlenA(fixed);
        if (ulen > vlen) ulen = vlen;  // Don't overflow original space / Не переполнять оригинальное пространство
        
        CopyMemory((char*)vbeg, fixed, ulen);
        
        // Zero out any remaining space / Обнулить оставшееся пространство
        if (ulen < vlen) 
            ZeroMemory((char*)vbeg + ulen, vlen - ulen);
    }
}

/*******************************************************************************
 * FixHeaderFieldsInChunk
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Processes a chunk of data that may contain HTTP headers, identifying
 * individual header lines and applying encoding fixes to each.
 * 
 * Обрабатывает блок данных, который может содержать HTTP-заголовки,
 * идентифицируя отдельные строки заголовков и применяя исправления
 * кодировки к каждой.
 * 
 * PARAMETERS / ПАРАМЕТРЫ:
 * chunk - Pointer to data chunk / Указатель на блок данных
 * len   - Length of chunk in bytes / Длина блока в байтах
 * 
 * ALGORITHM / АЛГОРИТМ:
 * 1. Scan through chunk byte by byte
 * 2. Identify line boundaries (lines end with \r\n or \n)
 * 3. For each line, call FixHeaderValueInplace
 * 4. Stop after HDR_SCAN_MAX bytes to avoid excessive processing
 * 
 * 1. Сканировать блок побайтно
 * 2. Идентифицировать границы строк (строки заканчиваются \r\n или \n)
 * 3. Для каждой строки вызвать FixHeaderValueInplace
 * 4. Остановиться после HDR_SCAN_MAX байт, чтобы избежать чрезмерной обработки
 ******************************************************************************/
static void FixHeaderFieldsInChunk(char* chunk, int len) {
    if (!chunk || len <= 0) return;
    
    int i = 0;
    while (i < len) {
        // Find start of line / Найти начало строки
        int s = i;
        
        // Find end of line (before CR or LF)
        // Найти конец строки (перед CR или LF)
        while (i < len && chunk[i] != '\n' && chunk[i] != '\r') ++i;
        int e = i;
        
        // Skip CR/LF characters / Пропустить символы CR/LF
        while (i < len && (chunk[i] == '\r' || chunk[i] == '\n')) ++i;
        
        // Process line if it has content / Обработать строку, если она имеет содержимое
        if (e > s) 
            FixHeaderValueInplace(chunk + s, chunk + e);
        
        // Safety limit / Ограничение безопасности
        if (i > HDR_SCAN_MAX) break;
    }
}

/*******************************************************************************
 * ParseDec32
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Parses a decimal integer from a string (used for icy-metaint value).
 * 
 * Парсит десятичное целое число из строки (используется для значения icy-metaint).
 * 
 * PARAMETERS / ПАРАМЕТРЫ:
 * s - String containing decimal number / Строка, содержащая десятичное число
 * 
 * RETURNS / ВОЗВРАЩАЕТ:
 * DWORD - Parsed value, or 0 if invalid / Разобранное значение или 0, если недопустимо
 ******************************************************************************/
static DWORD ParseDec32(const char* s) {
    DWORD v = 0;
    if (!s) return 0;
    
    // Skip leading whitespace / Пропустить ведущие пробелы
    while (*s == ' ' || *s == '\t') ++s;
    
    // Parse digits / Разобрать цифры
    while (*s >= '0' && *s <= '9') {
        v = v * 10 + (DWORD)(*s - '0');
        ++s;
        
        // Prevent overflow / Предотвратить переполнение
        if (v > 0x7FFFFFFF) break;
    }
    
    return v;
}

/*******************************************************************************
 * FindHeaderEnd
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Finds the end of HTTP headers in a buffer. HTTP headers end with either
 * \r\n\r\n (CRLF CRLF) or \n\n (LF LF).
 * 
 * Находит конец HTTP-заголовков в буфере. HTTP-заголовки заканчиваются либо
 * \r\n\r\n (CRLF CRLF), либо \n\n (LF LF).
 * 
 * PARAMETERS / ПАРАМЕТРЫ:
 * b - Buffer to search / Буфер для поиска
 * n - Buffer length / Длина буфера
 * 
 * RETURNS / ВОЗВРАЩАЕТ:
 * Position of first byte after headers (start of body), or -1 if not found
 * Позиция первого байта после заголовков (начало тела), или -1 если не найдено
 ******************************************************************************/
static int FindHeaderEnd(const char* b, int n) {
    // Look for \r\n\r\n / Искать \r\n\r\n
    for (int i = 0; i + 3 < n; i++)
        if (b[i] == '\r' && b[i + 1] == '\n' && b[i + 2] == '\r' && b[i + 3] == '\n') 
            return i + 4;
    
    // Look for \n\n / Искать \n\n
    for (int i = 0; i + 1 < n; i++)
        if (b[i] == '\n' && b[i + 1] == '\n') 
            return i + 2;
    
    return -1;  // Not found / Не найдено
}

/*******************************************************************************
 * FixIcyMetadataInPlace
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Fixes encoding of ICY metadata (StreamTitle, StreamUrl) in-place. This is
 * the metadata that's embedded in the audio stream at regular intervals.
 * 
 * Исправляет кодировку ICY-метаданных (StreamTitle, StreamUrl) на месте. Это
 * метаданные, которые встроены в аудиопоток через регулярные интервалы.
 * 
 * PARAMETERS / ПАРАМЕТРЫ:
 * meta    - Pointer to metadata buffer / Указатель на буфер метаданных
 * metaLen - Length of metadata in bytes / Длина метаданных в байтах
 * 
 * METADATA FORMAT / ФОРМАТ МЕТАДАННЫХ:
 * StreamTitle='Artist - Song Title';StreamUrl='http://...';
 * 
 * PROCESS / ПРОЦЕСС:
 * 1. Copy metadata to temporary buffer
 * 2. Find "StreamTitle='" in the string
 * 3. Extract the value between single quotes
 * 4. Apply encoding fix to the extracted value
 * 5. Reconstruct the metadata string with fixed value
 * 6. Write back to original buffer (in-place)
 * 
 * 1. Копировать метаданные во временный буфер
 * 2. Найти "StreamTitle='" в строке
 * 3. Извлечь значение между одинарными кавычками
 * 4. Применить исправление кодировки к извлечённому значению
 * 5. Реконструировать строку метаданных с исправленным значением
 * 6. Записать обратно в оригинальный буфер (на месте)
 ******************************************************************************/
static void FixIcyMetadataInPlace(unsigned char* meta, int metaLen) {
    // Copy to temporary buffer for processing
    // Копировать во временный буфер для обработки
    char in[4096] = {0};
    int m = (metaLen < (int)sizeof(in) - 1) ? metaLen : (int)sizeof(in) - 1;
    CopyMemory(in, meta, m);
    in[m] = 0;

    // Look for StreamTitle field / Искать поле StreamTitle
    const char* key = "StreamTitle='";
    const char* pkey = strstr(in, key);
    if (pkey) {
        // Find value between quotes / Найти значение между кавычками
        const char* p = pkey + lstrlenA(key);
        const char* e = strchr(p, '\'');
        if (e) {
            // Extract title value / Извлечь значение названия
            char title[2048] = {0};
            int L = (int)(e - p);
            if (L > (int)sizeof(title) - 1) L = (int)sizeof(title) - 1;
            if (L > 0) CopyMemory(title, p, L);
            title[L] = 0;

            // Apply encoding fix / Применить исправление кодировки
            char fixed[2048] = {0};
            if (DECRYPT_ToACP_Best(title, fixed, sizeof(fixed)) > 0) {
                // Reconstruct metadata string / Реконструировать строку метаданных
                char out[4096] = {0};
                int pos = 0;
                
                // Copy everything before StreamTitle / Копировать всё перед StreamTitle
                int pre = (int)(pkey - in);
                if (pre > 0) { 
                    CopyMemory(out, in, pre); 
                    pos += pre; 
                }

                // Write StreamTitle with fixed value
                // Записать StreamTitle с исправленным значением
                lstrcpynA(out + pos, "StreamTitle='", 4096 - pos);
                pos += lstrlenA("StreamTitle='");
                lstrcpynA(out + pos, fixed, 4096 - pos);
                pos += lstrlenA(fixed);
                out[pos++] = '\'';
                out[pos++] = ';';

                // Copy everything after the closing quote
                // Копировать всё после закрывающей кавычки
                const char* semi = strchr(e, ';');
                if (semi) 
                    lstrcpynA(out + pos, semi + 1, 4096 - pos);
                
                // Write fixed metadata back (in-place)
                // Записать исправленные метаданные обратно (на месте)
                int outLen = lstrlenA(out);
                if (outLen > metaLen) outLen = metaLen;
                CopyMemory(meta, out, outLen);
                
                // Zero out remaining space / Обнулить оставшееся пространство
                if (outLen < metaLen) 
                    ZeroMemory(meta + outLen, metaLen - outLen);
            }
        }
    }
}

/*******************************************************************************
 * looks_like_new_response_start
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Detects if data looks like the start of a new HTTP/ICY response. This is
 * used to reset context state when a new stream starts on the same socket.
 * 
 * Обнаруживает, выглядят ли данные как начало нового HTTP/ICY-ответа. Это
 * используется для сброса состояния контекста, когда новый поток начинается
 * на том же сокете.
 * 
 * PARAMETERS / ПАРАМЕТРЫ:
 * b - Buffer to check / Буфер для проверки
 * n - Buffer length / Длина буфера
 * 
 * RETURNS / ВОЗВРАЩАЕТ:
 * TRUE if data starts with "HTTP/" or "ICY", FALSE otherwise
 * TRUE если данные начинаются с "HTTP/" или "ICY", FALSE иначе
 ******************************************************************************/
static BOOL looks_like_new_response_start(const char* b, int n) {
    if (!b || n < 4) return FALSE;
    
    // Check for "HTTP/" / Проверить на "HTTP/"
    if (_strnicmp(b, "HTTP/", 5) == 0) return TRUE;
    
    // Check for "ICY" (SHOUTcast protocol) / Проверить на "ICY" (протокол SHOUTcast)
    if (_strnicmp(b, "ICY", 3) == 0) return TRUE;
    
    return FALSE;
}

/*******************************************************************************
 * MAIN DATA PROCESSING FUNCTION
 * ГЛАВНАЯ ФУНКЦИЯ ОБРАБОТКИ ДАННЫХ
 ******************************************************************************/

/*******************************************************************************
 * ProcessWinsockData
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Main state machine for processing streaming data. Handles:
 * 1. HTTP header parsing and icy-metaint extraction
 * 2. Audio/metadata chunk separation based on metaint
 * 3. Encoding fix application to headers and metadata
 * 
 * Основная машина состояний для обработки потоковых данных. Обрабатывает:
 * 1. Парсинг HTTP-заголовков и извлечение icy-metaint
 * 2. Разделение блоков аудио/метаданных на основе metaint
 * 3. Применение исправления кодировки к заголовкам и метаданным
 * 
 * PARAMETERS / ПАРАМЕТРЫ:
 * c - Socket context / Контекст сокета
 * p - Data buffer / Буфер данных
 * n - Data length / Длина данных
 * 
 * STATE MACHINE PHASES / ФАЗЫ МАШИНЫ СОСТОЯНИЙ:
 * 
 * PHASE 1: Header Processing (headerDone == 0)
 * - Accumulate header data across multiple recv calls
 * - Look for end of headers (\r\n\r\n or \n\n)
 * - When found: apply encoding fixes, extract icy-metaint, advance to Phase 2
 * 
 * ФАЗА 1: Обработка заголовка (headerDone == 0)
 * - Накапливать данные заголовка через несколько вызовов recv
 * - Искать конец заголовков (\r\n\r\n или \n\n)
 * - Когда найдено: применить исправления кодировки, извлечь icy-metaint, перейти к Фазе 2
 * 
 * PHASE 2: Stream Processing (headerDone == 1, metaInt > 0)
 * - Track position in audio/metadata stream
 * - State: reading audio > reading metadata length byte > reading metadata > back to audio
 * - Apply encoding fix to metadata chunks as they're received
 * 
 * ФАЗА 2: Обработка потока (headerDone == 1, metaInt > 0)
 * - Отслеживать позицию в потоке аудио/метаданных
 * - Состояние: чтение аудио > чтение байта длины метаданных > чтение метаданных > обратно к аудио
 * - Применять исправление кодировки к блокам метаданных по мере их получения
 * 
 * STREAM FORMAT / ФОРМАТ ПОТОКА:
 * [metaInt bytes audio][1 byte len][len*16 bytes metadata][metaInt bytes audio][...]
 * 
 * NOTES / ПРИМЕЧАНИЯ:
 * This function modifies data in-place, which is safe because:
 * 1. Fixed text is never longer than original (we truncate if needed)
 * 2. We're processing data after recv, before it reaches the plugin
 * 
 * Эта функция изменяет данные на месте, что безопасно, потому что:
 * 1. Исправленный текст никогда не длиннее оригинала (мы обрезаем при необходимости)
 * 2. Мы обрабатываем данные после recv, до того как они попадут в плагин
 ******************************************************************************/
static void ProcessWinsockData(SockCtx* c, unsigned char* p, DWORD n) {
    if (!c || !p || n == 0) return;

    // Reset context if we see a new HTTP response starting
    // Сбросить контекст, если видим начало нового HTTP-ответа
    if (looks_like_new_response_start((const char*)p, (int)n)) {
        c->headerDone = 0; 
        c->hdrLen = 0; 
        c->metaInt = 0;
    }

    DWORD pos = 0;

    // ===== PHASE 1: Process HTTP Header =====
    // ===== ФАЗА 1: Обработка HTTP-заголовка =====
    if (!c->headerDone) {
        // Append new data to header buffer
        // Добавить новые данные к буферу заголовка
        int cap = HDR_SCAN_MAX - c->hdrLen;
        int app = (int)((n < (DWORD)cap) ? n : (DWORD)cap);
        if (app > 0) {
            CopyMemory(c->hdrBuf + c->hdrLen, p, app);
            c->hdrLen += app;
        }

        // Check if we have complete headers now
        // Проверить, есть ли у нас полные заголовки теперь
        int eoh = FindHeaderEnd(c->hdrBuf, c->hdrLen);
        if (eoh >= 0) {
            // Headers are complete! / Заголовки завершены!
            c->headerDone = 1;
            
            // Calculate how much of current buffer is header
            // Вычислить, сколько текущего буфера является заголовком
            int prevLen = c->hdrLen - app;
            int headerInThis = eoh - prevLen;
            
            // Fix encoding in the header portion of this buffer
            // Исправить кодировку в части заголовка этого буфера
            if (headerInThis > 0) {
                int scan = (headerInThis < HDR_SCAN_MAX) ? headerInThis : HDR_SCAN_MAX;
                FixHeaderFieldsInChunk((char*)p, scan);
            }

            // Parse icy-metaint from accumulated headers
            // Парсить icy-metaint из накопленных заголовков
            char* hb = c->hdrBuf;
            int i = 0;
            while(i < c->hdrLen) {
                // Find line boundaries / Найти границы строк
                int s = i;
                while(i < c->hdrLen && hb[i] != '\r' && hb[i] != '\n') ++i;
                int e = i;
                
                // Check for icy-metaint header / Проверить заголовок icy-metaint
                const char* k = "icy-metaint:";
                int klen = lstrlenA(k);
                if (e - s > klen && _strnicmp(hb + s, k, klen) == 0) {
                    // Found it! Parse the value / Нашли! Парсить значение
                    DWORD v = ParseDec32(hb + s + klen);
                    if (v > 0 && v < 2*1024*1024) {  // Sanity check (2MB max) / Проверка разумности (максимум 2МБ)
                        c->metaInt = v;
                        c->leftAudio = v;
                        c->needLenByte = 1;
                        c->metaLeft = 0;
                    }
                }
                
                // Skip line endings / Пропустить окончания строк
                while(i < c->hdrLen && (hb[i] == '\r' || hb[i] == '\n')) ++i;
            }

            // Advance position past header in current buffer
            // Продвинуть позицию за заголовок в текущем буфере
            pos = (DWORD)(eoh - prevLen);
            if (pos > n) pos = n;  // Safety clamp / Ограничение безопасности
        } else {
            // Headers not complete yet - fix what we have and wait for more
            // Заголовки ещё не завершены - исправить что есть и ждать больше
            FixHeaderFieldsInChunk((char*)p, app);
            return;
        }
    }

    // If no metadata interval, nothing more to do
    // Если нет интервала метаданных, больше нечего делать
    if (c->metaInt == 0) return;

    // ===== PHASE 2: Process Stream Data (Audio + Metadata) =====
    // ===== ФАЗА 2: Обработка потоковых данных (Аудио + Метаданные) =====
    while (pos < n) {
        if (c->leftAudio > 0) {
            // STATE: Reading audio data / СОСТОЯНИЕ: Чтение аудиоданных
            DWORD avail = n - pos;
            DWORD take = (c->leftAudio < avail) ? c->leftAudio : avail;
            pos += take;
            c->leftAudio -= take;
        } else {
            if (pos >= n) break;
            
            // STATE: Reading metadata length byte / СОСТОЯНИЕ: Чтение байта длины метаданных
            if (c->needLenByte) {
                BYTE L = p[pos++];
                c->metaLeft = (DWORD)L * 16;  // Length is multiplied by 16 / Длина умножается на 16
                c->needLenByte = 0;
                
                if (c->metaLeft == 0) {
                    // No metadata this time - go back to reading audio
                    // Нет метаданных в этот раз - вернуться к чтению аудио
                    c->leftAudio = c->metaInt;
                    c->needLenByte = 1;
                    continue;
                }
            }

            // STATE: Reading metadata / СОСТОЯНИЕ: Чтение метаданных
            if (c->metaLeft > 0) {
                DWORD avail = n - pos;
                if (avail < c->metaLeft) {
                    // Partial metadata chunk - can't process yet
                    // Частичный блок метаданных - пока не можем обработать
                    c->metaLeft -= avail;
                    pos = n;
                } else {
                    // Complete metadata chunk available - process it!
                    // Полный блок метаданных доступен - обработать его!
                    FixIcyMetadataInPlace(p + pos, (int)c->metaLeft);
                    pos += c->metaLeft;
                    
                    // Reset state for next audio chunk
                    // Сбросить состояние для следующего блока аудио
                    c->metaLeft = 0;
                    c->leftAudio = c->metaInt;
                    c->needLenByte = 1;
                }
            }
        }
    }
}

/*******************************************************************************
 * WINSOCK API HOOK IMPLEMENTATIONS
 * РЕАЛИЗАЦИИ ПЕРЕХВАТЧИКОВ API WINSOCK
 ******************************************************************************/

/*******************************************************************************
 * Hook_send
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Intercepts send() calls to track new socket connections. When data is sent,
 * we add the socket to our tracking pool if not already tracked.
 * 
 * Перехватывает вызовы send() для отслеживания новых соединений сокетов. Когда
 * данные отправляются, мы добавляем сокет в наш пул отслеживания, если ещё не отслеживается.
 ******************************************************************************/
static int WINAPI Hook_send(SOCKET s, const char* buf, int len, int flags) {
    SockCtx* c;
    Lock();
    
    // Always track new sockets to catch header requests
    // Всегда отслеживать новые сокеты для перехвата запросов заголовков
    c = Sock_Find(s);
    if (!c) { 
        Unlock(); 
        Sock_Add(s);  // Add if not found / Добавить, если не найден
    } else {
        Unlock();
    }
    
    // Call real send / Вызвать реальный send
    return Real_send ? Real_send(s, buf, len, flags) : -1;
}

/*******************************************************************************
 * Hook_recv
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Intercepts recv() calls to process received streaming data. This is where
 * most of the magic happens - we parse headers, separate audio from metadata,
 * and fix encoding issues.
 * 
 * Перехватывает вызовы recv() для обработки получаемых потоковых данных. Здесь
 * происходит большая часть магии - мы парсим заголовки, разделяем аудио от
 * метаданных и исправляем проблемы кодировки.
 ******************************************************************************/
static int WINAPI Hook_recv(SOCKET s, char* buf, int len, int flags) {
    // Call real recv first / Сначала вызвать реальный recv
    int r = Real_recv ? Real_recv(s, buf, len, flags) : -1;
    if (r <= 0) return r;  // Error or connection closed / Ошибка или соединение закрыто
    
    // Find or create context for this socket / Найти или создать контекст для этого сокета
    SockCtx* c;
    Lock();
    c = Sock_Find(s);
    if (!c) { 
        Unlock(); 
        c = Sock_Add(s);  // [FIX] Always track / [ИСПРАВЛЕНИЕ] Всегда отслеживать
    } else { 
        Unlock(); 
    }

    if (c) {
        // Process data through our state machine
        // Обработать данные через нашу машину состояний
        ProcessWinsockData(c, (unsigned char*)buf, (DWORD)r);
    } else {
        // Fallback if context creation failed: simple header scan
        // Резервный вариант, если создание контекста не удалось: простое сканирование заголовка
        if (r > 0 && r < HDR_SCAN_MAX) 
            FixHeaderFieldsInChunk(buf, r);
    }
    
    return r;
}

/*******************************************************************************
 * Hook_WSARecv
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Intercepts WSARecv() calls (async/scatter-gather receive). Works similarly
 * to Hook_recv but handles multiple buffers.
 * 
 * Перехватывает вызовы WSARecv() (асинхронное/scatter-gather получение). Работает
 * аналогично Hook_recv, но обрабатывает несколько буферов.
 ******************************************************************************/
static int WINAPI Hook_WSARecv(SOCKET s, LPWSABUF bufs, DWORD cnt, LPDWORD pRecvd, LPDWORD pFlags,
                               LPWSAOVERLAPPED ovl, LPWSAOVERLAPPED_COMPLETION_ROUTINE cr) 
{
    // Call real WSARecv first / Сначала вызвать реальный WSARecv
    int r = Real_WSARecv ? Real_WSARecv(s, bufs, cnt, pRecvd, pFlags, ovl, cr) : -1;
    
    // Process if successful and data was received
    // Обработать, если успешно и данные получены
    if (r == 0 && pRecvd && *pRecvd > 0 && bufs && cnt > 0) {
        // [FIX] Full processing for WSARecv too
        // [ИСПРАВЛЕНИЕ] Полная обработка для WSARecv тоже
        SockCtx* c;
        Lock();
        c = Sock_Find(s);
        if (!c) { 
            Unlock(); 
            c = Sock_Add(s); 
        } else { 
            Unlock(); 
        }

        // Process each buffer / Обработать каждый буфер
        DWORD remain = *pRecvd;
        for (DWORD i = 0; i < cnt && remain > 0; ++i) {
            DWORD take = (bufs[i].len < remain) ? bufs[i].len : remain;
            if (take > 0 && bufs[i].buf) {
                if (c) {
                    // Full state machine processing / Полная обработка машиной состояний
                    ProcessWinsockData(c, (unsigned char*)bufs[i].buf, take);
                } else {
                    // Fallback: simple header scan / Резервный вариант: простое сканирование заголовка
                    int scan = (take < HDR_SCAN_MAX) ? (int)take : HDR_SCAN_MAX;
                    FixHeaderFieldsInChunk(bufs[i].buf, scan);
                }
            }
            remain -= take;
        }
    }
    return r;
}

/*******************************************************************************
 * Hook_WSASend
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Intercepts WSASend() calls (async/scatter-gather send). Currently just
 * passes through to real implementation - no processing needed for send.
 * 
 * Перехватывает вызовы WSASend() (асинхронная/scatter-gather отправка). В данный
 * момент просто передаёт реальной реализации - обработка для отправки не нужна.
 ******************************************************************************/
static int WINAPI Hook_WSASend(SOCKET s, LPWSABUF bufs, DWORD cnt, LPDWORD pSent, DWORD flags,
                               LPWSAOVERLAPPED ovl, LPWSAOVERLAPPED_COMPLETION_ROUTINE cr) {
    return Real_WSASend ? Real_WSASend(s, bufs, cnt, pSent, flags, ovl, cr) : -1;
}

/*******************************************************************************
 * Hook_closesocket
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Intercepts closesocket() calls to clean up socket contexts when connections
 * are closed. Removes socket from tracking pool.
 * 
 * Перехватывает вызовы closesocket() для очистки контекстов сокетов при
 * закрытии соединений. Удаляет сокет из пула отслеживания.
 ******************************************************************************/
static int WINAPI Hook_closesocket(SOCKET s) {
    // Remove from tracking pool / Удалить из пула отслеживания
    Sock_Del(s);
    
    // Call real closesocket / Вызвать реальный closesocket
    return Real_closesocket ? Real_closesocket(s) : 0;
}

/*******************************************************************************
 * PUBLIC API AND HOOK INSTALLATION
 * ПУБЛИЧНЫЙ API И УСТАНОВКА ХУКОВ
 ******************************************************************************/

// External IAT patching functions (defined elsewhere)
// Внешние функции патчинга IAT (определены в другом месте)
extern "C" BOOL IAT_PatchByName(HMODULE module, const char* importedDll, const char* funcName, void* newFn, void** pOrigFn);
extern "C" BOOL IAT_PatchByAddr(HMODULE module, const char* importedDll, void* origFn, void* newFn, void** pOrigFn);
extern "C" BOOL DECRYPT_IsWin9x(void);

/*******************************************************************************
 * InstallWinsockHooks_9x_Targeted
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Installs Winsock hooks for Windows 9x systems. Win9x uses WSOCK32.dll and
 * streaming happens in in_mp3.dll plugin, so we only patch that specific module.
 * 
 * Устанавливает хуки Winsock для систем Windows 9x. Win9x использует WSOCK32.dll,
 * и потоковая передача происходит в плагине in_mp3.dll, поэтому мы патчим только
 * этот конкретный модуль.
 * 
 * WIN9X SPECIFICS / ОСОБЕННОСТИ WIN9X:
 * - Uses WSOCK32.dll (not WS2_32.dll)
 * - Winamp 2.95 uses in_mp3.dll for streaming
 * - Need to wait briefly for in_mp3.dll to load
 * 
 * - Использует WSOCK32.dll (не WS2_32.dll)
 * - Winamp 2.95 использует in_mp3.dll для потоковой передачи
 * - Нужно немного подождать загрузки in_mp3.dll
 ******************************************************************************/
static void InstallWinsockHooks_9x_Targeted(void) {
    // Get WSOCK32.dll module / Получить модуль WSOCK32.dll
    HMODULE hWS1 = GetModuleHandleA("WSOCK32.dll");
    if (!hWS1) return;

    // Get original function addresses from WSOCK32.dll
    // Use GetProcAddress to avoid link dependencies
    // Получить адреса оригинальных функций из WSOCK32.dll
    // Использовать GetProcAddress для избежания зависимостей компоновки
    if (!Real_send)        Real_send        = (PFN_send)       GetProcAddress(hWS1, "send");
    if (!Real_recv)        Real_recv        = (PFN_recv)       GetProcAddress(hWS1, "recv");
    if (!Real_closesocket) Real_closesocket = (PFN_closesocket)GetProcAddress(hWS1, "closesocket");

    // Only patch in_mp3.dll for Winamp 2.95
    // Патчить только in_mp3.dll для Winamp 2.95
    HMODULE hInMp3 = GetModuleHandleA("in_mp3.dll");
    if (hInMp3) {
        // Try patching by name first (preferred)
        // Сначала попытаться пропатчить по имени (предпочтительно)
        BOOL a = IAT_PatchByName(hInMp3, "WSOCK32.dll", "send",        (void*)Hook_send,        (void**)&Real_send);
        BOOL b = IAT_PatchByName(hInMp3, "WSOCK32.dll", "recv",        (void*)Hook_recv,        (void**)&Real_recv);
        BOOL c = IAT_PatchByName(hInMp3, "WSOCK32.dll", "closesocket", (void*)Hook_closesocket, (void**)&Real_closesocket);

        // Fallback to patching by address if name patching failed
        // Резервный вариант патчинга по адресу, если патчинг по имени не удался
        if (!a && Real_send)        IAT_PatchByAddr(hInMp3, "WSOCK32.dll", (void*)Real_send,        (void*)Hook_send,        (void**)&Real_send);
        if (!b && Real_recv)        IAT_PatchByAddr(hInMp3, "WSOCK32.dll", (void*)Real_recv,        (void*)Hook_recv,        (void**)&Real_recv);
        if (!c && Real_closesocket) IAT_PatchByAddr(hInMp3, "WSOCK32.dll", (void*)Real_closesocket, (void*)Hook_closesocket, (void**)&Real_closesocket);
    }
}

/*******************************************************************************
 * InstallWinsockHooks_NT_All
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Installs Winsock hooks for Windows NT/2000/XP/Vista/7+ systems. These systems
 * use WS2_32.dll, and WSOCK32.dll is just a compatibility wrapper that forwards
 * to WS2_32.dll. We patch WSOCK32's IAT to intercept all plugins at once.
 * 
 * Устанавливает хуки Winsock для систем Windows NT/2000/XP/Vista/7+. Эти системы
 * используют WS2_32.dll, а WSOCK32.dll - это просто обёртка совместимости, которая
 * перенаправляет в WS2_32.dll. Мы патчим IAT WSOCK32 для перехвата всех плагинов сразу.
 * 
 * NT+ SPECIFICS / ОСОБЕННОСТИ NT+:
 * - Uses WS2_32.dll (main Winsock implementation)
 * - WSOCK32.dll forwards to WS2_32.dll
 * - By patching WSOCK32, we catch all plugins using either DLL
 * - Supports both recv/send and WSARecv/WSASend
 * 
 * - Использует WS2_32.dll (основная реализация Winsock)
 * - WSOCK32.dll перенаправляет в WS2_32.dll
 * - Патчем WSOCK32, мы ловим все плагины, использующие любую DLL
 * - Поддерживает как recv/send, так и WSARecv/WSASend
 ******************************************************************************/
static void InstallWinsockHooks_NT_All(void) {
    // Get both DLL modules / Получить модули обеих DLL
    HMODULE hWS2 = GetModuleHandleA("WS2_32.dll");
    HMODULE hWS1 = GetModuleHandleA("WSOCK32.dll");
    if (!hWS2 || !hWS1) return;

    // Get original function addresses from WS2_32.dll
    // Получить адреса оригинальных функций из WS2_32.dll
    if (!Real_send)        Real_send        = (PFN_send)       GetProcAddress(hWS2, "send");
    if (!Real_WSASend)     Real_WSASend     = (PFN_WSASend)    GetProcAddress(hWS2, "WSASend");
    if (!Real_recv)        Real_recv        = (PFN_recv)       GetProcAddress(hWS2, "recv");
    if (!Real_WSARecv)     Real_WSARecv     = (PFN_WSARecv)    GetProcAddress(hWS2, "WSARecv");
    if (!Real_closesocket) Real_closesocket = (PFN_closesocket)GetProcAddress(hWS2, "closesocket");

    if (!Real_send || !Real_recv || !Real_closesocket) return;

    // Patch WSOCK32's IAT to point to our hooks instead of forwarding to WS2_32
    // This catches all plugins using WSOCK32 (which forwards to WS2_32)
    // Пропатчить IAT WSOCK32 для указания на наши хуки вместо перенаправления в WS2_32
    // Это ловит все плагины, использующие WSOCK32 (который перенаправляет в WS2_32)
    IAT_PatchByName(hWS1, "WS2_32.dll", "send",        (void*)Hook_send,        (void**)&Real_send);
    IAT_PatchByName(hWS1, "WS2_32.dll", "recv",        (void*)Hook_recv,        (void**)&Real_recv);
    IAT_PatchByName(hWS1, "WS2_32.dll", "WSASend",     (void*)Hook_WSASend,     (void**)&Real_WSASend);
    IAT_PatchByName(hWS1, "WS2_32.dll", "WSARecv",     (void*)Hook_WSARecv,     (void**)&Real_WSARecv);
    IAT_PatchByName(hWS1, "WS2_32.dll", "closesocket", (void*)Hook_closesocket, (void**)&Real_closesocket);
}

/*******************************************************************************
 * EXPORTED PUBLIC API FUNCTIONS
 * ЭКСПОРТИРУЕМЫЕ ФУНКЦИИ ПУБЛИЧНОГО API
 ******************************************************************************/

/*******************************************************************************
 * MP3_StreamFix_Init
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Initializes the streaming metadata fix module. This is the main entry point
 * that should be called during Winamp initialization.
 * 
 * Инициализирует модуль исправления метаданных потоковой передачи. Это главная
 * точка входа, которая должна вызываться во время инициализации Winamp.
 * 
 * INITIALIZATION PROCESS / ПРОЦЕСС ИНИЦИАЛИЗАЦИИ:
 * 1. Initialize critical section
 * 2. Detect Windows version (9x vs NT+)
 * 3. For Win9x: Wait briefly for in_mp3.dll to load, then patch it
 * 4. For NT+: Patch WSOCK32.dll immediately (catches all plugins)
 * 
 * 1. Инициализировать критическую секцию
 * 2. Определить версию Windows (9x против NT+)
 * 3. Для Win9x: немного подождать загрузки in_mp3.dll, затем пропатчить её
 * 4. Для NT+: немедленно пропатчить WSOCK32.dll (ловит все плагины)
 * 
 * NOTES / ПРИМЕЧАНИЯ:
 * The wait loop for Win9x ensures in_mp3.dll has time to load before we
 * try to patch it. NT+ systems don't need this as we patch the wrapper DLL.
 * 
 * Цикл ожидания для Win9x гарантирует, что у in_mp3.dll есть время загрузиться
 * до того, как мы попытаемся её пропатчить. Системы NT+ не нуждаются в этом,
 * так как мы патчим обёрточную DLL.
 ******************************************************************************/
extern "C" void MP3_StreamFix_Init(void) {
    // Initialize critical section for thread-safe operations
    // Инициализировать критическую секцию для потокобезопасных операций
    CS_Init();

    if (DECRYPT_IsWin9x()) {
        // Windows 9x path / Путь для Windows 9x
        
        // Wait briefly for in_mp3.dll to load on 9x systems (up to 2 seconds)
        // Немного подождать загрузки in_mp3.dll на системах 9x (до 2 секунд)
        for (int i = 0; i < 20; i++) {
            if (GetModuleHandleA("in_mp3.dll")) break;
            Sleep(100);
        }
        
        // Install hooks for Win9x / Установить хуки для Win9x
        InstallWinsockHooks_9x_Targeted();
    } else {
        // Windows NT/2000/XP/Vista/7+ path / Путь для Windows NT/2000/XP/Vista/7+
        
        // Install hooks for NT+ systems (immediate, no wait needed)
        // Установить хуки для систем NT+ (немедленно, ожидание не требуется)
        InstallWinsockHooks_NT_All();
    }
}

/*******************************************************************************
 * MP3_StreamFix_Quit
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Cleans up the streaming metadata fix module. Should be called during Winamp
 * shutdown.
 * 
 * Очищает модуль исправления метаданных потоковой передачи. Должна вызываться
 * при завершении Winamp.
 * 
 * CLEANUP PROCESS / ПРОЦЕСС ОЧИСТКИ:
 * 1. Delete critical section
 * 
 * 1. Удалить критическую секцию
 * 
 * NOTES / ПРИМЕЧАНИЯ:
 * We don't unpatch the IAT hooks because:
 * 1. It's risky to unpatch while streams might be active
 * 2. Winamp is shutting down anyway
 * 3. The hooks are designed to be safe to leave in place
 * 
 * Мы не снимаем патчи хуков IAT, потому что:
 * 1. Рискованно снимать патчи, пока потоки могут быть активны
 * 2. Winamp всё равно завершает работу
 * 3. Хуки разработаны так, чтобы быть безопасными для оставления на месте
 ******************************************************************************/
extern "C" void MP3_StreamFix_Quit(void) {
    // Clean up critical section / Очистить критическую секцию
    CS_Done();
}