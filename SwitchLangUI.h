#pragma once

/*
  BUILD_LANG:
    0 - English
    1 - Russian
*/

#ifndef BUILD_LANG
#define BUILD_LANG 0
#endif


#ifndef PLUG_VER
#define PLUG_VER "0.8"
#endif

#if BUILD_LANG == 0

// -------------------- ENGLISH --------------------

		#define PLUGIN_NAME "Advanced settings for Winamp v" PLUG_VER
		#define PREFFS_NAME "Advanced"
		#define APPCONFIG "Adds an 'Advanced' item to Preferences\n\n" \
						"Fixes several Winamp 2.95 bugs and applies\n" \
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
#define TIP_PEZONES     "Enables functional zones in Playlist Editor. Similar to Winamp 5 Modern skins."
#define TIP_RESTART     "Adds 'Restart Winamp' option to the main menu for quick restart."
#define TIP_MUTE        "Enables quick mute/unmute using Ctrl+Space keyboard shortcut."
#define TIP_MLFONT      "Also enables ClearType font smoothing in Media Library for better readability."
#define TIP_MLICO       "Automatically tints Media Library icons to match the current skin colors."
#define TIP_MLCD        "Remove CD Ripping item from Preferences. Perfect if you don't have CD-ROM anymore."
#define TIP_PATCHMB     "Blocks immediately. Restart Winamp to disable."
#define TIP_PATCHMBSKIN "Some skins can open Minibrowser window. Select to prevent this."
#define TIP_UNITAG      "Fixes ID3v2 tag encoding issues for proper display of Cyrillic and other Unicode."
#define TIP_UNISTR      "Fixes SHOUTcast metadata encoding for proper display of stream info."
#define TIP_PLSEARCH    "Enables Cyrillic character search in main playlist window."
#define TIP_MLSEARCH    "Enables Cyrillic character search in Media Library window."
#define TIP_VIDEOFONTFIX "Fixes Cyrillic font rendering issues in video fullscreen mode."
#define TIP_STARTUP     "Forces Winamp window to foreground when starting the application."
#define TIP_ICONFIX     "Fixes Preferences window icon to display correctly." \
#define TIP_ID3         "Prevents Winamp from damaging custom ID3v2 frames."
#define TIP_PATCHURL    "Replaces the broken 'Get More Skins' menu item with a link to the Winamp Skin Museum."
#define TIP_SKININSTALL "Adds 'Install Winamp Skin' option to right-click menu for .wsz files in Explorer."


#elif BUILD_LANG == 1

