/*******************************************************************************
 * mod_unicode_tags_fix.h
 * * MP3 TAG ENCODING FIX HEADER
 * ЗАГОЛОВОК ИСПРАВЛЕНИЯ КОДИРОВКИ ТЕГОВ MP3
 * * PURPOSE / НАЗНАЧЕНИЕ:
 * Interface for the module that creates a virtual file overlay to fix ID3v2
 * tag encoding (mojibake) on the fly without modifying files on disk.
 * * Интерфейс модуля, который создает виртуальное наложение файлов для
 * исправления кодировки тегов ID3v2 (mojibake) на лету без изменения файлов на диске.
 ******************************************************************************/

#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#ifdef __cplusplus
extern "C" {
#endif

/*******************************************************************************
 * MP3_TagsFix_Init
 * * PURPOSE / НАЗНАЧЕНИЕ:
 * Initializes the tag fix module. Installs hooks on file I/O functions
 * (CreateFileA, ReadFile, etc.) to intercept MP3 file access.
 * * Инициализирует модуль исправления тегов. Устанавливает хуки на функции
 * файлового ввода-вывода (CreateFileA, ReadFile и т.д.) для перехвата доступа к MP3.
 ******************************************************************************/
__declspec(dllexport) void MP3_TagsFix_Init(void);

/*******************************************************************************
 * MP3_TagsFix_Quit
 * * PURPOSE / НАЗНАЧЕНИЕ:
 * Stops the tag fix module. Removes hooks and frees all virtual file contexts
 * and memory buffers.
 * * Останавливает модуль исправления тегов. Удаляет хуки и освобождает все
 * контексты виртуальных файлов и буферы памяти.
 ******************************************************************************/
__declspec(dllexport) void MP3_TagsFix_Quit(void);

#ifdef __cplusplus
}
#endif