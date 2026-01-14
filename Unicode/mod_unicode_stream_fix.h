/*******************************************************************************
 * mod_unicode_stream_fix.h
 * * SHOUTCAST STREAM METADATA FIX HEADER
 * ЗАГОЛОВОК ИСПРАВЛЕНИЯ МЕТАДАННЫХ ПОТОКОВ SHOUTCAST
 * * PURPOSE / НАЗНАЧЕНИЕ:
 * Interface for the module that fixes encoding issues in internet radio
 * metadata (ICY headers) by hooking Winsock functions.
 * * Интерфейс модуля, исправляющего проблемы кодировки в метаданных
 * интернет-радио (ICY заголовки) путём перехвата функций Winsock.
 ******************************************************************************/

#pragma once
#ifdef __cplusplus
extern "C" {
#endif

/*******************************************************************************
 * MP3_StreamFix_Init
 * * PURPOSE / НАЗНАЧЕНИЕ:
 * Installs hooks on Winsock network functions (recv, WSARecv) to detect
 * and fix incoming metadata streams in real-time.
 * * Устанавливает хуки на сетевые функции Winsock (recv, WSARecv) для
 * обнаружения и исправления входящих потоков метаданных в реальном времени.
 ******************************************************************************/
void MP3_StreamFix_Init(void);

/*******************************************************************************
 * MP3_StreamFix_Quit
 * * PURPOSE / НАЗНАЧЕНИЕ:
 * Cleans up resources used by the stream fixer.
 * Очищает ресурсы, используемые фиксером потоков.
 * * NOTE / ПРИМЕЧАНИЕ:
 * Hooks may be left active if safe removal is impossible during playback,
 * but internal states are reset.
 * * Хуки могут остаться активными, если безопасное удаление невозможно во
 * время воспроизведения, но внутренние состояния сбрасываются.
 ******************************************************************************/
void MP3_StreamFix_Quit(void);

#ifdef __cplusplus
}
#endif