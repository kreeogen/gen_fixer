/*******************************************************************************
 * mod_skininstall.cpp
 * 
 * WINAMP SKIN INSTALLATION EXPLORER CONTEXT MENU MODULE
 * Модуль контекстного меню Проводника для установки скинов Winamp
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * This module ensures that Winamp skin files (.wsz, .zip) have a proper
 * "Install skin" context menu entry in Windows Explorer. It modifies Windows
 * Registry settings to register the correct verb and display text for the
 * install action.
 * 
 * Этот модуль гарантирует, что файлы скинов Winamp (.wsz, .zip) имеют
 * правильный пункт контекстного меню "Установить скин" в проводнике Windows.
 * Он изменяет настройки реестра Windows для регистрации правильного глагола
 * и отображаемого текста для действия установки.
 * 
 * HOW IT WORKS / КАК ЭТО РАБОТАЕТ:
 * 1. Runs once during Winamp startup (idempotent - safe to call multiple times)
 * 2. Attempts to write to HKEY_CLASSES_ROOT first (system-wide registration)
 * 3. Falls back to HKEY_CURRENT_USER if system-wide write fails (per-user)
 * 4. Updates both default value and empty-string value for maximum compatibility
 * 5. Only writes if the current value differs from desired value
 * 
 * 1. Выполняется один раз при запуске Winamp (идемпотентно - безопасно вызывать многократно)
 * 2. Сначала пытается записать в HKEY_CLASSES_ROOT (системная регистрация)
 * 3. Переходит на HKEY_CURRENT_USER, если системная запись не удалась (для пользователя)
 * 4. Обновляет как значение по умолчанию, так и значение пустой строки для максимальной совместимости
 * 5. Записывает только если текущее значение отличается от желаемого
 * 
 * REGISTRY STRUCTURE / СТРУКТУРА РЕЕСТРА:
 * The module modifies the following registry paths:
 * Модуль изменяет следующие пути реестра:
 * 
 * Primary (system-wide):
 *   HKEY_CLASSES_ROOT\Winamp.SkinZip\shell\install
 *     (Default) = "Install skin" (localized)
 *     "" = "Install skin" (localized)
 * 
 * Fallback (per-user):
 *   HKEY_CURRENT_USER\Software\Classes\Winamp.SkinZip\shell\install
 *     (Default) = "Install skin" (localized)
 *     "" = "Install skin" (localized)
 * 
 * LOCALIZATION / ЛОКАЛИЗАЦИЯ:
 * The display text (kDispA) is defined in SwitchLangUI.h and can be localized
 * to different languages.
 * 
 * Отображаемый текст (kDispA) определён в SwitchLangUI.h и может быть
 * локализован для разных языков.
 * 
 ******************************************************************************/

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "mod_skininstall.h"
#include "..\SwitchLangUI.h"

/*******************************************************************************
 * REGISTRY PATH CONSTANTS
 * КОНСТАНТЫ ПУТЕЙ РЕЕСТРА
 ******************************************************************************/

// Registry subkey path for HKEY_CLASSES_ROOT (system-wide registration)
// Путь подключа реестра для HKEY_CLASSES_ROOT (системная регистрация)
// This is the preferred location as it applies to all users on the system
// Это предпочтительное расположение, так как применяется ко всем пользователям системы
static const char kSubCR_A[] = "Winamp.SkinZip\\shell\\install";

// Registry subkey path for HKEY_CURRENT_USER (per-user registration)
// Путь подключа реестра для HKEY_CURRENT_USER (регистрация для пользователя)
// This is used as a fallback when system-wide registration fails (no admin rights)
// Используется как резервный вариант, когда системная регистрация не удаётся (нет прав администратора)
static const char kSubCU_A[] = "Software\\Classes\\Winamp.SkinZip\\shell\\install";

/*******************************************************************************
 * GLOBAL STATE VARIABLES
 * ГЛОБАЛЬНЫЕ ПЕРЕМЕННЫЕ СОСТОЯНИЯ
 ******************************************************************************/

// Flag to ensure this module runs only once per Winamp session
// Флаг для гарантии, что этот модуль выполняется только один раз за сеанс Winamp
// TRUE = already executed, FALSE = not yet executed
// TRUE = уже выполнен, FALSE = ещё не выполнен
static BOOL s_doneOnce = FALSE;

