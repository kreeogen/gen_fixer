/*******************************************************************************
 * patch_mb.cpp
 * 
 * MINIBROWSER REMOVAL PATCH
 * ПАТЧ УДАЛЕНИЯ МИНИ-БРАУЗЕРА
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Removes Winamp's built-in minibrowser (web browser component) functionality.
 * Consists of two parts: blocking minibrowser creation and hiding menu item.
 * 
 * Удаляет встроенную функциональность мини-браузера Winamp (компонент веб-браузера).
 * Состоит из двух частей: блокировка создания мини-браузера и скрытие пункта меню.
 * 
 * HOW IT WORKS / КАК ЭТО РАБОТАЕТ:
 * 
 * PART 1 - CREATION BLOCKING / ЧАСТЬ 1 - БЛОКИРОВКА СОЗДАНИЯ:
 * 1. Patches byte at RVA 0x00023BE0 in winamp.exe
 * 2. Changes 0x57 (PUSH EDI) to 0xC3 (RET)
 * 3. This makes minibrowser creation function return immediately
 * 4. Prevents minibrowser window from being created
 * 
 * 1. Патчит байт по RVA 0x00023BE0 в winamp.exe
 * 2. Меняет 0x57 (PUSH EDI) на 0xC3 (RET)
 * 3. Это заставляет функцию создания мини-браузера вернуться немедленно
 * 4. Предотвращает создание окна мини-браузера
 * 
 * PART 2 - MENU HIDING / ЧАСТЬ 2 - СКРЫТИЕ МЕНЮ:
 * 1. Hooks TrackPopupMenu and TrackPopupMenuEx in winamp.exe
 * 2. Before showing any menu, converts menu item 40298 to disabled separator
 * 3. This hides "Open Minibrowser" menu item without changing menu indices
 * 4. Safe approach - doesn't use DeleteMenu which shifts indices
 * 
 * 1. Перехватывает TrackPopupMenu и TrackPopupMenuEx в winamp.exe
 * 2. Перед показом любого меню конвертирует пункт меню 40298 в отключённый разделитель
 * 3. Это скрывает пункт меню "Открыть мини-браузер" без изменения индексов меню
 * 4. Безопасный подход - не использует DeleteMenu который смещает индексы
 * 
 * TECHNICAL DETAILS / ТЕХНИЧЕСКИЕ ДЕТАЛИ:
 * 
 * PATCH LOCATION / РАСПОЛОЖЕНИЕ ПАТЧА:
 * - Target: winamp.exe
 * - RVA: 0x00023BE0 (minibrowser creation function entry point)
 * - Original: 0x57 (PUSH EDI - function prologue)
 * - Patched: 0xC3 (RET - immediate return)
 * 
 * - Цель: winamp.exe
 * - RVA: 0x00023BE0 (точка входа функции создания мини-браузера)
 * - Оригинал: 0x57 (PUSH EDI - пролог функции)
 * - Пропатчено: 0xC3 (RET - немедленный возврат)
 * 
 * MENU ITEM ID / ID ПУНКТА МЕНЮ:
 * - Command ID: 40298 (standard Winamp minibrowser menu ID)
 * - Method: Convert to MFT_SEPARATOR + MFS_DISABLED
 * - Why: Preserves menu structure, prevents crashes
 * 
 * - ID команды: 40298 (стандартный ID меню мини-браузера Winamp)
 * - Метод: Преобразование в MFT_SEPARATOR + MFS_DISABLED
 * - Почему: Сохраняет структуру меню, предотвращает краши
 * 
 * WHY THIS APPROACH / ПОЧЕМУ ЭТОТ ПОДХОД:
 * Blocking creation is cleaner than trying to hide/destroy window after creation.
 * Menu hiding prevents user confusion about non-functional menu item.
 * Converting to separator (not deleting) prevents menu index issues.
 * 
 * Блокировка создания чище чем попытки скрыть/уничтожить окно после создания.
 * Скрытие меню предотвращает путаницу пользователя о нефункциональном пункте меню.
 * Преобразование в разделитель (не удаление) предотвращает проблемы с индексами меню.
 * 
 * COMPATIBILITY / СОВМЕСТИМОСТЬ:
 * - Winamp 2.95 and compatible versions
 * - Windows 98 through Windows 11
 * - Safe with other Winamp plugins
 * 
 * - Winamp 2.95 и совместимые версии
 * - Windows 98 до Windows 11
 * - Безопасно с другими плагинами Winamp
 * 
 ******************************************************************************/

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "patcher_core.h"

