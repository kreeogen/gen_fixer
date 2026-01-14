/*******************************************************************************
 * mod_mute.cpp
 * 
 * WINAMP MUTE/UNMUTE MODULE
 * Модуль отключения/включения звука в Winamp
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * This module provides global hotkey functionality for muting and unmuting
 * Winamp audio playback. It installs a keyboard hook to monitor the Ctrl+Space
 * key combination and toggles the mute state, saving and restoring the volume
 * level automatically.
 * 
 * Этот модуль предоставляет функциональность глобальной горячей клавиши для
 * отключения и включения звука воспроизведения Winamp. Он устанавливает хук
 * клавиатуры для отслеживания комбинации клавиш Ctrl+Space и переключает
 * состояние отключения звука, автоматически сохраняя и восстанавливая уровень
 * громкости.
 * 
 * HOW IT WORKS / КАК ЭТО РАБОТАЕТ:
 * 1. Installs a keyboard hook for the Winamp thread
 * 2. Monitors for Ctrl+Space key combination
 * 3. When detected, saves current volume and sets it to 0 (mute)
 * 4. When pressed again, restores the previously saved volume (unmute)
 * 5. Uses Winamp's IPC mechanism to query and control volume
 * 
 * 1. Устанавливает хук клавиатуры для потока Winamp
 * 2. Отслеживает комбинацию клавиш Ctrl+Space
 * 3. При обнаружении сохраняет текущую громкость и устанавливает её в 0 (отключение звука)
 * 4. При повторном нажатии восстанавливает ранее сохранённую громкость (включение звука)
 * 5. Использует механизм IPC Winamp для запроса и управления громкостью
 * 
 * HOTKEY / ГОРЯЧАЯ КЛАВИША:
 * Ctrl + Space - Toggle mute/unmute
 *                Переключение отключения/включения звука
 * 
 ******************************************************************************/

#include "mod_mute.h"

/*******************************************************************************
 * GLOBAL VARIABLES / ГЛОБАЛЬНЫЕ ПЕРЕМЕННЫЕ
 ******************************************************************************/

// Handle to the keyboard hook that monitors key presses
// Дескриптор хука клавиатуры, который отслеживает нажатия клавиш
static HHOOK g_kbHook = NULL;

// Saved volume level before muting (0-255 range in Winamp)
// Сохранённый уровень громкости до отключения звука (диапазон 0-255 в Winamp)
static int   g_savedVolume = 0;

// Current mute state: TRUE if muted, FALSE if unmuted
// Текущее состояние отключения звука: TRUE если звук отключён, FALSE если включён
static BOOL  g_isMuted = FALSE;

/*******************************************************************************
 * QueryVolume
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Queries the current volume level from Winamp using the IPC mechanism.
 * 
 * Запрашивает текущий уровень громкости из Winamp с использованием механизма IPC.
 * 
 * RETURNS / ВОЗВРАЩАЕТ:
 * int - Current volume level (0-255, where 0 is muted and 255 is maximum)
 *       Текущий уровень громкости (0-255, где 0 - звук отключён, 255 - максимум)
 * 
 * IMPLEMENTATION / РЕАЛИЗАЦИЯ:
 * Uses WM_WA_IPC with IPC_SETVOLUME command and special parameter -666 to
 * query (rather than set) the volume. This is part of Winamp's IPC API.
 * 
 * Использует WM_WA_IPC с командой IPC_SETVOLUME и специальным параметром -666
 * для запроса (а не установки) громкости. Это часть IPC API Winamp.
 ******************************************************************************/
static int QueryVolume()
{
    // Send IPC message to Winamp to query current volume
    // Parameter -666 is a special value that means "query current volume"
    // Отправка IPC-сообщения в Winamp для запроса текущей громкости
    // Параметр -666 - это специальное значение, означающее "запросить текущую громкость"
    return (int)SendMessageA(FindWinamp(), WM_WA_IPC, (WPARAM)-666, IPC_SETVOLUME);
}

/*******************************************************************************
 * SetVolumeVal
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Sets the Winamp volume to a specific level using the IPC mechanism.
 * 
 * Устанавливает громкость Winamp на определённый уровень с использованием
 * механизма IPC.
 * 
 * PARAMETERS / ПАРАМЕТРЫ:
 * v - Volume level to set (0-255, where 0 is muted and 255 is maximum)
 *     Уровень громкости для установки (0-255, где 0 - отключён, 255 - максимум)
 * 
 * IMPLEMENTATION / РЕАЛИЗАЦИЯ:
 * Uses WM_WA_IPC with IPC_SETVOLUME command. When wParam is not -666, it
 * sets the volume to the specified value instead of querying it.
 * 
 * Использует WM_WA_IPC с командой IPC_SETVOLUME. Когда wParam не равен -666,
 * устанавливает громкость на указанное значение вместо запроса.
 ******************************************************************************/
