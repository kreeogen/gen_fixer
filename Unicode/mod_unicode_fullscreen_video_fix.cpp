/*******************************************************************************
 * mod_videofontfix.cpp
 * 
 * WINAMP VIDEO WINDOW FONT FIX MODULE (You don't need that module if you are using English Winamp version)
 * Модуль исправления шрифтов для окна видео Winamp
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * This module fixes font rendering issues in Winamp's video input plugins
 * (particularly in_dshow.dll and in_nsv.dll) by intercepting font creation
 * API calls and forcing a specific font face and character set. This solves
 * problems with incorrect character display in non-English locales.
 * 
 * Этот модуль исправляет проблемы отображения шрифтов в видео-плагинах Winamp
 * (особенно в in_dshow.dll и in_nsv.dll), перехватывая вызовы API создания
 * шрифтов и принудительно устанавливая определённое начертание шрифта и набор
 * символов. Это решает проблемы с неправильным отображением символов в
 * нелатинских локалях.
 * 
 * HOW IT WORKS / КАК ЭТО РАБОТАЕТ:
 * 1. Patches the Import Address Table (IAT) of target video plugin DLLs
 * 2. Redirects CreateFontA and CreateFontIndirectA calls to custom functions
 * 3. Custom functions modify LOGFONT structure to use Tahoma font with
 *    Cyrillic charset (204) before creating the actual font
 * 4. Special handling for symbol fonts (Marlett, Wingdings, etc.) - these
 *    are left unchanged to preserve UI elements like checkboxes and icons
 * 5. Uses a worker thread to patch plugins as they load (asynchronous)
 * 
 * 1. Патчит таблицу адресов импорта (IAT) целевых DLL видео-плагинов
 * 2. Перенаправляет вызовы CreateFontA и CreateFontIndirectA на пользовательские функции
 * 3. Пользовательские функции изменяют структуру LOGFONT для использования
 *    шрифта Tahoma с кириллической кодировкой (204) перед созданием шрифта
 * 4. Специальная обработка символьных шрифтов (Marlett, Wingdings и т.д.) -
 *    они остаются неизменными для сохранения элементов интерфейса, таких как
 *    флажки и значки
 * 5. Использует рабочий поток для патчинга плагинов по мере их загрузки (асинхронно)
 * 
 * TECHNICAL DETAILS / ТЕХНИЧЕСКИЕ ДЕТАЛИ:
 * - Uses IAT (Import Address Table) patching technique
 * - Non-invasive: only modifies function pointers, not code
 * - Thread-safe: uses atomic operations for thread control
 * - Preserves original function pointers for calling real implementations
 * - Targets specific problematic plugins (in_dshow.dll, in_nsv.dll)
 * 
 * - Использует технику патчинга IAT (таблицы адресов импорта)
 * - Неинвазивно: изменяет только указатели функций, а не код
 * - Потокобезопасно: использует атомарные операции для управления потоком
 * - Сохраняет оригинальные указатели функций для вызова реальных реализаций
 * - Нацелено на конкретные проблемные плагины (in_dshow.dll, in_nsv.dll)
 * 
 * CONFIGURATION / КОНФИГУРАЦИЯ:
 * Character set: 204 (Cyrillic) - can be changed for other locales
 * Font face: Tahoma - widely available, good Unicode support
 * 
 * Набор символов: 204 (кириллица) - может быть изменён для других локалей
 * Начертание шрифта: Tahoma - широко доступен, хорошая поддержка Unicode
 * 
 ******************************************************************************/

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <string.h> 
#include "..\Decrypt Engine\unicode_decrypt_engine.h"

#pragma comment(lib, "kernel32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "user32.lib") 

// Array size macro for compile-time array length calculation
// Макрос размера массива для вычисления длины массива во время компиляции
#ifndef ARRAYSIZE
#define ARRAYSIZE(a) (sizeof(a)/sizeof((a)[0]))
#endif

/*******************************************************************************
 * CONFIGURATION CONSTANTS
 * КОНСТАНТЫ КОНФИГУРАЦИИ
 ******************************************************************************/

