/******************************************************************************
 * unicode_decrypt_engine.cpp
 * 
 * UNICODE DECRYPTION ENGINE FOR MP3 ID3 TAG ENCODING FIXES
 * ДВИЖОК РАСШИФРОВКИ UNICODE ДЛЯ ИСПРАВЛЕНИЯ КОДИРОВОК ID3-ТЕГОВ MP3
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * This module provides core functionality for detecting and fixing character
 * encoding issues (mojibake) in MP3 ID3 tags. It includes functions for:
 * - Converting between various encodings (UTF-8, Windows-1251, ACP)
 * - Reading and parsing ID3v2 tags (versions 2.2, 2.3, 2.4)
 * - IAT (Import Address Table) patching for hooking
 * - Syncsafe integer conversion (ID3v2.4 format)
 * 
 * Этот модуль предоставляет основную функциональность для обнаружения и
 * исправления проблем кодировки символов (mojibake) в ID3-тегах MP3. Включает:
 * - Преобразование между различными кодировками (UTF-8, Windows-1251, ACP)
 * - Чтение и парсинг ID3v2 тегов (версии 2.2, 2.3, 2.4)
 * - IAT (таблица импорта адресов) патчинг для перехватов
 * - Преобразование syncsafe целых чисел (формат ID3v2.4)
 * 
 * KEY CONCEPTS / КЛЮЧЕВЫЕ КОНЦЕПЦИИ:
 * 
 * MOJIBAKE (????):
 * Character encoding corruption that occurs when text is decoded using the
 * wrong character encoding. Common in Russian ID3 tags where Windows-1251
 * text is incorrectly interpreted as UTF-8 or vice versa.
 * 
 * Искажение кодировки символов, возникающее когда текст декодируется с
 * использованием неправильной кодировки. Часто встречается в русских ID3-тегах,
 * где текст Windows-1251 неправильно интерпретируется как UTF-8 или наоборот.
 * 
 * SYNCSAFE INTEGERS:
 * Special integer format used in ID3v2.4 where the MSB of each byte is always
 * 0. This prevents the value from being confused with MP3 frame sync markers.
 * Example: 0x000000FF (normal) > 0x000001FF (syncsafe)
 * 
 * Специальный формат целых чисел в ID3v2.4, где MSB каждого байта всегда 0.
 * Это предотвращает путаницу значения с маркерами синхронизации MP3-кадров.
 * Пример: 0x000000FF (обычный) > 0x000001FF (syncsafe)
 * 
 * IAT PATCHING:
 * Technique for intercepting Windows API calls by modifying the Import Address
 * Table of a loaded module. Used to hook file I/O functions in in_mp3.dll.
 * 
 * Техника перехвата вызовов Windows API путём модификации таблицы импорта
 * адресов загруженного модуля. Используется для перехвата функций файлового
 * ввода-вывода в in_mp3.dll.
 * 
 ******************************************************************************/

// Minimize Windows header inclusion
// Минимизация включения заголовков Windows
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tlhelp32.h>    // For CreateToolhelp32Snapshot / Для CreateToolhelp32Snapshot
#include <string.h>      // For string operations / Для строковых операций
#include "unicode_decrypt_engine.h"

// Link against kernel32.lib
// Линковка с kernel32.lib
#pragma comment(lib, "kernel32.lib")

/*******************************************************************************
 * CONSTANTS AND MACROS / КОНСТАНТЫ И МАКРОСЫ
 ******************************************************************************/

// Toolhelp32 flag for 32-bit modules on 64-bit systems
// Флаг Toolhelp32 для 32-битных модулей на 64-битных системах
#ifndef TH32CS_SNAPMODULE32
#define TH32CS_SNAPMODULE32 0x00000010
#endif

// Array size macro for compile-time calculations
// Макрос размера массива для вычислений во время компиляции
#ifndef ARRAYSIZE
#define ARRAYSIZE(a) (sizeof(a)/sizeof((a)[0]))
#endif

/*******************************************************************************
 * OPERATING SYSTEM DETECTION / ОПРЕДЕЛЕНИЕ ОПЕРАЦИОННОЙ СИСТЕМЫ
 ******************************************************************************/

/*******************************************************************************
 * DECRYPT_IsWin9x - Check if Running on Windows 9x
 *                   Проверка работы на Windows 9x
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Detects if the code is running on Windows 95/98/ME (Windows 9x family).
 * This is important for compatibility as these systems have different
 * character encoding and API behavior.
 * 
 * Определяет, работает ли код на Windows 95/98/ME (семейство Windows 9x).
 * Это важно для совместимости, так как эти системы имеют другое поведение
 * кодировки символов и API.
 * 
 * RETURNS / ВОЗВРАЩАЕТ:
 * TRUE  - Running on Windows 9x / Работает на Windows 9x
 * FALSE - Running on Windows NT/2000/XP+ / Работает на Windows NT/2000/XP+
 * 
 * TECHNICAL DETAILS / ТЕХНИЧЕСКИЕ ДЕТАЛИ:
 * Uses GetVersionExA to query OS version. Windows 9x has platform ID
 * VER_PLATFORM_WIN32_WINDOWS, while NT-based systems use
 * VER_PLATFORM_WIN32_NT.
 * 
 * Использует GetVersionExA для запроса версии ОС. Windows 9x имеет ID
 * платформы VER_PLATFORM_WIN32_WINDOWS, в то время как NT-системы используют
 * VER_PLATFORM_WIN32_NT.
 ******************************************************************************/
extern "C" BOOL DECRYPT_IsWin9x(void) {
    OSVERSIONINFOA vi = { sizeof(OSVERSIONINFOA) };
    if (!GetVersionExA(&vi)) return FALSE;  // Query failed / Запрос не удался
    return (vi.dwPlatformId == VER_PLATFORM_WIN32_WINDOWS);
}

/*******************************************************************************
 * SYNCSAFE INTEGER CONVERSION FUNCTIONS
 * ФУНКЦИИ ПРЕОБРАЗОВАНИЯ SYNCSAFE ЦЕЛЫХ ЧИСЕЛ
 * 
 * BACKGROUND / ПРЕДЫСТОРИЯ:
 * ID3v2.4 uses "syncsafe integers" to avoid confusion with MP3 sync markers.
 * In syncsafe format, the MSB (bit 7) of each byte is always 0, effectively
 * storing a 28-bit value in 4 bytes instead of 32 bits.
 * 
 * ID3v2.4 использует "syncsafe целые числа" чтобы избежать путаницы с
 * маркерами синхронизации MP3. В syncsafe формате MSB (бит 7) каждого байта
 * всегда 0, фактически храня 28-битное значение в 4 байтах вместо 32 бит.
 * 
 * EXAMPLE / ПРИМЕР:
 * Normal 32-bit:  0xFF 0xFF 0xFF 0xFF = 4,294,967,295
 * Syncsafe 28-bit: 0x7F 0x7F 0x7F 0x7F = 268,435,455
 ******************************************************************************/

/*******************************************************************************
 * DECRYPT_SyncsafeToSize - Convert Syncsafe to Normal Integer
 *                           Преобразование Syncsafe в обычное целое
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Converts a 4-byte syncsafe integer (ID3v2.4) to a normal 32-bit integer.
 * Преобразует 4-байтовое syncsafe целое (ID3v2.4) в обычное 32-битное целое.
 * 
 * PARAMETERS / ПАРАМЕТРЫ:
 * p - Pointer to 4-byte syncsafe integer (MSB first, big-endian)
 *     Указатель на 4-байтовое syncsafe целое (старший байт первым, big-endian)
 * 
 * RETURNS / ВОЗВРАЩАЕТ:
 * Normal 32-bit integer value / Обычное 32-битное целое значение
 * 
 * ALGORITHM / АЛГОРИТМ:
 * Each byte contributes 7 bits (bits 0-6), bit 7 is ignored:
 * result = (p[0] & 0x7F) << 21 | (p[1] & 0x7F) << 14 | 
 *          (p[2] & 0x7F) << 7  | (p[3] & 0x7F)
 * 
 * Каждый байт вносит 7 бит (биты 0-6), бит 7 игнорируется:
 * результат = (p[0] & 0x7F) << 21 | (p[1] & 0x7F) << 14 | 
 *             (p[2] & 0x7F) << 7  | (p[3] & 0x7F)
 ******************************************************************************/
extern "C" DWORD DECRYPT_SyncsafeToSize(const BYTE* p) {
    return ((p[0]&0x7F)<<21) | ((p[1]&0x7F)<<14) | ((p[2]&0x7F)<<7) | (p[3]&0x7F);
}

/*******************************************************************************
 * DECRYPT_SizeToSyncsafe - Convert Normal Integer to Syncsafe
 *                           Преобразование обычного целого в Syncsafe
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Converts a normal 32-bit integer to syncsafe format (ID3v2.4).
 * Преобразует обычное 32-битное целое в формат syncsafe (ID3v2.4).
 * 
 * PARAMETERS / ПАРАМЕТРЫ:
 * v - Normal 32-bit integer value (max 0x0FFFFFFF for syncsafe)
 *     Обычное 32-битное целое значение (макс 0x0FFFFFFF для syncsafe)
 * 
 * RETURNS / ВОЗВРАЩАЕТ:
 * 32-bit value with syncsafe encoding / 32-битное значение с syncsafe кодировкой
 * 
 * ALGORITHM / АЛГОРИТМ:
 * Redistributes bits to leave MSB of each byte as 0:
 * - Bits 21-27 > byte 0 (bits 0-6)
 * - Bits 14-20 > byte 1 (bits 0-6)
 * - Bits  7-13 > byte 2 (bits 0-6)
 * - Bits  0-6  > byte 3 (bits 0-6)
 * 
 * Перераспределяет биты, оставляя MSB каждого байта как 0:
 * - Биты 21-27 > байт 0 (биты 0-6)
 * - Биты 14-20 > байт 1 (биты 0-6)
 * - Биты  7-13 > байт 2 (биты 0-6)
 * - Биты  0-6  > байт 3 (биты 0-6)
 ******************************************************************************/
