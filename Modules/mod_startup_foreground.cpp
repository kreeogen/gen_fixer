/*******************************************************************************
 * mod_startup_foreground.cpp
 * 
 * WINAMP STARTUP FOREGROUND ENFORCEMENT MODULE
 * Модуль принудительного вывода Winamp на передний план при запуске
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * This module ensures that Winamp always starts in the foreground and is never
 * minimized during the startup phase. It aggressively prevents the window from
 * being minimized and uses multiple techniques to force the window to the
 * foreground, overcoming Windows' various anti-foreground-stealing mechanisms.
 * 
 * Этот модуль гарантирует, что Winamp всегда запускается на переднем плане
 * и никогда не сворачивается во время фазы запуска. Он агрессивно предотвращает
 * сворачивание окна и использует множество техник для принудительного вывода
 * окна на передний план, преодолевая различные механизмы Windows, которые
 * препятствуют "краже" переднего плана.
 * 
 * HOW IT WORKS / КАК ЭТО РАБОТАЕТ:
 * 1. Monitors Winamp window during startup phase (first few seconds)
 * 2. Intercepts WM_SIZE/SIZE_MINIMIZED and WM_SHOWWINDOW messages
 * 3. If minimization is detected, immediately restores the window
 * 4. Periodically attempts to bring Winamp to foreground using a timer
 * 5. Uses multiple fallback techniques if simple methods fail
 * 6. Stops monitoring after window is successfully foregrounded or max attempts reached
 * 
 * 1. Отслеживает окно Winamp во время фазы запуска (первые несколько секунд)
 * 2. Перехватывает сообщения WM_SIZE/SIZE_MINIMIZED и WM_SHOWWINDOW
 * 3. Если обнаружено сворачивание, немедленно восстанавливает окно
 * 4. Периодически пытается вывести Winamp на передний план с помощью таймера
 * 5. Использует множество резервных техник, если простые методы не работают
 * 6. Прекращает мониторинг после успешного вывода окна на передний план или достижения максимального количества попыток
 * 
 * TECHNIQUES USED / ИСПОЛЬЗУЕМЫЕ ТЕХНИКИ:
 * The ForceWinampForeground function employs multiple strategies in sequence:
 * Функция ForceWinampForeground использует множество стратегий последовательно:
 * 
 * 1. Standard API calls (ShowWindow, SetForegroundWindow, etc.)
 *    Стандартные вызовы API (ShowWindow, SetForegroundWindow и т.д.)
 * 
 * 2. Thread input attachment to current foreground window
 *    Прикрепление ввода потока к текущему окну переднего плана
 * 
 * 3. Undocumented SwitchToThisWindow API
 *    Недокументированный API SwitchToThisWindow
 * 
 * 4. TOPMOST window positioning trick
 *    Трюк с позиционированием окна TOPMOST
 * 
 * 5. Alt key simulation to gain focus
 *    Симуляция клавиши Alt для получения фокуса
 * 
 * WHY SO AGGRESSIVE / ПОЧЕМУ ТАК АГРЕССИВНО:
 * Windows has multiple mechanisms to prevent applications from stealing focus,
 * especially since Windows 2000. This module needs to overcome these restrictions
 * during Winamp's startup to ensure proper user experience.
 * 
 * Windows имеет множество механизмов для предотвращения "кражи" фокуса
 * приложениями, особенно начиная с Windows 2000. Этот модуль должен преодолеть
 * эти ограничения во время запуска Winamp для обеспечения правильного
 * пользовательского опыта.
 * 
 ******************************************************************************/

#include "mod_startup_foreground.h"

/*******************************************************************************
 * GLOBAL STATE VARIABLES
 * ГЛОБАЛЬНЫЕ ПЕРЕМЕННЫЕ СОСТОЯНИЯ
 ******************************************************************************/

// Timer handle for periodic foreground enforcement attempts
// Дескриптор таймера для периодических попыток вывода на передний план
static UINT_PTR g_fgTimer = 0;

// Counter for number of foreground enforcement attempts made
// Счётчик количества выполненных попыток вывода на передний план
static int      g_fgTries = 0;

