/*******************************************************************************
 * mod_unicode_fullscreen_video_fix.h
 * * VIDEO WINDOW FONT FIX HEADER
 * ЗАГОЛОВОК ИСПРАВЛЕНИЯ ШРИФТОВ ОКНА ВИДЕО
 * * PURPOSE / НАЗНАЧЕНИЕ:
 * Interface for the module that fixes font rendering in Winamp's video plugins
 * (in_dshow, in_nsv), ensuring Cyrillic characters display correctly in OSD.
 * * Интерфейс модуля, исправляющего отрисовку шрифтов в видео-плагинах Winamp
 * (in_dshow, in_nsv), обеспечивая корректное отображение кириллицы в OSD.
 ******************************************************************************/

#pragma once

#include <windows.h>

#ifdef __cplusplus
extern "C" {
#endif

/*******************************************************************************
 * VideoFontFix_Init
 * * PURPOSE / НАЗНАЧЕНИЕ:
 * Initializes the fix. Starts a thread to wait for video plugins to load,
 * then patches their Import Address Table (IAT) to redirect font creation calls.
 * * Инициализирует фикс. Запускает поток для ожидания загрузки видео-плагинов,
 * затем патчит их таблицу импорта (IAT) для перенаправления вызовов создания шрифтов.
 * * PARAMETERS / ПАРАМЕТРЫ:
 * hwndWinamp - Handle to main Winamp window (unused in current implementation but kept for API).
 * Дескриптор главного окна Winamp (не используется в текущей реализации, но оставлен для API).
 ******************************************************************************/
void VideoFontFix_Init(HWND hwndWinamp);

/*******************************************************************************
 * VideoFontFix_Quit
 * * PURPOSE / НАЗНАЧЕНИЕ:
 * Stops the monitoring thread and cleans up resources.
 * Останавливает поток мониторинга и очищает ресурсы.
 ******************************************************************************/
void VideoFontFix_Quit(void);

#ifdef __cplusplus
}

// Overload for void parameter (C++ compatibility)
// Перегрузка для параметра void (совместимость с C++)
void VideoFontFix_Init(void);
#endif