extern "C" DWORD DECRYPT_SizeToSyncsafe(DWORD v) {
    return ((v&0xFE00000)<<3) | ((v&0x1FC000)<<2) | ((v&0x3F80)<<1) | (v&0x7F);
}

/*******************************************************************************
 * BIG-ENDIAN INTEGER FUNCTIONS / ФУНКЦИИ BIG-ENDIAN ЦЕЛЫХ ЧИСЕЛ
 * 
 * BACKGROUND / ПРЕДЫСТОРИЯ:
 * ID3v2 tags use big-endian (network byte order) for multi-byte integers.
 * Intel x86/x64 uses little-endian, so conversion is necessary.
 * 
 * ID3v2 теги используют big-endian (сетевой порядок байтов) для многобайтовых
 * целых. Intel x86/x64 использует little-endian, поэтому необходимо преобразование.
 ******************************************************************************/

/*******************************************************************************
 * DECRYPT_BE32Read - Read 32-bit Big-Endian Integer
 *                    Чтение 32-битного Big-Endian целого
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Reads a 32-bit integer from memory in big-endian format.
 * Читает 32-битное целое из памяти в формате big-endian.
 * 
 * PARAMETERS / ПАРАМЕТРЫ:
 * p - Pointer to 4 bytes in big-endian order
 *     Указатель на 4 байта в порядке big-endian
 * 
 * RETURNS / ВОЗВРАЩАЕТ:
 * 32-bit integer in host byte order / 32-битное целое в порядке байтов хоста
 * 
 * EXAMPLE / ПРИМЕР:
 * Bytes: [0x12, 0x34, 0x56, 0x78] > Result: 0x12345678
 ******************************************************************************/
extern "C" DWORD DECRYPT_BE32Read(const BYTE* p) {
    return (p[0]<<24) | (p[1]<<16) | (p[2]<<8) | p[3];
}

/*******************************************************************************
 * DECRYPT_BE32Write - Write 32-bit Integer in Big-Endian Format
 *                     Запись 32-битного целого в формате Big-Endian
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Writes a 32-bit integer to memory in big-endian format.
 * Записывает 32-битное целое в память в формате big-endian.
 * 
 * PARAMETERS / ПАРАМЕТРЫ:
 * p - Pointer to 4-byte buffer / Указатель на 4-байтовый буфер
 * v - 32-bit integer value to write / 32-битное целое значение для записи
 * 
 * EXAMPLE / ПРИМЕР:
 * Value: 0x12345678 > Bytes: [0x12, 0x34, 0x56, 0x78]
 ******************************************************************************/
extern "C" void DECRYPT_BE32Write(BYTE* p, DWORD v) {
    p[0] = (BYTE)(v>>24);  // Most significant byte / Старший байт
    p[1] = (BYTE)(v>>16);  
    p[2] = (BYTE)(v>>8);   
    p[3] = (BYTE)v;        // Least significant byte / Младший байт
}

/*******************************************************************************
 * STRING MANIPULATION FUNCTIONS / ФУНКЦИИ РАБОТЫ СО СТРОКАМИ
 ******************************************************************************/

/*******************************************************************************
 * towlower0 - Convert Wide Character to Lowercase
 *             Преобразование широкого символа в нижний регистр
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Simple case conversion for ASCII range (A-Z > a-z). Used internally for
 * case-insensitive string comparisons.
 * 
 * Простое преобразование регистра для ASCII диапазона (A-Z > a-z). Используется
 * внутренне для сравнений строк без учёта регистра.
 * 
 * PARAMETERS / ПАРАМЕТРЫ:
 * ch - Wide character to convert / Широкий символ для преобразования
 * 
 * RETURNS / ВОЗВРАЩАЕТ:
 * Lowercase version of ch if A-Z, otherwise unchanged
 * Версия ch в нижнем регистре если A-Z, иначе без изменений
 * 
 * NOTE / ПРИМЕЧАНИЕ:
 * Only handles ASCII A-Z. Does not handle international characters or locale.
 * Обрабатывает только ASCII A-Z. Не обрабатывает международные символы или локаль.
 ******************************************************************************/
static WCHAR towlower0(WCHAR ch) {
    return (ch >= L'A' && ch <= L'Z') ? (WCHAR)(ch + 32) : ch;
}

/*******************************************************************************
 * DECRYPT_WideStrContainsI - Case-Insensitive Substring Search
 *                             Поиск подстроки без учёта регистра
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Searches for a substring within a wide string, ignoring case for ASCII chars.
 * Ищет подстроку в широкой строке, игнорируя регистр для ASCII символов.
 * 
 * PARAMETERS / ПАРАМЕТРЫ:
 * str - String to search in / Строка для поиска
 * sub - Substring to find / Подстрока для поиска
 * 
 * RETURNS / ВОЗВРАЩАЕТ:
 * TRUE if substring found / TRUE если подстрока найдена
 * FALSE otherwise / FALSE в противном случае
 * 
 * ALGORITHM / АЛГОРИТМ:
 * Uses sliding window approach - tries to match substring starting at each
 * position in the main string.
 * 
 * Использует подход скользящего окна - пытается найти совпадение подстроки,
 * начиная с каждой позиции в основной строке.
 * 
 * COMPLEXITY / СЛОЖНОСТЬ:
 * O(n*m) where n = length of str, m = length of sub
 * O(n*m) где n = длина str, m = длина sub
 ******************************************************************************/
extern "C" BOOL DECRYPT_WideStrContainsI(const WCHAR* str, const WCHAR* sub) {
    if (!str || !sub || !*sub) return FALSE;  // Validate parameters / Проверка параметров
    
    while (*str) {
        const WCHAR *p1 = str, *p2 = sub;
        
        // Try to match substring starting at current position
        // Попытка найти совпадение подстроки начиная с текущей позиции
        while (*p1 && *p2 && towlower0(*p1) == towlower0(*p2)) { 
            p1++; 
            p2++; 
        }
        
        // If we reached end of substring, we found a match
        // Если достигли конца подстроки, нашли совпадение
        if (!*p2) return TRUE;
        
        str++;  // Try next position / Попробовать следующую позицию
    }
    return FALSE;
}

/*******************************************************************************
 * DECRYPT_WideToACP - Convert Wide String to ANSI Code Page
 *                     Преобразование широкой строки в ANSI кодовую страницу
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Converts a Unicode (wide) string to the system's ANSI code page (CP_ACP).
 * This is used to convert decoded ID3 tags to the format Winamp expects.
 * 
 * Преобразует Unicode (широкую) строку в ANSI кодовую страницу системы (CP_ACP).
 * Используется для преобразования декодированных ID3-тегов в формат, ожидаемый Winamp.
 * 
 * PARAMETERS / ПАРАМЕТРЫ:
 * w      - Input wide string (null-terminated) / Входная широкая строка (с null-терминатором)
 * out    - Output ANSI buffer / Выходной ANSI буфер
 * outlen - Size of output buffer / Размер выходного буфера
 * 
 * RETURNS / ВОЗВРАЩАЕТ:
 * Number of characters written (excluding null terminator)
 * Количество записанных символов (исключая null-терминатор)
 * 0 on failure / 0 при неудаче
 * 
 * IMPORTANT / ВАЖНО:
 * Characters that cannot be represented in the ANSI code page may be replaced
 * with '?' or best-fit characters.
 * 
 * Символы, которые не могут быть представлены в ANSI кодовой странице, могут
 * быть заменены на '?' или наилучшим образом подходящие символы.
 ******************************************************************************/
extern "C" int DECRYPT_WideToACP(const WCHAR* w, char* out, int outlen) {
    if (!w || !out || outlen <= 0) return 0;  // Validate parameters / Проверка параметров
    
    // Convert using Windows API / Преобразование используя Windows API
    int m = WideCharToMultiByte(CP_ACP, 0, w, -1, out, outlen, NULL, NULL);
    
    // Subtract 1 to exclude null terminator from count
    // Вычесть 1 чтобы исключить null-терминатор из счёта
    return m ? m - 1 : 0;
}

/*******************************************************************************
 * DECRYPT_Utf8ToWide - Convert UTF-8 String to Wide String
 *                      Преобразование UTF-8 строки в широкую строку
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Converts a UTF-8 encoded string to Unicode (UTF-16LE) wide string. This
 * function first tries the Windows API, then falls back to manual decoding
 * if the API fails (for robustness with malformed UTF-8).
 * 
 * Преобразует UTF-8 кодированную строку в Unicode (UTF-16LE) широкую строку.
 * Эта функция сначала пробует Windows API, затем возвращается к ручному
 * декодированию если API не удаётся (для надёжности с искажённым UTF-8).
 * 
 * PARAMETERS / ПАРАМЕТРЫ:
 * utf8   - Input UTF-8 string / Входная UTF-8 строка
 * out    - Output wide string buffer / Буфер выходной широкой строки
 * cchOut - Size of output buffer in wide characters / Размер выходного буфера в широких символах
 * 
 * RETURNS / ВОЗВРАЩАЕТ:
 * Number of wide characters written (excluding null terminator)
 * Количество записанных широких символов (исключая null-терминатор)
 * 
 * UTF-8 ENCODING SCHEME / СХЕМА КОДИРОВАНИЯ UTF-8:
 * 1-byte: 0xxxxxxx                    (U+0000 to U+007F)
 * 2-byte: 110xxxxx 10xxxxxx           (U+0080 to U+07FF)
 * 3-byte: 1110xxxx 10xxxxxx 10xxxxxx  (U+0800 to U+FFFF)
 * 
 * FALLBACK DECODER / РЕЗЕРВНЫЙ ДЕКОДЕР:
 * The manual decoder handles 1-3 byte sequences (covers Basic Multilingual
 * Plane, sufficient for most ID3 tags). 4-byte sequences (rare) are not supported.
 * 
 * Ручной декодер обрабатывает 1-3 байтовые последовательности (покрывает
 * базовую многоязычную плоскость, достаточно для большинства ID3-тегов).
 * 4-байтовые последовательности (редкие) не поддерживаются.
 ******************************************************************************/