static void SetVolumeVal(int v)
{
    // Send IPC message to Winamp to set volume to specified level
    // Отправка IPC-сообщения в Winamp для установки громкости на указанный уровень
    SendMessageA(FindWinamp(), WM_WA_IPC, (WPARAM)v, IPC_SETVOLUME);
}

/*******************************************************************************
 * ToggleMute
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Toggles between muted and unmuted states, saving and restoring the volume
 * level as needed.
 * 
 * Переключает между состояниями отключённого и включённого звука, сохраняя
 * и восстанавливая уровень громкости по необходимости.
 * 
 * LOGIC / ЛОГИКА:
 * If currently unmuted:
 *   1. Save the current volume level
 *   2. Set volume to 0 (mute)
 *   3. Mark state as muted
 * If currently muted:
 *   1. Restore the previously saved volume level
 *   2. Mark state as unmuted
 * 
 * Если звук в данный момент включён:
 *   1. Сохранить текущий уровень громкости
 *   2. Установить громкость на 0 (отключить звук)
 *   3. Отметить состояние как отключённое
 * Если звук в данный момент отключён:
 *   1. Восстановить ранее сохранённый уровень громкости
 *   2. Отметить состояние как включённое
 ******************************************************************************/
static void ToggleMute()
{
    if (!g_isMuted) {
        // Currently unmuted - save volume and mute
        // В данный момент включён - сохранить громкость и отключить звук
        g_savedVolume = QueryVolume();  // Save current volume / Сохранить текущую громкость
        SetVolumeVal(0);                 // Set volume to 0 (mute) / Установить громкость на 0 (отключить)
        g_isMuted = TRUE;                // Mark as muted / Отметить как отключённый
    }
    else {
        // Currently muted - restore saved volume
        // В данный момент отключён - восстановить сохранённую громкость
        SetVolumeVal(g_savedVolume);     // Restore saved volume / Восстановить сохранённую громкость
        g_isMuted = FALSE;               // Mark as unmuted / Отметить как включённый
    }
}

/*******************************************************************************
 * KeyboardProc
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Keyboard hook callback function that monitors keyboard events and detects
 * the Ctrl+Space key combination to trigger mute/unmute.
 * 
 * Функция обратного вызова хука клавиатуры, которая отслеживает события
 * клавиатуры и обнаруживает комбинацию клавиш Ctrl+Space для переключения
 * отключения/включения звука.
 * 
 * PARAMETERS / ПАРАМЕТРЫ:
 * code   - Hook code that determines how to process the message
 *          Код хука, определяющий способ обработки сообщения
 * wParam - Virtual key code of the pressed key
 *          Код виртуальной клавиши нажатой клавиши
 * lParam - Additional key information (flags, scan code, etc.)
 *          Дополнительная информация о клавише (флаги, скан-код и т.д.)
 * 
 * RETURNS / ВОЗВРАЩАЕТ:
 * LRESULT - TRUE if we handled the key (prevents further processing)
 *           Result from CallNextHookEx for all other keys
 *           TRUE если мы обработали клавишу (предотвращает дальнейшую обработку)
 *           Результат от CallNextHookEx для всех остальных клавиш
 * 
 * KEY DETECTION LOGIC / ЛОГИКА ОБНАРУЖЕНИЯ КЛАВИШ:
 * 1. Check if this is a valid action code (HC_ACTION)
 * 2. Extract key press/release state from lParam (bit 31)
 * 3. Check if it's a key press (not release) of Space key
 * 4. Check if Ctrl key is currently held down
 * 5. If all conditions met, toggle mute and consume the key event
 * 
 * 1. Проверить, является ли это действительным кодом действия (HC_ACTION)
 * 2. Извлечь состояние нажатия/отпускания клавиши из lParam (бит 31)
 * 3. Проверить, является ли это нажатием (не отпусканием) клавиши Space
 * 4. Проверить, удерживается ли в данный момент клавиша Ctrl
 * 5. Если все условия выполнены, переключить отключение звука и поглотить событие клавиши
 ******************************************************************************/
