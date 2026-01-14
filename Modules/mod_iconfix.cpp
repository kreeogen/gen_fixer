/*******************************************************************************
 * mod_iconfix.cpp
 * 
 * WINAMP SETTINGS ICON FIX MODULE
 * Модуль исправления иконки в заголовке окна Настройки
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * This module ensures that all windows created by Winamp display the correct
 * application icons (both big and small). It addresses the common issue where
 * child windows or dialogs may not inherit the proper icons from the main
 * application window.
 * 
 * Этот модуль гарантирует, что все окна, создаваемые Winamp, отображают
 * правильные иконки приложения (большую и маленькую). Он решает распространённую
 * проблему, когда дочерние окна или диалоги могут не наследовать правильные
 * иконки от главного окна приложения.
 * 
 * HOW IT WORKS / КАК ЭТО РАБОТАЕТ:
 * 1. Captures the main window's icons (big and small)
 * 2. Installs a CBT (Computer-Based Training) hook to monitor window creation
 * 3. Automatically applies the captured icons to any newly created windows
 * 4. Processes all existing windows in the thread
 * 
 * 1. Захватывает иконки главного окна (большую и маленькую)
 * 2. Устанавливает CBT-хук для отслеживания создания окон
 * 3. Автоматически применяет захваченные иконки ко всем вновь создаваемым окнам
 * 4. Обрабатывает все существующие окна в потоке
 * 
 ******************************************************************************/

#include "mod_iconfix.h"
#include <windows.h>

// Compatibility macro for older Windows SDK versions
// Макрос совместимости для старых версий Windows SDK
#ifndef GetClassLongPtrA
#define GetClassLongPtrA GetClassLongA
#endif

/*******************************************************************************
 * GLOBAL VARIABLES / ГЛОБАЛЬНЫЕ ПЕРЕМЕННЫЕ
 ******************************************************************************/

// Handle to the CBT hook that intercepts window creation events
// Дескриптор CBT-хука, который перехватывает события создания окон
static HHOOK g_cbtHook    = NULL;

// Handle to the large (32x32) icon retrieved from the main Winamp window
// Дескриптор большой иконки (32x32), полученной из главного окна Winamp
static HICON g_hBigIcon   = NULL;

// Handle to the small (16x16) icon retrieved from the main Winamp window
// Дескриптор маленькой иконки (16x16), полученной из главного окна Winamp
static HICON g_hSmallIcon = NULL;

// Thread ID of the Winamp UI thread, used to limit hook scope
// Идентификатор потока пользовательского интерфейса Winamp, используется для ограничения области действия хука
static DWORD g_uiThreadId = 0;

/*******************************************************************************
 * GrabMainIcons
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Retrieves the big and small icons from the main Winamp window using multiple
 * fallback strategies to ensure icons are always obtained.
 * 
 * Получает большую и маленькую иконки из главного окна Winamp, используя
 * несколько резервных стратегий для гарантии получения иконок.
 * 
 * PARAMETERS / ПАРАМЕТРЫ:
 * hwndWinamp - Handle to the main Winamp window
 *              Дескриптор главного окна Winamp
 * 
 * STRATEGY / СТРАТЕГИЯ:
 * 1. Try to get icons via WM_GETICON message (preferred method)
 * 2. Fall back to GetClassLongPtr if WM_GETICON returns NULL
 * 3. Use default application icon as last resort
 * 
 * 1. Попытка получить иконки через сообщение WM_GETICON (предпочтительный метод)
 * 2. Использование GetClassLongPtr, если WM_GETICON возвращает NULL
 * 3. Использование стандартной иконки приложения в крайнем случае
 ******************************************************************************/
static void GrabMainIcons(HWND hwndWinamp)
{
    // First attempt: Get icons via WM_GETICON message
    // Первая попытка: получить иконки через сообщение WM_GETICON
    g_hBigIcon   = (HICON)SendMessageA(hwndWinamp, WM_GETICON, ICON_BIG, 0);
    g_hSmallIcon = (HICON)SendMessageA(hwndWinamp, WM_GETICON, ICON_SMALL, 0);
    
    // Second attempt: Get icons from window class if WM_GETICON failed
    // Вторая попытка: получить иконки из класса окна, если WM_GETICON не сработал
    if (!g_hBigIcon)
        g_hBigIcon = (HICON)(UINT_PTR)GetClassLongPtrA(hwndWinamp, GCL_HICON);
    if (!g_hSmallIcon)
        g_hSmallIcon = (HICON)(UINT_PTR)GetClassLongPtrA(hwndWinamp, GCL_HICONSM);
    
    // Last resort: Use default Windows application icon
    // Последнее средство: использовать стандартную иконку приложения Windows
    if (!g_hBigIcon)   g_hBigIcon   = LoadIcon(NULL, IDI_APPLICATION);
    if (!g_hSmallIcon) g_hSmallIcon = LoadIcon(NULL, IDI_APPLICATION);
}