extern "C" int DECRYPT_Utf8ToWide(const char* utf8, WCHAR* out, int cchOut) {
    if (!utf8 || !out || cchOut <= 0) return 0;  // Validate parameters / Проверка параметров

    // Try Windows API first (codepage 65001 = UTF-8)
    // Сначала попробовать Windows API (кодовая страница 65001 = UTF-8)
    int wlen = MultiByteToWideChar(65001, 0, utf8, -1, out, cchOut);
    if (wlen > 0) return wlen - 1;  // Success, return length / Успех, вернуть длину

    // Fallback: Manual UTF-8 decoding for robustness
    // Резерв: Ручное декодирование UTF-8 для надёжности
    const unsigned char* p = (const unsigned char*)utf8;
    int n = 0;
    
    while (*p && n < cchOut - 1) {
        unsigned c = *p++;
        
        if (c < 0x80) {
            // 1-byte sequence (ASCII) / 1-байтовая последовательность (ASCII)
            out[n++] = (WCHAR)c;
        } 
        else if ((c & 0xE0) == 0xC0 && (p[0] & 0xC0) == 0x80) {
            // 2-byte sequence / 2-байтовая последовательность
            // 110xxxxx 10xxxxxx
            out[n++] = (WCHAR)(((c & 0x1F) << 6) | (p[0] & 0x3F)); 
            p++;
        } 
        else if ((c & 0xF0) == 0xE0 && (p[0] & 0xC0) == 0x80 && (p[1] & 0xC0) == 0x80) {
            // 3-byte sequence / 3-байтовая последовательность
            // 1110xxxx 10xxxxxx 10xxxxxx
            out[n++] = (WCHAR)(((c & 0x0F) << 12) | ((p[0] & 0x3F) << 6) | (p[1] & 0x3F)); 
            p += 2;
        } 
        else {
            // Invalid UTF-8 sequence, stop decoding
            // Неверная UTF-8 последовательность, остановить декодирование
            break; 
        }
    }
    
    out[n] = 0;  // Null terminate / Null-терминатор
    return n;
}

/*******************************************************************************
 * ENCODING DETECTION AND CORRECTION FUNCTIONS
 * ФУНКЦИИ ОБНАРУЖЕНИЯ И ИСПРАВЛЕНИЯ КОДИРОВКИ
 ******************************************************************************/

/*******************************************************************************
 * mbcp_to_acp - Convert from Specific Codepage to ACP
 *               Преобразование из конкретной кодовой страницы в ACP
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Converts a string from a specific code page (e.g., Windows-1251) to the
 * system's ANSI code page via Unicode intermediate representation.
 * 
 * Преобразует строку из конкретной кодовой страницы (например, Windows-1251)
 * в ANSI кодовую страницу системы через промежуточное представление Unicode.
 * 
 * PARAMETERS / ПАРАМЕТРЫ:
 * src    - Source string in specified codepage / Исходная строка в указанной кодовой странице
 * cp     - Source codepage (e.g., 1251 for Windows-1251) / Исходная кодовая страница (например, 1251 для Windows-1251)
 * out    - Output buffer / Выходной буфер
 * outlen - Output buffer size / Размер выходного буфера
 * 
 * RETURNS / ВОЗВРАЩАЕТ:
 * Number of characters written / Количество записанных символов
 * 0 on failure / 0 при неудаче
 * 
 * PROCESS / ПРОЦЕСС:
 * 1. Convert from source codepage to Unicode (wide string)
 *    Преобразовать из исходной кодовой страницы в Unicode (широкую строку)
 * 2. Convert from Unicode to system ACP
 *    Преобразовать из Unicode в системную ACP
 ******************************************************************************/
static int mbcp_to_acp(const char* src, UINT cp, char* out, int outlen) {
    // Get required buffer size for wide string conversion
    // Получить требуемый размер буфера для преобразования в широкую строку
    int wlen = MultiByteToWideChar(cp, 0, src, -1, NULL, 0);
    if (!wlen) return 0;  // Conversion failed / Преобразование не удалось
    
    // Allocate temporary wide string buffer
    // Выделить временный буфер широкой строки
    WCHAR* w = (WCHAR*)GlobalAlloc(GPTR, wlen * sizeof(WCHAR));
    if (!w) return 0;  // Allocation failed / Выделение не удалось
    
    // Convert to wide string
    // Преобразовать в широкую строку
    MultiByteToWideChar(cp, 0, src, -1, w, wlen);
    
    // Convert wide string to ACP
    // Преобразовать широкую строку в ACP
    int m = DECRYPT_WideToACP(w, out, outlen);
    
    // Free temporary buffer
    // Освободить временный буфер
    GlobalFree(w);
    return m;
}

/*******************************************************************************
 * DECRYPT_ToACP_Best - Intelligently Detect and Convert to ACP
 *                      Интеллектуальное обнаружение и преобразование в ACP
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Attempts to detect the encoding of a string and convert it to ACP using
 * the best matching encoding. This is the core mojibake-fixing function.
 * 
 * Пытается обнаружить кодировку строки и преобразовать её в ACP, используя
 * наилучшую подходящую кодировку. Это основная функция исправления mojibake.
 * 
 * PARAMETERS / ПАРАМЕТРЫ:
 * src    - Source string (unknown encoding) / Исходная строка (неизвестная кодировка)
 * out    - Output buffer for ACP string / Выходной буфер для ACP строки
 * outlen - Output buffer size / Размер выходного буфера
 * 
 * RETURNS / ВОЗВРАЩАЕТ:
 * Number of characters written / Количество записанных символов
 * 
 * DETECTION ALGORITHM / АЛГОРИТМ ОБНАРУЖЕНИЯ:
 * 1. Check for UTF-8 markers (multi-byte sequences starting with 0xC2-0xF4)
 *    Проверить маркеры UTF-8 (многобайтовые последовательности начинающиеся с 0xC2-0xF4)
 * 2. If UTF-8 detected, convert UTF-8 > Wide > ACP
 *    Если UTF-8 обнаружен, преобразовать UTF-8 > Wide > ACP
 * 3. If not UTF-8, try Windows-1251 (Cyrillic) > Wide > ACP
 *    Если не UTF-8, попробовать Windows-1251 (кириллица) > Wide > ACP
 * 4. If all fails, copy as-is
 *    Если всё не удаётся, копировать как есть
 * 
 * WHY WINDOWS-1251? / ПОЧЕМУ WINDOWS-1251?:
 * Windows-1251 is the most common encoding for Russian/Cyrillic text in old
 * ID3 tags, especially those created by Russian software in the 1990s-2000s.
 * 
 * Windows-1251 - самая распространённая кодировка для русского/кириллического
 * текста в старых ID3-тегах, особенно созданных русским ПО в 1990-2000-х.
 ******************************************************************************/
extern "C" int DECRYPT_ToACP_Best(const char* src, char* out, int outlen) {
    if (!src || !out || outlen <= 0) return 0;  // Validate parameters / Проверка параметров
    out[0] = 0;  // Initialize output / Инициализировать вывод
    
    // Phase 1: Detect if string contains UTF-8 sequences
    // Фаза 1: Обнаружить содержит ли строка UTF-8 последовательности
    BOOL is_utf8 = FALSE;
    const unsigned char* p = (const unsigned char*)src;
    
    while(*p) {
        // Check for valid UTF-8 multi-byte sequence start
        // Проверить начало валидной UTF-8 многобайтовой последовательности
        if (*p >= 0xC2 && *p < 0xF5 && (*(p+1) & 0xC0) == 0x80) { 
            is_utf8 = TRUE; 
            break; 
        }
        p++;
    }

    // Phase 2: If UTF-8 detected, convert UTF-8 > Wide > ACP
    // Фаза 2: Если UTF-8 обнаружен, преобразовать UTF-8 > Wide > ACP
    if (is_utf8) {
        // Allocate temporary wide string buffer
        // Выделить временный буфер широкой строки
        WCHAR* wtmp = (WCHAR*)GlobalAlloc(GPTR, outlen * sizeof(WCHAR));
        if (wtmp) {
            // Convert UTF-8 to wide string
            // Преобразовать UTF-8 в широкую строку
            int wn = DECRYPT_Utf8ToWide(src, wtmp, outlen);
            if (wn > 0) {
                // Convert wide string to ACP
                // Преобразовать широкую строку в ACP
                int res = DECRYPT_WideToACP(wtmp, out, outlen);
                GlobalFree(wtmp);
                return res;  // Success! / Успех!
            }
            GlobalFree(wtmp);
        }
    }

    // Phase 3: Try Windows-1251 (Cyrillic) conversion
    // Фаза 3: Попробовать преобразование Windows-1251 (кириллица)
    if (mbcp_to_acp(src, 1251, out, outlen)) 
        return lstrlenA(out);
    
    // Phase 4: Fallback - copy as-is
    // Фаза 4: Резерв - копировать как есть
    lstrcpynA(out, src, outlen);
    return lstrlenA(out);
}

