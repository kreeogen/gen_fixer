/*******************************************************************************
 * mod_ml_icons.h
 * * MEDIA LIBRARY ICON TINTING HEADER
 * ЗАГОЛОВОК ТОНИРОВАНИЯ ИКОНОК МЕДИАТЕКИ
 * * PURPOSE / НАЗНАЧЕНИЕ:
 * Provides functionality to recolor (tint) standard Media Library icons 
 * to match the current Winamp skin colors (W9x-style theme integration).
 * * Предоставляет функциональность для перекраски (тонирования) стандартных
 * иконок Медиатеки под цвета текущего скина Winamp (интеграция тем в стиле W9x).
 ******************************************************************************/

#pragma once
#include <windows.h>

#ifdef __cplusplus
extern "C" {
#endif

/*******************************************************************************
 * ML_IconsTint_Start
 * * PURPOSE / НАЗНАЧЕНИЕ:
 * Starts the icon tinting engine. Hooks into Media Library painting/loading
 * routines to apply color correction on the fly.
 * * Запускает движок тонирования иконок. Перехватывает процедуры отрисовки/загрузки
 * Медиатеки для применения цветокоррекции на лету.
 * * PARAMETERS / ПАРАМЕТРЫ:
 * hInst - Instance handle of the plugin (used to load resources).
 * Дескриптор экземпляра плагина (используется для загрузки ресурсов).
 * * RETURNS / ВОЗВРАЩАЕТ:
 * TRUE if successful / TRUE в случае успеха
 ******************************************************************************/
BOOL ML_IconsTint_Start(HINSTANCE hInst);

/*******************************************************************************
 * ML_IconsTint_Stop
 * * PURPOSE / НАЗНАЧЕНИЕ:
 * Stops tinting and restores original icons/hooks.
 * Останавливает тонирование и восстанавливает оригинальные иконки/хуки.
 ******************************************************************************/
void ML_IconsTint_Stop(void);

/*******************************************************************************
 * ML_IconsTint_ForceOnce
 * * PURPOSE / НАЗНАЧЕНИЕ:
 * Forces a one-time recolor with a specific color. Useful for debugging
 * or applying specific themes programmatically.
 * * Принудительно выполняет однократную перекраску в указанный цвет. Полезно
 * для отладки или программного применения конкретных тем.
 * * PARAMETERS / ПАРАМЕТРЫ:
 * forceColor - The color (RGB) to tint icons with.
 * Цвет (RGB), в который нужно окрасить иконки.
 ******************************************************************************/
void ML_IconsTint_ForceOnce(COLORREF forceColor);

#ifdef __cplusplus
}
#endif
