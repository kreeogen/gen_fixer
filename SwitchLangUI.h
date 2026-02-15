#pragma once

/*
  BUILD_LANG:
    0 - English
    1 - Russian

  Tip: it is better to define BUILD_LANG in project settings (/D BUILD_LANG=0 or 1)
  and NOT edit this header for every build.
*/

#ifndef BUILD_LANG
#define BUILD_LANG 1
#endif

// If some build config defines BUILD_LANG as a non-0/1 value (e.g. codepage),
// force a safe default so RC.EXE will still see all UI string macros.
#if (BUILD_LANG != 0) && (BUILD_LANG != 1)
#undef BUILD_LANG
#define BUILD_LANG 1
#endif

#ifndef PLUG_VER
#define PLUG_VER "0.8"
#endif

#if BUILD_LANG == 0

// -------------------- ENGLISH --------------------

		#define PLUGIN_NAME "Advanced settings for Winamp v" PLUG_VER
		#define PREFFS_NAME "Advanced"
		#define APPCONFIG "Adds an 'Advanced' item to Preferences\n\n" \
						"Fixes several Winamp 2.xx bugs and applies\n" \
						"small tweaks that are essential for pack authors =)\n\n" \
						"Special for Winamp PE\n" \
						"kreeogen & IFkO (2026)"

		#define APPCONFIG_TITLE "About Fixer v" PLUG_VER

		#define MENU_TEXT "Restart Winamp"

		#ifndef RC_INVOKED
		static const char kDispA[] = "Install Winamp skin";
		#endif

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

		#define TAB_OPT1 "General"
		#define TAB_OPT2 "Fixes"

		#define ADVSET "Advanced settings"

		#define M3U8 "Support .m3u8 and Unicode .pls playlists"
		#define DELSKIN "Delete skins with the Delete key"
		#define PL_MENUS "Use menus instead of buttons in Playlist Editor"
		#define REST_ITEM "Add 'Restart Winamp' menu item"
		#define MUTE "Enable 'Mute' hotkey (Ctrl + Space)"

		#define MLFONT "Sync Playlist and Media Library fonts"
		#define MLICO "Enable Media Library icon tinting"
		#define MLCD "Remove 'CD Ripping' item from Preferences"

		#define PATCHMB "Disable Minibrowser"
		#define PATCHMBSKIN "Prevent skins from opening Minibrowser"

		#define UNITAG "Fix reading of Unicode ID3v2 tags"
		#define UNISTR "Fix Cyrillic stream metadata"
		#define PLSEARCH "Fix Cyrillic search in Playlist Editor"
		#define MLSEARCH "Fix Cyrillic search in Media Library"
		#define VIDEOFONTFIX "Fix Cyrillic text in fullscreen video"

		#define ID3 "Use modern ID3v2 tag saving (recommended)"
		#define STARTUP "Prevent Winamp from starting minimized"
		#define ICONFIX "Show icon in this window s title"
		#define PATCH_URL "Replace 'Get more skins!' with Winamp Skin Museum"
		#define SKININSTALL "Fix Explorer menu item for installing skins"
	
		#define WINDOW_CANCEL      "Cancel"
		#define MENU_RENAME        "Rename Skin\tF2"
		#define RENAME_TITLE       "Rename Skin"
		#define MENU_DELITEM	   "Delete Skin\tDelete"
		#define RENAME_PROMPT      "Enter a new name:"

		#define ALL_PL     	"All playlists (*.pls;*.m3u;*.m3u8)"
		#define PLS_FILES	"PLS Playlists (*.pls)"
		#define M3U_FILES	"M3U/M3U8 Playlists (*.m3u;*.m3u8)"
		#define ALL_FILES	"All files (*.*)"

// ---- GroupBox captions (what you need) ----
#define STAT_1 "Add-ons"
#define STAT_2 "Media Library"
#define STAT_3 "Cyrillic and Unicode fixes"
#define STAT_4 "Other fixes"
#define STAT_5 "Minibrowser"


#define TIP_M3U8        "Enables support for M3U8 and PLS UTF-8 playlists. Fixes encoding issues with Cyrillic text."