/*******************************************************************************
 * DECRYPT_FixTagA - Fix Tag Encoding In-Place
 *                   Исправление кодировки тега на месте
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Convenience function that detects and fixes encoding of a tag buffer in-place.
 * Функция удобства, которая обнаруживает и исправляет кодировку буфера тега на месте.
 * 
 * PARAMETERS / ПАРАМЕТРЫ:
 * (first parameter unused) / (первый параметр не используется)
 * buf    - Buffer containing tag (modified in-place) / Буфер содержащий тег (изменяется на месте)
 * buflen - Buffer size / Размер буфера
 * 
 * NOTE / ПРИМЕЧАНИЕ:
 * This function modifies the buffer in-place. Ensure buffer is writable.
 * Эта функция изменяет буфер на месте. Убедитесь что буфер доступен для записи.
 ******************************************************************************/
extern "C" void DECRYPT_FixTagA(const char*, char* buf, int buflen) {
    if (!buf || buflen <= 0) return;  // Validate parameters / Проверка параметров
    
    char tmp[2048];  // Temporary buffer / Временный буфер
    
    // Detect and convert encoding
    // Обнаружить и преобразовать кодировку
    if (DECRYPT_ToACP_Best(buf, tmp, sizeof(tmp))) 
        lstrcpynA(buf, tmp, buflen);  // Copy result back / Копировать результат обратно
}

/*******************************************************************************
 * IAT (IMPORT ADDRESS TABLE) PATCHING FUNCTIONS
 * ФУНКЦИИ IAT (ТАБЛИЦА ИМПОРТА АДРЕСОВ) ПАТЧИНГА
 * 
 * BACKGROUND / ПРЕДЫСТОРИЯ:
 * The Import Address Table (IAT) is a data structure in Windows PE files that
 * contains pointers to imported functions from DLLs. By modifying these
 * pointers, we can redirect API calls to our own hook functions.
 * 
 * Таблица импорта адресов (IAT) - это структура данных в Windows PE файлах,
 * которая содержит указатели на импортированные функции из DLL. Изменяя эти
 * указатели, мы можем перенаправить вызовы API на наши собственные функции-перехватчики.
 * 
 * USE CASE / СЛУЧАЙ ИСПОЛЬЗОВАНИЯ:
 * We use IAT patching to intercept file I/O functions (CreateFileA, ReadFile,
 * etc.) in in_mp3.dll, allowing us to implement virtual file overlay for
 * corrected ID3 tags without modifying actual files.
 * 
 * Мы используем IAT патчинг для перехвата функций файлового ввода-вывода
 * (CreateFileA, ReadFile, и т.д.) в in_mp3.dll, позволяя нам реализовать
 * виртуальное наложение файла для исправленных ID3-тегов без изменения реальных файлов.
 ******************************************************************************/

/*******************************************************************************
 * GetNtHeaders - Get PE NT Headers from Module
 *                Получение PE NT заголовков из модуля
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Retrieves the IMAGE_NT_HEADERS structure from a loaded module. This structure
 * contains crucial metadata including the import directory location.
 * 
 * Извлекает структуру IMAGE_NT_HEADERS из загруженного модуля. Эта структура
 * содержит критически важные метаданные включая расположение директории импорта.
 * 
 * PARAMETERS / ПАРАМЕТРЫ:
 * base - Base address of module (HMODULE cast to BYTE*)
 *        Базовый адрес модуля (HMODULE приведённый к BYTE*)
 * 
 * RETURNS / ВОЗВРАЩАЕТ:
 * Pointer to IMAGE_NT_HEADERS on success / Указатель на IMAGE_NT_HEADERS при успехе
 * NULL on failure (invalid PE format) / NULL при неудаче (неверный формат PE)
 * 
 * PE FILE STRUCTURE / СТРУКТУРА PE ФАЙЛА:
 * DOS Header (e_magic = "MZ") > points to NT Headers via e_lfanew
 *   DOS заголовок (e_magic = "MZ") > указывает на NT заголовки через e_lfanew
 * NT Headers (Signature = "PE\0\0") > contains Optional Header
 *   NT заголовки (Signature = "PE\0\0") > содержит Optional Header
 * Optional Header > contains Data Directories including Import Directory
 *   Optional Header > содержит Data Directories включая Import Directory
 ******************************************************************************/
static IMAGE_NT_HEADERS* GetNtHeaders(BYTE* base) {
    // Get DOS header (first structure in PE file)
    // Получить DOS заголовок (первая структура в PE файле)
    IMAGE_DOS_HEADER* dos = (IMAGE_DOS_HEADER*)base;
    
    // Validate DOS signature ("MZ")
    // Проверить DOS сигнатуру ("MZ")
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) return NULL;
    
    // Get NT headers using offset from DOS header
    // Получить NT заголовки используя смещение из DOS заголовка
    IMAGE_NT_HEADERS* nt = (IMAGE_NT_HEADERS*)(base + dos->e_lfanew);
    
    // Validate NT signature ("PE\0\0")
    // Проверить NT сигнатуру ("PE\0\0")
    return (nt->Signature == IMAGE_NT_SIGNATURE) ? nt : NULL;
}

/*******************************************************************************
 * IAT_PatchByName - Patch IAT Entry by Function Name
 *                   Патчинг IAT записи по имени функции
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Patches a specific function in a module's IAT by searching for it by name.
 * This is the primary hooking mechanism used by the module.
 * 
 * Патчит конкретную функцию в IAT модуля путём поиска по имени.
 * Это основной механизм перехвата, используемый модулем.
 * 
 * PARAMETERS / ПАРАМЕТРЫ:
 * hMod  - Module to patch / Модуль для патчинга
 * dll   - DLL name (e.g., "KERNEL32.dll") / Имя DLL (например, "KERNEL32.dll")
 * func  - Function name (e.g., "CreateFileA") / Имя функции (например, "CreateFileA")
 * hook  - Pointer to hook function / Указатель на функцию-перехватчик
 * pOrig - [out] Receives pointer to original function (optional)
 *         [out] Получает указатель на оригинальную функцию (опционально)
 * 
 * RETURNS / ВОЗВРАЩАЕТ:
 * TRUE if patch successful / TRUE если патч успешен
 * FALSE otherwise / FALSE в противном случае
 * 
 * ALGORITHM / АЛГОРИТМ:
 * 1. Get NT headers and locate Import Directory
 *    Получить NT заголовки и найти Import Directory
 * 2. Iterate through Import Descriptors to find matching DLL
 *    Перебрать Import Descriptors для поиска подходящей DLL
 * 3. Iterate through function thunks to find matching function name
 *    Перебрать function thunks для поиска подходящего имени функции
 * 4. Change memory protection to PAGE_READWRITE
 *    Изменить защиту памяти на PAGE_READWRITE
 * 5. Replace function pointer with hook
 *    Заменить указатель функции на перехватчик
 * 6. Restore original memory protection
 *    Восстановить оригинальную защиту памяти
 * 
 * THREAD SAFETY / ПОТОКОБЕЗОПАСНОСТЬ:
 * This function is NOT thread-safe. The calling code should ensure proper
 * synchronization when patching from multiple threads.
 * 
 * Эта функция НЕ потокобезопасна. Вызывающий код должен обеспечить правильную
 * синхронизацию при патчинге из нескольких потоков.
 ******************************************************************************/extern "C" BOOL IAT_PatchByName(HMODULE hMod, const char* dll, const char* func, 
                                void* hook, void** pOrig) {
    if (!hMod || !dll || !func || !hook) return FALSE;  // Validate parameters / Проверка параметров
    
    BYTE* base = (BYTE*)hMod;  // Module base address / Базовый адрес модуля
    IMAGE_NT_HEADERS* nt = GetNtHeaders(base);
    if (!nt) return FALSE;  // Invalid PE format / Неверный формат PE

    // Get Import Directory from Data Directory
    // Получить Import Directory из Data Directory
    IMAGE_DATA_DIRECTORY imp = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    if (!imp.VirtualAddress) return FALSE;  // No imports / Нет импортов

    // Iterate through Import Descriptors (one per imported DLL)
    // Перебрать Import Descriptors (один на каждую импортированную DLL)
    for (IMAGE_IMPORT_DESCRIPTOR* d = (IMAGE_IMPORT_DESCRIPTOR*)(base + imp.VirtualAddress); 
         d->Name;  // Name == 0 marks end of array / Name == 0 отмечает конец массива
         ++d) {
        
        // Check if this descriptor is for the DLL we're looking for
        // Проверить является ли этот дескриптор для DLL, которую мы ищем
        if (lstrcmpiA((char*)(base + d->Name), dll) != 0) 
            continue;  // Not the DLL we want / Не та DLL которая нужна
        
        // Get thunk arrays
        // Получить массивы thunk
        // OriginalFirstThunk - points to IMAGE_IMPORT_BY_NAME structures
        // OriginalFirstThunk - указывает на структуры IMAGE_IMPORT_BY_NAME
        // FirstThunk - points to actual function pointers (IAT)
        // FirstThunk - указывает на реальные указатели функций (IAT)
        IMAGE_THUNK_DATA *oft = (IMAGE_THUNK_DATA*)(base + (d->OriginalFirstThunk ? d->OriginalFirstThunk : d->FirstThunk));
        IMAGE_THUNK_DATA *ft  = (IMAGE_THUNK_DATA*)(base + d->FirstThunk);

        // Iterate through functions in this DLL
        // Перебрать функции в этой DLL
        for (; oft->u1.AddressOfData; ++oft, ++ft) {
            // Skip ordinal imports (imported by number, not name)
            // Пропустить импорты по ординалу (импортированные по номеру, не по имени)
            if (oft->u1.Ordinal & IMAGE_ORDINAL_FLAG) 
                continue;
            
            // Get function name structure
            // Получить структуру имени функции
            IMAGE_IMPORT_BY_NAME* ibn = (IMAGE_IMPORT_BY_NAME*)(base + oft->u1.AddressOfData);
            
            // Check if this is the function we're looking for
            // Проверить является ли это функция, которую мы ищем
            if (lstrcmpiA((char*)ibn->Name, func) == 0) {
                // Save original function pointer if requested
                // Сохранить оригинальный указатель функции если запрошено
                if (pOrig && !*pOrig) 
                    *pOrig = (void*)ft->u1.Function;
                
                // Change memory protection to writable
                // Изменить защиту памяти на доступную для записи
                DWORD old;
                if (VirtualProtect(&ft->u1.Function, sizeof(void*), PAGE_READWRITE, &old)) {
                    // Replace function pointer with hook
                    // Заменить указатель функции на перехватчик
                    ft->u1.Function = (ULONG_PTR)hook;
                    
                    // Restore original protection
                    // Восстановить оригинальную защиту
                    VirtualProtect(&ft->u1.Function, sizeof(void*), old, &old);
                    return TRUE;  // Success! / Успех!
                }
            }
        }
    }
    return FALSE;  // Function not found / Функция не найдена
}