// Flag indicating whether we're still in startup phase
// TRUE = actively preventing minimize and enforcing foreground
// FALSE = startup complete, normal window behavior allowed
// Флаг, указывающий, находимся ли мы всё ещё в фазе запуска
// TRUE = активно предотвращаем сворачивание и выводим на передний план
// FALSE = запуск завершён, разрешено обычное поведение окна
static BOOL     g_startupPhase = TRUE;

/*******************************************************************************
 * CONFIGURATION CONSTANTS
 * КОНСТАНТЫ КОНФИГУРАЦИИ
 ******************************************************************************/

// Maximum number of foreground enforcement attempts before giving up
// Максимальное количество попыток вывода на передний план перед сдачей
#define FG_MAX_TRIES  50

// Timer interval in milliseconds between foreground enforcement attempts
// Интервал таймера в миллисекундах между попытками вывода на передний план
// 50 attempts * 200ms = 10 seconds total monitoring time
// 50 попыток * 200мс = 10 секунд общего времени мониторинга
#define FG_TRY_EVERY  200

/*******************************************************************************
 * FOREGROUND ENFORCEMENT FUNCTIONS
 * ФУНКЦИИ ВЫВОДА НА ПЕРЕДНИЙ ПЛАН
 ******************************************************************************/

/*******************************************************************************
 * ForceWinampForeground
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Aggressively forces Winamp to the foreground using multiple fallback
 * techniques. This function tries progressively more forceful methods until
 * one succeeds or all have been attempted.
 * 
 * Агрессивно выводит Winamp на передний план, используя множество резервных
 * техник. Эта функция пробует последовательно более сильные методы, пока
 * один не сработает или все не будут испробованы.
 * 
 * PARAMETERS / ПАРАМЕТРЫ:
 * wa - Handle to Winamp main window
 *      Дескриптор главного окна Winamp
 * 
 * TECHNIQUE PROGRESSION / ПОСЛЕДОВАТЕЛЬНОСТЬ ТЕХНИК:
 * 
 * 1. STANDARD APPROACH / СТАНДАРТНЫЙ ПОДХОД:
 *    Uses normal Windows API calls (ShowWindow, SetForegroundWindow, etc.)
 *    This works in most cases if no other window is actively demanding focus.
 * 
 *    Использует обычные вызовы Windows API (ShowWindow, SetForegroundWindow и т.д.)
 *    Работает в большинстве случаев, если никакое другое окно не требует фокус.
 * 
 * 2. THREAD INPUT ATTACHMENT / ПРИКРЕПЛЕНИЕ ВВОДА ПОТОКА:
 *    If standard approach fails, attaches our thread input to the thread that
 *    owns the current foreground window. This temporarily "tricks" Windows into
 *    thinking we're the same process, allowing us to set foreground.
 * 
 *    Если стандартный подход не работает, прикрепляет ввод нашего потока к потоку,
 *    владеющему текущим окном переднего плана. Это временно "обманывает" Windows,
 *    заставляя думать, что мы тот же процесс, позволяя нам установить передний план.
 * 
 * 3. SWITCHTOTHISWINDOW / SWITCHTOTHISWINDOW:
 *    Uses undocumented Windows API to forcefully switch to our window. This is
 *    a last-resort method that Windows itself uses internally.
 * 
 *    Использует недокументированный Windows API для принудительного переключения
 *    на наше окно. Это крайний метод, который Windows сама использует внутренне.
 * 
 * 4. TOPMOST TRICK / ТРЮК С TOPMOST:
 *    Temporarily makes window topmost, then removes topmost flag. The side effect
 *    of this operation often brings the window to foreground.
 * 
 *    Временно делает окно поверх всех, затем убирает флаг поверх всех. Побочный
 *    эффект этой операции часто выводит окно на передний план.
 * 
 * 5. ALT KEY SIMULATION / СИМУЛЯЦИЯ КЛАВИШИ ALT:
 *    Simulates Alt key press/release. Windows allows foreground change when Alt
 *    is pressed, as it assumes user interaction.
 * 
 *    Симулирует нажатие/отпускание клавиши Alt. Windows разрешает изменение
 *    переднего плана при нажатой Alt, предполагая взаимодействие пользователя.
 * 
 * NOTES / ПРИМЕЧАНИЯ:
 * Each technique is tried only if the previous one failed. The function checks
 * GetForegroundWindow() after each attempt to see if it succeeded.
 * 
 * Каждая техника пробуется только если предыдущая не сработала. Функция
 * проверяет GetForegroundWindow() после каждой попытки, чтобы увидеть, удалась ли она.
 ******************************************************************************/
