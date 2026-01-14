/*******************************************************************************
 * mod_menu_restart.h
 * * RESTART MENU ITEM HEADER
 * ЗАГОЛОВОК ПУНКТА МЕНЮ ПЕРЕЗАПУСКА
 * * PURPOSE / НАЗНАЧЕНИЕ:
 * Adds a "Restart Winamp" item to the main context menu.
 * Добавляет пункт "Перезапустить Winamp" в главное контекстное меню.
 ******************************************************************************/

#pragma once
#include "..\Resources\plugin_common.h"

/*******************************************************************************
 * Menu_Init
 * * PURPOSE / НАЗНАЧЕНИЕ:
 * Injects the "Restart" menu item into Winamp's main right-click menu.
 * Внедряет пункт меню "Перезапустить" в главное меню Winamp (ПКМ).
 * * PARAMETERS / ПАРАМЕТРЫ:
 * hwndWinamp - Handle to the main Winamp window.
 * Дескриптор главного окна Winamp.
 ******************************************************************************/
void Menu_Init(HWND hwndWinamp);

/*******************************************************************************
 * Menu_Quit
 * * PURPOSE / НАЗНАЧЕНИЕ:
 * Removes the menu item.
 * Удаляет пункт меню.
 ******************************************************************************/
void Menu_Quit(HWND hwndWinamp);