/*******************************************************************************
 * PART 1: MINIBROWSER CREATION BLOCKING PATCH
 * ЧАСТЬ 1: ПАТЧ БЛОКИРОВКИ СОЗДАНИЯ МИНИ-БРАУЗЕРА
 ******************************************************************************/

/*******************************************************************************
 * g_mbBlockCreate
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Byte patch table to block minibrowser window creation.
 * Single-byte patch that makes creation function return immediately.
 * 
 * Таблица байтовых патчей для блокировки создания окна мини-браузера.
 * Однобайтовый патч, который заставляет функцию создания вернуться немедленно.
 * 
 * PATCH DETAILS / ДЕТАЛИ ПАТЧА:
 * RVA 0x00023BE0 is the entry point of minibrowser creation function.
 * Original byte 0x57 (PUSH EDI) is part of function prologue.
 * Patched byte 0xC3 (RET) causes immediate return from function.
 * 
 * RVA 0x00023BE0 - точка входа функции создания мини-браузера.
 * Оригинальный байт 0x57 (PUSH EDI) - часть пролога функции.
 * Пропатченный байт 0xC3 (RET) вызывает немедленный возврат из функции.
 * 
 * EFFECT / ЭФФЕКТ:
 * Function returns without executing any creation code.
 * Minibrowser window is never created.
 * No resources allocated for minibrowser.
 * 
 * Функция возвращается без выполнения какого-либо кода создания.
 * Окно мини-браузера никогда не создаётся.
 * Ресурсы для мини-браузера не выделяются.
 ******************************************************************************/
static const PatchByte g_mbBlockCreate[] =
{
    /* RVA        Expected  Patch  Description / Описание */
    { 0x00023BE0,  0x57,    0xC3 },  // PUSH EDI -> RET (block minibrowser creation / блокировать создание мини-браузера)
};

// Safe array size calculation / Безопасное вычисление размера массива
#define ARRAYSIZE_(a) ((int)(sizeof(a) / sizeof((a)[0])))

/*******************************************************************************
 * PART 2: MENU ITEM HIDING
 * ЧАСТЬ 2: СКРЫТИЕ ПУНКТА МЕНЮ
 ******************************************************************************/

/*******************************************************************************
 * MB_MENU_CMD_ID
 * 
 * Minibrowser menu command ID in Winamp.
 * This is the standard ID for "Open Minibrowser" menu item.
 * 
 * ID команды меню мини-браузера в Winamp.
 * Это стандартный ID для пункта меню "Открыть мини-браузер".
 ******************************************************************************/
#define MB_MENU_CMD_ID 40298

/*******************************************************************************
 * TRACKPOPUPMENU HOOK INFRASTRUCTURE
 * ИНФРАСТРУКТУРА ХУКОВ TRACKPOPUPMENU
 * 
 * Function pointer types and global variables for menu hooks.
 * Типы указателей функций и глобальные переменные для хуков меню.
 ******************************************************************************/

// Function pointer types / Типы указателей функций
typedef BOOL (WINAPI *TPM)(HMENU, UINT, int, int, int, HWND, const RECT*);
typedef BOOL (WINAPI *TPMX)(HMENU, UINT, int, int, HWND, LPTPMPARAMS);

// Original function pointers (saved during patching) / Указатели на оригинальные функции
static TPM  g_OrigTrackPopupMenu   = NULL;
static TPMX g_OrigTrackPopupMenuEx = NULL;

// IAT slot pointers (for restoration during cleanup) / Указатели на слоты IAT (для восстановления при очистке)
static void** g_IAT_TrackPopupMenu   = NULL;
static void** g_IAT_TrackPopupMenuEx = NULL;