/*******************************************************************************
 * ForceIcons
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Applies the captured icons to a specific window if it doesn't already have
 * icons set. Only processes top-level windows (not child windows).
 * 
 * Применяет захваченные иконки к конкретному окну, если у него ещё не
 * установлены иконки. Обрабатывает только окна верхнего уровня (не дочерние).
 * 
 * PARAMETERS / ПАРАМЕТРЫ:
 * hwnd - Handle to the window to apply icons to
 *        Дескриптор окна, к которому применяются иконки
 * 
 * NOTES / ПРИМЕЧАНИЯ:
 * - Skips invalid window handles
 * - Ignores child windows (WS_CHILD style)
 * - Only sets icons if the window doesn't already have them
 * 
 * - Пропускает недействительные дескрипторы окон
 * - Игнорирует дочерние окна (стиль WS_CHILD)
 * - Устанавливает иконки только если у окна их ещё нет
 ******************************************************************************/
static void ForceIcons(HWND hwnd)
{
    // Validate window handle
    // Проверка дескриптора окна
    if (!IsWindow(hwnd)) return;
    
    // Get window style to check if it's a child window
    // Получение стиля окна для проверки, является ли оно дочерним
    LONG style = GetWindowLongA(hwnd, GWL_STYLE);
    
    // Skip child windows - they typically shouldn't have their own icons
    // Пропуск дочерних окон - у них обычно не должно быть собственных иконок
    if (style & WS_CHILD) return; 
    
    // Set big icon only if the window doesn't already have one
    // Установка большой иконки только если у окна её ещё нет
    if (!SendMessageA(hwnd, WM_GETICON, ICON_BIG, 0))
        SendMessageA(hwnd, WM_SETICON, ICON_BIG,   (LPARAM)g_hBigIcon);
    
    // Set small icon only if the window doesn't already have one
    // Установка маленькой иконки только если у окна её ещё нет
    if (!SendMessageA(hwnd, WM_GETICON, ICON_SMALL, 0))
        SendMessageA(hwnd, WM_SETICON, ICON_SMALL, (LPARAM)g_hSmallIcon);
}

/*******************************************************************************
 * CbtHookProc
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * CBT (Computer-Based Training) hook callback function that intercepts window
 * events. Specifically handles HCBT_CREATEWND to apply icons to newly created
 * windows.
 * 
 * Функция обратного вызова CBT-хука, которая перехватывает события окон.
 * Специально обрабатывает HCBT_CREATEWND для применения иконок к вновь
 * создаваемым окнам.
 * 
 * PARAMETERS / ПАРАМЕТРЫ:
 * nCode  - Hook code that determines how to process the message
 *          Код хука, определяющий способ обработки сообщения
 * wParam - For HCBT_CREATEWND, contains the handle to the new window
 *          Для HCBT_CREATEWND содержит дескриптор нового окна
 * lParam - Additional information about the hooked event
 *          Дополнительная информация о перехваченном событии
 * 
 * RETURNS / ВОЗВРАЩАЕТ:
 * Result from CallNextHookEx, allowing the hook chain to continue
 * Результат от CallNextHookEx, позволяющий продолжить цепочку хуков
 ******************************************************************************/
static LRESULT CALLBACK CbtHookProc(int nCode, WPARAM wParam, LPARAM lParam)
{
    // Check if a new window is being created
    // Проверка, создаётся ли новое окно
    if (nCode == HCBT_CREATEWND)
        ForceIcons((HWND)wParam);
    
    // Pass the hook information to the next hook in the chain
    // Передача информации о хуке следующему хуку в цепочке
    return CallNextHookEx(g_cbtHook, nCode, wParam, lParam);
}