static void ForceWinampForeground(HWND wa)
{
    // Validate window handle / Проверить дескриптор окна
    if (!wa) return;
    
    // TECHNIQUE 1: Standard window restoration and foreground setting
    // ТЕХНИКА 1: Стандартное восстановление окна и установка переднего плана
    
    // Restore window if it's minimized (iconic state)
    // Восстановить окно, если оно свёрнуто (состояние значка)
    if (IsIconic(wa)) ShowWindow(wa, SW_RESTORE);
    else              ShowWindow(wa, SW_SHOWNORMAL);
    
    // Standard foreground setting sequence
    // Стандартная последовательность установки переднего плана
    SetForegroundWindow(wa);
    BringWindowToTop(wa);
    SetActiveWindow(wa);
    SetFocus(wa);
    
    // TECHNIQUE 2: Thread input attachment (if technique 1 failed)
    // ТЕХНИКА 2: Прикрепление ввода потока (если техника 1 не сработала)
    
    // Check if window is now in foreground
    // Проверить, находится ли окно теперь на переднем плане
    if (GetForegroundWindow() != wa)
    {
        // Get current foreground window and thread IDs
        // Получить текущее окно переднего плана и идентификаторы потоков
        HWND  fg    = GetForegroundWindow();
        DWORD tidMe = GetCurrentThreadId();
        DWORD tidFG = fg ? GetWindowThreadProcessId(fg, NULL) : 0;
        
        if (tidFG)
        {
            // Attach our thread input to foreground window's thread
            // This makes Windows think we're the same process
            // Прикрепить ввод нашего потока к потоку окна переднего плана
            // Это заставляет Windows думать, что мы тот же процесс
            AttachThreadInput(tidMe, tidFG, TRUE);
            
            // Try setting foreground again (now with attached input)
            // Попытаться установить передний план снова (теперь с прикреплённым вводом)
            SetForegroundWindow(wa);
            BringWindowToTop(wa);
            SetActiveWindow(wa);
            SetFocus(wa);
            
            // Detach thread input / Отсоединить ввод потока
            AttachThreadInput(tidMe, tidFG, FALSE);
        }
    }
    
    // TECHNIQUE 3: SwitchToThisWindow (if technique 2 failed)
    // ТЕХНИКА 3: SwitchToThisWindow (если техника 2 не сработала)
    
    if (GetForegroundWindow() != wa)
    {
        // Dynamically load undocumented SwitchToThisWindow function
        // Динамически загрузить недокументированную функцию SwitchToThisWindow
        typedef void (WINAPI *PFN_SwitchToThisWindow)(HWND, BOOL);
        PFN_SwitchToThisWindow pSTW =
            (PFN_SwitchToThisWindow)GetProcAddress(GetModuleHandleA("user32"), "SwitchToThisWindow");
        
        // Call if function was found / Вызвать, если функция найдена
        if (pSTW) pSTW(wa, TRUE);
    }
    
    // TECHNIQUE 4: TOPMOST positioning trick (if technique 3 failed)
    // ТЕХНИКА 4: Трюк с позиционированием TOPMOST (если техника 3 не сработала)
    
    if (GetForegroundWindow() != wa)
    {
        // Temporarily make window topmost, then remove the flag
        // The side effect often brings window to foreground
        // Временно сделать окно поверх всех, затем убрать флаг
        // Побочный эффект часто выводит окно на передний план
        SetWindowPos(wa, HWND_TOPMOST,   0,0,0,0, SWP_NOMOVE|SWP_NOSIZE);
        SetWindowPos(wa, HWND_NOTOPMOST, 0,0,0,0, SWP_NOMOVE|SWP_NOSIZE);
    }
    
    // TECHNIQUE 5: Alt key simulation (if all else failed)
    // ТЕХНИКА 5: Симуляция клавиши Alt (если всё остальное не сработало)
    
    if (GetForegroundWindow() != wa)
    {
        // Simulate Alt key press and release
        // Windows allows foreground change when Alt is pressed
        // Симулировать нажатие и отпускание клавиши Alt
        // Windows разрешает изменение переднего плана при нажатой Alt
        keybd_event(VK_MENU, 0, 0, 0);
        keybd_event(VK_MENU, 0, KEYEVENTF_KEYUP, 0);
        
        // Try setting foreground one more time
        // Попытаться установить передний план ещё раз
        SetForegroundWindow(wa);
        BringWindowToTop(wa);
    }
}