/*******************************************************************************
 * IAT_PatchByAddr - Patch IAT Entry by Function Address
 *                   Патчинг IAT записи по адресу функции
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Patches a specific function in a module's IAT by searching for it by address.
 * This is a fallback method when patching by name fails.
 * 
 * Патчит конкретную функцию в IAT модуля путём поиска по адресу.
 * Это резервный метод когда патчинг по имени не удаётся.
 * 
 * PARAMETERS / ПАРАМЕТРЫ:
 * hMod     - Module to patch / Модуль для патчинга
 * dll      - DLL name (e.g., "KERNEL32.dll") / Имя DLL (например, "KERNEL32.dll")
 * realAddr - Address of original function / Адрес оригинальной функции
 * hook     - Pointer to hook function / Указатель на функцию-перехватчик
 * pOrig    - [out] Receives pointer to original function (optional)
 *            [out] Получает указатель на оригинальную функцию (опционально)
 * 
 * RETURNS / ВОЗВРАЩАЕТ:
 * TRUE if patch successful / TRUE если патч успешен
 * FALSE otherwise / FALSE в противном случае
 * 
 * USE CASE / СЛУЧАЙ ИСПОЛЬЗОВАНИЯ:
 * Some modules import functions by ordinal or the IAT may be already patched
 * by another module. In these cases, we need to search by address instead of name.
 * 
 * Некоторые модули импортируют функции по ординалу или IAT может быть уже
 * пропатчена другим модулем. В этих случаях нам нужно искать по адресу вместо имени.
 ******************************************************************************/
extern "C" BOOL IAT_PatchByAddr(HMODULE hMod, const char* dll, void* realAddr, 
                                void* hook, void** pOrig) {
    if (!hMod || !dll || !realAddr || !hook) return FALSE;  // Validate parameters / Проверка параметров
    
    BYTE* base = (BYTE*)hMod;
    IMAGE_NT_HEADERS* nt = GetNtHeaders(base);
    if (!nt) return FALSE;

    IMAGE_DATA_DIRECTORY imp = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    if (!imp.VirtualAddress) return FALSE;

    // Iterate through Import Descriptors
    // Перебрать Import Descriptors
    for (IMAGE_IMPORT_DESCRIPTOR* d = (IMAGE_IMPORT_DESCRIPTOR*)(base + imp.VirtualAddress); 
         d->Name; 
         ++d) {
        
        // Check if this is the DLL we want
        // Проверить является ли это DLL которая нужна
        if (lstrcmpiA((char*)(base + d->Name), dll) != 0) 
            continue;
        
        // Get FirstThunk (IAT)
        // Получить FirstThunk (IAT)
        IMAGE_THUNK_DATA *ft = (IMAGE_THUNK_DATA*)(base + d->FirstThunk);
        
        // Iterate through function pointers
        // Перебрать указатели функций
        for (; ft->u1.Function; ++ft) {
            // Check if this pointer matches the address we're looking for
            // Проверить совпадает ли этот указатель с адресом который мы ищем
            if ((void*)ft->u1.Function == realAddr) {
                // Change memory protection / Изменить защиту памяти
                DWORD old;
                if (VirtualProtect(&ft->u1.Function, sizeof(void*), PAGE_READWRITE, &old)) {
                    // Save original if requested / Сохранить оригинал если запрошено
                    if (pOrig && !*pOrig) 
                        *pOrig = (void*)ft->u1.Function;
                    
                    // Replace with hook / Заменить на перехватчик
                    ft->u1.Function = (ULONG_PTR)hook;
                    
                    // Restore protection / Восстановить защиту
                    VirtualProtect(&ft->u1.Function, sizeof(void*), old, &old);
                    return TRUE;
                }
            }
        }
    }
    return FALSE;
}

/*******************************************************************************
 * IAT_PatchAllModules - Patch Function in All Loaded Modules
 *                       Патчинг функции во всех загруженных модулях
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Patches a Windows API function in all currently loaded modules (except
 * system DLLs). This ensures our hooks work even for dynamically loaded plugins.
 * 
 * Патчит функцию Windows API во всех текущо загруженных модулях (кроме
 * системных DLL). Это гарантирует что наши перехваты работают даже для
 * динамически загруженных плагинов.
 * 
 * PARAMETERS / ПАРАМЕТРЫ:
 * dll      - DLL name containing function / Имя DLL содержащей функцию
 * func     - Function name to patch / Имя функции для патчинга
 * hook     - Hook function pointer / Указатель функции-перехватчика
 * pOrig    - [out] Original function pointer / [out] Указатель оригинальной функции
 * realAddr - Fallback address for IAT_PatchByAddr / Резервный адрес для IAT_PatchByAddr
 * 
 * PROCESS / ПРОЦЕСС:
 * 1. Take snapshot of all loaded modules using CreateToolhelp32Snapshot
 *    Сделать снимок всех загруженных модулей используя CreateToolhelp32Snapshot
 * 2. Iterate through modules, skipping system DLLs and winamp.exe
 *    Перебрать модули, пропуская системные DLL и winamp.exe
 * 3. Try patching by name first, then by address if name fails
 *    Попробовать патчинг по имени сначала, затем по адресу если имя не удаётся
 * 
 * EXCLUSIONS / ИСКЛЮЧЕНИЯ:
 * - kernel32.dll, user32.dll, gdi32.dll - core Windows DLLs
 * - winamp.exe - main executable (patching it could cause issues)
 * 
 * WHY EXCLUDE SYSTEM DLLS? / ПОЧЕМУ ИСКЛЮЧИТЬ СИСТЕМНЫЕ DLL?:
 * Patching system DLLs is risky and unnecessary. They don't call into in_mp3.dll,
 * so hooking them provides no benefit and could destabilize the system.
 * 
 * Патчинг системных DLL рискован и не нужен. Они не вызывают in_mp3.dll,
 * поэтому их перехват не приносит пользы и может дестабилизировать систему.
 ******************************************************************************/
extern "C" void IAT_PatchAllModules(const char* dll, const char* func, 
                                    void* hook, void** pOrig, void* realAddr) {
    // Take snapshot of all modules in current process
    // Сделать снимок всех модулей в текущем процессе
    HANDLE s = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE|TH32CS_SNAPMODULE32, 
                                       GetCurrentProcessId());
    if (s == INVALID_HANDLE_VALUE) return;
    
    MODULEENTRY32 me = {sizeof(me)};
    
    // Iterate through all modules
    // Перебрать все модули
    if (Module32First(s, &me)) {
        do {
            const char* n = me.szModule;  // Module name / Имя модуля
            
            // Skip system DLLs / Пропустить системные DLL
            if (lstrcmpiA(n, "kernel32.dll") == 0 || 
                lstrcmpiA(n, "user32.dll") == 0 || 
                lstrcmpiA(n, "gdi32.dll") == 0) 
                continue;
            
            // Skip main executable / Пропустить главный исполняемый файл
            if (lstrcmpiA(n, "winamp.exe") == 0) 
                continue;

            // Try patching by name, fallback to address if needed
            // Попробовать патчинг по имени, вернуться к адресу если нужно
            if (!IAT_PatchByName(me.hModule, dll, func, hook, pOrig) && realAddr)
                IAT_PatchByAddr(me.hModule, dll, realAddr, hook, pOrig);
                
        } while (Module32Next(s, &me));
    }
    
    CloseHandle(s);  // Clean up snapshot handle / Очистить дескриптор снимка
}

/*******************************************************************************
 * ID3 TAG DECODING FUNCTIONS / ФУНКЦИИ ДЕКОДИРОВАНИЯ ID3-ТЕГОВ
 * 
 * BACKGROUND / ПРЕДЫСТОРИЯ:
 * ID3v2 tags support multiple text encodings specified by an encoding byte:
 * 0 - ISO-8859-1 (Latin-1) or system ANSI codepage
 * 1 - UTF-16 with BOM (byte order mark)
 * 2 - UTF-16BE (big-endian, no BOM) - rare
 * 3 - UTF-8
 * 
 * ID3v2 теги поддерживают множественные текстовые кодировки, указанные байтом кодировки:
 * 0 - ISO-8859-1 (Latin-1) или системная ANSI кодовая страница
 * 1 - UTF-16 с BOM (маркером порядка байтов)
 * 2 - UTF-16BE (big-endian, без BOM) - редко
 * 3 - UTF-8
 ******************************************************************************/