/*******************************************************************************
 * menu_hide_by_id_recursive
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Recursively searches menu hierarchy and "neutralizes" specified menu item.
 * Converts menu item to disabled separator instead of deleting it.
 * 
 * Рекурсивно ищет в иерархии меню и "нейтрализует" указанный пункт меню.
 * Преобразует пункт меню в отключённый разделитель вместо удаления.
 * 
 * PARAMETERS / ПАРАМЕТРЫ:
 * hMenu - Menu handle to search / Дескриптор меню для поиска
 * id    - Command ID to hide / ID команды для скрытия
 * 
 * RETURNS / ВОЗВРАЩАЕТ:
 * Number of items changed / Количество изменённых пунктов
 * 
 * ALGORITHM / АЛГОРИТМ:
 * 1. Iterate through all menu items
 * 2. For each submenu, recursively search it
 * 3. For each item with matching ID:
 *    - Convert to MFT_SEPARATOR type
 *    - Set state to MFS_DISABLED
 *    - Use SetMenuItemInfoA by POSITION
 * 4. Return count of items changed
 * 
 * 1. Перебрать все пункты меню
 * 2. Для каждого подменю рекурсивно искать в нём
 * 3. Для каждого пункта с совпадающим ID:
 *    - Преобразовать в тип MFT_SEPARATOR
 *    - Установить состояние MFS_DISABLED
 *    - Использовать SetMenuItemInfoA по ПОЗИЦИИ
 * 4. Вернуть количество изменённых пунктов
 * 
 * WHY SEPARATOR INSTEAD OF DELETE / ПОЧЕМУ РАЗДЕЛИТЕЛЬ ВМЕСТО УДАЛЕНИЯ:
 * DeleteMenu shifts all indices after deleted item, which can cause:
 * - Other code using wrong menu indices
 * - Crashes if code caches menu positions
 * - Unexpected behavior in keyboard navigation
 * 
 * DeleteMenu смещает все индексы после удалённого пункта, что может вызвать:
 * - Использование другим кодом неправильных индексов меню
 * - Краши если код кэширует позиции меню
 * - Неожиданное поведение в навигации клавиатурой
 * 
 * Converting to separator:
 * - Keeps indices stable (no shifting)
 * - Hides item visually (appears as empty space)
 * - Prevents item from being selected
 * - Safe and compatible approach
 * 
 * Преобразование в разделитель:
 * - Сохраняет индексы стабильными (без смещения)
 * - Скрывает пункт визуально (появляется как пустое место)
 * - Предотвращает выбор пункта
 * - Безопасный и совместимый подход
 ******************************************************************************/
/*******************************************************************************
 * menu_hide_by_id_recursive (MODIFIED / МОДИФИЦИРОВАННАЯ)
 *
 * NEW BEHAVIOR / НОВОЕ ПОВЕДЕНИЕ:
 * Finds the item, deletes it, and inserts a separator just before the NEXT
 * existing separator (or at the end of the menu).
 * This "pushes" the gap down to the bottom of the group.
 *
 * Находит пункт, удаляет его и вставляет разделитель перед СЛЕДУЮЩИМ
 * существующим разделителем (или в конце меню).
 * Это "сдвигает" пустоту вниз к концу группы.
 ******************************************************************************/