/*******************************************************************************
 * ForegroundTimerProc
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Timer callback function that periodically attempts to bring Winamp to the
 * foreground during the startup phase. Called every FG_TRY_EVERY milliseconds
 * until successful or max attempts reached.
 * 
 * Функция обратного вызова таймера, которая периодически пытается вывести
 * Winamp на передний план во время фазы запуска. Вызывается каждые
 * FG_TRY_EVERY миллисекунд до успеха или достижения максимального количества попыток.
 * 
 * PARAMETERS / ПАРАМЕТРЫ:
 * Standard timer callback parameters (mostly unused)
 * id - Timer ID (used for killing the timer)
 * 
 * Стандартные параметры обратного вызова таймера (в основном не используются)
 * id - Идентификатор таймера (используется для уничтожения таймера)
 * 
 * TERMINATION CONDITIONS / УСЛОВИЯ ЗАВЕРШЕНИЯ:
 * The timer stops and startup phase ends when:
 * 1. Winamp window is not found (Winamp closed)
 * 2. Window successfully brought to foreground
 * 3. Maximum number of attempts (FG_MAX_TRIES) reached
 * 
 * Таймер останавливается и фаза запуска заканчивается, когда:
 * 1. Окно Winamp не найдено (Winamp закрыт)
 * 2. Окно успешно выведено на передний план
 * 3. Достигнуто максимальное количество попыток (FG_MAX_TRIES)
 * 
 * PROCESS / ПРОЦЕСС:
 * 1. Find Winamp window
 * 2. If not found, cleanup and exit
 * 3. If window is hidden, show it
 * 4. Attempt to force window to foreground
 * 5. Check if successful or max tries reached
 * 6. If either condition met, cleanup and exit
 * 7. Otherwise, wait for next timer tick
 * 
 * 1. Найти окно Winamp
 * 2. Если не найдено, очистить и выйти
 * 3. Если окно скрыто, показать его
 * 4. Попытаться принудительно вывести окно на передний план
 * 5. Проверить, успешно ли или достигнут максимум попыток
 * 6. Если любое условие выполнено, очистить и выйти
 * 7. Иначе ждать следующего тика таймера
 ******************************************************************************/
static VOID CALLBACK ForegroundTimerProc(HWND, UINT, UINT_PTR id, DWORD)
{
    // Try to find Winamp main window / Попытаться найти главное окно Winamp
    HWND wa = FindWinamp();
    
    // If Winamp window not found (closed or not started yet), cleanup and exit
    // Если окно Winamp не найдено (закрыто или ещё не запущено), очистить и выйти
    if (!wa) { 
        KillTimer(NULL, id); 
        g_fgTimer = 0; 
        g_startupPhase = FALSE; 
        return; 
    }
    
    // If window is hidden, show it / Если окно скрыто, показать его
    if (!IsWindowVisible(wa)) 
        ShowWindow(wa, SW_SHOWNORMAL);
    
    // Attempt to force window to foreground using all available techniques
    // Попытаться принудительно вывести окно на передний план, используя все доступные техники
    ForceWinampForeground(wa);
    
    // Check termination conditions / Проверить условия завершения
    // Success: Window is now in foreground
    // OR
    // Give up: Maximum number of attempts reached
    // Успех: окно теперь на переднем плане
    // ИЛИ
    // Сдаться: достигнуто максимальное количество попыток
    if ((GetForegroundWindow() == wa) || (++g_fgTries >= FG_MAX_TRIES))
    {
        // Stop timer and cleanup / Остановить таймер и очистить
        KillTimer(NULL, id);
        g_fgTimer = 0;
        g_startupPhase = FALSE;  // Exit startup phase / Выйти из фазы запуска
    }
    
    // If neither condition met, timer will fire again in FG_TRY_EVERY ms
    // Если ни одно условие не выполнено, таймер сработает снова через FG_TRY_EVERY мс
}