// Character set to force for all non-symbol fonts
// 204 = RUSSIAN_CHARSET (Cyrillic), can be changed for other locales:
// - 0   = ANSI_CHARSET (Western European)
// - 128 = SHIFTJIS_CHARSET (Japanese)
// - 129 = HANGUL_CHARSET (Korean)
// - 134 = GB2312_CHARSET (Simplified Chinese)
// - 136 = CHINESEBIG5_CHARSET (Traditional Chinese)
// - 161 = GREEK_CHARSET
// - 162 = TURKISH_CHARSET
// - 177 = HEBREW_CHARSET
// - 178 = ARABIC_CHARSET
// - 204 = RUSSIAN_CHARSET (Cyrillic)
// - 222 = THAI_CHARSET
// - 238 = EASTEUROPE_CHARSET (Central European)
//
// Набор символов для принудительной установки для всех не-символьных шрифтов
// 204 = RUSSIAN_CHARSET (кириллица), может быть изменён для других локалей
static const BYTE kForceCharset = 204;

// Font face name to force for all non-symbol fonts
// Tahoma is chosen for:
// - Wide availability on all Windows versions
// - Good Unicode coverage
// - Clean, readable appearance
// - Similar metrics to many default fonts
//
// Имя начертания шрифта для принудительной установки для всех не-символьных шрифтов
// Tahoma выбран из-за:
// - Широкой доступности на всех версиях Windows
// - Хорошего покрытия Unicode
// - Чистого, читаемого вида
// - Схожих метрик со многими шрифтами по умолчанию
static const char kForceFace[]  = "Tahoma";

/*******************************************************************************
 * REAL API FUNCTION POINTERS
 * УКАЗАТЕЛИ НА РЕАЛЬНЫЕ ФУНКЦИИ API
 ******************************************************************************/

// Function pointer types for the GDI font creation functions we intercept
// Типы указателей на функции для функций создания шрифтов GDI, которые мы перехватываем

// CreateFontIndirectA: Creates a font from a LOGFONT structure
// CreateFontIndirectA: создаёт шрифт из структуры LOGFONT
typedef HFONT (WINAPI *PFN_CreateFontIndirectA)(const LOGFONTA*);

// CreateFontA: Creates a font from individual parameters
// CreateFontA: создаёт шрифт из отдельных параметров
typedef HFONT (WINAPI *PFN_CreateFontA)(int, int, int, int, int, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, LPCSTR);

// Pointers to the real (original) GDI functions
// These are obtained before patching and used to call the actual implementation
// Указатели на реальные (оригинальные) функции GDI
// Они получаются до патчинга и используются для вызова фактической реализации
static PFN_CreateFontIndirectA s_real_CreateFontIndirectA = NULL;
static PFN_CreateFontA         s_real_CreateFontA         = NULL;

/*******************************************************************************
 * MODULE STATE VARIABLES
 * ПЕРЕМЕННЫЕ СОСТОЯНИЯ МОДУЛЯ
 ******************************************************************************/

// Handle to the worker thread that performs IAT patching
// Дескриптор рабочего потока, который выполняет патчинг IAT
static HANDLE s_hThread = NULL;

// Atomic flag to signal worker thread to stop
// Uses InterlockedExchange for thread-safe access
// 0 = continue running, 1 = stop requested
// Атомарный флаг для сигнала рабочему потоку остановиться
// Использует InterlockedExchange для потокобезопасного доступа
// 0 = продолжать работу, 1 = запрошена остановка
static volatile LONG s_stopThread = 0;

/*******************************************************************************
 * HELPER FUNCTIONS
 * ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ
 ******************************************************************************/

