/*******************************************************************************
 * mod_skininstall.h
 * * SKIN INSTALLATION MODULE HEADER
 * ЗАГОЛОВОК МОДУЛЯ УСТАНОВКИ СКИНОВ
 * * PURPOSE / НАЗНАЧЕНИЕ:
 * Interface for the module that ensures Winamp skin files (.wsz, .zip) have
 * a proper "Install" context menu option in Windows Explorer.
 * * Интерфейс модуля, который гарантирует, что файлы скинов Winamp (.wsz, .zip)
 * имеют правильную опцию контекстного меню "Установить" в Проводнике Windows.
 ******************************************************************************/

#pragma once
#include <windows.h>

/*******************************************************************************
 * SkinInstall_RunOnce
 * * PURPOSE / НАЗНАЧЕНИЕ:
 * Checks and fixes the Windows Registry associations for Winamp skin files.
 * Tries to register the "Install" verb in HKEY_CLASSES_ROOT (system-wide),
 * falling back to HKEY_CURRENT_USER (per-user) if access is denied.
 * * Проверяет и исправляет ассоциации реестра Windows для файлов скинов Winamp.
 * Пытается зарегистрировать глагол "Установить" в HKEY_CLASSES_ROOT (системно),
 * при отказе в доступе переключается на HKEY_CURRENT_USER (для пользователя).
 * * NOTES / ПРИМЕЧАНИЯ:
 * Idempotent function - it runs only once per plugin session to avoid
 * unnecessary registry writes.
 * * Идемпотентная функция - выполняется только один раз за сеанс работы плагина,
 * чтобы избежать ненужных записей в реестр.
 ******************************************************************************/
void SkinInstall_RunOnce();