/*******************************************************************************
 * mod_iconfix.h
 * * WINDOW ICON FIX HEADER
 * ЗАГОЛОВОК ИСПРАВЛЕНИЯ ИКОНОК ОКОН
 * * PURPOSE / НАЗНАЧЕНИЕ:
 * Fixes missing or incorrect icons in the title bars of Winamp's auxiliary
 * windows (Playlist, Equalizer, etc.) on modern Windows versions.
 * * Исправляет отсутствующие или некорректные иконки в заголовках вспомогательных
 * окон Winamp (Плейлист, Эквалайзер и т.д.) на современных версиях Windows.
 ******************************************************************************/

#pragma once
#include "..\Resources\plugin_common.h"

/*******************************************************************************
 * IconFix_Init
 * * PURPOSE / НАЗНАЧЕНИЕ:
 * Applies correct icons to all Winamp windows.
 * Применяет правильные иконки ко всем окнам Winamp.
 * * PARAMETERS / ПАРАМЕТРЫ:
 * hwndWinamp - Handle to the main Winamp window.
 * Дескриптор главного окна Winamp.
 ******************************************************************************/
void IconFix_Init(HWND hwndWinamp);

/*******************************************************************************
 * IconFix_Quit
 * * PURPOSE / НАЗНАЧЕНИЕ:
 * cleanup resources (if any).
 * Очистка ресурсов (если есть).
 ******************************************************************************/
void IconFix_Quit(void);