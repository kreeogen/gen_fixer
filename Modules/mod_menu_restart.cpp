/*******************************************************************************
 * mod_menu_restart.cpp
 * 
 * WINAMP RESTART MENU MODULE
 * Модуль добавления пункта "Перезапуск" в меню Winamp
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * This module adds a "Restart Winamp" menu item to the main Winamp menu,
 * allowing users to quickly restart the application without manually closing
 * and reopening it. The module handles menu manipulation, window procedure
 * subclassing, and proper cleanup.
 * 
 * Этот модуль добавляет пункт меню "Перезапустить Winamp" в главное меню
 * Winamp, позволяя пользователям быстро перезапустить приложение без ручного
 * закрытия и повторного открытия. Модуль обрабатывает манипуляции с меню,
 * подмену оконной процедуры и корректную очистку.
 * 
 * HOW IT WORKS / КАК ЭТО РАБОТАЕТ:
 * 1. Retrieves the Winamp main menu handle
 * 2. Inserts a new menu item with a unique ID
 * 3. Subclasses the Winamp window to intercept menu commands
 * 4. When the restart item is clicked, sends IPC_RESTARTWINAMP message
 * 5. Properly removes the menu item and restores window procedure on cleanup
 * 
 * 1. Получает дескриптор главного меню Winamp
 * 2. Вставляет новый пункт меню с уникальным идентификатором
 * 3. Подменяет оконную процедуру Winamp для перехвата команд меню
 * 4. При нажатии на пункт перезапуска отправляет сообщение IPC_RESTARTWINAMP
 * 5. Корректно удаляет пункт меню и восстанавливает оконную процедуру при очистке
 * 
 ******************************************************************************/

#include "mod_menu_restart.h"
#include "mod_startup_foreground.h" 
#include <windows.h>
#include "..\SDK\wa_ipc.h"
#include "..\SwitchLangUI.h"

// Compatibility macros for older Windows SDK versions
// Макросы совместимости для старых версий Windows SDK
#ifndef GetWindowLongPtrA
#define GetWindowLongPtrA GetWindowLongA
#endif
#ifndef SetWindowLongPtrA
#define SetWindowLongPtrA SetWindowLongA
#endif

/*******************************************************************************
 * GLOBAL VARIABLES / ГЛОБАЛЬНЫЕ ПЕРЕМЕННЫЕ
 ******************************************************************************/

// Pointer to the original Winamp window procedure before subclassing
// Указатель на оригинальную оконную процедуру Winamp до подмены
static WNDPROC s_oldWndProc = NULL;

// Unique menu item identifier for the restart command
// Уникальный идентификатор пункта меню для команды перезапуска
static const UINT MENU_ID = 0xE901;

/*******************************************************************************
 * GetWinampMenu
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Retrieves the handle to Winamp's main menu using the Winamp IPC mechanism.
 * Uses a fallback strategy to ensure the menu is obtained.
 * 
 * Получает дескриптор главного меню Winamp, используя механизм IPC Winamp.
 * Использует резервную стратегию для гарантии получения меню.
 * 
 * PARAMETERS / ПАРАМЕТРЫ:
 * hwndWinamp - Handle to the main Winamp window
 *              Дескриптор главного окна Winamp
 * 
 * RETURNS / ВОЗВРАЩАЕТ:
 * HMENU - Handle to the Winamp menu, or NULL if not available
 *         Дескриптор меню Winamp или NULL, если недоступно
 * 
 * STRATEGY / СТРАТЕГИЯ:
 * 1. First attempt: Request menu with parameter 0 (main menu)
 * 2. Fallback: Request menu with parameter 2 (alternative menu)
 * 
 * 1. Первая попытка: запрос меню с параметром 0 (главное меню)
 * 2. Резервный вариант: запрос меню с параметром 2 (альтернативное меню)
 ******************************************************************************/
static HMENU GetWinampMenu(HWND hwndWinamp)
{
    // First attempt: Get main menu (parameter 0)
    // Первая попытка: получить главное меню (параметр 0)
    HMENU m = (HMENU)SendMessageA(hwndWinamp, WM_WA_IPC, 0, IPC_GET_HMENU);
    
    // Fallback: Try alternative menu request (parameter 2)
    // Резервный вариант: попытка альтернативного запроса меню (параметр 2)
    if (!m) m = (HMENU)SendMessageA(hwndWinamp, WM_WA_IPC, 2, IPC_GET_HMENU);
    
    return m;
}

