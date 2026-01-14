/*******************************************************************************
 * mod_ml_search.h
 * * MEDIA LIBRARY CYRILLIC SEARCH FIX HEADER
 * ЗАГОЛОВОК ИСПРАВЛЕНИЯ ПОИСКА КИРИЛЛИЦЫ В МЕДИАТЕКЕ
 * * PURPOSE / НАЗНАЧЕНИЕ:
 * Fixes the bug where searching for Cyrillic (or non-Latin) text in the Media
 * Library would fail or return incorrect results due to ANSI case comparison issues.
 * * Исправляет баг, когда поиск кириллического (или нелатинского) текста в
 * Медиатеке не работал или возвращал неверные результаты из-за проблем
 * сравнения регистра в ANSI.
 ******************************************************************************/

#ifndef MOD_ML_SEARCH_H
#define MOD_ML_SEARCH_H

#pragma once
#include <windows.h>

/*******************************************************************************
 * ML_CyrSearchFix_Init
 * * PURPOSE / НАЗНАЧЕНИЕ:
 * Installs the search fix hook. Patches the string comparison logic inside
 * the Media Library database engine.
 * * Устанавливает хук исправления поиска. Патчит логику сравнения строк внутри
 * движка базы данных Медиатеки.
 * * RETURNS / ВОЗВРАЩАЕТ:
 * TRUE if hook installed successfully / TRUE если хук установлен успешно
 ******************************************************************************/
BOOL ML_CyrSearchFix_Init(void);

/*******************************************************************************
 * ML_CyrSearchFix_Quit
 * * PURPOSE / НАЗНАЧЕНИЕ:
 * Removes the hook and restores original search behavior.
 * Удаляет хук и восстанавливает оригинальное поведение поиска.
 ******************************************************************************/
void ML_CyrSearchFix_Quit(void);

/*******************************************************************************
 * ML_CyrSearchFix_IsActive
 * * PURPOSE / НАЗНАЧЕНИЕ:
 * Checks if the fix is currently active.
 * Проверяет, активен ли в данный момент фикс.
 ******************************************************************************/
BOOL ML_CyrSearchFix_IsActive(void);

#endif