// -------------------- RUSSIAN --------------------

		#define PLUGIN_NAME "                      Winamp v" PLUG_VER
		#define PREFFS_NAME "           "

		#define APPCONFIG "                     '           '            \n\n"\
						"                     Winamp 2.95               ,\n"\
						"                                           =)\n\n"\
						"                               \n"\
						"kreeogen & IFkO (2026)"

		#define APPCONFIG_TITLE "         Fixer v" PLUG_VER

		#define MENU_TEXT "              Winamp"

		#ifndef RC_INVOKED
		static const char kDispA[] = "                   Winamp";
		#endif

		#define ADD_FILE "             ( )\tL"
		#define ADD_FOLDER "              \tShift+L"
		#define ADD_LOCATION "         URL       \tCtrl+L"

		#define REM_SEL "                 \tDel"
		#define CROP_SEL "          ,                 \tCtrl+Del"
		#define NEW_PL "               (        )\tCtrl+Shift+Del"
		#define RMV_DEAD "                     \tAlt+Delete"
		#define DEL_ALL "                               "
		#define SUB_DEL "        "

		#define SEL_ALL "           \tCtrl+A"
		#define SEL_NONE "               \tCtrl+Alt+D"
		#define INVERT "                       \tCtrl+I"

		#define FILE_INFO "              \tAlt+3"
		#define ENTRY "                     \tCtrl+E"
		#define INFO "          "

		#define SORT_TIT "                       \tCtrl+Shift+1"
		#define SORT_NAM "                          \tCtrl+Shift+2"
		#define SORT_FOL "                     \tCtrl+Shift+3"
		#define SORT_REV "                  \tCtrl+R"
		#define SORT_RAN "                   \tCtrl+Shift+R"
		#define SORTALL "           "

		#define HTML_PL "        HTML               \tCtrl+Alt+G"
		#define EXT_INFO "                                        \tCtrl+Alt+E"
		#define MISC "      "

		#define NEW_PL2 "              \tCtrl+N"
		#define OPEN_PL "                \tCtrl+O"
		#define SAVE_PL "                  \tCtrl+S"

		#define TAB_OPT1 "          "
		#define TAB_OPT2 "           "

		#define ADVSET "                     "
	
		#define M3U8 "                             *.M3U8"
		#define DELSKIN "                                     "
		#define PL_MENUS "                                          "
		#define REST_ITEM "               ''          ''       "
		#define MUTE "Ctrl + Space =                      (Mute)"

		#define MLFONT "                          ,                "
		#define MLICO "                                           "
		#define MLCD "              ''            CD''            "
		
		#define PATCHMB "                           "
		#define PATCHMBSKIN "                                        "

		#define UNITAG "                            ID3v2"
		#define UNISTR "                                   "
		#define PLSEARCH "                                  "
		#define MLSEARCH "                                  "
		#define VIDEOFONTFIX "                               "

		#define ID3 "                            ID3v2 (          )"
		#define STARTUP "                     Winamp                "
		#define ICONFIX "                                        "
		#define PATCH_URL "                                   Winamp"
		#define SKININSTALL "''                   Winamp''                  "


		#define WINDOW_CANCEL      "      "
		#define MENU_RENAME        "                     \tF2"
		#define RENAME_TITLE       "              "
		#define MENU_DELITEM	   "               \tDelete"
		#define RENAME_PROMPT      "                 :"

		#define ALL_PL      "              (*.pls;*.m3u;*.m3u8)"
		#define PLS_FILES	"PLS           (*.pls)"
		#define M3U_FILES	"M3U/M3U8           (*.m3u;*.m3u8)"
		#define ALL_FILES	"          (*.*)"


// ---- GroupBox captions (what you need) ----
#define STAT_1 "     "
#define STAT_2 "         "
#define STAT_3 "                     "
#define STAT_4 "      "
#define STAT_5 "           "

#define TIP_M3U8        "                              M3U8   PLS UTF-8.                                           ."
#define TIP_SKINDEL     "                                                  .             F2, Delete                       ."
#define TIP_PEZONES     "                                                             Modern         Winamp5."
#define TIP_RESTART     "                               Winamp                            ."
#define TIP_MUTE        "                           /                                  Ctrl+      ."
#define TIP_MLFONT      "                                                          ,                      ClearType            WinXP+."
#define TIP_MLICO       "                                                                     ."
#define TIP_MLCD        "                               ''            CD''.         ,                CD-ROM                  ."
#define TIP_PATCHMB     "                                          .                  ,                                   Winamp."
#define TIP_PATCHMBSKIN "                                                       .                   ,                    ."
#define TIP_UNITAG      "                           ID3v2                                         Unicode."
#define TIP_UNISTR      "                                SHOUTcast                                                 -     ."
#define TIP_PLSEARCH    "                                                           ."
#define TIP_MLSEARCH    "                                                        ."
#define TIP_VIDEOFONTFIX "                                                                         ."
#define TIP_STARTUP     "                           Winamp                                                                                          ."
#define TIP_ICONFIX     "                                                           ."
#define TIP_ID3         "                           ,                            ID3v2                Winamp'  ."
#define TIP_PATCHURL    "                                                          Winamp           Winamp Skin Museum."
#define TIP_SKININSTALL "                                    Winamp                                           .wsz         Install ."

#endif
