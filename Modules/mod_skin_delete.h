/*******************************************************************************
 * mod_skin_delete.h
 * * SKIN DELETE/RENAME MODULE HEADER
 * ЗАГОЛОВОК МОДУЛЯ УДАЛЕНИЯ/ПЕРЕИМЕНОВАНИЯ СКИНОВ
 * * PURPOSE / НАЗНАЧЕНИЕ:
 * Adds ability to delete (Delete key) and rename (F2 key) skins directly
 * inside the standard Winamp Skin Browser dialog.
 * * Добавляет возможность удалять (клавиша Delete) и переименовывать (клавиша F2)
 * скины прямо внутри стандартного диалога выбора скинов Winamp.
 ******************************************************************************/

#pragma once
#include "..\Resources\plugin_common.h"

/*******************************************************************************
 * SkinDel_Init
 * * PURPOSE / НАЗНАЧЕНИЕ:
 * Hooks the Skin Browser dialog to intercept keyboard events (Del, F2).
 * Перехватывает диалог выбора скинов для обработки событий клавиатуры (Del, F2).
 ******************************************************************************/
void SkinDel_Init(void);

/*******************************************************************************
 * SkinDel_Quit
 * * PURPOSE / НАЗНАЧЕНИЕ:
 * Removes the hook from the Skin Browser.
 * Удаляет хук с диалога выбора скинов.
 ******************************************************************************/
void SkinDel_Quit(void);