/*******************************************************************************
 * DecodeText - Decode ID3 Text Field to Wide String
 *              Декодирование текстового поля ID3 в широкую строку
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Decodes a text field from an ID3 tag into a wide string based on the
 * encoding byte. This is the core text decoding function used by all
 * ID3 frame parsers.
 * 
 * Декодирует текстовое поле из ID3-тега в широкую строку на основе
 * байта кодировки. Это основная функция декодирования текста, используемая
 * всеми парсерами ID3-фреймов.
 * 
 * PARAMETERS / ПАРАМЕТРЫ:
 * enc - Encoding byte (0=ANSI, 1/2=UTF-16, 3=UTF-8)
 *       Байт кодировки (0=ANSI, 1/2=UTF-16, 3=UTF-8)
 * p   - Pointer to text data / Указатель на текстовые данные
 * n   - Length of text data in bytes / Длина текстовых данных в байтах
 * out - Output wide string buffer / Буфер выходной широкой строки
 * max - Maximum wide characters in output / Максимум широких символов в выводе
 * 
 * ENCODING HANDLING / ОБРАБОТКА КОДИРОВОК:
 * 
 * For enc==0 (ANSI/ISO-8859-1):
 *   Direct conversion using CP_ACP (system ANSI codepage)
 *   Прямое преобразование используя CP_ACP (системная ANSI кодовая страница)
 * 
 * For enc==3 (UTF-8):
 *   Uses DECRYPT_Utf8ToWide for robust UTF-8 decoding
 *   Использует DECRYPT_Utf8ToWide для надёжного декодирования UTF-8
 * 
 * For enc==1/2 (UTF-16):
 *   Checks for BOM (0xFF 0xFE) and skips it if present
 *   Copies remaining bytes as wide characters (assuming little-endian)
 *   Проверяет BOM (0xFF 0xFE) и пропускает его если присутствует
 *   Копирует оставшиеся байты как широкие символы (предполагая little-endian)
 ******************************************************************************/
static void DecodeText(BYTE enc, const BYTE* p, int n, WCHAR* out, int max) {
    if (!out || max <= 0) return;  // Validate output buffer / Проверка выходного буфера
    out[0] = 0;  // Initialize to empty / Инициализировать пустой строкой
    if (!p || n <= 0) return;  // Validate input / Проверка ввода

    if (enc == 0 || enc == 3) {
        // ANSI or UTF-8 encoding / Кодировка ANSI или UTF-8
        char tmp[4096];  // Temporary ANSI buffer / Временный ANSI буфер
        int c = (n < 4095) ? n : 4095;  // Limit to buffer size / Ограничить размером буфера
        CopyMemory(tmp, p, c); 
        tmp[c] = 0;  // Null terminate / Null-терминатор
        
        if (enc == 0) 
            // Convert from ANSI codepage / Преобразовать из ANSI кодовой страницы
            MultiByteToWideChar(CP_ACP, 0, tmp, -1, out, max);
        else 
            // Convert from UTF-8 / Преобразовать из UTF-8
            DECRYPT_Utf8ToWide(tmp, out, max);
    } else {
        // UTF-16 encoding (enc == 1 or 2) / Кодировка UTF-16 (enc == 1 или 2)
        
        // Check for and skip BOM (0xFF 0xFE = little-endian)
        // Проверить и пропустить BOM (0xFF 0xFE = little-endian)
        int skip = (n >= 2 && p[0] == 0xFF && p[1] == 0xFE) ? 2 : 0;
        
        // Calculate number of wide characters
        // Вычислить количество широких символов
        int wc = (n - skip) / 2;
        if (wc >= max) wc = max - 1;  // Limit to buffer / Ограничить буфером
        
        // Direct copy as wide characters / Прямое копирование как широкие символы
        memcpy(out, p + skip, wc * 2);
        out[wc] = 0;  // Null terminate / Null-терминатор
    }
}

/*******************************************************************************
 * ID3_DecodeTextToWide - Public Text Decoding Function
 *                        Публичная функция декодирования текста
 * 
 * Simple wrapper around DecodeText for external use.
 * Простая обёртка вокруг DecodeText для внешнего использования.
 ******************************************************************************/
void ID3_DecodeTextToWide(BYTE enc, const BYTE* p, int cb, WCHAR* out, int cchOut) {
    DecodeText(enc, p, cb, out, cchOut);
}

/*******************************************************************************
 * ID3_DecodeCOMM_PayloadToWide - Decode COMM Frame Comment Text
 *                                 Декодирование текста комментария фрейма COMM
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Decodes the comment text from a COMM (Comment) frame payload. COMM frames
 * have a special structure that includes encoding, language, description, and
 * the actual comment text.
 * 
 * Декодирует текст комментария из payload фрейма COMM (комментарий). COMM
 * фреймы имеют специальную структуру, которая включает кодировку, язык,
 * описание и собственно текст комментария.
 * 
 * COMM FRAME STRUCTURE / СТРУКТУРА ФРЕЙМА COMM:
 * Byte 0:     Text encoding (0/1/2/3) / Кодировка текста (0/1/2/3)
 * Bytes 1-3:  Language code (e.g., "eng", "rus") / Код языка (например, "eng", "rus")
 * Bytes 4+:   Content descriptor (null-terminated) / Дескриптор содержимого (с null-терминатором)
 * Remaining:  Actual comment text / Собственно текст комментария
 * 
 * PARAMETERS / ПАРАМЕТРЫ:
 * pay    - Pointer to COMM frame payload / Указатель на payload фрейма COMM
 * sz     - Size of payload in bytes / Размер payload в байтах
 * out    - Output wide string buffer / Буфер выходной широкой строки
 * cchOut - Size of output buffer / Размер выходного буфера
 * 
 * ALGORITHM / АЛГОРИТМ:
 * 1. Extract encoding byte (byte 0)
 *    Извлечь байт кодировки (байт 0)
 * 2. Skip encoding byte + 3 language bytes
 *    Пропустить байт кодировки + 3 байта языка
 * 3. Skip content descriptor (find null terminator)
 *    Пропустить дескриптор содержимого (найти null-терминатор)
 *    - For enc 0/3: skip single null byte
 *      Для enc 0/3: пропустить один null байт
 *    - For enc 1/2: skip double null bytes (wide null)
 *      Для enc 1/2: пропустить двойные null байты (широкий null)
 * 4. Decode remaining text using DecodeText
 *    Декодировать оставшийся текст используя DecodeText
 * 
 * EXAMPLE / ПРИМЕР:
 * Input bytes: [0x00, 'e','n','g', 0x00, 'M','y',' ','c','o','m','m','e','n','t']
 * Encoding: 0 (ANSI)
 * Language: "eng"
 * Description: "" (empty)
 * Comment: "My comment"
 ******************************************************************************/
void ID3_DecodeCOMM_PayloadToWide(const BYTE* pay, int sz, WCHAR* out, int cchOut) {
    if (!out || cchOut <= 0) return;  // Validate output / Проверка вывода
    out[0] = 0;  // Initialize / Инициализировать
    if (!pay || sz <= 4) return;  // Need at least encoding + language + 1 byte / Нужно минимум кодировка + язык + 1 байт

    BYTE enc = pay[0];  // Get encoding / Получить кодировку
    int skip = 1 + 3;   // Skip encoding byte + 3 language bytes / Пропустить байт кодировки + 3 байта языка

    // Skip content descriptor (find null terminator)
    // Пропустить дескриптор содержимого (найти null-терминатор)
    if (enc == 0 || enc == 3) {
        // ANSI/UTF-8: skip until single null byte
        // ANSI/UTF-8: пропустить до одного null байта
        while (skip < sz && pay[skip]) skip++;
    } else {
        // UTF-16: skip until double null bytes
        // UTF-16: пропустить до двойных null байтов
        while (skip < sz - 1 && (pay[skip] || pay[skip + 1])) skip += 2;
    }

    // Skip the null terminator itself
    // Пропустить сам null-терминатор
    skip += (enc == 0 || enc == 3) ? 1 : 2;
    
    // Sanity check / Проверка разумности
    if (skip < 0) skip = 0;
    if (skip > sz) skip = sz;

    // Decode the remaining text as comment
    // Декодировать оставшийся текст как комментарий
    DecodeText(enc, pay + skip, sz - skip, out, cchOut);
}