/*******************************************************************************
 * EnumThreadWndProc
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Callback function for EnumThreadWindows. Applies icons to each window
 * enumerated in the Winamp thread.
 * 
 * Функция обратного вызова для EnumThreadWindows. Применяет иконки к каждому
 * окну, перечисленному в потоке Winamp.
 * 
 * PARAMETERS / ПАРАМЕТРЫ:
 * h - Handle to the enumerated window
 *     Дескриптор перечисляемого окна
 * 
 * RETURNS / ВОЗВРАЩАЕТ:
 * TRUE to continue enumeration
 * TRUE для продолжения перечисления
 ******************************************************************************/
static BOOL CALLBACK EnumThreadWndProc(HWND h, LPARAM)
{
    ForceIcons(h);
    return TRUE; // Continue enumeration / Продолжить перечисление
}

/*******************************************************************************
 * IconFix_Init
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Initializes the icon fix module. This is the main entry point that should
 * be called when the module is loaded.
 * 
 * Инициализирует модуль исправления иконок. Это главная точка входа, которая
 * должна вызываться при загрузке модуля.
 * 
 * PARAMETERS / ПАРАМЕТРЫ:
 * hwndWinamp - Handle to the main Winamp window
 *              Дескриптор главного окна Winamp
 * 
 * PROCESS / ПРОЦЕСС:
 * 1. Validates the Winamp window handle
 * 2. Retrieves and stores the main window's icons
 * 3. Gets the thread ID for the Winamp UI thread
 * 4. Installs a CBT hook to monitor future window creation
 * 5. Applies icons to all existing windows in the thread
 * 
 * 1. Проверяет дескриптор окна Winamp
 * 2. Получает и сохраняет иконки главного окна
 * 3. Получает идентификатор потока пользовательского интерфейса Winamp
 * 4. Устанавливает CBT-хук для отслеживания будущего создания окон
 * 5. Применяет иконки ко всем существующим окнам в потоке
 ******************************************************************************/
void IconFix_Init(HWND hwndWinamp)
{
    // Validate that the window handle is valid
    // Проверка, что дескриптор окна действителен
    if (!IsWindow(hwndWinamp)) return;
    
    // Capture the icons from the main Winamp window
    // Захват иконок из главного окна Winamp
    GrabMainIcons(hwndWinamp);
    
    // Get the thread ID that owns the Winamp window
    // Получение идентификатора потока, владеющего окном Winamp
    g_uiThreadId = GetWindowThreadProcessId(hwndWinamp, NULL);
    
    // Install CBT hook if not already installed (thread-specific hook)
    // Установка CBT-хука, если он ещё не установлен (хук для конкретного потока)
    if (!g_cbtHook)
        g_cbtHook = SetWindowsHookExA(WH_CBT, CbtHookProc, NULL, g_uiThreadId);
    
    // Apply icons to all windows that already exist in the thread
    // Применение иконок ко всем окнам, которые уже существуют в потоке
    EnumThreadWindows(g_uiThreadId, EnumThreadWndProc, 0);
}

/*******************************************************************************
 * IconFix_Quit
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Cleans up the icon fix module. Should be called when the module is unloaded
 * or when Winamp is shutting down.
 * 
 * Очищает модуль исправления иконок. Должна вызываться при выгрузке модуля
 * или при завершении работы Winamp.
 * 
 * CLEANUP PROCESS / ПРОЦЕСС ОЧИСТКИ:
 * 1. Removes the CBT hook if it was installed
 * 2. Clears stored icon handles
 * 3. Resets the thread ID
 * 
 * 1. Удаляет CBT-хук, если он был установлен
 * 2. Очищает сохранённые дескрипторы иконок
 * 3. Сбрасывает идентификатор потока
 * 
 * NOTES / ПРИМЕЧАНИЯ:
 * This function does not destroy the icon handles because they belong to
 * the system or the main window, not to this module.
 * 
 * Эта функция не уничтожает дескрипторы иконок, потому что они принадлежат
 * системе или главному окну, а не этому модулю.
 ******************************************************************************/
void IconFix_Quit(void)
{
    // Remove the CBT hook if it was installed
    // Удаление CBT-хука, если он был установлен
    if (g_cbtHook) { UnhookWindowsHookEx(g_cbtHook); g_cbtHook = NULL; }
    
    // Clear icon handles (but don't destroy them - they're not owned by this module)
    // Очистка дескрипторов иконок (но не их уничтожение - они не принадлежат этому модулю)
    g_hBigIcon = g_hSmallIcon = NULL;
    
    // Reset thread ID
    // Сброс идентификатора потока
    g_uiThreadId = 0;
}