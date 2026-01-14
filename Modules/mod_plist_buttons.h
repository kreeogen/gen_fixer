/*******************************************************************************
 * mod_plist_buttons.h
 * * PLAYLIST EDITOR MENU ZONES HEADER
 * ЗАГОЛОВОК ЗОН МЕНЮ РЕДАКТОРА ПЛЕЙЛИСТОВ
 * * PURPOSE / НАЗНАЧЕНИЕ:
 * Interface for the module that adds custom clickable zones to the Winamp
 * Playlist Editor. These zones act as buttons to open context menus for
 * common operations (Add, Remove, Select, etc.).
 * * Интерфейс модуля, который добавляет пользовательские кликабельные зоны в
 * редактор плейлистов Winamp. Эти зоны действуют как кнопки для открытия
 * контекстных меню для общих операций (Добавить, Удалить, Выделить и т.д.).
 ******************************************************************************/

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/*******************************************************************************
 * PeZones_Init
 * * PURPOSE / НАЗНАЧЕНИЕ:
 * Initializes the module. Finds the Playlist Editor window and installs a
 * subclass to intercept mouse clicks in specific zones.
 * * Инициализирует модуль. Находит окно редактора плейлистов и устанавливает
 * сабклассинг (подмену процедуры) для перехвата кликов мыши в определенных зонах.
 ******************************************************************************/
void PeZones_Init(void);

/*******************************************************************************
 * PeZones_Quit
 * * PURPOSE / НАЗНАЧЕНИЕ:
 * Shuts down the module. Destroys all created menus and restores the original
 * window procedure for the Playlist Editor.
 * * Выключает модуль. Уничтожает все созданные меню и восстанавливает
 * оригинальную оконную процедуру для редактора плейлистов.
 * * CRITICAL / КРИТИЧНО:
 * Must be called before the plugin is unloaded to prevent crashes (Winamp
 * calling into unloaded code).
 * * Должна быть вызвана перед выгрузкой плагина для предотвращения падений
 * (вызов Winamp'ом выгруженного кода).
 ******************************************************************************/
void PeZones_Quit(void);

/*******************************************************************************
 * PeZones_SetInstance
 * * PURPOSE / НАЗНАЧЕНИЕ:
 * Passes the DLL instance handle to the module and triggers the creation of
 * the popup menus.
 * * Передает дескриптор экземпляра DLL в модуль и инициирует создание
 * всплывающих меню.
 * * PARAMETERS / ПАРАМЕТРЫ:
 * hInst - The HINSTANCE of the plugin DLL (casted to void*).
 * HINSTANCE DLL плагина (приведенный к void*).
 ******************************************************************************/
void PeZones_SetInstance(void* hInst); 

#ifdef __cplusplus
}
#endif