/*******************************************************************************
 * IsSymbolFont
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Determines if a LOGFONT structure describes a symbol font (non-alphabetic
 * fonts used for UI elements, icons, checkboxes, etc.). Symbol fonts should
 * not be modified as changing their charset or face would break UI rendering.
 * 
 * Определяет, описывает ли структура LOGFONT символьный шрифт (неалфавитные
 * шрифты, используемые для элементов интерфейса, значков, флажков и т.д.).
 * Символьные шрифты не должны изменяться, так как изменение их набора символов
 * или начертания нарушит отрисовку интерфейса.
 * 
 * PARAMETERS / ПАРАМЕТРЫ:
 * lf - Pointer to LOGFONT structure to check
 *      Указатель на структуру LOGFONT для проверки
 * 
 * RETURNS / ВОЗВРАЩАЕТ:
 * BOOL - TRUE if this is a symbol font (should not be modified)
 *        FALSE if this is a regular font (can be modified)
 *        TRUE если это символьный шрифт (не должен изменяться)
 *        FALSE если это обычный шрифт (может быть изменён)
 * 
 * DETECTION CRITERIA / КРИТЕРИИ ОБНАРУЖЕНИЯ:
 * 1. lfCharSet == SYMBOL_CHARSET (explicit symbol charset)
 * 2. Face name matches known symbol fonts:
 *    - Marlett (UI elements: checkboxes, radio buttons, arrows)
 *    - Webdings (web icons and symbols)
 *    - Wingdings, Wingdings 2, Wingdings 3 (various symbols)
 *    - Symbol (mathematical and Greek symbols)
 * 
 * 1. lfCharSet == SYMBOL_CHARSET (явный символьный набор)
 * 2. Имя начертания соответствует известным символьным шрифтам:
 *    - Marlett (элементы интерфейса: флажки, радиокнопки, стрелки)
 *    - Webdings (веб-иконки и символы)
 *    - Wingdings, Wingdings 2, Wingdings 3 (различные символы)
 *    - Symbol (математические и греческие символы)
 ******************************************************************************/
static BOOL IsSymbolFont(const LOGFONTA* lf)
{
    // Validate pointer / Проверить указатель
    if (!lf) return FALSE;

    // Check if charset is explicitly marked as symbol font
    // Проверить, явно ли набор символов отмечен как символьный шрифт
    if (lf->lfCharSet == SYMBOL_CHARSET) return TRUE;

    // Check face name against known symbol fonts
    // Проверить имя начертания на соответствие известным символьным шрифтам
    const char* face = lf->lfFaceName;
    if (!face || !*face) return FALSE;  // No face name / Нет имени начертания

    // UI elements font (checkboxes, scrollbars, etc.)
    // Шрифт элементов интерфейса (флажки, полосы прокрутки и т.д.)
    if (lstrcmpiA(face, "Marlett") == 0) return TRUE;
    
    // Web symbols and icons / Веб-символы и иконки
    if (lstrcmpiA(face, "Webdings") == 0) return TRUE;
    
    // Various symbol sets / Различные наборы символов
    if (lstrcmpiA(face, "Wingdings") == 0) return TRUE;
    if (lstrcmpiA(face, "Wingdings 2") == 0) return TRUE;
    if (lstrcmpiA(face, "Wingdings 3") == 0) return TRUE;
    
    // Mathematical and Greek symbols / Математические и греческие символы
    if (lstrcmpiA(face, "Symbol") == 0) return TRUE;

    return FALSE;  // Not a symbol font / Не символьный шрифт
}

/*******************************************************************************
 * HOOK FUNCTIONS (INTERCEPT FUNCTIONS)
 * ФУНКЦИИ-ПЕРЕХВАТЧИКИ
 ******************************************************************************/

/*******************************************************************************
 * My_CreateFontIndirectA
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Replacement function for CreateFontIndirectA that forces specific font
 * face and charset for non-symbol fonts. This function is installed in the
 * IAT (Import Address Table) of target plugins.
 * 
 * Функция-замена для CreateFontIndirectA, которая принудительно устанавливает
 * определённое начертание шрифта и набор символов для не-символьных шрифтов.
 * Эта функция устанавливается в IAT (таблицу адресов импорта) целевых плагинов.
 * 
 * PARAMETERS / ПАРАМЕТРЫ:
 * plf - Pointer to LOGFONT structure describing the desired font
 *       Указатель на структуру LOGFONT, описывающую желаемый шрифт
 * 
 * RETURNS / ВОЗВРАЩАЕТ:
 * HFONT - Handle to the created font, or NULL on failure
 *         Дескриптор созданного шрифта или NULL при ошибке
 * 
 * ALGORITHM / АЛГОРИТМ:
 * 1. Copy input LOGFONT structure (or create empty one if NULL)
 * 2. Check if font is a symbol font
 * 3. If NOT a symbol font:
 *    - Replace lfCharSet with kForceCharset (204 = Cyrillic)
 *    - Replace lfFaceName with kForceFace ("Tahoma")
 * 4. Call real CreateFontIndirectA with modified structure
 * 
 * 1. Копировать входную структуру LOGFONT (или создать пустую, если NULL)
 * 2. Проверить, является ли шрифт символьным
 * 3. Если НЕ символьный шрифт:
 *    - Заменить lfCharSet на kForceCharset (204 = кириллица)
 *    - Заменить lfFaceName на kForceFace ("Tahoma")
 * 4. Вызвать реальную CreateFontIndirectA с изменённой структурой
 * 
 * NOTES / ПРИМЕЧАНИЯ:
 * Symbol fonts are left unchanged to preserve UI elements. All other font
 * properties (size, weight, italic, etc.) are preserved from the original.
 * 
 * Символьные шрифты остаются неизменными для сохранения элементов интерфейса.
 * Все остальные свойства шрифта (размер, вес, курсив и т.д.) сохраняются из оригинала.
 ******************************************************************************/
