/*******************************************************************************
 * mod_ml_fonts.h
 * * MEDIA LIBRARY FONT SMOOTHING HEADER
 * ЗАГОЛОВОК СГЛАЖИВАНИЯ ШРИФТОВ МЕДИАТЕКИ
 * * PURPOSE / НАЗНАЧЕНИЕ:
 * Forces Font Smoothing (ClearType/Antialiasing) for Media Library tree and
 * list views, which often look jagged in older Winamp versions.
 * * Принудительно включает сглаживание шрифтов (ClearType/Antialiasing) для
 * деревьев и списков Медиатеки, которые часто выглядят неровно в старых
 * версиях Winamp.
 ******************************************************************************/

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/*******************************************************************************
 * ML_SmoothFonts_Init
 * * PURPOSE / НАЗНАЧЕНИЕ:
 * Initializes font smoothing hooks.
 * Инициализирует хуки сглаживания шрифтов.
 ******************************************************************************/
void ML_SmoothFonts_Init(void);

/*******************************************************************************
 * ML_SmoothFonts_Quit
 * * PURPOSE / НАЗНАЧЕНИЕ:
 * Restores default font rendering behavior.
 * Восстанавливает стандартное поведение отрисовки шрифтов.
 ******************************************************************************/
void ML_SmoothFonts_Quit(void);

#ifdef __cplusplus
}
#endif