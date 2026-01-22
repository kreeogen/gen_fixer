#pragma once

// ==========================================
// SET PLUGIN LANGUAGE \ НАСТРОЙКА ЯЗЫКА СБОРКИ
// 0 - English
// 1 - Russian
// ==========================================
#define BUILD_LANG 0 
#define PLUG_VER "0.7"
// ==========================================


#if BUILD_LANG == 0
    // ******************************************
    // ENGLISH
    // ******************************************
		#define PLUGIN_NAME "Advanced settings for Winamp v" PLUG_VER
		#define PREFFS_NAME "Advanced"
		#define APPCONFIG "Adds an 'Advanced' item to Preferences\n\n" \
						"Fixes several Winamp 2.xx bugs and applies\n" \
						"small tweaks that are essential for pack authors =)\n\n" \
						"* fixed launching in minimized state\n" \
						"* fixed icon in the Preferences window titlebar\n" \
						"* fixed ID3v2 tag compatibility with modern standards\n" \
						"* fixed Explorer menu when installing .wsz skins\n" \
						"* fixed Cyrillic in ID3v2 Unicode tags\n" \
						"* fixed Cyrillic in internet radio station names\n" \
						"* fixed Cyrillic in fullscreen video mode\n" \
						"* cyrillic search support in the Media Library\n" \
						"+ Media Library & Playlist font synchronization\n" \
						"+ unicode playlist support (*.m3u8, *.pls)\n" \
						"+ added 'Restart' item to the context menu\n" \
						"+ replaced buttons in the playlist window with a menu\n" \
						"+ added ability to delete and rename skins\n" \
						"+ added mute toggle (Ctrl+Space)\n\n" \
						"Special for Winamp PE\n\n" \
						"kreeogen & IFkO (2026)"

		#define APPCONFIG_TITLE "About Fixer v" PLUG_VER

		#define MENU_TEXT "Restart Winamp"

		static const char kDispA[]   = "Install Winamp Skin";

		#define ADD_FILE "Add file(s)\tL"
		#define ADD_FOLDER "Add directory\tShift+L"
		#define ADD_LOCATION "Add location\tCtrl+L"

		#define REM_SEL "Remove selected\tDel"
		#define CROP_SEL "Crop selected\tCtrl+Del"
		#define NEW_PL "New playlist (clear)\tCtrl+Shift+Del"
		#define RMV_DEAD "Remove all &dead files\tAlt+Delete"
		#define DEL_ALL "Physically remove selected file(s)"
		#define SUB_DEL "Remove"

		#define SEL_ALL "Select all\tCtrl+A"
		#define SEL_NONE "Select none\tCtrl+Alt+D"
		#define INVERT "Select none\tCtrl+Alt+D"

		#define FILE_INFO "File info...\tAlt+3"
		#define ENTRY "Playlist entry...\tCtrl+E"
		#define INFO "Info"

		#define SORT_TIT "Sort list by title\tCtrl+Shift+1"
		#define SORT_NAM "Sort list by filename\tCtrl+Shift+2"
		#define SORT_FOL "Sort list by path and filename\tCtrl+Shift+3"
		#define SORT_REV "Reverse list\tCtrl+R"
		#define SORT_RAN "Randomize list\tCtrl+Shift+R"
		#define SORTALL "Sort"

		#define HTML_PL "Generate HTML playlist\tCtrl+Alt+G"
		#define EXT_INFO "Read extended info on selection\tCtrl+Alt+E"
		#define MISC "Misc"

		#define NEW_PL2 "New playlist (clear)\tCtrl+N"
		#define OPEN_PL "Open playlist\tCtrl+O"
		#define SAVE_PL "Save playlist\tCtrl+S"

		#define ADVSET "Advanced settings"
		#define REST_ITEM "Add 'Restart Winamp' menu item"
		#define MUTE "Ctrl + Space = Mute"
		#define DELSKIN "Delete skins with Delete button"
		#define PL_MENUS "Menus instead of buttons in PE"
		#define LOGOINFO "Winamp Fixer for Win98-Win11 \nkreeogen and IFkO for Winamp PE (2026)"

		#define WINDOW_CANCEL      "Cancel"
		#define MENU_RENAME        "Rename Skin\tF2"
		#define RENAME_TITLE       "Rename Skin"
		#define MENU_DELITEM	   "Delete Skin\tDelete"
		#define RENAME_PROMPT      "Enter a new name:"

		#define ALL_PL      "All playlists (*.pls;*.m3u;*.m3u8)"
		#define PLS_FILES	"PLS Playlists (*.pls)"
		#define M3U_FILES	"M3U/M3U8 Playlists (*.m3u;*.m3u8)"
		#define ALL_FILES	"All files (*.*)"