/*******************************************************************************
 * ExtractFrame - Extract and Decode Specific ID3 Frame
 *                Извлечение и декодирование конкретного ID3-фрейма
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Extracts and decodes a specific frame from an ID3 tag. Handles both ID3v2.2
 * (3-char IDs) and ID3v2.3/2.4 (4-char IDs).
 * 
 * Извлекает и декодирует конкретный фрейм из ID3-тега. Обрабатывает как ID3v2.2
 * (3-символьные ID), так и ID3v2.3/2.4 (4-символьные ID).
 * 
 * PARAMETERS / ПАРАМЕТРЫ:
 * p    - Pointer to frame start / Указатель на начало фрейма
 * ver  - ID3 version (2, 3, or 4) / Версия ID3 (2, 3 или 4)
 * id3  - 3-character frame ID (for ID3v2.2) / 3-символьный ID фрейма (для ID3v2.2)
 * id4  - 4-character frame ID (for ID3v2.3/2.4) / 4-символьный ID фрейма (для ID3v2.3/2.4)
 * out  - Output wide string buffer / Буфер выходной широкой строки
 * max  - Size of output buffer / Размер выходного буфера
 * 
 * RETURNS / ВОЗВРАЩАЕТ:
 * TRUE if frame found and decoded / TRUE если фрейм найден и декодирован
 * FALSE otherwise / FALSE в противном случае
 * 
 * FRAME HEADER STRUCTURE / СТРУКТУРА ЗАГОЛОВКА ФРЕЙМА:
 * 
 * ID3v2.2 (6 bytes):
 *   Bytes 0-2: Frame ID (e.g., "TT2" = Title) / ID фрейма (например, "TT2" = Название)
 *   Bytes 3-5: Frame size (24-bit big-endian) / Размер фрейма (24-бит big-endian)
 * 
 * ID3v2.3/2.4 (10 bytes):
 *   Bytes 0-3: Frame ID (e.g., "TIT2" = Title) / ID фрейма (например, "TIT2" = Название)
 *   Bytes 4-7: Frame size (32-bit, syncsafe in v2.4) / Размер фрейма (32-бит, syncsafe в v2.4)
 *   Bytes 8-9: Frame flags / Флаги фрейма
 * 
 * FRAME TYPES / ТИПЫ ФРЕЙМОВ:
 * 
 * Text frames (T*): TIT2, TPE1, TALB, etc.
 *   Structure: [encoding byte] [text data]
 *   Структура: [байт кодировки] [текстовые данные]
 * 
 * Comment frames (COMM):
 *   Structure: [encoding] [language] [descriptor] [text]
 *   Структура: [кодировка] [язык] [дескриптор] [текст]
 *   (handled by special decoder / обрабатывается специальным декодером)
 ******************************************************************************/
static BOOL ExtractFrame(BYTE* p, int ver, const char* id3, const char* id4, 
                        WCHAR* out, int max) {
    // Calculate frame header dimensions / Вычислить размеры заголовка фрейма
    int idLen = (ver == 2) ? 3 : 4;      // ID length / Длина ID
    int szLen = (ver == 2) ? 3 : 4;      // Size field length / Длина поля размера
    int headSz = idLen + szLen + (ver == 2 ? 0 : 2);  // Total header size / Общий размер заголовка
    
    // Check frame ID match / Проверить совпадение ID фрейма
    if (ver == 2) { 
        if (memcmp(p, id3, 3) != 0) return FALSE;  // Not our frame / Не наш фрейм
    } else { 
        if (memcmp(p, id4, 4) != 0) return FALSE;  // Not our frame / Не наш фрейм
    }

    // Extract frame size / Извлечь размер фрейма
    DWORD sz = 0;
    if (ver == 2) 
        // ID3v2.2: 24-bit big-endian / ID3v2.2: 24-бит big-endian
        sz = (p[3]<<16) | (p[4]<<8) | p[5];
    else if (ver == 4) 
        // ID3v2.4: 32-bit syncsafe / ID3v2.4: 32-бит syncsafe
        sz = DECRYPT_SyncsafeToSize(p + 4);
    else 
        // ID3v2.3: 32-bit big-endian / ID3v2.3: 32-бит big-endian
        sz = DECRYPT_BE32Read(p + 4);

    if (sz == 0) return FALSE;  // Empty frame / Пустой фрейм
    
    BYTE* pay = p + headSz;  // Point to frame payload / Указать на payload фрейма
    
    // Decode based on frame type / Декодировать на основе типа фрейма
    if (id4[0] == 'C') {
        // Comment frame (COMM) - special handling
        // Фрейм комментария (COMM) - специальная обработка
        BYTE enc = pay[0];  // Encoding byte / Байт кодировки
        int skip = 4;  // Skip encoding + 3 language bytes / Пропустить кодировку + 3 байта языка
        
        // Skip content descriptor / Пропустить дескриптор содержимого
        if (enc == 0 || enc == 3) 
            // ANSI/UTF-8: skip to null / ANSI/UTF-8: пропустить до null
            while (skip < (int)sz && pay[skip]) skip++;
        else 
            // UTF-16: skip to double null / UTF-16: пропустить до двойного null
            while (skip < (int)sz - 1 && (pay[skip] || pay[skip+1])) skip += 2;
        
        skip += (enc == 0 || enc == 3) ? 1 : 2;  // Skip null terminator / Пропустить null-терминатор
        DecodeText(enc, pay + skip, sz - skip, out, max);  // Decode comment / Декодировать комментарий
    } 
    else if (id4[0] == 'T') {
        // Text frame (T*) - encoding byte + text
        // Текстовый фрейм (T*) - байт кодировки + текст
        DecodeText(pay[0], pay + 1, sz - 1, out, max);
    }
    
    return TRUE;  // Successfully extracted / Успешно извлечено
}

/*******************************************************************************
 * ID3_ReadAllFieldsW - Read All ID3 Tag Fields
 *                      Чтение всех полей ID3-тега
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Reads all major text fields from an ID3v2 tag into wide string buffers.
 * This is the main function for extracting tag information from MP3 files.
 * 
 * Читает все основные текстовые поля из ID3v2 тега в буферы широких строк.
 * Это основная функция для извлечения информации о тегах из MP3-файлов.
 * 
 * PARAMETERS / ПАРАМЕТРЫ:
 * hf           - File handle (positioned at start) / Дескриптор файла (в начале)
 * wArtist      - Output buffer for artist / Выходной буфер для исполнителя
 * cchA         - Size of artist buffer / Размер буфера исполнителя
 * wTitle       - Output buffer for title / Выходной буфер для названия
 * cchT         - Size of title buffer / Размер буфера названия
 * wAlbum       - Output buffer for album / Выходной буфер для альбома
 * cchAlb       - Size of album buffer / Размер буфера альбома
 * wYear        - Output buffer for year / Выходной буфер для года
 * cchY         - Size of year buffer / Размер буфера года
 * wGenre       - Output buffer for genre / Выходной буфер для жанра
 * cchG         - Size of genre buffer / Размер буфера жанра
 * wTrack       - Output buffer for track number / Выходной буфер для номера трека
 * cchTrk       - Size of track buffer / Размер буфера трека
 * wComposer    - Output buffer for composer / Выходной буфер для композитора
 * cchC         - Size of composer buffer / Размер буфера композитора
 * wComment     - Output buffer for comment / Выходной буфер для комментария
 * cchM         - Size of comment buffer / Размер буфера комментария
 * wO           - Output buffer for original artist / Выходной буфер для оригинального исполнителя
 * cchO         - Size of original artist buffer / Размер буфера оригинального исполнителя
 * wP           - Output buffer for copyright / Выходной буфер для авторских прав
 * cchP         - Size of copyright buffer / Размер буфера авторских прав
 * wU           - Unused parameter / Неиспользуемый параметр
 * cchU         - Unused parameter / Неиспользуемый параметр
 * wE           - Output buffer for encoded by / Выходной буфер для закодировано
 * cchE         - Size of encoded by buffer / Размер буфера закодировано
 * pOldTagSize  - [out] Receives size of ID3 tag (optional) / [out] Получает размер ID3-тега (опционально)
 * 
 * RETURNS / ВОЗВРАЩАЕТ:
 * TRUE if tag found and read successfully / TRUE если тег найден и прочитан успешно
 * FALSE if no ID3v2 tag present / FALSE если ID3v2 тег отсутствует
 * 
 * FIELD MAPPING / ОТОБРАЖЕНИЕ ПОЛЕЙ:
 * ID3v2.2 > ID3v2.3/2.4:
 * TT2 > TIT2 (Title / Название)
 * TP1 > TPE1 (Artist / Исполнитель)
 * TAL > TALB (Album / Альбом)
 * TYE > TYER (Year / Год)
 * TCO > TCON (Genre / Жанр)
 * TRK > TRCK (Track / Трек)
 * TCM > TCOM (Composer / Композитор)
 * COM > COMM (Comment / Комментарий)
 * TOA > TOPE (Original artist / Оригинальный исполнитель)
 * TCR > TCOP (Copyright / Авторские права)
 * TEN > TENC (Encoded by / Закодировано)
 * 
 * ALGORITHM / АЛГОРИТМ:
 * 1. Read and validate 10-byte ID3v2 header ("ID3" signature)
 *    Прочитать и проверить 10-байтовый заголовок ID3v2 (сигнатура "ID3")
 * 2. Extract version and tag size from header
 *    Извлечь версию и размер тега из заголовка
 * 3. Allocate buffer and read entire tag into memory
 *    Выделить буфер и прочитать весь тег в память
 * 4. Skip extended header if present (v2.3/2.4)
 *    Пропустить расширенный заголовок если присутствует (v2.3/2.4)
 * 5. Iterate through frames, extracting matching fields
 *    Перебрать фреймы, извлекая совпадающие поля
 * 6. Free buffer and return success
 *    Освободить буфер и вернуть успех
 * 
 * EXTENDED HEADER HANDLING / ОБРАБОТКА РАСШИРЕННОГО ЗАГОЛОВКА:
 * ID3v2.3 and v2.4 support optional extended headers (flag 0x40).
 * These contain additional metadata and must be skipped when parsing frames.
 * 
 * ID3v2.3 и v2.4 поддерживают опциональные расширенные заголовки (флаг 0x40).
 * Они содержат дополнительные метаданные и должны быть пропущены при парсинге фреймов.
 ******************************************************************************/