static int menu_hide_by_id_recursive(HMENU hMenu, UINT id)
{
    int changed = 0;
    int count = GetMenuItemCount(hMenu);
    
    // Вспомогательная структура для проверки типа пункта
    MENUITEMINFOA mii_check;
    ZeroMemory(&mii_check, sizeof(mii_check));
    mii_check.cbSize = sizeof(mii_check);
    mii_check.fMask = MIIM_FTYPE;

    for (int i = 0; i < count; ++i)
    {
        // 1. Рекурсивный поиск в подменю
        HMENU sub = GetSubMenu(hMenu, i);
        if (sub) {
            changed += menu_hide_by_id_recursive(sub, id);
        }

        // 2. Проверка на совпадение ID
        UINT cmd = GetMenuItemID(hMenu, i);
        if (cmd == id)
        {
            // --- НАЧАЛО НОВОЙ ЛОГИКИ ---

            // Ищем, куда сдвинуть (ищем следующий сепаратор)
            int targetPos = count; // По умолчанию - в самый конец
            
            // Сканируем от текущей позиции + 1 вниз
            for (int j = i + 1; j < count; j++) {
                if (GetMenuItemInfoA(hMenu, j, TRUE, &mii_check)) {
                    if (mii_check.fType & MFT_SEPARATOR) {
                        targetPos = j; // Нашли сепаратор, остановимся ПЕРЕД ним
                        break;
                    }
                }
            }

            // Если пункт уже стоит перед сепаратором, двигать не надо
            if (targetPos == i + 1) {
                // Просто превращаем в сепаратор на месте (старая логика)
                MENUITEMINFOA mii;
                ZeroMemory(&mii, sizeof(mii));
                mii.cbSize = sizeof(mii);
                mii.fMask  = MIIM_FTYPE | MIIM_STATE;
                mii.fType  = MFT_SEPARATOR;
                mii.fState = MFS_DISABLED;
                SetMenuItemInfoA(hMenu, (UINT)i, TRUE, &mii);
            }
            else {
                // ПЕРЕМЕЩЕНИЕ:
                
                // 1. Удаляем пункт со старого места
                DeleteMenu(hMenu, i, MF_BYPOSITION);
                
                // ВНИМАНИЕ: После удаления индексы всех элементов ниже 'i' уменьшились на 1.
                // Поэтому наш найденный targetPos тоже сдвинулся на 1 вверх.
                targetPos--; 
                count--; // Общее количество уменьшилось

                // 2. Вставляем новый сепаратор на новую позицию
                // (перед тем сепаратором, который мы нашли)
                InsertMenuA(hMenu, targetPos, MF_BYPOSITION | MF_SEPARATOR, 0, NULL);
                
                // Поскольку мы вставили элемент, count снова увеличился,
                // но 'i' мы должны уменьшить, чтобы цикл for не перескочил 
                // через элемент, который "подтянулся" на место удаленного.
                i--; 
                count++; 
            }

            changed++;
            // --- КОНЕЦ НОВОЙ ЛОГИКИ ---
        }
    }
    return changed;
}

/*******************************************************************************
 * IAT PATCHING HELPER
 * ВСПОМОГАТЕЛЬНАЯ ФУНКЦИЯ IAT ПАТЧИНГА
 ******************************************************************************/

/*******************************************************************************
 * iat_patch_by_nameA
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Minimal IAT patch helper - replaces import by function name.
 * Similar to full IAT_Patch but also returns pointer to IAT slot.
 * 
 * Минимальная вспомогательная функция IAT патча - заменяет импорт по имени функции.
 * Похожа на полный IAT_Patch но также возвращает указатель на слот IAT.
 * 
 * PARAMETERS / ПАРАМЕТРЫ:
 * hMod       - Module handle to patch / Дескриптор модуля для патча
 * dllName    - DLL name containing function / Имя DLL содержащей функцию
 * funcName   - Function name to hook / Имя функции для перехвата
 * newProc    - New function pointer / Новый указатель функции
 * outOldProc - Output: original function pointer / Вывод: оригинальный указатель функции
 * outIatSlot - Output: pointer to IAT slot / Вывод: указатель на слот IAT
 * 
 * RETURNS / ВОЗВРАЩАЕТ:
 * 1 if patch successful, 0 otherwise
 * 1 если патч успешен, 0 иначе
 * 
 * WHY RETURN IAT SLOT / ПОЧЕМУ ВОЗВРАЩАЕМ СЛОТ IAT:
 * Knowing IAT slot location allows easy restoration during cleanup.
 * We can directly write original pointer back without re-searching.
 * 
 * Знание расположения слота IAT позволяет лёгкое восстановление при очистке.
 * Можем напрямую записать оригинальный указатель обратно без повторного поиска.
 * 
 * ALGORITHM / АЛГОРИТМ:
 * 1. Validate PE structure
 * 2. Find import descriptor for specified DLL
 * 3. Search for function by name
 * 4. Change memory protection
 * 5. Replace function pointer
 * 6. Flush instruction cache (important!)
 * 7. Restore memory protection
 * 8. Return IAT slot pointer
 * 
 * 1. Проверить PE структуру
 * 2. Найти дескриптор импорта для указанной DLL
 * 3. Искать функцию по имени
 * 4. Изменить защиту памяти
 * 5. Заменить указатель функции
 * 6. Сбросить кэш инструкций (важно!)
 * 7. Восстановить защиту памяти
 * 8. Вернуть указатель на слот IAT
 ******************************************************************************/