/*******************************************************************************
 * MODULE LIFECYCLE FUNCTIONS
 * ФУНКЦИИ ЖИЗНЕННОГО ЦИКЛА МОДУЛЯ
 ******************************************************************************/

/*******************************************************************************
 * Startup_Init
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Initializes the startup foreground enforcement module. Should be called
 * during Winamp initialization, before the main window is shown.
 * 
 * Инициализирует модуль вывода на передний план при запуске. Должна быть
 * вызвана во время инициализации Winamp, до показа главного окна.
 * 
 * INITIALIZATION PROCESS / ПРОЦЕСС ИНИЦИАЛИЗАЦИИ:
 * 1. Reset attempt counter
 * 2. Set startup phase flag to TRUE
 * 3. Start timer for periodic foreground enforcement
 * 
 * 1. Сбросить счётчик попыток
 * 2. Установить флаг фазы запуска в TRUE
 * 3. Запустить таймер для периодического вывода на передний план
 * 
 * NOTES / ПРИМЕЧАНИЯ:
 * The timer will automatically stop after success or max attempts. Safe to
 * call if timer is already running (check prevents duplicate timers).
 * 
 * Таймер автоматически остановится после успеха или максимального количества
 * попыток. Безопасно вызывать, если таймер уже работает (проверка предотвращает дубликаты).
 ******************************************************************************/
void Startup_Init(void)
{
    // Reset attempt counter / Сбросить счётчик попыток
    g_fgTries = 0;
    
    // Enter startup phase (enable minimize prevention and foreground enforcement)
    // Войти в фазу запуска (включить предотвращение сворачивания и вывод на передний план)
    g_startupPhase = TRUE;
    
    // Start timer if not already running
    // Запустить таймер, если ещё не работает
    if (!g_fgTimer)
        g_fgTimer = SetTimer(NULL, 0, FG_TRY_EVERY, (TIMERPROC)ForegroundTimerProc);
}

/*******************************************************************************
 * Startup_Quit
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Cleans up the startup foreground enforcement module. Should be called when
 * the module is no longer needed or when Winamp is shutting down.
 * 
 * Очищает модуль вывода на передний план при запуске. Должна быть вызвана,
 * когда модуль больше не нужен или когда Winamp завершает работу.
 * 
 * CLEANUP PROCESS / ПРОЦЕСС ОЧИСТКИ:
 * 1. Stop and destroy the timer if it's running
 * 2. Clear timer handle
 * 3. Exit startup phase (disable enforcement)
 * 
 * 1. Остановить и уничтожить таймер, если он работает
 * 2. Очистить дескриптор таймера
 * 3. Выйти из фазы запуска (отключить принуждение)
 * 
 * NOTES / ПРИМЕЧАНИЯ:
 * After calling this function, window minimize will work normally and no
 * foreground enforcement will occur.
 * 
 * После вызова этой функции сворачивание окна будет работать нормально и
 * принуждение к выводу на передний план не будет происходить.
 ******************************************************************************/
void Startup_Quit(void)
{
    // Stop timer if running / Остановить таймер, если работает
    if (g_fgTimer) { 
        KillTimer(NULL, g_fgTimer); 
        g_fgTimer = 0; 
    }
    
    // Exit startup phase / Выйти из фазы запуска
    g_startupPhase = FALSE;
}

/*******************************************************************************
 * MESSAGE INTERCEPTION FUNCTION
 * ФУНКЦИЯ ПЕРЕХВАТА СООБЩЕНИЙ
 ******************************************************************************/