/*******************************************************************************
 * UTILITY FUNCTIONS
 * УТИЛИТАРНЫЕ ФУНКЦИИ
 ******************************************************************************/

/*******************************************************************************
 * ReadStringValueA
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Reads a string value from a registry key with proper error handling and
 * null-termination.
 * 
 * Читает строковое значение из ключа реестра с правильной обработкой ошибок
 * и завершением нулём.
 * 
 * PARAMETERS / ПАРАМЕТРЫ:
 * h      - Handle to an open registry key
 *          Дескриптор открытого ключа реестра
 * name   - Name of the value to read (NULL or "" for default value)
 *          Имя значения для чтения (NULL или "" для значения по умолчанию)
 * out    - Buffer to receive the string value
 *          Буфер для получения строкового значения
 * outcch - Size of output buffer in characters
 *          Размер выходного буфера в символах
 * 
 * RETURNS / ВОЗВРАЩАЕТ:
 * BOOL - TRUE if value was read successfully, FALSE on error or if value
 *        doesn't exist or is not a string type
 *        TRUE если значение прочитано успешно, FALSE при ошибке или если
 *        значение не существует или не является строковым типом
 * 
 * NOTES / ПРИМЕЧАНИЯ:
 * - Accepts both REG_SZ and REG_EXPAND_SZ value types
 * - Always null-terminates the output buffer
 * - Clears output buffer before reading
 * 
 * - Принимает типы значений как REG_SZ, так и REG_EXPAND_SZ
 * - Всегда завершает выходной буфер нулём
 * - Очищает выходной буфер перед чтением
 ******************************************************************************/
static BOOL ReadStringValueA(HKEY h, const char* name, char* out, DWORD outcch)
{
    // Validate parameters / Проверка параметров
    if (!h || !out || outcch == 0) return FALSE;
    
    // Initialize output buffer / Инициализировать выходной буфер
    out[0] = 0;
    
    // Query the registry value / Запросить значение реестра
    DWORD type = 0;
    DWORD cb = outcch - 1;  // Leave room for null terminator / Оставить место для нулевого символа
    LONG r = RegQueryValueExA(h, name, NULL, &type, (LPBYTE)out, &cb);
    
    // Check if read was successful and type is string
    // Проверить, успешно ли чтение и является ли тип строковым
    if (r != ERROR_SUCCESS || (type != REG_SZ && type != REG_EXPAND_SZ)) 
        return FALSE;
    
    // Ensure null termination (cb contains the number of bytes read)
    // Гарантировать завершение нулём (cb содержит количество прочитанных байт)
    out[cb] = 0;
    
    return TRUE;
}

/*******************************************************************************
 * EnsureOneValueEqualsA
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Ensures that a specific registry value equals the desired string. Creates
 * the key if it doesn't exist, and updates the value only if it differs from
 * the desired value (minimizes unnecessary registry writes).
 * 
 * Гарантирует, что конкретное значение реестра равно желаемой строке. Создаёт
 * ключ, если он не существует, и обновляет значение только если оно отличается
 * от желаемого (минимизирует ненужные записи в реестр).
 * 
 * PARAMETERS / ПАРАМЕТРЫ:
 * root   - Root registry key (e.g., HKEY_CLASSES_ROOT, HKEY_CURRENT_USER)
 *          Корневой ключ реестра (например, HKEY_CLASSES_ROOT, HKEY_CURRENT_USER)
 * subKey - Path to the subkey relative to root
 *          Путь к подключу относительно корня
 * name   - Name of the value to check/set (NULL or "" for default value)
 *          Имя значения для проверки/установки (NULL или "" для значения по умолчанию)
 * want   - Desired string value
 *          Желаемое строковое значение
 * 
 * RETURNS / ВОЗВРАЩАЕТ:
 * BOOL - TRUE if value is now equal to desired value (either already was, or
 *        was successfully updated), FALSE if operation failed
 *        TRUE если значение теперь равно желаемому (либо уже было, либо
 *        было успешно обновлено), FALSE если операция не удалась
 * 
 * ALGORITHM / АЛГОРИТМ:
 * 1. Open or create the registry key with read/write access
 * 2. Read the current value
 * 3. If current value doesn't exist or differs from desired value, update it
 * 4. Close the key and return success status
 * 
 * 1. Открыть или создать ключ реестра с доступом на чтение/запись
 * 2. Прочитать текущее значение
 * 3. Если текущее значение не существует или отличается от желаемого, обновить его
 * 4. Закрыть ключ и вернуть статус успеха
 * 
 * OPTIMIZATION / ОПТИМИЗАЦИЯ:
 * Only writes to registry if the value actually needs to be changed, avoiding
 * unnecessary disk I/O and registry change notifications.
 * 
 * Записывает в реестр только если значение действительно нужно изменить,
 * избегая ненужного дискового ввода-вывода и уведомлений об изменении реестра.
 ******************************************************************************/