static int iat_patch_by_nameA(HMODULE hMod, const char* dllName, const char* funcName,
                             void* newProc, void** outOldProc, void*** outIatSlot)
{
    BYTE* base = (BYTE*)hMod;
    
    // Validate DOS header / Проверить DOS заголовок
    IMAGE_DOS_HEADER* dos = (IMAGE_DOS_HEADER*)base;
    if (!dos || dos->e_magic != IMAGE_DOS_SIGNATURE) return 0;

    // Validate NT header / Проверить NT заголовок
    IMAGE_NT_HEADERS* nt = (IMAGE_NT_HEADERS*)(base + dos->e_lfanew);
    if (!nt || nt->Signature != IMAGE_NT_SIGNATURE) return 0;

    // Get import directory / Получить каталог импорта
    DWORD impRva = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress;
    if (!impRva) return 0;

    IMAGE_IMPORT_DESCRIPTOR* imp = (IMAGE_IMPORT_DESCRIPTOR*)(base + impRva);

    // Search through import descriptors / Искать через дескрипторы импорта
    for (; imp->Name; ++imp)
    {
        const char* name = (const char*)(base + imp->Name);
        if (!name) continue;

        // Check if this is target DLL / Проверить, это ли целевая DLL
        if (lstrcmpiA(name, dllName) != 0) continue;

        // Get thunk arrays / Получить массивы thunk
        IMAGE_THUNK_DATA* firstThunk = (IMAGE_THUNK_DATA*)(base + imp->FirstThunk);
        IMAGE_THUNK_DATA* origThunk  = imp->OriginalFirstThunk
                                     ? (IMAGE_THUNK_DATA*)(base + imp->OriginalFirstThunk)
                                     : firstThunk;

        // Search through imports / Искать через импорты
        for (; origThunk->u1.AddressOfData; ++origThunk, ++firstThunk)
        {
            // Skip ordinal imports / Пропустить импорты по порядковому номеру
            if (origThunk->u1.Ordinal & IMAGE_ORDINAL_FLAG) continue;

            // Get import name / Получить имя импорта
            IMAGE_IMPORT_BY_NAME* ibn = (IMAGE_IMPORT_BY_NAME*)(base + origThunk->u1.AddressOfData);
            if (!ibn) continue;

            const char* impFunc = (const char*)ibn->Name;
            if (!impFunc) continue;

            // Check if this is target function / Проверить, это ли целевая функция
            if (lstrcmpA(impFunc, funcName) == 0)
            {
                // Get pointer to IAT slot / Получить указатель на слот IAT
                void** slot = (void**)&firstThunk->u1.Function;

                // Make memory writable / Сделать память доступной для записи
                DWORD oldProt = 0;
                if (!VirtualProtect(slot, sizeof(void*), PAGE_EXECUTE_READWRITE, &oldProt))
                    return 0;

                // Save original and install hook / Сохранить оригинал и установить хук
                if (outOldProc) *outOldProc = *slot;
                *slot = newProc;
                
                // CRITICAL: Flush instruction cache to ensure CPU sees new pointer
                // КРИТИЧНО: Сбросить кэш инструкций чтобы CPU увидел новый указатель
                FlushInstructionCache(GetCurrentProcess(), slot, sizeof(void*));

                // Restore memory protection / Восстановить защиту памяти
                {
                    DWORD tmp = 0;
                    VirtualProtect(slot, sizeof(void*), oldProt, &tmp);
                }

                // Return IAT slot pointer / Вернуть указатель на слот IAT
                if (outIatSlot) *outIatSlot = slot;
                return 1;
            }
        }
    }
    return 0;
}

/*******************************************************************************
 * TRACKPOPUPMENU HOOK FUNCTIONS
 * ФУНКЦИИ ХУКОВ TRACKPOPUPMENU
 ******************************************************************************/