/*******************************************************************************
 * Startup_OnWinampMessage
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Message handler that intercepts Winamp window messages during startup phase
 * to prevent minimization and enforce foreground status. Should be called from
 * Winamp's window procedure (or subclass procedure).
 * 
 * Обработчик сообщений, который перехватывает сообщения окна Winamp во время
 * фазы запуска для предотвращения сворачивания и обеспечения статуса переднего
 * плана. Должен вызываться из оконной процедуры Winamp (или процедуры подмены).
 * 
 * PARAMETERS / ПАРАМЕТРЫ:
 * hwnd - Handle to Winamp window / Дескриптор окна Winamp
 * msg  - Message identifier / Идентификатор сообщения
 * wp   - First message parameter / Первый параметр сообщения
 * lp   - Second message parameter / Второй параметр сообщения
 * 
 * RETURNS / ВОЗВРАЩАЕТ:
 * LRESULT - 1 if message was handled (prevent default processing)
 *           0 if message was not handled (allow default processing)
 *           1 если сообщение было обработано (предотвратить обработку по умолчанию)
 *           0 если сообщение не было обработано (разрешить обработку по умолчанию)
 * 
 * INTERCEPTED MESSAGES / ПЕРЕХВАТЫВАЕМЫЕ СООБЩЕНИЯ:
 * 
 * WM_SIZE with SIZE_MINIMIZED:
 *   Detects when window is being minimized and posts SC_RESTORE command
 *   to immediately restore it. This prevents the minimize animation from
 *   completing.
 * 
 *   Обнаруживает, когда окно сворачивается, и отправляет команду SC_RESTORE
 *   для немедленного восстановления. Это предотвращает завершение анимации сворачивания.
 * 
 * WM_SHOWWINDOW with wp != 0:
 *   Detects when window is being shown and immediately forces it to foreground.
 *   This ensures window appears in foreground, not behind other windows.
 * 
 *   Обнаруживает, когда окно показывается, и немедленно принудительно выводит
 *   его на передний план. Это гарантирует, что окно появляется на переднем
 *   плане, а не за другими окнами.
 * 
 * USAGE / ИСПОЛЬЗОВАНИЕ:
 * This function should be called early in Winamp's window procedure:
 * Эта функция должна вызываться рано в оконной процедуре Winamp:
 * 
 * LRESULT WinampProc(HWND h, UINT m, WPARAM w, LPARAM l) {
 *     if (Startup_OnWinampMessage(h, m, w, l)) return 0;
 *     // ... rest of window procedure
 * }
 * 
 * NOTES / ПРИМЕЧАНИЯ:
 * Only active during startup phase (g_startupPhase == TRUE). After startup
 * completes, this function returns 0 immediately, allowing normal behavior.
 * 
 * Активно только во время фазы запуска (g_startupPhase == TRUE). После
 * завершения запуска эта функция возвращает 0 немедленно, разрешая обычное поведение.
 ******************************************************************************/
LRESULT Startup_OnWinampMessage(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    // Only intercept messages during startup phase
    // Перехватывать сообщения только во время фазы запуска
    if (!g_startupPhase) return 0;
    
    // Intercept WM_SIZE message with SIZE_MINIMIZED parameter
    // Перехватить сообщение WM_SIZE с параметром SIZE_MINIMIZED
    if (msg == WM_SIZE && wp == SIZE_MINIMIZED)
    {
        // Window is being minimized - post restore command to counteract it
        // Окно сворачивается - отправить команду восстановления для противодействия
        // Using PostMessage instead of SendMessage to avoid potential deadlocks
        // Использование PostMessage вместо SendMessage для избежания возможных блокировок
        PostMessageA(hwnd, WM_SYSCOMMAND, SC_RESTORE, 0);
        
        return 1;  // Message handled, prevent default processing
                   // Сообщение обработано, предотвратить обработку по умолчанию
    }
    
    // Intercept WM_SHOWWINDOW message when window is being shown (wp != 0)
    // Перехватить сообщение WM_SHOWWINDOW, когда окно показывается (wp != 0)
    if (msg == WM_SHOWWINDOW && wp != 0)
    {
        // Window is being shown - force it to foreground immediately
        // Окно показывается - принудительно вывести его на передний план немедленно
        ForceWinampForeground(hwnd);
        
        return 1;  // Message handled, prevent default processing
                   // Сообщение обработано, предотвратить обработку по умолчанию
    }
    
    // Message not handled by this module, allow default processing
    // Сообщение не обработано этим модулем, разрешить обработку по умолчанию
    return 0;
}