extern "C" BOOL ID3_ReadAllFieldsW(HANDLE hf,
    WCHAR* wArtist, int cchA, WCHAR* wTitle, int cchT, WCHAR* wAlbum, int cchAlb,
    WCHAR* wYear, int cchY, WCHAR* wGenre, int cchG, WCHAR* wTrack, int cchTrk,
    WCHAR* wComposer, int cchC, WCHAR* wComment, int cchM, WCHAR* wO, int cchO,
    WCHAR* wP, int cchP, WCHAR* wU, int cchU, WCHAR* wE, int cchE,
    DWORD* pOldTagSize)
{
    if (pOldTagSize) *pOldTagSize = 0;  // Initialize output / Инициализировать вывод
    
    // Read 10-byte ID3v2 header / Прочитать 10-байтовый заголовок ID3v2
    BYTE hdr[10]; 
    DWORD rd;
    SetFilePointer(hf, 0, 0, FILE_BEGIN);  // Seek to start / Перейти к началу
    
    // Validate header: must be exactly 10 bytes and start with "ID3"
    // Проверить заголовок: должен быть ровно 10 байт и начинаться с "ID3"
    if (!ReadFile(hf, hdr, 10, &rd, 0) || rd != 10 || memcmp(hdr, "ID3", 3)) 
        return FALSE;  // Not a valid ID3v2 tag / Не валидный ID3v2 тег

    int ver = hdr[3];  // Version number (2, 3, or 4) / Номер версии (2, 3 или 4)
    DWORD tagSz = DECRYPT_SyncsafeToSize(hdr + 6);  // Tag size (excluding header) / Размер тега (исключая заголовок)
    if (pOldTagSize) *pOldTagSize = tagSz + 10;  // Total size with header / Общий размер с заголовком

    // Allocate buffer for entire tag / Выделить буфер для всего тега
    BYTE* buf = (BYTE*)HeapAlloc(GetProcessHeap(), 0, tagSz);
    if (!buf) return FALSE;  // Allocation failed / Выделение не удалось
    
    // Read entire tag into memory / Прочитать весь тег в память
    if (ReadFile(hf, buf, tagSz, &rd, 0) && rd == tagSz) {
        BYTE* p = buf;  // Current position in tag / Текущая позиция в теге
        int rem = tagSz;  // Remaining bytes / Оставшиеся байты
        
        // Skip extended header if present (ID3v2.3/2.4 only)
        // Пропустить расширенный заголовок если присутствует (только ID3v2.3/2.4)
        if (ver >= 3 && (hdr[5] & 0x40)) {
            // Extended header flag is set / Установлен флаг расширенного заголовка
            DWORD extSz = (ver == 3) ? DECRYPT_BE32Read(p) : DECRYPT_SyncsafeToSize(p);
            p += extSz + (ver == 3 ? 4 : 0);  // Skip header / Пропустить заголовок
            rem -= extSz + (ver == 3 ? 4 : 0);
        }

        // Frame mapping table / Таблица отображения фреймов
        // Maps output buffers to ID3v2.2 and ID3v2.3/2.4 frame IDs
        // Отображает выходные буферы на ID3v2.2 и ID3v2.3/2.4 ID фреймов
        while (rem > 10) {
            if (p[0] == 0) break;  // Padding or end of tag / Заполнение или конец тега
            
            struct { 
                WCHAR* buf;      // Output buffer / Выходной буфер
                int sz;          // Buffer size / Размер буфера
                const char* i3;  // ID3v2.2 frame ID (3 chars) / ID фрейма ID3v2.2 (3 символа)
                const char* i4;  // ID3v2.3/2.4 frame ID (4 chars) / ID фрейма ID3v2.3/2.4 (4 символа)
            } map[] = {
                {wTitle, cchT, "TT2", "TIT2"},       // Title / Название
                {wArtist, cchA, "TP1", "TPE1"},      // Artist / Исполнитель
                {wAlbum, cchAlb, "TAL", "TALB"},     // Album / Альбом
                {wYear, cchY, "TYE", "TYER"},        // Year / Год
                {wGenre, cchG, "TCO", "TCON"},       // Genre / Жанр
                {wTrack, cchTrk, "TRK", "TRCK"},     // Track / Трек
                {wComment, cchM, "COM", "COMM"},     // Comment / Комментарий
                {wComposer, cchC, "TCM", "TCOM"},    // Composer / Композитор
                {wO, cchO, "TOA", "TOPE"},           // Original artist / Оригинальный исполнитель
                {wP, cchP, "TCR", "TCOP"},           // Copyright / Авторские права
                {wE, cchE, "TEN", "TENC"}            // Encoded by / Закодировано
            };
            
            // Try to extract each mapped field if not already filled
            // Попытаться извлечь каждое отображённое поле если ещё не заполнено
            for (int i = 0; i < ARRAYSIZE(map); i++) {
                if (map[i].buf && map[i].buf[0] == 0)  // Buffer exists and empty / Буфер существует и пуст
                    ExtractFrame(p, ver, map[i].i3, map[i].i4, map[i].buf, map[i].sz);
            }
            
            // Calculate frame size and skip to next frame
            // Вычислить размер фрейма и перейти к следующему фрейму
            int fsz = 0;
            if (ver == 2) 
                fsz = (p[3]<<16) | (p[4]<<8) | p[5];  // 24-bit size / 24-бит размер
            else if (ver == 4) 
                fsz = DECRYPT_SyncsafeToSize(p+4);    // Syncsafe size / Syncsafe размер
            else 
                fsz = DECRYPT_BE32Read(p+4);          // 32-bit size / 32-бит размер
            
            int inc = fsz + (ver == 2 ? 6 : 10);  // Frame size + header / Размер фрейма + заголовок
            p += inc;    // Move to next frame / Перейти к следующему фрейму
            rem -= inc;  // Update remaining bytes / Обновить оставшиеся байты
        }
    }
    
    HeapFree(GetProcessHeap(), 0, buf);  // Free tag buffer / Освободить буфер тега
    return TRUE;  // Success! / Успех!
}

/*******************************************************************************
 * ID3_ReadFieldA - Read Single ID3 Field to ANSI String
 *                  Чтение одного поля ID3 в ANSI строку
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Convenience function to read a single ID3 field by name and convert it to
 * ANSI string. Useful for simple tag reading without needing multiple buffers.
 * 
 * Функция удобства для чтения одного поля ID3 по имени и преобразования его
 * в ANSI строку. Полезна для простого чтения тегов без необходимости
 * множественных буферов.
 * 
 * PARAMETERS / ПАРАМЕТРЫ:
 * file   - Path to MP3 file / Путь к MP3-файлу
 * key    - Field name ("title", "artist", etc.) / Имя поля ("title", "artist", и т.д.)
 * out    - Output ANSI buffer / Выходной ANSI буфер
 * outlen - Size of output buffer / Размер выходного буфера
 * 
 * RETURNS / ВОЗВРАЩАЕТ:
 * Number of characters written to output buffer
 * Количество символов записанных в выходной буфер
 * 0 on failure / 0 при неудаче
 * 
 * SUPPORTED FIELDS / ПОДДЕРЖИВАЕМЫЕ ПОЛЯ:
 * - "title"  - Song title / Название песни
 * - "artist" - Artist name / Имя исполнителя
 * 
 * PROCESS / ПРОЦЕСС:
 * 1. Open file for reading / Открыть файл для чтения
 * 2. Call ID3_ReadAllFieldsW with wide buffer / Вызвать ID3_ReadAllFieldsW с широким буфером
 * 3. Convert wide string to ANSI / Преобразовать широкую строку в ANSI
 * 4. Close file and return result / Закрыть файл и вернуть результат
 * 
 * LIMITATIONS / ОГРАНИЧЕНИЯ:
 * - Only supports title and artist fields (easily extendable)
 *   Поддерживает только поля title и artist (легко расширяемо)
 * - Opens file each time (inefficient for multiple fields)
 *   Открывает файл каждый раз (неэффективно для множественных полей)
 * 
 * NOTE / ПРИМЕЧАНИЕ:
 * For reading multiple fields, use ID3_ReadAllFieldsW directly for better
 * performance (single file open, single tag parse).
 * 
 * Для чтения множественных полей используйте ID3_ReadAllFieldsW напрямую
 * для лучшей производительности (одно открытие файла, один парсинг тега).
 ******************************************************************************/
extern "C" int ID3_ReadFieldA(const char* file, const char* key, char* out, int outlen) {
    if (!file || !key || !out) return 0;  // Validate parameters / Проверка параметров
    out[0] = 0;  // Initialize output / Инициализировать вывод
    
    // Open file with shared access / Открыть файл с общим доступом
    HANDLE h = CreateFileA(file, GENERIC_READ, FILE_SHARE_READ|FILE_SHARE_WRITE, 
                          0, OPEN_EXISTING, 0, 0);
    if (h == INVALID_HANDLE_VALUE) return 0;  // Failed to open / Не удалось открыть

    WCHAR wBuf[1024] = {0};  // Wide string buffer / Буфер широкой строки
    int wLen = 1024;
    BOOL found = FALSE;

    // Read specific field based on key / Прочитать конкретное поле на основе ключа
    if (lstrcmpiA(key, "title") == 0)
        // Read title field / Прочитать поле названия
        found = ID3_ReadAllFieldsW(h, 0,0, wBuf, wLen, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, NULL);
    else if (lstrcmpiA(key, "artist") == 0)
        // Read artist field / Прочитать поле исполнителя
        found = ID3_ReadAllFieldsW(h, wBuf, wLen, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, NULL);
    
    CloseHandle(h);  // Always close handle / Всегда закрывать дескриптор
    
    // Convert wide string to ANSI if found
    // Преобразовать широкую строку в ANSI если найдено
    if (found && wBuf[0]) 
        return DECRYPT_WideToACP(wBuf, out, outlen);
    
    return 0;  // Field not found / Поле не найдено
}