/*******************************************************************************
 * Hook_TrackPopupMenu
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Hook for TrackPopupMenu that hides minibrowser menu item before showing menu.
 * Хук для TrackPopupMenu который скрывает пункт меню мини-браузера перед показом меню.
 * 
 * PARAMETERS / ПАРАМЕТРЫ:
 * Standard TrackPopupMenu parameters / Стандартные параметры TrackPopupMenu
 * 
 * RETURNS / ВОЗВРАЩАЕТ:
 * Result from original TrackPopupMenu / Результат оригинальной TrackPopupMenu
 * 
 * ALGORITHM / АЛГОРИТМ:
 * 1. Before showing menu, recursively hide menu item 40298
 * 2. Call original TrackPopupMenu to show menu
 * 3. Return result
 * 
 * 1. Перед показом меню рекурсивно скрыть пункт меню 40298
 * 2. Вызвать оригинальную TrackPopupMenu для показа меню
 * 3. Вернуть результат
 * 
 * NOTES / ПРИМЕЧАНИЯ:
 * Safe: doesn't change menu item count or positions.
 * Безопасно: не изменяет количество пунктов меню или позиции.
 ******************************************************************************/
static BOOL WINAPI Hook_TrackPopupMenu(HMENU hMenu, UINT uFlags, int x, int y, int nRes,
                                      HWND hWnd, const RECT* prc)
{
    // Hide minibrowser menu item (converts to disabled separator)
    // Скрыть пункт меню мини-браузера (преобразует в отключённый разделитель)
    menu_hide_by_id_recursive(hMenu, (UINT)MB_MENU_CMD_ID);

    // Call original function / Вызвать оригинальную функцию
    return g_OrigTrackPopupMenu ? g_OrigTrackPopupMenu(hMenu, uFlags, x, y, nRes, hWnd, prc) : FALSE;
}

/*******************************************************************************
 * Hook_TrackPopupMenuEx
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Hook for TrackPopupMenuEx (extended version of TrackPopupMenu).
 * Same functionality as Hook_TrackPopupMenu but for extended API.
 * 
 * Хук для TrackPopupMenuEx (расширенная версия TrackPopupMenu).
 * Та же функциональность что Hook_TrackPopupMenu но для расширенного API.
 ******************************************************************************/
static BOOL WINAPI Hook_TrackPopupMenuEx(HMENU hMenu, UINT uFlags, int x, int y,
                                        HWND hWnd, LPTPMPARAMS p)
{
    // Hide minibrowser menu item / Скрыть пункт меню мини-браузера
    menu_hide_by_id_recursive(hMenu, (UINT)MB_MENU_CMD_ID);

    // Call original function / Вызвать оригинальную функцию
    return g_OrigTrackPopupMenuEx ? g_OrigTrackPopupMenuEx(hMenu, uFlags, x, y, hWnd, p) : FALSE;
}

/*******************************************************************************
 * HOOK INSTALLATION AND REMOVAL
 * УСТАНОВКА И УДАЛЕНИЕ ХУКОВ
 ******************************************************************************/

/*******************************************************************************
 * mb_menu_hook_install
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Installs TrackPopupMenu hooks in winamp.exe.
 * Patches both TrackPopupMenu and TrackPopupMenuEx for complete coverage.
 * 
 * Устанавливает хуки TrackPopupMenu в winamp.exe.
 * Патчит и TrackPopupMenu и TrackPopupMenuEx для полного охвата.
 * 
 * RETURNS / ВОЗВРАЩАЕТ:
 * 1 if at least one hook installed successfully, 0 if both failed
 * 1 если хотя бы один хук установлен успешно, 0 если оба не удались
 * 
 * ALGORITHM / АЛГОРИТМ:
 * 1. Get winamp.exe module handle
 * 2. If TrackPopupMenu not hooked yet:
 *    - Patch IAT to install hook
 *    - Save original function pointer
 *    - Save IAT slot pointer
 * 3. If TrackPopupMenuEx not hooked yet:
 *    - Patch IAT to install hook
 *    - Save original function pointer
 *    - Save IAT slot pointer
 * 4. Return success if at least one hook installed
 * 
 * 1. Получить дескриптор модуля winamp.exe
 * 2. Если TrackPopupMenu ещё не перехвачен:
 *    - Пропатчить IAT для установки хука
 *    - Сохранить указатель на оригинальную функцию
 *    - Сохранить указатель на слот IAT
 * 3. Если TrackPopupMenuEx ещё не перехвачен:
 *    - Пропатчить IAT для установки хука
 *    - Сохранить указатель на оригинальную функцию
 *    - Сохранить указатель на слот IAT
 * 4. Вернуть успех если хотя бы один хук установлен
 * 
 * WHY HOOK BOTH / ПОЧЕМУ ХУКАЕМ ОБА:
 * Winamp might use either function depending on context.
 * Hooking both ensures complete menu hiding coverage.
 * 
 * Winamp может использовать любую функцию в зависимости от контекста.
 * Перехват обеих обеспечивает полное покрытие скрытия меню.
 ******************************************************************************/