#define TIP_SKINDEL     "Adds Delete and Rename buttons to the Skin Selection dialog. Use F2, DEL or Right-Click to do it"

#define TIP_PEZONES     "Enables functional menus in Playlist Editor. Similar to Winamp 5 Modern skins."

#define TIP_RESTART     "Adds 'Restart Winamp' option to the main menu for quick restart."

#define TIP_MUTE        "Enables quick mute/unmute using Ctrl+Space keyboard shortcut."

#define TIP_MLFONT      "Playlist Editor and Media Library font sync. Also enables ClearType font smoothing (Windows XP+ )."

#define TIP_MLICO       "Automatically tints Media Library icons to match the current skin colors."

#define TIP_MLCD        "Remove CD Ripping item from Preferences. Perfect if you don't have CD-ROM anymore."

#define TIP_PATCHMB     "Blocks immediately. Restart Winamp to disable."

#define TIP_PATCHMBSKIN "Some skins can open Minibrowser window, Select to prevent this."

#define TIP_UNITAG      "Fixes ID3v2 tag encoding issues for proper display of Cyrillic and other Unicode."

#define TIP_UNISTR      "Fixes SHOUTcast metadata encoding for proper display of stream info."

#define TIP_PLSEARCH    "Enables Cyrillic character search in Playlist Editor."

#define TIP_MLSEARCH    "Enables Cyrillic character search in Media Library window."

#define TIP_VIDEOFONTFIX "Fixes Cyrillic font rendering issues in video fullscreen mode."

#define TIP_STARTUP     "Forces Winamp window to foreground when starting the application."

#define TIP_ICONFIX     "Fixes Preferences window icon to display correctly." \

#define TIP_ID3         "Prevents Winamp from damaging custom ID3v2 frames."

#define TIP_PATCHURL    "Replaces the broken 'Get More Skins' menu item with a link to the Winamp Skin Museum."

#define TIP_SKININSTALL "Adds 'Install Winamp Skin' option to right-click menu for .wsz files in Explorer."



#elif BUILD_LANG == 1

// -------------------- RUSSIAN --------------------

		#define PLUGIN_NAME "Расширенные настройки Winamp v" PLUG_VER
		#define PREFFS_NAME "Расширенные"

		#define APPCONFIG "Добавляет пункт меню 'Расширенные' в Настройки\n\n"\
						"Фикс некоторых багов Winamp 2.xx и мелкие твики,\n"\
						"которые жизненно необходимы авторам сборки =)\n\n"\
						"Специально для Пиратской Версии\n"\
						"kreeogen & IFkO (2026)"

		#define APPCONFIG_TITLE "О модуле Fixer v" PLUG_VER

		#define MENU_TEXT "Перезапустить Winamp"

		#ifndef RC_INVOKED
		static const char kDispA[] = "Установить обложку Winamp";
		#endif

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

		#define TAB_OPT1 "Дополнения"
		#define TAB_OPT2 "Исправления"

		#define ADVSET "Расширенные настройки"
	
		#define M3U8 "Поддержка плейлистов формата *.M3U8"
		#define DELSKIN "Удаление и смена имён обложек в списке"
		#define PL_MENUS "Контекстные меню вместо кнопок в Плейлисте"
		#define REST_ITEM "Добавить пункт ''Перезапуск'' в меню"
		#define MUTE "Ctrl + Space = Выключение громкости (Mute)"

		#define MLFONT "Шрифт в Медиатеке такой же, как в плейлисте"
		#define MLICO "Тонирование иконок Медиатеки в цвет обложки"
		#define MLCD "Удалить пункт ''Копирование CD'' из Настроек"
		
		#define PATCHMB "Отключить окно Минибраузера"
		#define PATCHMBSKIN "Запретить обложкам открывать Минибраузер"

		#define UNITAG "Кириллица в юникодных тегах ID3v2"
		#define UNISTR "Кириллица в метаданных радиостанций"
		#define PLSEARCH "Поиск русских названий в Плейлисте"
		#define MLSEARCH "Поиск русских названий в Медиатеке"
		#define VIDEOFONTFIX "Кириллица в полноэкранном видео"

		#define ID3 "Корректное сохранение тегов ID3v2 (без потерь)"
		#define STARTUP "Предотвращать запуск Winamp в свёрнутом виде"
		#define ICONFIX "Отображать иконку в заголовке этого окна"
		#define PATCH_URL "Скачивать обложки из Музея обложек Winamp"
		#define SKININSTALL "''Установить обложку Winamp'' в меню Проводника"


		#define WINDOW_CANCEL      "Отмена"
		#define MENU_RENAME        "Переименовать обложку\tF2"
		#define RENAME_TITLE       "Переименование"
		#define MENU_DELITEM	   "Удалить обложку\tDelete"
		#define RENAME_PROMPT      "Введите новое имя:"

		#define ALL_PL      "Все плейлисты (*.pls;*.m3u;*.m3u8)"
		#define PLS_FILES	"PLS плейлисты (*.pls)"
		#define M3U_FILES	"M3U/M3U8 плейлисты (*.m3u;*.m3u8)"
		#define ALL_FILES	"Все файлы (*.*)"