static HFONT WINAPI My_CreateFontIndirectA(const LOGFONTA* plf)
{
    LOGFONTA lf;
    
    // Copy input LOGFONT or create empty structure
    // Копировать входную LOGFONT или создать пустую структуру
    if (plf) lf = *plf;
    else ZeroMemory(&lf, sizeof(lf));

    // Modify font properties only for non-symbol fonts
    // Изменять свойства шрифта только для не-символьных шрифтов
    if (!IsSymbolFont(&lf)) {
        // Force specific charset (Cyrillic in this case)
        // Принудительно установить определённый набор символов (кириллицу в данном случае)
        lf.lfCharSet = kForceCharset;
        
        // Force specific font face (Tahoma)
        // Принудительно установить определённое начертание шрифта (Tahoma)
        lstrcpynA(lf.lfFaceName, kForceFace, LF_FACESIZE);
    }

    // Call real CreateFontIndirectA function
    // Вызвать реальную функцию CreateFontIndirectA
    if (s_real_CreateFontIndirectA)
        return s_real_CreateFontIndirectA(&lf);
    
    // Fallback if real function pointer not available (should never happen)
    // Резервный вариант, если указатель на реальную функцию недоступен (не должно произойти)
    return CreateFontIndirectA(&lf);
}

/*******************************************************************************
 * My_CreateFontA
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Replacement function for CreateFontA that converts individual font parameters
 * to a LOGFONT structure and delegates to My_CreateFontIndirectA for processing.
 * This ensures consistent font modification logic for both font creation APIs.
 * 
 * Функция-замена для CreateFontA, которая преобразует отдельные параметры
 * шрифта в структуру LOGFONT и делегирует обработку My_CreateFontIndirectA.
 * Это обеспечивает согласованную логику изменения шрифта для обоих API создания шрифтов.
 * 
 * PARAMETERS / ПАРАМЕТРЫ (standard CreateFontA parameters):
 * h    - Height of font / Высота шрифта
 * w    - Width of font / Ширина шрифта
 * esc  - Escapement angle / Угол наклона
 * ori  - Orientation angle / Угол ориентации
 * wt   - Font weight (bold, normal, etc.) / Вес шрифта (жирный, обычный и т.д.)
 * it   - Italic flag / Флаг курсива
 * ul   - Underline flag / Флаг подчёркивания
 * so   - Strikeout flag / Флаг зачёркивания
 * cs   - Character set / Набор символов
 * outp - Output precision / Точность вывода
 * clp  - Clipping precision / Точность обрезки
 * qual - Output quality / Качество вывода
 * pf   - Pitch and family / Шаг и семейство
 * face - Font face name / Имя начертания шрифта
 * 
 * RETURNS / ВОЗВРАЩАЕТ:
 * HFONT - Handle to the created font, or NULL on failure
 *         Дескриптор созданного шрифта или NULL при ошибке
 * 
 * ALGORITHM / АЛГОРИТМ:
 * 1. Convert individual parameters to LOGFONT structure
 * 2. Call My_CreateFontIndirectA which applies font modifications
 * 3. Return resulting font handle
 * 
 * 1. Преобразовать отдельные параметры в структуру LOGFONT
 * 2. Вызвать My_CreateFontIndirectA, которая применяет изменения шрифта
 * 3. Вернуть результирующий дескриптор шрифта
 * 
 * NOTES / ПРИМЕЧАНИЯ:
 * This approach centralizes all font modification logic in one place
 * (My_CreateFontIndirectA), making the code more maintainable.
 * 
 * Этот подход централизует всю логику изменения шрифта в одном месте
 * (My_CreateFontIndirectA), делая код более поддерживаемым.
 ******************************************************************************/