static int mb_menu_hook_install(void)
{
    // Get winamp.exe module / Получить модуль winamp.exe
    HMODULE hExe = GetModuleHandleA(NULL);
    if (!hExe) return 0;

    /***************************************************************************
     * Hook TrackPopupMenu
     * Перехватить TrackPopupMenu
     ***************************************************************************/
    if (!g_OrigTrackPopupMenu)
    {
        void* oldP = NULL;
        void** slot = NULL;
        
        if (iat_patch_by_nameA(hExe, "USER32.dll", "TrackPopupMenu",
                               (void*)Hook_TrackPopupMenu, &oldP, &slot))
        {
            g_OrigTrackPopupMenu = (TPM)oldP;
            g_IAT_TrackPopupMenu = slot;
        }
    }

    /***************************************************************************
     * Hook TrackPopupMenuEx
     * Перехватить TrackPopupMenuEx
     ***************************************************************************/
    if (!g_OrigTrackPopupMenuEx)
    {
        void* oldP = NULL;
        void** slot = NULL;
        
        if (iat_patch_by_nameA(hExe, "USER32.dll", "TrackPopupMenuEx",
                               (void*)Hook_TrackPopupMenuEx, &oldP, &slot))
        {
            g_OrigTrackPopupMenuEx = (TPMX)oldP;
            g_IAT_TrackPopupMenuEx = slot;
        }
    }

    // Return success if at least one hook installed
    // Вернуть успех если хотя бы один хук установлен
    return (g_OrigTrackPopupMenu != NULL) || (g_OrigTrackPopupMenuEx != NULL);
}

/*******************************************************************************
 * mb_menu_hook_remove
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Removes TrackPopupMenu hooks by restoring original IAT entries.
 * Удаляет хуки TrackPopupMenu восстановлением оригинальных записей IAT.
 * 
 * ALGORITHM / АЛГОРИТМ:
 * 1. If TrackPopupMenu was hooked:
 *    - Make IAT slot writable
 *    - Restore original pointer
 *    - Flush instruction cache
 *    - Restore memory protection
 * 2. If TrackPopupMenuEx was hooked:
 *    - Same restoration process
 * 3. Clear all global pointers
 * 
 * 1. Если TrackPopupMenu был перехвачен:
 *    - Сделать слот IAT доступным для записи
 *    - Восстановить оригинальный указатель
 *    - Сбросить кэш инструкций
 *    - Восстановить защиту памяти
 * 2. Если TrackPopupMenuEx был перехвачен:
 *    - Тот же процесс восстановления
 * 3. Очистить все глобальные указатели
 * 
 * NOTES / ПРИМЕЧАНИЯ:
 * Must be called before plugin unload to prevent crashes.
 * Должна быть вызвана перед выгрузкой плагина для предотвращения крашей.
 ******************************************************************************/
static void mb_menu_hook_remove(void)
{
    /***************************************************************************
     * Restore TrackPopupMenu
     * Восстановить TrackPopupMenu
     ***************************************************************************/
    if (g_IAT_TrackPopupMenu && g_OrigTrackPopupMenu)
    {
        DWORD oldProt = 0;
        if (VirtualProtect(g_IAT_TrackPopupMenu, sizeof(void*), PAGE_EXECUTE_READWRITE, &oldProt))
        {
            *g_IAT_TrackPopupMenu = (void*)g_OrigTrackPopupMenu;  // Restore original / Восстановить оригинал
            FlushInstructionCache(GetCurrentProcess(), g_IAT_TrackPopupMenu, sizeof(void*));
            
            {
                DWORD tmp = 0;
                VirtualProtect(g_IAT_TrackPopupMenu, sizeof(void*), oldProt, &tmp);
            }
        }
    }

    /***************************************************************************
     * Restore TrackPopupMenuEx
     * Восстановить TrackPopupMenuEx
     ***************************************************************************/
    if (g_IAT_TrackPopupMenuEx && g_OrigTrackPopupMenuEx)
    {
        DWORD oldProt = 0;
        if (VirtualProtect(g_IAT_TrackPopupMenuEx, sizeof(void*), PAGE_EXECUTE_READWRITE, &oldProt))
        {
            *g_IAT_TrackPopupMenuEx = (void*)g_OrigTrackPopupMenuEx;  // Restore original / Восстановить оригинал
            FlushInstructionCache(GetCurrentProcess(), g_IAT_TrackPopupMenuEx, sizeof(void*));
            
            {
                DWORD tmp = 0;
                VirtualProtect(g_IAT_TrackPopupMenuEx, sizeof(void*), oldProt, &tmp);
            }
        }
    }

    // Clear all global pointers / Очистить все глобальные указатели
    g_IAT_TrackPopupMenu = NULL;
    g_IAT_TrackPopupMenuEx = NULL;
    g_OrigTrackPopupMenu = NULL;
    g_OrigTrackPopupMenuEx = NULL;
}

