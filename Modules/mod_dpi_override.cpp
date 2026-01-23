// mod_dpi_override.cpp
// EN: Module for setting "System (Enhanced)" DPI scaling compatibility flag
// RU: Модуль для установки флага совместимости "Системное (расширенное)" масштабирование DPI
#include "mod_dpi_override.h"
#include <windows.h>
#include <winreg.h>

#ifndef ARRAYSIZE
#define ARRAYSIZE(x) (sizeof(x)/sizeof((x)[0]))
#endif

// EN: Registry key path for Windows compatibility layers (per-user)
// RU: Путь к ключу реестра для слоёв совместимости Windows (для текущего пользователя)
static const char *kLayersKey =
    "Software\\Microsoft\\Windows NT\\CurrentVersion\\AppCompatFlags\\Layers";

// EN: System (Enhanced) layer string: GDIDPISCALING + DPIUNAWARE
//     Forces Windows to bitmap-scale the entire app window (like on 4K monitors)
// RU: Строка слоя "Системное (расширенное)": GDIDPISCALING + DPIUNAWARE
//     Заставляет Windows растягивать всё окно приложения (полезно на 4K мониторах)
static const char *kSystemEnhancedValue = "~ GDIDPISCALING DPIUNAWARE";

/* ---------------------------------------------------------
   EN: Small string helpers (no CRT/shlwapi dependency)
   RU: Вспомогательные строковые функции (без зависимости от CRT/shlwapi)
   --------------------------------------------------------- */

// EN: Convert character to lowercase (ASCII only)
// RU: Преобразовать символ в нижний регистр (только ASCII)
static char ToLowerA(char c)
{
    if (c >= 'A' && c <= 'Z') return (char)(c - 'A' + 'a');
    return c;
}

// EN: Case-insensitive substring search (like StrStrI but without shlwapi.lib)
// RU: Поиск подстроки без учёта регистра (аналог StrStrI, но без shlwapi.lib)
static const char* StrStrI_A(const char *haystack, const char *needle)
{
    if (!haystack || !needle) return NULL;
    if (!*needle) return haystack; // EN: Empty needle matches anything | RU: Пустая подстрока совпадает с любой строкой
    
    for (const char *h = haystack; *h; ++h)
    {
        const char *h2 = h;
        const char *n2 = needle;
        // EN: Compare characters case-insensitively | RU: Сравниваем символы без учёта регистра
        while (*h2 && *n2 && (ToLowerA(*h2) == ToLowerA(*n2)))
        {
            ++h2;
            ++n2;
        }
        if (!*n2) return h; // EN: Full match found | RU: Полное совпадение найдено
    }
    return NULL;
}

// EN: Get full path to current executable (e.g., "C:\Program Files\Winamp\winamp.exe")
// RU: Получить полный путь к текущему исполняемому файлу (например, "C:\Program Files\Winamp\winamp.exe")
static void GetExePathA(char *outPath, DWORD cchOut)
{
    if (!outPath || cchOut == 0) return;
    outPath[0] = 0;
    GetModuleFileNameA(NULL, outPath, cchOut);
    outPath[cchOut - 1] = 0; // EN: Ensure null-termination | RU: Гарантируем нуль-терминатор
}

// EN: Read registry value for specific EXE path from Layers key
// RU: Прочитать значение реестра для конкретного пути EXE из ключа Layers
static BOOL ReadLayerValueA(HKEY hKey, const char *exePath, char *out, DWORD cchOut, DWORD *outType)
{
    if (!out || cchOut == 0) return FALSE;
    out[0] = 0;
    if (outType) *outType = 0;
    
    DWORD type = 0;
    DWORD cb = cchOut;
    LONG r = RegQueryValueExA(hKey, exePath, NULL, &type, (LPBYTE)out, &cb);
    if (r != ERROR_SUCCESS) return FALSE;
    
    out[cchOut - 1] = 0; // EN: Safety null-termination | RU: Безопасный нуль-терминатор
    if (outType) *outType = type;
    return TRUE;
}