static LRESULT CALLBACK KeyboardProc(int code, WPARAM wParam, LPARAM lParam)
{
    // Only process if this is a valid action code
    // Обрабатывать только если это действительный код действия
    if (code == HC_ACTION)
    {
        // Extract key state: bit 31 = 0 means key down, 1 means key up
        // Извлечение состояния клавиши: бит 31 = 0 означает нажатие, 1 означает отпускание
        const BOOL down = ((lParam & 0x80000000L) == 0);
        
        // Check if: key is being pressed (not released), it's the Space key,
        // and Ctrl is currently held down (high bit of key state is set)
        // Проверка: клавиша нажимается (не отпускается), это клавиша Space,
        // и Ctrl в данный момент удерживается (старший бит состояния клавиши установлен)
        if (down && wParam == VK_SPACE && (GetKeyState(VK_CONTROL) & 0x8000))
        {
            // Ctrl+Space detected - toggle mute state
            // Обнаружено Ctrl+Space - переключить состояние отключения звука
            ToggleMute();
            
            // Return TRUE to consume the key event (prevent default handling)
            // Вернуть TRUE для поглощения события клавиши (предотвращение обработки по умолчанию)
            return TRUE;
        }
    }
    
    // For all other keys or states, pass to the next hook in the chain
    // Для всех остальных клавиш или состояний передать следующему хуку в цепочке
    return CallNextHookEx(g_kbHook, code, wParam, lParam);
}

/*******************************************************************************
 * Mute_Init
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Initializes the mute module by installing the keyboard hook and determining
 * the initial mute state based on current volume.
 * 
 * Инициализирует модуль отключения звука, устанавливая хук клавиатуры и
 * определяя начальное состояние отключения звука на основе текущей громкости.
 * 
 * INITIALIZATION PROCESS / ПРОЦЕСС ИНИЦИАЛИЗАЦИИ:
 * 1. Install keyboard hook if not already installed (thread-specific)
 * 2. Query and save the current volume level
 * 3. Determine initial mute state (consider volume 0 as muted)
 * 
 * 1. Установить хук клавиатуры, если ещё не установлен (для конкретного потока)
 * 2. Запросить и сохранить текущий уровень громкости
 * 3. Определить начальное состояние отключения звука (считать громкость 0 как отключённую)
 * 
 * NOTES / ПРИМЕЧАНИЯ:
 * The hook is thread-specific (using GetCurrentThreadId()), meaning it only
 * monitors keyboard events for the Winamp UI thread, not system-wide.
 * 
 * Хук является потоко-специфичным (используя GetCurrentThreadId()), что означает,
 * что он отслеживает события клавиатуры только для потока пользовательского
 * интерфейса Winamp, а не для всей системы.
 ******************************************************************************/
void Mute_Init(void)
{
    // Install keyboard hook if not already installed
    // Установка хука клавиатуры, если ещё не установлен
    if (!g_kbHook)
        g_kbHook = SetWindowsHookExA(WH_KEYBOARD, KeyboardProc, NULL, GetCurrentThreadId());
    
    // Query current volume and save it
    // Запрос текущей громкости и её сохранение
    g_savedVolume = QueryVolume();
    
    // Determine initial mute state based on current volume
    // If volume is already 0, consider it muted
    // Определение начального состояния отключения звука на основе текущей громкости
    // Если громкость уже 0, считать звук отключённым
    g_isMuted = (g_savedVolume == 0);
}

/*******************************************************************************
 * Mute_Quit
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Cleans up the mute module by removing the keyboard hook. Must be called
 * when the module is unloaded.
 * 
 * Очищает модуль отключения звука, удаляя хук клавиатуры. Должна быть вызвана
 * при выгрузке модуля.
 * 
 * CLEANUP PROCESS / ПРОЦЕСС ОЧИСТКИ:
 * 1. Remove the keyboard hook if it was installed
 * 2. Clear the hook handle
 * 
 * 1. Удалить хук клавиатуры, если он был установлен
 * 2. Очистить дескриптор хука
 * 
 * IMPORTANCE / ВАЖНОСТЬ:
 * CRITICAL: This function must be called to prevent crashes. Failing to
 * remove the hook will cause Windows to call unloaded code when processing
 * keyboard events.
 * 
 * КРИТИЧНО: Эта функция должна быть вызвана для предотвращения крашей.
 * Неспособность удалить хук приведёт к тому, что Windows попытается вызвать
 * выгруженный код при обработке событий клавиатуры.
 * 
 * NOTES / ПРИМЕЧАНИЯ:
 * The function does not restore the volume or change mute state - it only
 * removes the hook. The volume remains as is when the module is unloaded.
 * 
 * Функция не восстанавливает громкость и не изменяет состояние отключения звука -
 * она только удаляет хук. Громкость остаётся как есть при выгрузке модуля.
 ******************************************************************************/
void Mute_Quit(void)
{
    // Remove the keyboard hook if it was installed
    // Удаление хука клавиатуры, если он был установлен
    if (g_kbHook) { 
        UnhookWindowsHookEx(g_kbHook);  // Uninstall the hook / Деинсталляция хука
        g_kbHook = NULL;                 // Clear the handle / Очистка дескриптора
    }
}