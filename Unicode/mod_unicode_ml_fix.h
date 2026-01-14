/*******************************************************************************
 * mod_unicode_ml_fix.h
 * * MEDIA LIBRARY METADATA FIX HEADER
 * ЗАГОЛОВОК ИСПРАВЛЕНИЯ МЕТАДАННЫХ МЕДИАТЕКИ
 * * PURPOSE / НАЗНАЧЕНИЕ:
 * Interface for the module that intercepts IPC messages to the Media Library
 * to ensure file metadata (Artist, Title) is correctly decoded from ANSI/UTF-8.
 * * Интерфейс модуля, перехватывающего сообщения IPC к Медиатеке, чтобы
 * гарантировать правильное декодирование метаданных (Артист, Название) из ANSI/UTF-8.
 ******************************************************************************/

#pragma once
#include <windows.h>

/*******************************************************************************
 * ML_UnicodeFix_Init
 * * PURPOSE / НАЗНАЧЕНИЕ:
 * Subclasses the main Winamp window to intercept WM_WA_IPC messages related
 * to file information (IPC_GET_EXTENDED_FILE_INFO).
 * * Сабклассит (подменяет) главное окно Winamp для перехвата сообщений WM_WA_IPC,
 * связанных с информацией о файлах (IPC_GET_EXTENDED_FILE_INFO).
 * * PARAMETERS / ПАРАМЕТРЫ:
 * hwndWinamp - Handle to the main Winamp window to subclass.
 * Дескриптор главного окна Winamp для подмены.
 ******************************************************************************/
void ML_UnicodeFix_Init(HWND hwndWinamp);

/*******************************************************************************
 * ML_UnicodeFix_Quit
 * * PURPOSE / НАЗНАЧЕНИЕ:
 * Restores the original window procedure for the main Winamp window.
 * Восстанавливает оригинальную оконную процедуру главного окна Winamp.
 * * CRITICAL / КРИТИЧНО:
 * Essential to call before unload to prevent crashes.
 * Обязательно вызывать перед выгрузкой во избежание крашей.
 ******************************************************************************/
void ML_UnicodeFix_Quit(HWND hwndWinamp);