#elif BUILD_LANG == 1
    // ******************************************
    // RUSSIAN
    // ******************************************
		#define PLUGIN_NAME "Расширенные настройки Winamp v" PLUG_VER
		#define PREFFS_NAME "Расширенные"

		#define APPCONFIG "Добавляет пункт меню 'Расширенные' в Настройки\n\n"\
						"Фикс некоторых багов Winamp 2.xx и мелкие твики,\n"\
						"которые жизненно необходимы авторам сборки =)\n\n"\
						"* фикс запуска в свернутом состоянии\n"\
						"* фикс иконки в заголовке окна Настройки\n"\
						"* фикс меню Проводника при установке wsz-обложек\n"\
						"* фикс тегов id3v2 под современные стандарты\n"\
						"* фикс кириллицы в тегах id3v2 unicode\n"\
						"* фикс кириллицы в названиях интернет-радиостанций\n"\
						"* фикс кириллицы в полноэкранном видео-режиме\n"\
						"* фикс поиска кириллических треков в Медиатеке\n"\
						"+ синхронизация шрифтов Медиатеки и Плейлиста\n"\
						"+ поддержка плейлистов в unicode (*m3u8,*pls)\n"\
						"+ добавление пункта 'Перезапуск' в контекстное меню\n"\
						"+ замена кнопок в окне плейлиста на меню\n"\
						"+ добавлена возможность удалять и менять имя обложки\n"\
						"+ добавлена возможность мутить звук (Ctrl+Пробел)\n\n"\
						"Специально для Пиратской Версии\n\n"\
						"kreeogen & IFkO (2026)"

		#define APPCONFIG_TITLE "О модуле Fixer v" PLUG_VER

		#define MENU_TEXT "Перезапустить Winamp"

		static const char kDispA[]   = "Установить обложку Winamp";

		#define ADD_FILE "Добавить файл(ы)\tL"
		#define ADD_FOLDER "Добавить папку\tShift+L"
		#define ADD_LOCATION "Добавить URL ссылку\tCtrl+L"

		#define REM_SEL "Удалить выбранные\tDel"
		#define CROP_SEL "Удалить всё, кроме выбранного\tCtrl+Del"
		#define NEW_PL "Новый плэйлист (очистить)\tCtrl+Shift+Del"
		#define RMV_DEAD "Убрать несуществующие\tAlt+Delete"
		#define DEL_ALL "Стереть выбранные файлы с диска"
		#define SUB_DEL "Удаление"

		#define SEL_ALL "Выделить всё\tCtrl+A"
		#define SEL_NONE "Снять выделение\tCtrl+Alt+D"
		#define INVERT "Инвертировать выделение\tCtrl+I"

		#define FILE_INFO "Свойства файла\tAlt+3"
		#define ENTRY "Изменить путь к файлу\tCtrl+E"
		#define INFO "Информация"

		#define SORT_TIT "Сортировать по названию\tCtrl+Shift+1"
		#define SORT_NAM "Сортировать по имени файла\tCtrl+Shift+2"
		#define SORT_FOL "Сортировать по папкам\tCtrl+Shift+3"
		#define SORT_REV "В обратном порядке\tCtrl+R"
		#define SORT_RAN "Перемешать плейлист\tCtrl+Shift+R"
		#define SORTALL "Сортировать"

		#define HTML_PL "Создать HTML файл плейлиста\tCtrl+Alt+G"
		#define EXT_INFO "Обновление данных композиций в плейлисте\tCtrl+Alt+E"
		#define MISC "Прочее"

		#define NEW_PL2 "Новый плейлист\tCtrl+N"
		#define OPEN_PL "Открыть плейлист\tCtrl+O"
		#define SAVE_PL "Сохранить плейлист\tCtrl+S"

		#define ADVSET "Расширенные настройки"
		#define REST_ITEM "Добавить пункт 'Перезапуск' в контекстное меню"
		#define MUTE "Ctrl + Space = Выключение громкости (функция Mute)"
		#define DELSKIN "Удалять обложки из списка кнопкой Delete (или ПКМ)"
		#define PL_MENUS "Контекстные меню вместо кнопок в плэйлисте"
		#define LOGOINFO "Winamp Fixer для Win98-Win11 \nkreeogen и IFkO для Пиратской Версии (2026)"

		#define WINDOW_CANCEL      "Отмена"
		#define MENU_RENAME        "Переименовать обложку\tF2"
		#define RENAME_TITLE       "Переименование"
		#define MENU_DELITEM	   "Удалить обложку\tDelete"
		#define RENAME_PROMPT      "Введите новое имя:"

		#define ALL_PL      "Все плейлисты (*.pls;*.m3u;*.m3u8)"
		#define PLS_FILES	"PLS плейлисты (*.pls)"
		#define M3U_FILES	"M3U/M3U8 плейлисты (*.m3u;*.m3u8)"
		#define ALL_FILES	"Все файлы (*.*)"

#else
    #error "Language not selected! Check BUILD_LANG in SwitchLangUI.h"
#endif
