/*******************************************************************************
 * unicode_decrypt_engine.h
 * * HEADER FOR UNICODE DECRYPTION ENGINE
 * ЗАГОЛОВОЧНЫЙ ФАЙЛ ДЛЯ ДВИЖКА РАСШИФРОВКИ UNICODE
 * * PURPOSE / НАЗНАЧЕНИЕ:
 * Declarations for shared Unicode utilities, encoding converters, and ID3 parsing
 * functions. Used to fix encoding issues in Winamp plugins.
 * * Объявления общих утилит Unicode, конвертеров кодировок и функций парсинга ID3.
 * Используется для исправления проблем с кодировкой в плагинах Winamp.
 ******************************************************************************/

#ifndef DECRYPT_H
#define DECRYPT_H

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#ifdef __cplusplus
extern "C" {
#endif

/*******************************************************************************
 * ENCODING CONVERSION FUNCTIONS
 * ФУНКЦИИ ПРЕОБРАЗОВАНИЯ КОДИРОВОК
 ******************************************************************************/

/*******************************************************************************
 * DECRYPT_ToACP_Best
 * * PURPOSE / НАЗНАЧЕНИЕ:
 * Intelligently detects string encoding (UTF-8, Windows-1251) and converts it
 * to the system ANSI code page (CP_ACP).
 * * Интеллектуально определяет кодировку строки (UTF-8, Windows-1251) и
 * преобразует её в системную кодовую страницу ANSI (CP_ACP).
 * * PARAMETERS / ПАРАМЕТРЫ:
 * src    - Source string / Исходная строка
 * out    - Output buffer / Выходной буфер
 * outlen - Buffer size / Размер буфера
 * * RETURNS / ВОЗВРАЩАЕТ:
 * Number of characters written / Количество записанных символов
 ******************************************************************************/
int DECRYPT_ToACP_Best(const char* src, char* out, int outlen);

/*******************************************************************************
 * DECRYPT_Utf8ToWide
 * * PURPOSE / НАЗНАЧЕНИЕ:
 * Converts a UTF-8 string to a Unicode (UTF-16LE) wide string.
 * Supports manual decoding fallback if Windows API fails.
 * * Преобразует строку UTF-8 в широкую строку Unicode (UTF-16LE).
 * Поддерживает ручное декодирование, если Windows API не справляется.
 * * PARAMETERS / ПАРАМЕТРЫ:
 * utf8   - Input UTF-8 string / Входная строка UTF-8
 * out    - Output wide string buffer / Буфер выходной широкой строки
 * cchOut - Size in wide chars / Размер в широких символах
 ******************************************************************************/
int DECRYPT_Utf8ToWide(const char* utf8, WCHAR* out, int cchOut);

/*******************************************************************************
 * DECRYPT_WideToACP
 * * PURPOSE / НАЗНАЧЕНИЕ:
 * Converts a wide string (UTF-16) to system ANSI code page.
 * * Преобразует широкую строку (UTF-16) в системную кодовую страницу ANSI.
 * * PARAMETERS / ПАРАМЕТРЫ:
 * wide   - Input wide string / Входная широкая строка
 * out    - Output ANSI buffer / Выходной ANSI буфер
 * outlen - Buffer size / Размер буфера
 ******************************************************************************/
int DECRYPT_WideToACP(const WCHAR* wide, char* out, int outlen);

/*******************************************************************************
 * DECRYPT_FixTagA
 * * PURPOSE / НАЗНАЧЕНИЕ:
 * Detects and fixes encoding of a tag buffer in-place.
 * * Обнаруживает и исправляет кодировку буфера тега "на месте".
 * * PARAMETERS / ПАРАМЕТРЫ:
 * key    - Unused (legacy) / Не используется (наследие)
 * buf    - Buffer to fix / Буфер для исправления
 * buflen - Buffer size / Размер буфера
 ******************************************************************************/
void DECRYPT_FixTagA(const char* key, char* buf, int buflen);

/*******************************************************************************
 * ID3 TAG PARSING FUNCTIONS
 * ФУНКЦИИ ПАРСИНГА ID3-ТЕГОВ
 ******************************************************************************/

/*******************************************************************************
 * ID3_ReadFieldA
 * * PURPOSE / НАЗНАЧЕНИЕ:
 * Reads a single ID3 field by key name (e.g., "title") and converts to ANSI.
 * * Читает одно поле ID3 по имени ключа (например, "title") и преобразует в ANSI.
 * * PARAMETERS / ПАРАМЕТРЫ:
 * filename - Path to file / Путь к файлу
 * key      - Field name / Имя поля
 * out      - Output buffer / Выходной буфер
 * outlen   - Buffer size / Размер буфера
 ******************************************************************************/
int ID3_ReadFieldA(const char* filename, const char* key, char* out, int outlen);

/*******************************************************************************
 * ID3_ReadAllFieldsW
 * * PURPOSE / НАЗНАЧЕНИЕ:
 * Reads all supported ID3v2 text fields into wide string buffers.
 * Handles ID3v2.2, v2.3, and v2.4 frames.
 * * Читает все поддерживаемые текстовые поля ID3v2 в буферы широких строк.
 * Обрабатывает фреймы ID3v2.2, v2.3 и v2.4.
 * * PARAMETERS / ПАРАМЕТРЫ:
 * hFile       - Open file handle / Дескриптор открытого файла
 * wArtist...  - Output buffers for fields / Выходные буферы для полей
 * pOldTagSize - [out] Total tag size / [out] Общий размер тега
 ******************************************************************************/
BOOL ID3_ReadAllFieldsW(HANDLE hFile,
    WCHAR* wArtist, int cchA,
    WCHAR* wTitle, int cchT,
    WCHAR* wAlbum, int cchAlb,
    WCHAR* wYear, int cchY,
    WCHAR* wGenre, int cchG,
    WCHAR* wTrack, int cchTrk,
    WCHAR* wComposer, int cchC,
    WCHAR* wComment, int cchM,
    WCHAR* wOrigArt, int cchO,
    WCHAR* wCopy, int cchP,
    WCHAR* wURL, int cchU,
    WCHAR* wEncoded, int cchE,
    DWORD* pOldTagSize);

/*******************************************************************************
 * ID3_DecodeTextToWide
 * * PURPOSE / НАЗНАЧЕНИЕ:
 * Decodes raw ID3 frame data to wide string based on encoding byte.
 * * Декодирует сырые данные фрейма ID3 в широкую строку на основе байта кодировки.
 * * PARAMETERS / ПАРАМЕТРЫ:
 * enc    - Encoding byte (0-3) / Байт кодировки (0-3)
 * p      - Data pointer / Указатель на данные
 * cb     - Data length / Длина данных
 * out    - Output buffer / Выходной буфер
 * cchOut - Buffer size / Размер буфера
 ******************************************************************************/
void ID3_DecodeTextToWide(BYTE enc, const BYTE* p, int cb, WCHAR* out, int cchOut);

/*******************************************************************************
 * ID3_DecodeCOMM_PayloadToWide
 * * PURPOSE / НАЗНАЧЕНИЕ:
 * Decodes a COMM (Comment) frame, handling language and description fields.
 * * Декодирует фрейм COMM (Комментарий), обрабатывая поля языка и описания.
 * * PARAMETERS / ПАРАМЕТРЫ:
 * pay    - Frame payload / Payload фрейма
 * sz     - Payload size / Размер payload
 * out    - Output buffer / Выходной буфер
 * cchOut - Buffer size / Размер буфера
 ******************************************************************************/
void ID3_DecodeCOMM_PayloadToWide(const BYTE* pay, int sz, WCHAR* out, int cchOut);

/*******************************************************************************
 * IAT HOOKING FUNCTIONS
 * ФУНКЦИИ ПЕРЕХВАТА IAT
 ******************************************************************************/

/*******************************************************************************
 * IAT_PatchByName
 * * PURPOSE / НАЗНАЧЕНИЕ:
 * Hooks a function in a specific module by searching its Import Address Table
 * by function name.
 * * Перехватывает функцию в конкретном модуле, ища её в таблице импорта адресов
 * по имени функции.
 * * RETURNS / ВОЗВРАЩАЕТ:
 * TRUE if successful / TRUE в случае успеха
 ******************************************************************************/
BOOL IAT_PatchByName(HMODULE hMod, const char* importDll, const char* funcName,
                     void* hookFunc, void** ppOrigFunc);

/*******************************************************************************
 * IAT_PatchByAddr
 * * PURPOSE / НАЗНАЧЕНИЕ:
 * Hooks a function by searching IAT for a specific function address.
 * Fallback when hooking by name fails.
 * * Перехватывает функцию, ища в IAT конкретный адрес функции.
 * Резервный метод, когда перехват по имени не удается.
 ******************************************************************************/
BOOL IAT_PatchByAddr(HMODULE hMod, const char* importDll, void* realFuncAddr,
                     void* hookFunc, void** ppOrigFunc);

/*******************************************************************************
 * IAT_PatchAllModules
 * * PURPOSE / НАЗНАЧЕНИЕ:
 * Applies a hook to all currently loaded modules in the process.
 * * Применяет перехват ко всем текущо загруженным модулям в процессе.
 ******************************************************************************/
void IAT_PatchAllModules(const char* importDll, const char* funcName,
                         void* hookFunc, void** ppOrigFunc, void* realAddr);

/*******************************************************************************
 * UTILITY FUNCTIONS
 * ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ
 ******************************************************************************/

/*******************************************************************************
 * DECRYPT_IsWin9x
 * * PURPOSE / НАЗНАЧЕНИЕ:
 * Checks if running on Windows 95/98/ME.
 * * Проверяет, запущена ли программа на Windows 95/98/ME.
 ******************************************************************************/
BOOL DECRYPT_IsWin9x(void);

/*******************************************************************************
 * DECRYPT_StrLenA
 * * PURPOSE / НАЗНАЧЕНИЕ:
 * Safe string length check (handles NULL).
 * * Безопасная проверка длины строки (обрабатывает NULL).
 ******************************************************************************/
int DECRYPT_StrLenA(const char* s);

/*******************************************************************************
 * DECRYPT_WideStrContainsI
 * * PURPOSE / НАЗНАЧЕНИЕ:
 * Case-insensitive substring search for wide strings.
 * * Поиск подстроки в широкой строке без учета регистра.
 ******************************************************************************/
BOOL DECRYPT_WideStrContainsI(const WCHAR* str, const WCHAR* sub);

/*******************************************************************************
 * BYTE HELPERS
 * ПОБАЙТОВЫЕ ПОМОЩНИКИ
 ******************************************************************************/

/*******************************************************************************
 * DECRYPT_SyncsafeToSize
 * * PURPOSE / НАЗНАЧЕНИЕ:
 * Converts 28-bit syncsafe integer (ID3v2.4) to 32-bit integer.
 * * Преобразует 28-битное syncsafe целое (ID3v2.4) в 32-битное целое.
 ******************************************************************************/
DWORD DECRYPT_SyncsafeToSize(const BYTE* p);

/*******************************************************************************
 * DECRYPT_SizeToSyncsafe
 * * PURPOSE / НАЗНАЧЕНИЕ:
 * Converts 32-bit integer to 28-bit syncsafe format.
 * * Преобразует 32-битное целое в 28-битный формат syncsafe.
 ******************************************************************************/
DWORD DECRYPT_SizeToSyncsafe(DWORD v);

/*******************************************************************************
 * DECRYPT_BE32Read
 * * PURPOSE / НАЗНАЧЕНИЕ:
 * Reads a 32-bit big-endian integer from memory.
 * * Читает 32-битное big-endian целое из памяти.
 ******************************************************************************/
DWORD DECRYPT_BE32Read(const BYTE* p);

/*******************************************************************************
 * DECRYPT_BE32Write
 * * PURPOSE / НАЗНАЧЕНИЕ:
 * Writes a 32-bit integer to memory in big-endian format.
 * * Записывает 32-битное целое в память в формате big-endian.
 ******************************************************************************/
void DECRYPT_BE32Write(BYTE* p, DWORD v);

#ifdef __cplusplus
}
#endif

#endif