// EN: Check if layer string contains "GDIDPISCALING" AND "DPIUNAWARE"
//     (this is the "System (Enhanced)" compatibility mode)
// RU: Проверить, содержит ли строка слоя "GDIDPISCALING" И "DPIUNAWARE"
//     (это режим совместимости "Системное (расширенное)")
static BOOL ContainsSystemEnhanced(const char *s)
{
    if (!s || !*s) return FALSE;
    return (StrStrI_A(s, "GDIDPISCALING") != NULL) &&
           (StrStrI_A(s, "DPIUNAWARE")   != NULL);
}

// EN: Write "~ GDIDPISCALING DPIUNAWARE" value for specified EXE path
// RU: Записать значение "~ GDIDPISCALING DPIUNAWARE" для указанного пути EXE
static BOOL WriteSystemEnhancedForExe(HKEY hKey, const char *exePath)
{
    const BYTE *data = (const BYTE*)kSystemEnhancedValue;
    DWORD cb = (DWORD)lstrlenA(kSystemEnhancedValue) + 1; // EN: Include null terminator | RU: Включая нуль-терминатор
    LONG r = RegSetValueExA(hKey, exePath, 0, REG_SZ, data, cb);
    return (r == ERROR_SUCCESS);
}

/* ---------------------------------------------------------
   EN: Public API entry point
   RU: Публичная точка входа API
   --------------------------------------------------------- */

// EN: Ensures "System (Enhanced)" DPI scaling is enabled for current EXE
//     - Creates registry entry in HKCU (no admin rights needed)
//     - Windows will bitmap-scale the entire app window automatically
//     - Useful for old apps (like Winamp 2.95) on modern 4K displays
//
// RU: Гарантирует включение "Системного (расширенного)" масштабирования DPI для текущего EXE
//     - Создаёт запись в реестре HKCU (права администратора не нужны)
//     - Windows автоматически растянет всё окно приложения
//     - Полезно для старых программ (вроде Winamp 2.95) на современных 4K мониторах
void DpiOverride_EnsureSystemEnhanced(HWND /*hwndOwner*/)
{
    // EN: 1. Get full path to our EXE
    // RU: 1. Получаем полный путь к нашему EXE
    char exePath[MAX_PATH] = {0};
    GetExePathA(exePath, ARRAYSIZE(exePath));
    if (!exePath[0]) return; // EN: Failed to get path | RU: Не удалось получить путь

    // EN: 2. Open/create Layers registry key (HKCU, no admin needed)
    // RU: 2. Открываем/создаём ключ реестра Layers (HKCU, админ не требуется)
    HKEY hKey = NULL;
    DWORD disp = 0;
    LONG r = RegCreateKeyExA(
        HKEY_CURRENT_USER,
        kLayersKey,
        0,
        NULL,
        REG_OPTION_NON_VOLATILE,
        KEY_QUERY_VALUE | KEY_SET_VALUE,
        NULL,
        &hKey,
        &disp
    );
    if (r != ERROR_SUCCESS || !hKey)
        return; // EN: Failed to access registry | RU: Не удалось получить доступ к реестру

    // EN: 3. Check if "System (Enhanced)" is already set
    // RU: 3. Проверяем, установлено ли уже "Системное (расширенное)"
    char cur[512] = {0};
    DWORD type = 0;
    BOOL has = ReadLayerValueA(hKey, exePath, cur, ARRAYSIZE(cur), &type);
    
    if (has && type == REG_SZ && ContainsSystemEnhanced(cur))
    {
        // EN: Already configured, nothing to do
        // RU: Уже настроено, ничего делать не нужно
        RegCloseKey(hKey);
        return;
    }

    // EN: 4. Write/replace with "~ GDIDPISCALING DPIUNAWARE"
    // RU: 4. Записываем/заменяем на "~ GDIDPISCALING DPIUNAWARE"
    WriteSystemEnhancedForExe(hKey, exePath);
    RegCloseKey(hKey);
    
    // EN: NOTE: App restart required for Windows to apply the new DPI mode
    // RU: ПРИМЕЧАНИЕ: Требуется перезапуск приложения, чтобы Windows применил новый режим DPI
}