/*******************************************************************************
 * WinampSubclassProc
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Custom window procedure that intercepts messages sent to the Winamp window.
 * Specifically handles the restart menu command and forwards startup messages.
 * 
 * Пользовательская оконная процедура, которая перехватывает сообщения,
 * отправляемые в окно Winamp. Специально обрабатывает команду перезапуска
 * из меню и перенаправляет сообщения запуска.
 * 
 * PARAMETERS / ПАРАМЕТРЫ:
 * hwnd - Handle to the window receiving the message
 *        Дескриптор окна, получающего сообщение
 * msg  - Message identifier
 *        Идентификатор сообщения
 * wp   - First message parameter (wParam)
 *        Первый параметр сообщения (wParam)
 * lp   - Second message parameter (lParam)
 *        Второй параметр сообщения (lParam)
 * 
 * RETURNS / ВОЗВРАЩАЕТ:
 * LRESULT - Message processing result
 *           Результат обработки сообщения
 * 
 * LOGIC / ЛОГИКА:
 * 1. First, check if startup module needs to handle the message
 * 2. Check if it's a menu command (WM_COMMAND/WM_SYSCOMMAND) with our ID
 * 3. If it's our restart command, trigger Winamp restart via IPC
 * 4. Otherwise, forward the message to the original window procedure
 * 
 * 1. Сначала проверить, нужно ли модулю запуска обработать сообщение
 * 2. Проверить, является ли это командой меню с нашим идентификатором
 * 3. Если это наша команда перезапуска, вызвать перезапуск через IPC
 * 4. Иначе передать сообщение оригинальной оконной процедуре
 ******************************************************************************/
static LRESULT CALLBACK WinampSubclassProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    // Allow startup module to handle messages first (e.g., for foreground activation)
    // Позволить модулю запуска обработать сообщения первым (например, для активации на переднем плане)
    if (Startup_OnWinampMessage(hwnd, msg, wp, lp))
        return 0;
    
    // Check if this is a menu command message with our restart menu ID
    // Проверка, является ли это сообщением команды меню с нашим идентификатором перезапуска
    if ((msg == WM_COMMAND || msg == WM_SYSCOMMAND) && LOWORD(wp) == MENU_ID)
    {
        // Trigger Winamp restart using the official IPC mechanism
        // Запуск перезапуска Winamp с использованием официального механизма IPC
        SendMessageA(hwnd, WM_WA_IPC, 0, IPC_RESTARTWINAMP);
        return 0;
    }
    
    // Forward all other messages to the original window procedure
    // Передача всех остальных сообщений оригинальной оконной процедуре
    return CallWindowProcA(s_oldWndProc, hwnd, msg, wp, lp);
}

/*******************************************************************************
 * HookWndProc
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Subclasses the Winamp window by replacing its window procedure with our
 * custom procedure. Saves the original procedure for later restoration.
 * 
 * Подменяет окно Winamp, заменяя его оконную процедуру на нашу
 * пользовательскую процедуру. Сохраняет оригинальную процедуру для
 * последующего восстановления.
 * 
 * PARAMETERS / ПАРАМЕТРЫ:
 * hwnd - Handle to the Winamp window to subclass
 *        Дескриптор окна Winamp для подмены
 * 
 * NOTES / ПРИМЕЧАНИЯ:
 * This function performs window subclassing, a technique where we intercept
 * messages before they reach the original window procedure. This allows us
 * to add custom behavior without modifying Winamp's code.
 * 
 * Эта функция выполняет подмену окна (subclassing), технику, при которой мы
 * перехватываем сообщения до того, как они достигнут оригинальной оконной
 * процедуры. Это позволяет добавить пользовательское поведение без изменения
 * кода Winamp.
 ******************************************************************************/
static void HookWndProc(HWND hwnd)
{
    // Save the original window procedure for later restoration
    // Сохранение оригинальной оконной процедуры для последующего восстановления
    s_oldWndProc = (WNDPROC)(LONG_PTR)GetWindowLongPtrA(hwnd, GWL_WNDPROC);
    
    // Replace the window procedure with our custom subclass procedure
    // Замена оконной процедуры на нашу пользовательскую подменённую процедуру
    SetWindowLongPtrA(hwnd, GWL_WNDPROC, (LONG_PTR)WinampSubclassProc);
}

/*******************************************************************************
 * UnhookWndProc
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Restores the original Winamp window procedure, removing our subclass.
 * Essential for proper cleanup when the module is unloaded.
 * 
 * Восстанавливает оригинальную оконную процедуру Winamp, удаляя нашу подмену.
 * Критически важно для корректной очистки при выгрузке модуля.
 * 
 * PARAMETERS / ПАРАМЕТРЫ:
 * hwnd - Handle to the Winamp window to restore
 *        Дескриптор окна Winamp для восстановления
 * 
 * NOTES / ПРИМЕЧАНИЯ:
 * This function must be called before the module is unloaded to prevent
 * crashes. If we don't restore the original procedure, Winamp will try to
 * call our procedure after our DLL is unloaded, causing an access violation.
 * 
 * Эта функция должна быть вызвана до выгрузки модуля для предотвращения
 * крашей. Если мы не восстановим оригинальную процедуру, Winamp попытается
 * вызвать нашу процедуру после выгрузки нашей DLL, что вызовет нарушение доступа.
 ******************************************************************************/