static BOOL EnsureOneValueEqualsA(HKEY root, const char* subKey, const char* name,
                                  const char* want)
{
    HKEY h = NULL;
    
    // Open or create the registry key / Открыть или создать ключ реестра
    LONG r = RegCreateKeyExA(root, subKey, 0, NULL, 0, 
                             KEY_QUERY_VALUE | KEY_SET_VALUE, NULL, &h, NULL);
    if (r != ERROR_SUCCESS || !h) return FALSE;
    
    // Read current value / Прочитать текущее значение
    char cur[260]; 
    cur[0] = 0;
    BOOL has = ReadStringValueA(h, name, cur, sizeof(cur));
    
    // Assume success initially / Предположить успех изначально
    BOOL ok = TRUE;
    
    // Check if value needs to be updated (doesn't exist or differs from desired)
    // Проверить, нужно ли обновить значение (не существует или отличается от желаемого)
    if (!has || lstrcmpiA(cur, want) != 0)
    {
        // Write the desired value to the registry
        // Записать желаемое значение в реестр
        ok = (RegSetValueExA(h, name, 0, REG_SZ, 
                            (const BYTE*)want, 
                            (DWORD)lstrlenA(want) + 1) == ERROR_SUCCESS);
    }
    
    // Close the key / Закрыть ключ
    RegCloseKey(h);
    
    return ok;
}

/*******************************************************************************
 * EnsureVerbExactA
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Ensures that a shell verb (context menu item) has the correct display text
 * by updating both the default value and the empty-string value. This dual
 * update provides maximum compatibility across different Windows versions.
 * 
 * Гарантирует, что глагол оболочки (пункт контекстного меню) имеет правильный
 * отображаемый текст, обновляя как значение по умолчанию, так и значение
 * пустой строки. Это двойное обновление обеспечивает максимальную
 * совместимость между различными версиями Windows.
 * 
 * PARAMETERS / ПАРАМЕТРЫ:
 * root   - Root registry key (e.g., HKEY_CLASSES_ROOT, HKEY_CURRENT_USER)
 *          Корневой ключ реестра (например, HKEY_CLASSES_ROOT, HKEY_CURRENT_USER)
 * subKey - Path to the shell verb subkey
 *          Путь к подключу глагола оболочки
 * want   - Desired display text for the context menu item
 *          Желаемый отображаемый текст для пункта контекстного меню
 * 
 * RETURNS / ВОЗВРАЩАЕТ:
 * BOOL - TRUE if both values were successfully set, FALSE if either failed
 *        TRUE если оба значения были успешно установлены, FALSE если одно не удалось
 * 
 * WHY TWO VALUES / ЗАЧЕМ ДВА ЗНАЧЕНИЯ:
 * Different Windows versions and configurations may read the display text from
 * either the default value (NULL) or the empty-string value (""). Setting both
 * ensures consistent behavior across all systems.
 * 
 * Разные версии Windows и конфигурации могут читать отображаемый текст либо
 * из значения по умолчанию (NULL), либо из значения пустой строки ("").
 * Установка обоих гарантирует согласованное поведение во всех системах.
 * 
 * NOTES / ПРИМЕЧАНИЯ:
 * This is a common pattern in Windows registry manipulation for shell
 * integration to ensure maximum compatibility.
 * 
 * Это распространённый паттерн в манипуляции реестром Windows для интеграции
 * с оболочкой для обеспечения максимальной совместимости.
 ******************************************************************************/