// ---- GroupBox captions (what you need) ----
#define STAT_1 "Общие"
#define STAT_2 "Медиатека"
#define STAT_3 "Исправления кодировок"
#define STAT_4 "Прочие"
#define STAT_5 "Минибраузер"

#define TIP_M3U8        "Включает поддержку плейлистов M3U8 и PLS UTF-8."

#define TIP_SKINDEL     "Добавляет контекстное меню в диалог выбора обложки.\r\n" \
                        "Используйте F2, DEL или правый клик мыши."

#define TIP_PEZONES     "Включает меню вместо кнопок в редакторе плейлиста в стиле Modern-обложек Winamp5."

#define TIP_RESTART     "Добавляет опцию «Перезапустить Winamp» в главное меню для быстрого перезапуска."

#define TIP_MUTE        "Включает быстрое отключение/включение звука сочетанием клавиш Ctrl+Пробел."

#define TIP_MLFONT      "Синхронизирует шрифт Медиатеки с Плейлистом. Также, включает сглаживание ClearType (WindowsXP и новее)."

#define TIP_MLICO       "Автоматически подкрашивает иконки Медиатеки под цвета текущей обложки."

#define TIP_MLCD        "Убирает из окна Настроек пункт ''Копирование CD''. Идеально, если у вас нет CD-ROM привода в системе."

#define TIP_PATCHMB     "Полностью отключает создание окна браузера. Требуется перезапуск Winamp для отмены."

#define TIP_PATCHMBSKIN "Некоторые обложки принудительно открывают браузер. Выберите эту опцию, чтобы отключмть это."

#define TIP_UNITAG      "Исправляет кодировку тегов ID3v2 для корректного отображения кириллицы и Unicode."

#define TIP_UNISTR      "Исправляет кодировку метаданных SHOUTcast для корректного отображения кириллицы при воспроизведении интернет-радио."

#define TIP_PLSEARCH    "Включает поиск по русским символам в окне Плейлиста."

#define TIP_MLSEARCH    "Включает поддержку кириллицы при поиске в окне Медиатеки."

#define TIP_VIDEOFONTFIX "Исправляет отображение кириллицы в интерфейсе полноэкранного режима видео."

#define TIP_STARTUP     "Принудительно выводит окно Winamp на передний план при запуске приложения предотвращая баг с запуском в свернутом состоянии."

#define TIP_ICONFIX     "Восстанавливает исчезающую иконку в заголовке окна Настроек."

#define TIP_ID3         "Исправляет сохранение тегов, защищая нестандартные поля ID3v2 от повреждения Winamp'ом (а он так часто делает)."

#define TIP_PATCHURL    "Заменяет нерабочую ссылку на сайт Winamp «Скачать обложки» в меню, ссылкой на Winamp Skin Museum."

#define TIP_SKININSTALL "Добавляет пункт «Установить обложку Winamp» в контекстное меню Проводника для файлов .wsz вместо простого «Install»."

#endif