static void UnhookWndProc(HWND hwnd)
{
    // Only restore if we actually subclassed the window
    // Восстанавливать только если мы действительно подменили окно
    if (s_oldWndProc)
    {
        // Restore the original window procedure
        // Восстановление оригинальной оконной процедуры
        SetWindowLongPtrA(hwnd, GWL_WNDPROC, (LONG_PTR)s_oldWndProc);
        
        // Clear our stored pointer
        // Очистка нашего сохранённого указателя
        s_oldWndProc = NULL;
    }
}

/*******************************************************************************
 * AddRestartToMenu
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Adds the "Restart Winamp" menu item to the main Winamp menu. Checks if
 * the item already exists to prevent duplicates, and subclasses the window
 * to handle the menu command.
 * 
 * Добавляет пункт меню "Перезапустить Winamp" в главное меню Winamp.
 * Проверяет, существует ли пункт, чтобы предотвратить дубликаты, и подменяет
 * окно для обработки команды меню.
 * 
 * PARAMETERS / ПАРАМЕТРЫ:
 * hwndWinamp - Handle to the main Winamp window
 *              Дескриптор главного окна Winamp
 * 
 * ALGORITHM / АЛГОРИТМ:
 * 1. Get the Winamp menu handle
 * 2. Determine the insertion position (before last item, typically "Close")
 * 3. Check if our menu item already exists (by ID)
 * 4. If it exists, just ensure window is subclassed and return
 * 5. If not, insert the new menu item
 * 6. Redraw the menu bar to show changes
 * 7. Subclass the window to intercept menu commands
 * 
 * 1. Получить дескриптор меню Winamp
 * 2. Определить позицию вставки (перед последним пунктом, обычно "Закрыть")
 * 3. Проверить, существует ли уже наш пункт меню (по идентификатору)
 * 4. Если существует, просто убедиться, что окно подменено, и вернуться
 * 5. Если нет, вставить новый пункт меню
 * 6. Перерисовать строку меню для отображения изменений
 * 7. Подменить окно для перехвата команд меню
 ******************************************************************************/
static void AddRestartToMenu(HWND hwndWinamp)
{
    // Retrieve the main Winamp menu
    // Получение главного меню Winamp
    HMENU hMenu = GetWinampMenu(hwndWinamp);
    if (!hMenu) return;
    
    // Get the total number of items in the menu
    // Получение общего количества пунктов в меню
    const int count = GetMenuItemCount(hMenu);
    
    // Calculate insertion position (before the last item, typically "Close")
    // Вычисление позиции вставки (перед последним пунктом, обычно "Закрыть")
    int insertPos = (count > 0) ? (count - 1) : 0;
    
    // Check if our menu item already exists to prevent duplicates
    // Проверка, существует ли уже наш пункт меню, чтобы предотвратить дубликаты
    for (int i = 0; i < count; ++i) {
        MENUITEMINFOA mii; ZeroMemory(&mii, sizeof(mii));
        mii.cbSize = sizeof(mii); 
        mii.fMask = MIIM_ID; // We only need the menu item ID
                             // Нам нужен только идентификатор пункта меню
        
        // Get menu item information
        // Получение информации о пункте меню
        if (GetMenuItemInfoA(hMenu, i, TRUE, &mii) && mii.wID == MENU_ID) {
            // Item already exists, just ensure window is subclassed
            // Пункт уже существует, просто убедимся, что окно подменено
            HookWndProc(hwndWinamp);
            return;
        }
    }
    
    // Insert the new menu item (MENU_TEXT is defined in SwitchLangUI.h)
    // Вставка нового пункта меню (MENU_TEXT определён в SwitchLangUI.h)
    InsertMenuA(hMenu, insertPos, MF_BYPOSITION | MF_STRING, MENU_ID, MENU_TEXT);
    
    // Redraw the menu bar to reflect the changes
    // Перерисовка строки меню для отражения изменений
    DrawMenuBar(hwndWinamp);
    
    // Subclass the window to intercept our menu command
    // Подмена окна для перехвата нашей команды меню
    HookWndProc(hwndWinamp);
}