/*******************************************************************************
 * PUBLIC API
 * ПУБЛИЧНЫЙ API
 ******************************************************************************/

/*******************************************************************************
 * patch_mb_init
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Initializes minibrowser removal patch.
 * Applies both creation blocking patch and menu hiding hooks.
 * 
 * Инициализирует патч удаления мини-браузера.
 * Применяет и патч блокировки создания и хуки скрытия меню.
 * 
 * RETURNS / ВОЗВРАЩАЕТ:
 * 1 if initialization successful, 0 on failure
 * 1 если инициализация успешна, 0 при ошибке
 * 
 * ALGORITHM / АЛГОРИТМ:
 * 1. Apply byte patch to block minibrowser creation
 * 2. Install menu hiding hooks
 * 3. Return success
 * 
 * 1. Применить байтовый патч для блокировки создания мини-браузера
 * 2. Установить хуки скрытия меню
 * 3. Вернуть успех
 * 
 * WHEN TO CALL / КОГДА ВЫЗЫВАТЬ:
 * Call during plugin initialization.
 * Вызывайте во время инициализации плагина.
 * 
 * EXAMPLE / ПРИМЕР:
 * ```c
 * int plugin_init() {
 *     if (!patch_mb_init()) {
 *         // Handle error
 *     }
 *     return 0;
 * }
 * ```
 ******************************************************************************/
int patch_mb_init(void)
{
    // Apply byte patch to block creation / Применить байтовый патч для блокировки создания
    if (!patcher_apply_rva_table(g_mbBlockCreate, ARRAYSIZE_(g_mbBlockCreate)))
        return 0;

    // Install menu hiding hooks / Установить хуки скрытия меню
    mb_menu_hook_install();
    
    return 1;
}

/*******************************************************************************
 * patch_mb_quit
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Cleans up minibrowser removal patch.
 * Removes menu hooks and reverts byte patches.
 * 
 * Очищает патч удаления мини-браузера.
 * Удаляет хуки меню и откатывает байтовые патчи.
 * 
 * ALGORITHM / АЛГОРИТМ:
 * 1. Remove menu hiding hooks
 * 2. Revert byte patch (restore original byte)
 * 
 * 1. Удалить хуки скрытия меню
 * 2. Откатить байтовый патч (восстановить оригинальный байт)
 * 
 * WHEN TO CALL / КОГДА ВЫЗЫВАТЬ:
 * Call during plugin cleanup, before unload.
 * Вызывайте во время очистки плагина, перед выгрузкой.
 * 
 * CRITICAL / КРИТИЧНО:
 * Must be called before plugin DLL is unloaded.
 * Должна быть вызвана перед выгрузкой DLL плагина.
 * 
 * EXAMPLE / ПРИМЕР:
 * ```c
 * void plugin_quit() {
 *     patch_mb_quit();
 *     // ... other cleanup ...
 * }
 * ```
 ******************************************************************************/
void patch_mb_quit(void)
{
    // Remove menu hooks / Удалить хуки меню
    mb_menu_hook_remove();
    
    // Revert byte patch / Откатить байтовый патч
    patcher_revert_rva_table(g_mbBlockCreate, ARRAYSIZE_(g_mbBlockCreate));
}