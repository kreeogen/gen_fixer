/*******************************************************************************
 * mod_unicode_m3u8.h
 * * M3U8 AND UNICODE PLAYLIST LOADER HEADER
 * ЗАГОЛОВОК ЗАГРУЗЧИКА ПЛЕЙЛИСТОВ M3U8 И UNICODE
 * * PURPOSE / НАЗНАЧЕНИЕ:
 * Interface for the module that enables loading of M3U8 (UTF-8) and Unicode
 * playlists (M3U/PLS) in Winamp 2.95, which natively supports only ANSI.
 * * Интерфейс модуля, включающего загрузку M3U8 (UTF-8) и Unicode плейлистов
 * (M3U/PLS) в Winamp 2.95, который нативно поддерживает только ANSI.
 ******************************************************************************/

#ifndef MOD_PLAYLIST_LOADER_H
#define MOD_PLAYLIST_LOADER_H

#include <windows.h>

#ifdef __cplusplus
extern "C" {
#endif

/*******************************************************************************
 * m3u8_Loader_Init
 * * PURPOSE / НАЗНАЧЕНИЕ:
 * Initializes the playlist loader. Installs IAT hooks for file I/O functions
 * (fopen, GetPrivateProfileString, etc.) to intercept playlist loading.
 * * Инициализирует загрузчик плейлистов. Устанавливает перехватчики IAT для
 * функций файлового ввода-вывода (fopen, GetPrivateProfileString и т.д.)
 * для перехвата загрузки плейлистов.
 ******************************************************************************/
void m3u8_Loader_Init(void);

/*******************************************************************************
 * m3u8_Loader_Quit
 * * PURPOSE / НАЗНАЧЕНИЕ:
 * Shuts down the module and cleans up resources. Deletes all temporary ANSI
 * files created during the session.
 * * Выключает модуль и очищает ресурсы. Удаляет все временные ANSI файлы,
 * созданные во время сеанса.
 * * CRITICAL / КРИТИЧНО:
 * Must be called on exit to prevent temporary file leaks in the system temp folder.
 * Должна вызываться при выходе для предотвращения утечек временных файлов
 * в системной временной папке.
 ******************************************************************************/
void m3u8_Loader_Quit(void);

#ifdef __cplusplus
}
#endif

#endif