/*******************************************************************************
 * RemoveRestartFromMenu
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Removes the "Restart Winamp" menu item from the main menu and restores
 * the original window procedure. Essential cleanup function.
 * 
 * Удаляет пункт меню "Перезапустить Winamp" из главного меню и восстанавливает
 * оригинальную оконную процедуру. Критически важная функция очистки.
 * 
 * PARAMETERS / ПАРАМЕТРЫ:
 * hwndWinamp - Handle to the main Winamp window
 *              Дескриптор главного окна Winamp
 * 
 * ALGORITHM / АЛГОРИТМ:
 * 1. Get the Winamp menu handle
 * 2. If menu not available, still restore window procedure and return
 * 3. Search for our menu item by ID
 * 4. If found, remove it from the menu
 * 5. Redraw the menu bar to show changes
 * 6. Restore the original window procedure
 * 
 * 1. Получить дескриптор меню Winamp
 * 2. Если меню недоступно, всё равно восстановить оконную процедуру и вернуться
 * 3. Найти наш пункт меню по идентификатору
 * 4. Если найден, удалить его из меню
 * 5. Перерисовать строку меню для отображения изменений
 * 6. Восстановить оригинальную оконную процедуру
 ******************************************************************************/
static void RemoveRestartFromMenu(HWND hwndWinamp)
{
    // Get the main Winamp menu
    // Получение главного меню Winamp
    HMENU hMenu = GetWinampMenu(hwndWinamp);
    
    // If menu not available, still unhook the window procedure
    // Если меню недоступно, всё равно снять подмену оконной процедуры
    if (!hMenu) { UnhookWndProc(hwndWinamp); return; }
    
    // Get the number of menu items
    // Получение количества пунктов меню
    const int count = GetMenuItemCount(hMenu);
    
    // Search for our menu item
    // Поиск нашего пункта меню
    for (int i = 0; i < count; ++i) {
        MENUITEMINFOA mii; ZeroMemory(&mii, sizeof(mii));
        mii.cbSize = sizeof(mii);
        mii.fMask = MIIM_ID; // We only need the ID to identify our item
                             // Нам нужен только идентификатор для определения нашего пункта
        
        // Check if this is our menu item
        // Проверка, является ли это нашим пунктом меню
        if (GetMenuItemInfoA(hMenu, i, TRUE, &mii) && mii.wID == MENU_ID) {
            // Remove our menu item
            // Удаление нашего пункта меню
            RemoveMenu(hMenu, i, MF_BYPOSITION);
            break; // Found and removed, no need to continue
                   // Найдено и удалено, нет необходимости продолжать
        }
    }
    
    // Redraw the menu bar to reflect the removal
    // Перерисовка строки меню для отражения удаления
    DrawMenuBar(hwndWinamp);
    
    // Restore the original window procedure (critical for preventing crashes)
    // Восстановление оригинальной оконной процедуры (критично для предотвращения крашей)
    UnhookWndProc(hwndWinamp);
}

/*******************************************************************************
 * Menu_Init
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Public initialization function for the restart menu module. This is the
 * main entry point that should be called when the module is loaded.
 * 
 * Публичная функция инициализации модуля меню перезапуска. Это главная
 * точка входа, которая должна вызываться при загрузке модуля.
 * 
 * PARAMETERS / ПАРАМЕТРЫ:
 * hwndWinamp - Handle to the main Winamp window
 *              Дескриптор главного окна Winamp
 * 
 * NOTES / ПРИМЕЧАНИЯ:
 * This function validates the window handle before proceeding with menu
 * manipulation to ensure safety.
 * 
 * Эта функция проверяет дескриптор окна перед манипуляциями с меню для
 * обеспечения безопасности.
 ******************************************************************************/
void Menu_Init(HWND hwndWinamp)
{
    // Validate window handle before proceeding
    // Проверка дескриптора окна перед продолжением
    if (IsWindow(hwndWinamp))
        AddRestartToMenu(hwndWinamp);
}

/*******************************************************************************
 * Menu_Quit
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Public cleanup function for the restart menu module. This must be called
 * when the module is unloaded or Winamp is shutting down.
 * 
 * Публичная функция очистки модуля меню перезапуска. Должна быть вызвана
 * при выгрузке модуля или завершении работы Winamp.
 * 
 * PARAMETERS / ПАРАМЕТРЫ:
 * hwndWinamp - Handle to the main Winamp window
 *              Дескриптор главного окна Winamp
 * 
 * IMPORTANCE / ВАЖНОСТЬ:
 * CRITICAL: This function must be called to prevent crashes. Failing to
 * restore the window procedure will cause Winamp to call unloaded code.
 * 
 * КРИТИЧНО: Эта функция должна быть вызвана для предотвращения крашей.
 * Неспособность восстановить оконную процедуру приведёт к тому, что Winamp
 * попытается вызвать выгруженный код.
 ******************************************************************************/
void Menu_Quit(HWND hwndWinamp)
{
    // Validate window handle before cleanup
    // Проверка дескриптора окна перед очисткой
    if (IsWindow(hwndWinamp))
        RemoveRestartFromMenu(hwndWinamp);
}