static BOOL EnsureVerbExactA(HKEY root, const char* subKey, const char* want)
{
    // Update default value (NULL parameter)
    // Обновить значение по умолчанию (параметр NULL)
    BOOL okDef = EnsureOneValueEqualsA(root, subKey, NULL, want);
    
    // Update empty-string value ("" parameter)
    // Обновить значение пустой строки (параметр "")
    BOOL okEmp = EnsureOneValueEqualsA(root, subKey, "", want);
    
    // Return TRUE only if both updates succeeded
    // Вернуть TRUE только если обе операции обновления успешны
    return okDef && okEmp;
}

/*******************************************************************************
 * EXPORTED FUNCTIONS
 * ЭКСПОРТИРУЕМЫЕ ФУНКЦИИ
 ******************************************************************************/

/*******************************************************************************
 * SkinInstall_RunOnce
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Main entry point for the skin installation context menu module. Ensures
 * that the "Install skin" context menu entry is properly registered in the
 * Windows Registry. Safe to call multiple times - will only execute once.
 * 
 * Главная точка входа для модуля контекстного меню установки скинов.
 * Гарантирует, что пункт контекстного меню "Установить скин" правильно
 * зарегистрирован в реестре Windows. Безопасно вызывать многократно -
 * выполнится только один раз.
 * 
 * EXECUTION STRATEGY / СТРАТЕГИЯ ВЫПОЛНЕНИЯ:
 * 1. Check if already executed - if yes, return immediately
 * 2. Mark as executed to prevent future runs
 * 3. Try to register in HKEY_CLASSES_ROOT (system-wide)
 * 4. If system-wide registration fails (no admin rights), fall back to
 *    HKEY_CURRENT_USER (per-user registration)
 * 
 * 1. Проверить, уже выполнено - если да, вернуться немедленно
 * 2. Отметить как выполненное для предотвращения будущих запусков
 * 3. Попытаться зарегистрировать в HKEY_CLASSES_ROOT (системно)
 * 4. Если системная регистрация не удалась (нет прав администратора),
 *    использовать HKEY_CURRENT_USER (регистрация для пользователя)
 * 
 * WHEN TO CALL / КОГДА ВЫЗЫВАТЬ:
 * This should be called during Winamp startup, typically from the main
 * initialization routine. It's safe to call on every startup as it will
 * only perform registry operations on the first call.
 * 
 * Это должно вызываться при запуске Winamp, обычно из основной процедуры
 * инициализации. Безопасно вызывать при каждом запуске, так как операции
 * с реестром будут выполнены только при первом вызове.
 * 
 * NOTES / ПРИМЕЧАНИЯ:
 * - The display text (kDispA) is defined in SwitchLangUI.h
 * - System-wide registration requires administrator privileges
 * - Per-user registration works without admin rights but only affects
 *   the current user account
 * 
 * - Отображаемый текст (kDispA) определён в SwitchLangUI.h
 * - Системная регистрация требует прав администратора
 * - Регистрация для пользователя работает без прав администратора, но влияет
 *   только на текущую учётную запись пользователя
 ******************************************************************************/
void SkinInstall_RunOnce()
{
    // Check if already executed - idempotency guard
    // Проверить, уже выполнено - защита от повторного выполнения
    if (s_doneOnce) return;
    
    // Mark as executed to prevent future calls from doing work
    // Отметить как выполненное, чтобы предотвратить работу будущих вызовов
    s_doneOnce = TRUE;
    
    // Try system-wide registration first (preferred, but requires admin rights)
    // Сначала попытаться системную регистрацию (предпочтительно, но требует прав администратора)
    if (!EnsureVerbExactA(HKEY_CLASSES_ROOT, kSubCR_A, kDispA))
    {
        // System-wide registration failed - fall back to per-user registration
        // Системная регистрация не удалась - использовать регистрацию для пользователя
        // This will work even without administrator privileges
        // Это будет работать даже без прав администратора
        EnsureVerbExactA(HKEY_CURRENT_USER, kSubCU_A, kDispA);
    }
    
    // Note: We don't check the return value of the fallback because there's
    // nothing we can do if both attempts fail. The function will have done
    // its best to register the context menu entry.
    // 
    // Примечание: мы не проверяем возвращаемое значение резервного варианта,
    // потому что нечего делать, если обе попытки не удались. Функция сделала
    // всё возможное для регистрации пункта контекстного меню.
}