static HFONT WINAPI My_CreateFontA(
    int h, int w, int esc, int ori, int wt,
    DWORD it, DWORD ul, DWORD so, DWORD cs,
    DWORD outp, DWORD clp, DWORD qual, DWORD pf,
    LPCSTR face)
{
    // Build LOGFONT structure from individual parameters
    // Построить структуру LOGFONT из отдельных параметров
    LOGFONTA lf = {0};
    lf.lfHeight = h;
    lf.lfWidth = w;
    lf.lfEscapement = esc;
    lf.lfOrientation = ori;
    lf.lfWeight = wt;
    lf.lfItalic = (BYTE)it;
    lf.lfUnderline = (BYTE)ul;
    lf.lfStrikeOut = (BYTE)so;
    lf.lfCharSet = (BYTE)cs;
    lf.lfOutPrecision = (BYTE)outp;
    lf.lfClipPrecision = (BYTE)clp;
    lf.lfQuality = (BYTE)qual;
    lf.lfPitchAndFamily = (BYTE)pf;
    
    // Copy font face name if provided / Копировать имя начертания, если предоставлено
    if (face) lstrcpynA(lf.lfFaceName, face, LF_FACESIZE);

    // Delegate to My_CreateFontIndirectA for consistent processing
    // Делегировать My_CreateFontIndirectA для согласованной обработки
    return My_CreateFontIndirectA(&lf);
}

/*******************************************************************************
 * WORKER THREAD FUNCTIONS
 * ФУНКЦИИ РАБОЧЕГО ПОТОКА
 ******************************************************************************/

/*******************************************************************************
 * TryPatchModule
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Attempts to patch the Import Address Table (IAT) of a specific module
 * (DLL) to redirect font creation functions to our replacement functions.
 * 
 * Пытается пропатчить таблицу адресов импорта (IAT) конкретного модуля
 * (DLL) для перенаправления функций создания шрифтов на наши функции-замены.
 * 
 * PARAMETERS / ПАРАМЕТРЫ:
 * modName - Name of the module/DLL to patch (e.g., "in_dshow.dll")
 *           Имя модуля/DLL для патчинга (например, "in_dshow.dll")
 * 
 * PROCESS / ПРОЦЕСС:
 * 1. Get module handle (checks if module is loaded)
 * 2. If loaded, patch CreateFontIndirectA in its IAT
 * 3. Patch CreateFontA in its IAT
 * 4. Store original function pointers for later use
 * 
 * 1. Получить дескриптор модуля (проверить, загружен ли модуль)
 * 2. Если загружен, пропатчить CreateFontIndirectA в его IAT
 * 3. Пропатчить CreateFontA в его IAT
 * 4. Сохранить оригинальные указатели функций для последующего использования
 * 
 * NOTES / ПРИМЕЧАНИЯ:
 * - Safe to call multiple times (IAT_PatchByName handles this)
 * - Only patches if module is currently loaded
 * - Does not load modules that aren't already loaded
 * 
 * - Безопасно вызывать несколько раз (IAT_PatchByName обрабатывает это)
 * - Патчит только если модуль уже загружен
 * - Не загружает модули, которые ещё не загружены
 ******************************************************************************/
static void TryPatchModule(const char* modName)
{
    // Check if module is loaded / Проверить, загружен ли модуль
    HMODULE hMod = GetModuleHandleA(modName);
    if (!hMod) return;  // Module not loaded, nothing to patch / Модуль не загружен, нечего патчить

    // Patch CreateFontIndirectA in this module's IAT
    // Пропатчить CreateFontIndirectA в IAT этого модуля
    // Parameters: module handle, DLL name, function name, new function, pointer to store original
    // Параметры: дескриптор модуля, имя DLL, имя функции, новая функция, указатель для сохранения оригинала
    IAT_PatchByName(hMod, "GDI32.DLL", "CreateFontIndirectA", 
                   (void*)My_CreateFontIndirectA, (void**)&s_real_CreateFontIndirectA);

    // Patch CreateFontA in this module's IAT
    // Пропатчить CreateFontA в IAT этого модуля
    IAT_PatchByName(hMod, "GDI32.DLL", "CreateFontA", 
                   (void*)My_CreateFontA, (void**)&s_real_CreateFontA);
}

