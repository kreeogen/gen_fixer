/*******************************************************************************
 * mod_startup_foreground.h
 * * STARTUP FOREGROUND ENFORCEMENT HEADER
 * ЗАГОЛОВОК ПРИНУДИТЕЛЬНОГО ВЫВОДА НА ПЕРЕДНИЙ ПЛАН
 * * PURPOSE / НАЗНАЧЕНИЕ:
 * Interface for the module that ensures Winamp always starts in the foreground
 * and is never minimized during the startup phase. It fights against Windows'
 * background launching behavior.
 * * Интерфейс модуля, который гарантирует, что Winamp всегда запускается на
 * переднем плане и никогда не сворачивается во время фазы запуска. Он борется
 * с поведением Windows по запуску приложений в фоне.
 ******************************************************************************/

#pragma once
#include "..\Resources\plugin_common.h"

/*******************************************************************************
 * Startup_Init
 * * PURPOSE / НАЗНАЧЕНИЕ:
 * Initializes the foreground enforcement. Starts a timer that periodically
 * checks the window state and forces it to the top during the first few seconds.
 * * Инициализирует принудительный вывод на передний план. Запускает таймер,
 * который периодически проверяет состояние окна и выводит его наверх в течение
 * первых нескольких секунд.
 ******************************************************************************/
void Startup_Init(void);

/*******************************************************************************
 * Startup_Quit
 * * PURPOSE / НАЗНАЧЕНИЕ:
 * Stops the monitoring timer and cleans up resources.
 * Останавливает таймер мониторинга и очищает ресурсы.
 ******************************************************************************/
void Startup_Quit(void);

/*******************************************************************************
 * Startup_OnWinampMessage
 * * PURPOSE / НАЗНАЧЕНИЕ:
 * Message hook to intercept window state changes. It catches WM_SIZE (minimized)
 * and WM_SHOWWINDOW to immediately restore the window if it tries to hide.
 * * Хук сообщений для перехвата изменений состояния окна. Ловит WM_SIZE (свернуто)
 * и WM_SHOWWINDOW, чтобы немедленно восстановить окно, если оно пытается скрыться.
 * * PARAMETERS / ПАРАМЕТРЫ:
 * hwnd - Window handle / Дескриптор окна
 * msg  - Message ID / ID сообщения
 * wp   - wParam
 * lp   - lParam
 * * RETURNS / ВОЗВРАЩАЕТ:
 * 1 if the message was handled (blocked minimization), 0 otherwise.
 * 1 если сообщение обработано (сворачивание заблокировано), иначе 0.
 ******************************************************************************/
LRESULT Startup_OnWinampMessage(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);