/*******************************************************************************
 * WorkerProc
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Worker thread procedure that initializes real function pointers and
 * repeatedly attempts to patch target video plugins. Uses polling approach
 * because plugins may load asynchronously after Winamp starts.
 * 
 * Процедура рабочего потока, которая инициализирует указатели на реальные
 * функции и повторно пытается пропатчить целевые видео-плагины. Использует
 * подход опроса, потому что плагины могут загружаться асинхронно после запуска Winamp.
 * 
 * PARAMETERS / ПАРАМЕТРЫ:
 * Standard thread procedure parameter (unused)
 * Стандартный параметр процедуры потока (не используется)
 * 
 * RETURNS / ВОЗВРАЩАЕТ:
 * DWORD - Thread exit code (always 0)
 *         Код выхода потока (всегда 0)
 * 
 * ALGORITHM / АЛГОРИТМ:
 * 1. Get GDI32.DLL module handle
 * 2. Obtain real function pointers from GDI32.DLL
 * 3. Loop up to 50 times (5 seconds total):
 *    a. Check stop flag
 *    b. Try to patch in_dshow.dll
 *    c. Try to patch in_nsv.dll
 *    d. Sleep 100ms
 * 4. Exit thread
 * 
 * 1. Получить дескриптор модуля GDI32.DLL
 * 2. Получить указатели на реальные функции из GDI32.DLL
 * 3. Цикл до 50 раз (5 секунд всего):
 *    a. Проверить флаг остановки
 *    b. Попытаться пропатчить in_dshow.dll
 *    c. Попытаться пропатчить in_nsv.dll
 *    d. Спать 100мс
 * 4. Выйти из потока
 * 
 * WHY POLLING / ПОЧЕМУ ОПРОС:
 * Plugins may be loaded on-demand (e.g., when user opens a video file).
 * Polling for 5 seconds catches most delayed plugin loads while limiting
 * resource usage.
 * 
 * Плагины могут загружаться по требованию (например, когда пользователь
 * открывает видео-файл). Опрос в течение 5 секунд ловит большинство
 * отложенных загрузок плагинов, ограничивая использование ресурсов.
 ******************************************************************************/
static DWORD WINAPI WorkerProc(LPVOID)
{
    // Get GDI32.DLL module handle to obtain real function pointers
    // Получить дескриптор модуля GDI32.DLL для получения указателей на реальные функции
    HMODULE hGDI = GetModuleHandleA("GDI32.DLL");
    if (hGDI) {
        // Get real CreateFontIndirectA function pointer if not already obtained
        // Получить указатель на реальную функцию CreateFontIndirectA, если ещё не получен
        if (!s_real_CreateFontIndirectA)
            s_real_CreateFontIndirectA = (PFN_CreateFontIndirectA)GetProcAddress(hGDI, "CreateFontIndirectA");
        
        // Get real CreateFontA function pointer if not already obtained
        // Получить указатель на реальную функцию CreateFontA, если ещё не получен
        if (!s_real_CreateFontA)
            s_real_CreateFontA = (PFN_CreateFontA)GetProcAddress(hGDI, "CreateFontA");
    }

    // Poll for plugins to patch (50 iterations * 100ms = 5 seconds)
    // Опрашивать плагины для патчинга (50 итераций * 100мс = 5 секунд)
    for (int i = 0; i < 50; ++i) {
        // Check if stop requested (thread-safe check)
        // Проверить, запрошена ли остановка (потокобезопасная проверка)
        if (s_stopThread) break;

        // Try to patch DirectShow input plugin
        // Попытаться пропатчить плагин ввода DirectShow
        TryPatchModule("in_dshow.dll");
        
        // Try to patch NSV (Nullsoft Streaming Video) input plugin
        // Попытаться пропатчить плагин ввода NSV (Nullsoft Streaming Video)
        TryPatchModule("in_nsv.dll");

        // Sleep to avoid excessive CPU usage
        // Спать, чтобы избежать чрезмерного использования процессора
        Sleep(100);
    }
    
    return 0;
}

/*******************************************************************************
 * PUBLIC API FUNCTIONS
 * ФУНКЦИИ ПУБЛИЧНОГО API
 ******************************************************************************/

/*******************************************************************************
 * VideoFontFix_Init
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Initializes the video font fix module by starting the worker thread that
 * will patch target plugins. This is the main entry point for the module.
 * 
 * Инициализирует модуль исправления шрифтов видео, запуская рабочий поток,
 * который будет патчить целевые плагины. Это главная точка входа для модуля.
 * 
 * WHEN TO CALL / КОГДА ВЫЗЫВАТЬ:
 * Should be called during Winamp initialization, before video plugins are
 * likely to be loaded. Safe to call multiple times (creates thread only once).
 * 
 * Должна вызываться во время инициализации Winamp, до вероятной загрузки
 * видео-плагинов. Безопасно вызывать несколько раз (создаёт поток только один раз).
 * 
 * NOTES / ПРИМЕЧАНИЯ:
 * - Creates a worker thread that will run for up to 5 seconds
 * - Worker thread patches plugins as they load
 * - No-op if thread is already running
 * 
 * - Создаёт рабочий поток, который будет работать до 5 секунд
 * - Рабочий поток патчит плагины по мере их загрузки
 * - Ничего не делает, если поток уже работает
 ******************************************************************************/
extern "C" void VideoFontFix_Init(void)
{
    // Create worker thread only if not already created
    // Создать рабочий поток только если ещё не создан
    if (!s_hThread) {
        DWORD tid;  // Thread ID (not used, but required by CreateThread)
                    // Идентификатор потока (не используется, но требуется CreateThread)
        
        // Create worker thread / Создать рабочий поток
        s_hThread = CreateThread(NULL, 0, WorkerProc, NULL, 0, &tid);
    }
}

/*******************************************************************************
 * VideoFontFix_Quit
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Cleans up the video font fix module by signaling the worker thread to stop
 * and waiting for it to terminate. Should be called during Winamp shutdown.
 * 
 * Очищает модуль исправления шрифтов видео, сигнализируя рабочему потоку
 * остановиться и ожидая его завершения. Должна вызываться при завершении Winamp.
 * 
 * CLEANUP PROCESS / ПРОЦЕСС ОЧИСТКИ:
 * 1. Set stop flag using atomic operation (thread-safe)
 * 2. Wait for thread to terminate (up to 500ms timeout)
 * 3. Close thread handle
 * 4. Clear thread handle variable
 * 
 * 1. Установить флаг остановки с использованием атомарной операции (потокобезопасно)
 * 2. Ждать завершения потока (до 500мс тайм-аут)
 * 3. Закрыть дескриптор потока
 * 4. Очистить переменную дескриптора потока
 * 
 * NOTES / ПРИМЕЧАНИЯ:
 * - Uses InterlockedExchange for thread-safe flag setting
 * - Waits up to 500ms for graceful thread termination
 * - If thread doesn't terminate, handle is still closed (thread terminates on its own)
 * - Safe to call even if thread is not running
 * 
 * - Использует InterlockedExchange для потокобезопасной установки флага
 * - Ждёт до 500мс для корректного завершения потока
 * - Если поток не завершается, дескриптор всё равно закрывается (поток завершится сам)
 * - Безопасно вызывать, даже если поток не работает
 ******************************************************************************/
extern "C" void VideoFontFix_Quit(void)
{
    // Signal worker thread to stop (atomic operation, thread-safe)
    // Сигнализировать рабочему потоку остановиться (атомарная операция, потокобезопасно)
    InterlockedExchange(&s_stopThread, 1);
    
    // Wait for thread to terminate and cleanup
    // Ждать завершения потока и очистить
    if (s_hThread) {
        // Wait up to 500ms for thread to terminate gracefully
        // Ждать до 500мс для корректного завершения потока
        WaitForSingleObject(s_hThread, 500);
        
        // Close thread handle to free system resources
        // Закрыть дескриптор потока для освобождения системных ресурсов
        CloseHandle(s_hThread);
        
        // Clear handle variable / Очистить переменную дескриптора
        s_hThread = NULL;
    }
}