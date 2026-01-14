/*******************************************************************************
 * mod_plist_buttons.cpp 
 * 
 * WINAMP PLAYLIST EDITOR - CUSTOM MOUSE BUTTON MENU ZONES
 * Модуль пользовательских зон меню для кнопок мыши в редакторе плейлистов Winamp
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * This module adds custom clickable zones to the Winamp Playlist Editor window
 * that trigger context menus when clicked. These zones are positioned at specific
 * locations on the PE window and provide quick access to common playlist operations
 * through both left and right mouse button clicks.
 * 
 * Этот модуль добавляет пользовательские кликабельные зоны в окно редактора
 * плейлистов Winamp, которые вызывают контекстные меню при нажатии. Эти зоны
 * расположены в определённых местах окна PE и обеспечивают быстрый доступ к
 * распространённым операциям с плейлистом через нажатие левой и правой кнопок мыши.
 * 
 * HOW IT WORKS / КАК ЭТО РАБОТАЕТ:
 * 1. Subclasses the Playlist Editor window to intercept mouse events
 * 2. Defines 5 clickable zones (4 on left side, 1 on right side)
 * 3. Each zone has an outer detection area and inner button area
 * 4. When a zone is clicked, displays a context menu with relevant commands
 * 5. Menu commands are dispatched to either Winamp main window or PE window
 * 
 * 1. Подменяет окно редактора плейлистов для перехвата событий мыши
 * 2. Определяет 5 кликабельных зон (4 слева, 1 справа)
 * 3. Каждая зона имеет внешнюю область обнаружения и внутреннюю область кнопки
 * 4. При нажатии на зону отображается контекстное меню с соответствующими командами
 * 5. Команды меню отправляются либо в главное окно Winamp, либо в окно PE
 * 
 * ZONES / ЗОНЫ:
 * Zone 0 (Left): Add - File/Folder/Location menu
 *                Добавить - меню Файл/Папка/Адрес
 * Zone 1 (Left): Rem - Remove/Crop/Clear operations
 *                Удалить - операции удаления/обрезки/очистки
 * Zone 2 (Left): Sel - Selection operations (All/None/Invert)
 *                Выделение - операции выделения (Всё/Ничего/Инвертировать)
 * Zone 3 (Left): Misc - Info, Sort, and miscellaneous operations
 *                Разное - информация, сортировка и прочие операции
 * Zone 4 (Right): File - New/Open/Save playlist
 *                 Файл - Новый/Открыть/Сохранить плейлист
 * 
 * ARCHITECTURE / АРХИТЕКТУРА:
 * - Table-driven geometry: All zone positions defined in a single array
 * - Win9x compatible: Includes fallback for older TrackPopupMenu API
 * 
 * - Табличная геометрия: все позиции зон определены в одном массиве
 * - Совместимость с Win9x: включает резервный вариант для старого API TrackPopupMenu
 * 
 ******************************************************************************/

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "..\SDK\wa_ipc.h"
#include "..\SDK\gen.h"
#include "..\SwitchLangUI.h" // Must define menu string constants
                              // Должен определять константы строк меню

#pragma comment(lib, "user32.lib")

/*******************************************************************************
 * GEOMETRY CONFIGURATION (TABLE DRIVEN)
 * КОНФИГУРАЦИЯ ГЕОМЕТРИИ (ТАБЛИЧНЫЙ ПОДХОД)
 ******************************************************************************/

/*******************************************************************************
 * ZoneDef Structure
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Defines the geometry for a single clickable zone, including both the outer
 * detection rectangle and the inner button rectangle.
 * 
 * Определяет геометрию для одной кликабельной зоны, включая как внешний
 * прямоугольник обнаружения, так и внутренний прямоугольник кнопки.
 * 
 * FIELDS / ПОЛЯ:
 * x, y, w, h      - Base rectangle coordinates and dimensions
 *                   Координаты и размеры базового прямоугольника
 *                   (measured from bottom-left or bottom-right corner)
 *                   (измеряется от нижнего левого или нижнего правого угла)
 * bx, by, bw, bh  - Inner button rectangle (0 = auto-calculate with padding)
 *                   Внутренний прямоугольник кнопки (0 = автоматический расчёт с отступами)
 * rightAlign      - TRUE = measure from right edge, FALSE = from left edge
 *                   TRUE = измерять от правого края, FALSE = от левого края
 ******************************************************************************/
struct ZoneDef {
    int x, y, w, h;     // Base rect (from bottom-left or bottom-right)
                        // Базовый прямоугольник (от нижнего левого или правого угла)
    int bx, by, bw, bh; // Inner button rect (0 = auto calc)
                        // Внутренний прямоугольник кнопки (0 = автоматический расчёт)
    BOOL rightAlign;    // TRUE = from right edge
                        // TRUE = от правого края
};

// Zone definitions array: indices 0-3 are left-aligned zones, index 4 is right-aligned
// Массив определений зон: индексы 0-3 - зоны с выравниванием слева, индекс 4 - справа
static const ZoneDef g_zones[5] = {
    // Zone 0: Add menu (left side)
    // Зона 0: меню Добавить (левая сторона)
    {15, 13, 20, 20,  0,0,0,0,  FALSE}, 
    
    // Zone 1: Remove menu (left side)
    // Зона 1: меню Удалить (левая сторона)
    {44, 13, 20, 20,  0,0,0,0,  FALSE},
    
    // Zone 2: Selection menu (left side)
    // Зона 2: меню Выделение (левая сторона)
    {73, 13, 20, 20,  0,0,0,0,  FALSE},
    
    // Zone 3: Miscellaneous menu (left side)
    // Зона 3: меню Разное (левая сторона)
    {102,13, 20, 20,  0,0,0,0,  FALSE},
    
    // Zone 4: File menu (right side)
    // Зона 4: меню Файл (правая сторона)
    {22, 12, 23, 20,  0,0,0,0,  TRUE} 
};

// Tolerance for click detection (pixels added to outer rectangle)
// Допуск для обнаружения клика (пиксели, добавляемые к внешнему прямоугольнику)
static const int kTolerance = 1;

// Padding for inner button rectangle (pixels removed from each side)
// Отступ для внутреннего прямоугольника кнопки (пиксели, убираемые с каждой стороны)
static const int kBtnPadX   = 2;
static const int kBtnPadY   = 2;

/*******************************************************************************
 * GLOBAL STATE VARIABLES
 * ГЛОБАЛЬНЫЕ ПЕРЕМЕННЫЕ СОСТОЯНИЯ
 ******************************************************************************/

// Handle to the Playlist Editor window
// Дескриптор окна редактора плейлистов
static HWND    s_hwndPE   = NULL;

// Original window procedure before subclassing
// Оригинальная оконная процедура до подмены
static WNDPROC s_oldPE    = NULL;

// Currently active zone index (-1 = none, 0-4 = specific zone)
// Индекс текущей активной зоны (-1 = нет, 0-4 = конкретная зона)
static int     s_activeZone = -1;

// Menu handles for each zone (indices 0-4)
// Дескрипторы меню для каждой зоны (индексы 0-4)
static HMENU   s_menus[5] = {0};

/*******************************************************************************
 * HELPER FUNCTIONS
 * ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ
 ******************************************************************************/

/*******************************************************************************
 * GetZoneRect
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Calculates the rectangle for a specific zone, either the outer detection
 * rectangle or the inner button rectangle.
 * 
 * Вычисляет прямоугольник для конкретной зоны, либо внешний прямоугольник
 * обнаружения, либо внутренний прямоугольник кнопки.
 * 
 * PARAMETERS / ПАРАМЕТРЫ:
 * hwnd    - Handle to the Playlist Editor window
 *           Дескриптор окна редактора плейлистов
 * idx     - Zone index (0-4)
 *           Индекс зоны (0-4)
 * rOut    - Pointer to RECT structure to receive the calculated rectangle
 *           Указатель на структуру RECT для получения вычисленного прямоугольника
 * btnRect - TRUE = calculate inner button rect, FALSE = outer detection rect
 *           TRUE = вычислить внутренний прямоугольник кнопки, FALSE = внешний прямоугольник обнаружения
 * 
 * RETURNS / ВОЗВРАЩАЕТ:
 * BOOL - TRUE if successful, FALSE if invalid zone index
 *        TRUE если успешно, FALSE если недопустимый индекс зоны
 * 
 * ALGORITHM / АЛГОРИТМ:
 * 1. Validate zone index (must be 0-4)
 * 2. Get client rectangle of PE window
 * 3. Calculate base position based on alignment (left or right)
 * 4. If button rect requested, apply padding or custom inner rect
 * 5. If outer rect requested, apply tolerance expansion
 * 6. Return calculated rectangle
 * 
 * 1. Проверить индекс зоны (должен быть 0-4)
 * 2. Получить клиентский прямоугольник окна PE
 * 3. Вычислить базовую позицию на основе выравнивания (слева или справа)
 * 4. Если запрошен прямоугольник кнопки, применить отступы или пользовательский внутренний прямоугольник
 * 5. Если запрошен внешний прямоугольник, применить расширение допуска
 * 6. Вернуть вычисленный прямоугольник
 ******************************************************************************/
static BOOL GetZoneRect(HWND hwnd, int idx, RECT* rOut, BOOL btnRect)
{
    // Validate zone index
    // Проверка индекса зоны
    if (idx < 0 || idx > 4) return FALSE;
    
    // Get window client area dimensions
    // Получение размеров клиентской области окна
    RECT rc; GetClientRect(hwnd, &rc);
    int W = rc.right - rc.left;   // Window width / Ширина окна
    int H = rc.bottom - rc.top;   // Window height / Высота окна
    
    // Get zone definition from table
    // Получение определения зоны из таблицы
    const ZoneDef* z = &g_zones[idx];

    // Calculate base position depending on alignment
    // Left-aligned: x is offset from left edge
    // Right-aligned: x is offset from right edge (measured leftward)
    // Вычисление базовой позиции в зависимости от выравнивания
    // Выравнивание слева: x - смещение от левого края
    // Выравнивание справа: x - смещение от правого края (измеряется влево)
    int x = z->rightAlign ? (W - z->x - z->w) : z->x;
    
    // Y is always measured from bottom edge upward
    // Y всегда измеряется от нижнего края вверх
    int y = H - z->y - z->h;
    
    // Base dimensions
    // Базовые размеры
    int w = z->w;
    int h = z->h;

    // Apply modifications based on rectangle type
    // Применение модификаций на основе типа прямоугольника
    if (btnRect) {
        // Inner button rectangle calculation
        // Вычисление внутреннего прямоугольника кнопки
        if (z->bw > 0) { 
            // Custom inner rect defined in table (use explicit values)
            // Пользовательский внутренний прямоугольник определён в таблице (использовать явные значения)
            x += z->bx; y += z->by; w = z->bw; h = z->bh; // Simplified relative logic
                                                          // Упрощённая относительная логика
        } else { 
            // Auto-calculate with standard padding
            // Автоматический расчёт со стандартными отступами
            x += kBtnPadX; y += kBtnPadY;
            w -= 2*kBtnPadX; h -= 2*kBtnPadY;
        }
    } else {
        // Outer detection rectangle - expand by tolerance
        // Внешний прямоугольник обнаружения - расширить на величину допуска
        x -= kTolerance; y -= kTolerance;
        w += 2*kTolerance; h += 2*kTolerance;
    }

    // Store calculated rectangle
    // Сохранение вычисленного прямоугольника
    rOut->left = x; rOut->top = y;
    rOut->right = x + w; rOut->bottom = y + h;
    return TRUE;
}

/*******************************************************************************
 * Hit
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Tests if a point (x,y) is inside a rectangle.
 * 
 * Проверяет, находится ли точка (x,y) внутри прямоугольника.
 * 
 * PARAMETERS / ПАРАМЕТРЫ:
 * r - Pointer to rectangle to test
 *     Указатель на прямоугольник для проверки
 * x - X coordinate of point
 *     X-координата точки
 * y - Y coordinate of point
 *     Y-координата точки
 * 
 * RETURNS / ВОЗВРАЩАЕТ:
 * BOOL - TRUE if point is inside rectangle, FALSE otherwise
 *        TRUE если точка внутри прямоугольника, FALSE иначе
 * 
 * NOTES / ПРИМЕЧАНИЯ:
 * Uses semi-open interval: includes left/top edges, excludes right/bottom edges
 * Использует полуоткрытый интервал: включает левый/верхний края, исключает правый/нижний края
 ******************************************************************************/
static BOOL Hit(const RECT* r, int x, int y) {
    return (x >= r->left && x < r->right && y >= r->top && y < r->bottom);
}

/*******************************************************************************
 * ShowPopup
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Displays a popup menu with Win9x compatibility. Provides a fallback for
 * systems that don't have TrackPopupMenuEx available.
 * 
 * Отображает всплывающее меню с совместимостью с Win9x. Предоставляет
 * резервный вариант для систем, где TrackPopupMenuEx недоступна.
 * 
 * PARAMETERS / ПАРАМЕТРЫ:
 * hMenu      - Handle to the popup menu to display
 *              Дескриптор всплывающего меню для отображения
 * x, y       - Screen coordinates where to display the menu
 *              Экранные координаты, где отобразить меню
 * owner      - Handle to the owner window
 *              Дескриптор окна-владельца
 * alignRight - TRUE = align menu to the right, FALSE = align to the left
 *              TRUE = выровнять меню справа, FALSE = выровнять слева
 * 
 * RETURNS / ВОЗВРАЩАЕТ:
 * UINT - ID of the selected menu item, or 0 if no selection
 *        Идентификатор выбранного пункта меню или 0, если нет выбора
 * 
 * IMPLEMENTATION / РЕАЛИЗАЦИЯ:
 * Uses dynamic API loading to check for TrackPopupMenuEx availability.
 * On first call, attempts to load the extended function from user32.dll.
 * If not available, falls back to standard TrackPopupMenu.
 * 
 * Использует динамическую загрузку API для проверки доступности TrackPopupMenuEx.
 * При первом вызове пытается загрузить расширенную функцию из user32.dll.
 * Если недоступна, использует стандартную TrackPopupMenu.
 ******************************************************************************/
static UINT ShowPopup(HMENU hMenu, int x, int y, HWND owner, BOOL alignRight)
{
    // Function pointer type for TrackPopupMenuEx
    // Тип указателя на функцию для TrackPopupMenuEx
    typedef int (WINAPI *PFN)(HMENU, UINT, int, int, HWND, LPTPMPARAMS);
    
    // Static variables to cache API availability check
    // Статические переменные для кэширования проверки доступности API
    static PFN pEx = NULL;          // Pointer to TrackPopupMenuEx function
                                     // Указатель на функцию TrackPopupMenuEx
    static BOOL s_checked = FALSE;  // TRUE if we already checked for the function
                                     // TRUE если мы уже проверили наличие функции

    // On first call, try to load TrackPopupMenuEx
    // При первом вызове попытаться загрузить TrackPopupMenuEx
    if (!s_checked) {
        HMODULE hUser = GetModuleHandleA("user32.dll"); 
        if (hUser) {
            pEx = (PFN)GetProcAddress(hUser, "TrackPopupMenuEx");
        }
        s_checked = TRUE;  // Mark as checked to avoid repeated lookups
                          // Отметить как проверенное, чтобы избежать повторных поисков
    }
    
    // Build menu flags
    // Формирование флагов меню
    UINT flags = TPM_RIGHTBUTTON |   // Allow right-click on menu items
                                      // Разрешить правый клик на пунктах меню
                 TPM_RETURNCMD |      // Return command ID instead of posting message
                                      // Возвращать ID команды вместо отправки сообщения
                 TPM_BOTTOMALIGN |    // Align menu above the specified point
                                      // Выровнять меню над указанной точкой
                 (alignRight ? TPM_RIGHTALIGN : TPM_LEFTALIGN);  // Horizontal alignment
                                                                  // Горизонтальное выравнивание
    
    // Use extended function if available, otherwise use standard function
    // Использовать расширенную функцию если доступна, иначе использовать стандартную
    if (pEx) return (UINT)pEx(hMenu, flags, x, y, owner, NULL);
    return (UINT)TrackPopupMenu(hMenu, flags, x, y, 0, owner, NULL);
}

/*******************************************************************************
 * DispatchCmd
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Dispatches a menu command to the appropriate Winamp window (main window
 * or Playlist Editor) based on the command ID range.
 * 
 * Отправляет команду меню в соответствующее окно Winamp (главное окно или
 * редактор плейлистов) на основе диапазона идентификатора команды.
 * 
 * PARAMETERS / ПАРАМЕТРЫ:
 * cmd - Command ID to dispatch (from menu selection)
 *       Идентификатор команды для отправки (из выбора меню)
 * 
 * COMMAND ROUTING / МАРШРУТИЗАЦИЯ КОМАНД:
 * Commands 1000-1999 or 40170-40299: Sent to Playlist Editor
 * All other commands: Sent to main Winamp window
 * 
 * Команды 1000-1999 или 40170-40299: отправляются в редактор плейлистов
 * Все остальные команды: отправляются в главное окно Winamp
 * 
 * ALGORITHM / АЛГОРИТМ:
 * 1. Check if command is valid (non-zero)
 * 2. Find main Winamp window
 * 3. Find Playlist Editor window
 * 4. Determine target window based on command ID range
 * 5. Bring target window to foreground
 * 6. Send WM_COMMAND message to target window
 * 
 * 1. Проверить, является ли команда допустимой (ненулевой)
 * 2. Найти главное окно Winamp
 * 3. Найти окно редактора плейлистов
 * 4. Определить целевое окно на основе диапазона ID команды
 * 5. Вывести целевое окно на передний план
 * 6. Отправить сообщение WM_COMMAND в целевое окно
 ******************************************************************************/
static void DispatchCmd(int cmd) {
    // Ignore null commands
    // Игнорировать нулевые команды
    if (!cmd) return;
    
    // Find main Winamp window
    // Поиск главного окна Winamp
    HWND wa = FindWindowA("Winamp v1.x", NULL);
    
    // Find Playlist Editor window (try cached handle first, then search)
    // Поиск окна редактора плейлистов (сначала попытаться использовать кэшированный дескриптор, затем поиск)
    HWND pe = s_hwndPE ? s_hwndPE : FindWindowA("Winamp PE", 0);
    
    // Default target is main window
    // Целевое окно по умолчанию - главное окно
    HWND target = wa; 

    // Determine if this command should go to PE instead
    // Command ranges 1000-1999 and 40170-40299 are PE-specific
    // Определение, должна ли эта команда отправиться в PE
    // Диапазоны команд 1000-1999 и 40170-40299 специфичны для PE
    if ((cmd >= 1000 && cmd < 2000) || (cmd >= 40170 && cmd <= 40299)) {
        if (pe) target = pe;
    }

    // Dispatch command to target window
    // Отправка команды в целевое окно
    if (target) {
        // Bring window to foreground first
        // Сначала вывести окно на передний план
        SetForegroundWindow(target);
        
        // Send command message
        // Отправка сообщения команды
        SendMessageA(target, WM_COMMAND, MAKEWPARAM(cmd, 0), 0);
    }
}

/*******************************************************************************
 * MENU BUILDING FUNCTIONS
 * ФУНКЦИИ СОЗДАНИЯ МЕНЮ
 ******************************************************************************/

/*******************************************************************************
 * CreateMenuWithPopup
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Creates a menu bar with a single popup menu attached. This is a helper
 * function to simplify menu creation.
 * 
 * Создаёт строку меню с одним прикреплённым всплывающим меню. Это вспомогательная
 * функция для упрощения создания меню.
 * 
 * PARAMETERS / ПАРАМЕТРЫ:
 * title - Title text for the popup menu (not actually visible in this usage)
 *         Текст заголовка для всплывающего меню (фактически не виден в этом использовании)
 * popup - Handle to the popup menu to attach
 *         Дескриптор всплывающего меню для прикрепления
 * 
 * RETURNS / ВОЗВРАЩАЕТ:
 * HMENU - Handle to the created menu bar
 *         Дескриптор созданной строки меню
 * 
 * NOTES / ПРИМЕЧАНИЯ:
 * The menu bar is used as a container to hold the popup menu, allowing us
 * to extract the popup later with GetSubMenu(bar, 0).
 * 
 * Строка меню используется как контейнер для хранения всплывающего меню,
 * позволяя нам извлечь всплывающее меню позже с помощью GetSubMenu(bar, 0).
 ******************************************************************************/
static HMENU CreateMenuWithPopup(const char* title, HMENU popup) {
    HMENU bar = CreateMenu();
    AppendMenuA(bar, MF_POPUP, (UINT_PTR)popup, title);
    return bar;
}

/*******************************************************************************
 * BuildMenus
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Constructs all five zone menus with their respective menu items and submenus.
 * This function is called during initialization and whenever menus need to be
 * rebuilt (e.g., for language changes).
 * 
 * Создаёт все пять меню зон с их соответствующими пунктами и подменю.
 * Эта функция вызывается при инициализации и когда меню нужно пересоздать
 * (например, для изменения языка).
 * 
 * MENU STRUCTURE / СТРУКТУРА МЕНЮ:
 * 
 * Menu 0 (Add / Добавить):
 *   - Add File (ID: 1032)
 *   - Add Folder (ID: 1036)
 *   - Add Location (ID: 1039)
 * 
 * Menu 1 (Rem / Удалить):
 *   - Remove Selected (ID: 1034)
 *   - Crop Selected (ID: 1035)
 *   - New Playlist (ID: 40214)
 *   - Delete submenu:
 *     - Remove Dead Files (ID: 40222)
 *     - Delete All (ID: 40223)
 * 
 * Menu 2 (Sel / Выделение):
 *   - Select All (ID: 40205)
 *   - Select None (ID: 40207)
 *   - Invert Selection (ID: 40171)
 * 
 * Menu 3 (Misc / Разное):
 *   - Info submenu:
 *     - File Info (ID: 40208)
 *     - Entry (ID: 40255)
 *   - Sort submenu:
 *     - Sort by Title (ID: 40209)
 *     - Sort by Filename (ID: 40210)
 *     - Sort by Path (ID: 40211)
 *     - [Separator]
 *     - Reverse (ID: 40213)
 *     - Randomize (ID: 40212)
 *   - Misc submenu:
 *     - HTML Playlist (ID: 40292)
 *     - [Separator]
 *     - Extended Info (ID: 40293)
 * 
 * Menu 4 (File / Файл):
 *   - New Playlist (ID: 40214)
 *   - Open Playlist (ID: 40202)
 *   - Save Playlist (ID: 40204)
 * 
 * NOTES / ПРИМЕЧАНИЯ:
 * - All string constants (ADD_FILE, REM_SEL, etc.) are defined in SwitchLangUI.h
 * - Command IDs correspond to standard Winamp command IDs
 * - Old menus are destroyed before creating new ones to prevent leaks
 * 
 * - Все строковые константы (ADD_FILE, REM_SEL и т.д.) определены в SwitchLangUI.h
 * - Идентификаторы команд соответствуют стандартным идентификаторам команд Winamp
 * - Старые меню уничтожаются перед созданием новых для предотвращения утечек
 ******************************************************************************/
static void BuildMenus() {
    // Destroy existing menus to prevent memory leaks
    // Уничтожение существующих меню для предотвращения утечек памяти
    for(int i=0; i<5; i++) if(s_menus[i]) DestroyMenu(s_menus[i]);

    // Build Menu 0: Add
    // Создание меню 0: Добавить
    HMENU p0 = CreatePopupMenu();
    AppendMenuA(p0, MF_STRING, 1032, ADD_FILE);     // Add File / Добавить файл
    AppendMenuA(p0, MF_STRING, 1036, ADD_FOLDER);   // Add Folder / Добавить папку
    AppendMenuA(p0, MF_STRING, 1039, ADD_LOCATION); // Add Location / Добавить адрес
    s_menus[0] = CreateMenuWithPopup("Add", p0);

    // Build Menu 1: Rem (Remove)
    // Создание меню 1: Удалить
    HMENU p1 = CreatePopupMenu();
    AppendMenuA(p1, MF_STRING, 1034, REM_SEL);      // Remove Selected / Удалить выделенное
    AppendMenuA(p1, MF_STRING, 1035, CROP_SEL);     // Crop Selected / Обрезать до выделенного
    AppendMenuA(p1, MF_STRING, 40214, NEW_PL);      // New Playlist / Новый плейлист
    
    // Create Delete submenu / Создание подменю Удалить
    HMENU sub = CreatePopupMenu();
    AppendMenuA(sub, MF_STRING, 40222, RMV_DEAD);   // Remove Dead Files / Удалить несуществующие файлы
    AppendMenuA(sub, MF_STRING, 40223, DEL_ALL);    // Delete All / Удалить всё
    AppendMenuA(p1, MF_POPUP, (UINT_PTR)sub, SUB_DEL); // Attach submenu / Прикрепить подменю
    s_menus[1] = CreateMenuWithPopup("Rem", p1);

    // Build Menu 2: Sel (Selection)
    // Создание меню 2: Выделение
    HMENU p2 = CreatePopupMenu();
    AppendMenuA(p2, MF_STRING, 40205, SEL_ALL);     // Select All / Выделить всё
    AppendMenuA(p2, MF_STRING, 40207, SEL_NONE);    // Select None / Снять выделение
    AppendMenuA(p2, MF_STRING, 40171, INVERT);      // Invert Selection / Инвертировать выделение
    s_menus[2] = CreateMenuWithPopup("Sel", p2);

    // Build Menu 3: Misc (Miscellaneous)
    // Создание меню 3: Разное
    HMENU p3 = CreatePopupMenu();
    
    // Info submenu / Подменю Информация
    HMENU info = CreatePopupMenu();
    AppendMenuA(info, MF_STRING, 40208, FILE_INFO); // File Info / Информация о файле
    AppendMenuA(info, MF_STRING, 40255, ENTRY);     // Entry / Запись
    AppendMenuA(p3, MF_POPUP, (UINT_PTR)info, INFO);
    
    // Sort submenu / Подменю Сортировка
    HMENU sort = CreatePopupMenu();
    AppendMenuA(sort, MF_STRING, 40209, SORT_TIT);  // Sort by Title / Сортировать по названию
    AppendMenuA(sort, MF_STRING, 40210, SORT_NAM);  // Sort by Filename / Сортировать по имени файла
    AppendMenuA(sort, MF_STRING, 40211, SORT_FOL);  // Sort by Path / Сортировать по пути
    AppendMenuA(sort, MF_SEPARATOR, 0, 0);          // Separator / Разделитель
    AppendMenuA(sort, MF_STRING, 40213, SORT_REV);  // Reverse / Обратный порядок
    AppendMenuA(sort, MF_STRING, 40212, SORT_RAN);  // Randomize / Случайный порядок
    AppendMenuA(p3, MF_POPUP, (UINT_PTR)sort, SORTALL);
    
    // Misc submenu / Подменю Разное
    HMENU misc = CreatePopupMenu();
    AppendMenuA(misc, MF_STRING, 40292, HTML_PL);   // HTML Playlist / HTML плейлист
    AppendMenuA(misc, MF_SEPARATOR, 0, 0);          // Separator / Разделитель
    AppendMenuA(misc, MF_STRING, 40293, EXT_INFO);  // Extended Info / Расширенная информация
    AppendMenuA(p3, MF_POPUP, (UINT_PTR)misc, MISC);
    s_menus[3] = CreateMenuWithPopup("Misc", p3);

    // Build Menu 4: File (Right-aligned zone)
    // Создание меню 4: Файл (зона с выравниванием справа)
    HMENU p4 = CreatePopupMenu();
    AppendMenuA(p4, MF_STRING, 40214, NEW_PL2);     // New Playlist / Новый плейлист
    AppendMenuA(p4, MF_STRING, 40202, OPEN_PL);     // Open Playlist / Открыть плейлист
    AppendMenuA(p4, MF_STRING, 40204, SAVE_PL);     // Save Playlist / Сохранить плейлист
    s_menus[4] = CreateMenuWithPopup("File", p4);
}

/*******************************************************************************
 * WINDOW SUBCLASS PROCEDURE
 * ПРОЦЕДУРА ПОДМЕНЫ ОКНА
 ******************************************************************************/

/*******************************************************************************
 * PEProc
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Custom window procedure for the Playlist Editor. Intercepts mouse events
 * to handle zone clicks and menu display.
 * 
 * Пользовательская оконная процедура для редактора плейлистов. Перехватывает
 * события мыши для обработки кликов по зонам и отображения меню.
 * 
 * PARAMETERS / ПАРАМЕТРЫ:
 * h - Handle to the window
 *     Дескриптор окна
 * m - Message identifier
 *     Идентификатор сообщения
 * w - First message parameter (wParam)
 *     Первый параметр сообщения (wParam)
 * l - Second message parameter (lParam)
 *     Второй параметр сообщения (lParam)
 * 
 * RETURNS / ВОЗВРАЩАЕТ:
 * LRESULT - Message processing result
 *           Результат обработки сообщения
 * 
 * HANDLED MESSAGES / ОБРАБАТЫВАЕМЫЕ СООБЩЕНИЯ:
 * 
 * WM_LBUTTONDOWN / WM_RBUTTONDOWN:
 *   - Detects if click is within any zone's outer rectangle
 *   - Sets active zone and captures mouse
 *   - Returns 0 to prevent default processing
 * 
 *   - Обнаруживает, находится ли клик в пределах внешнего прямоугольника любой зоны
 *   - Устанавливает активную зону и захватывает мышь
 *   - Возвращает 0 для предотвращения обработки по умолчанию
 * 
 * WM_LBUTTONUP / WM_RBUTTONUP:
 *   - If active zone exists, checks if release is still within zone
 *   - Validates both outer and inner rectangle hit
 *   - Displays appropriate menu if validation passes
 *   - Dispatches selected menu command
 *   - Releases mouse capture and clears active zone
 * 
 *   - Если существует активная зона, проверяет, находится ли отпускание всё ещё в зоне
 *   - Проверяет попадание как во внешний, так и во внутренний прямоугольник
 *   - Отображает соответствующее меню, если проверка пройдена
 *   - Отправляет выбранную команду меню
 *   - Освобождает захват мыши и очищает активную зону
 * 
 * WM_CONTEXTMENU:
 *   - Currently a placeholder (empty handler)
 *   - Could be used to suppress default context menu
 * 
 *   - В данный момент заглушка (пустой обработчик)
 *   - Может использоваться для подавления контекстного меню по умолчанию
 * 
 * WM_NCDESTROY:
 *   - Restores original window procedure before destruction
 *   - Critical cleanup to prevent crashes
 * 
 *   - Восстанавливает оригинальную оконную процедуру перед уничтожением
 *   - Критическая очистка для предотвращения крашей
 ******************************************************************************/
static LRESULT CALLBACK PEProc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    // Handle mouse button down events (both left and right)
    // Обработка событий нажатия кнопок мыши (левой и правой)
    if (m == WM_LBUTTONDOWN || m == WM_RBUTTONDOWN) {
        // Extract click coordinates from lParam
        // Извлечение координат клика из lParam
        int x = (short)LOWORD(l);
        int y = (short)HIWORD(l);
        
        // Check all zones to see if click is within any zone's outer rectangle
        // Проверка всех зон на предмет попадания клика в пределы внешнего прямоугольника любой зоны
        for (int i=0; i<5; ++i) {
            RECT r; GetZoneRect(h, i, &r, FALSE); // Get outer rect / Получить внешний прямоугольник
            if (Hit(&r, x, y)) {
                // Click detected in zone - mark as active and capture mouse
                // Клик обнаружен в зоне - отметить как активную и захватить мышь
                s_activeZone = i;
                SetCapture(h);  // Capture mouse to receive button-up even if cursor moves outside
                                // Захватить мышь для получения отпускания кнопки, даже если курсор выйдет за пределы
                return 0;       // Consume the event / Поглотить событие
            }
        }
    }
    
    // Handle mouse button up events (both left and right)
    // Обработка событий отпускания кнопок мыши (левой и правой)
    if (m == WM_LBUTTONUP || m == WM_RBUTTONUP) {
        // Only process if we have an active zone from button-down
        // Обрабатывать только если есть активная зона от нажатия кнопки
        if (s_activeZone != -1) {
            // Extract release coordinates
            // Извлечение координат отпускания
            int x = (short)LOWORD(l);
            int y = (short)HIWORD(l);
            
            // Always release mouse capture first
            // Всегда сначала освобождать захват мыши
            ReleaseCapture();
         
            // Validate that release is within both outer and inner rectangles
            // This prevents accidental menu activation if user drags outside zone
            // Проверка, что отпускание находится в пределах как внешнего, так и внутреннего прямоугольников
            // Это предотвращает случайную активацию меню, если пользователь перетащит курсор за пределы зоны
            RECT rOuter, rInner;
            if (GetZoneRect(h, s_activeZone, &rOuter, FALSE) && Hit(&rOuter, x, y) &&  // Check outer rect / Проверить внешний прямоугольник
                GetZoneRect(h, s_activeZone, &rInner, TRUE)  && Hit(&rInner, x, y))    // Check inner rect / Проверить внутренний прямоугольник
            {
                // Valid click-release within zone - show menu
                // Допустимое нажатие-отпускание в пределах зоны - показать меню
                HMENU hBar = s_menus[s_activeZone];
                if (hBar) {
                    // Calculate menu display position
                    // Вычисление позиции отображения меню
                    POINT pt;
                    if (s_activeZone == 4) { 
                        // Right-aligned zone (File menu) - align menu to right edge
                        // Зона с выравниванием справа (меню Файл) - выровнять меню по правому краю
                        pt.x = rOuter.right - kTolerance; 
                        pt.y = rOuter.top + kTolerance;
                    } else { 
                        // Left-aligned zones - align menu to left edge
                        // Зоны с выравниванием слева - выровнять меню по левому краю
                        pt.x = rOuter.left + kTolerance;
                        pt.y = rOuter.top + kTolerance;
                    }
                    
                    // Convert client coordinates to screen coordinates
                    // Преобразование клиентских координат в экранные координаты
                    ClientToScreen(h, &pt);
                    
                    // Extract popup menu from menu bar
                    // Извлечение всплывающего меню из строки меню
                    HMENU hPop = GetSubMenu(hBar, 0);
                    if (hPop) {
                        // Display menu and dispatch selected command
                        // Отображение меню и отправка выбранной команды
                        DispatchCmd(ShowPopup(hPop, pt.x, pt.y, h, (s_activeZone==4)));
                    }
                }
            }
            
            // Clear active zone regardless of outcome
            // Очистить активную зону независимо от результата
            s_activeZone = -1;
            return 0; // Consume the event / Поглотить событие
        }
    }
    
    // Handle context menu message (currently placeholder)
    // Обработка сообщения контекстного меню (в данный момент заглушка)
    if (m == WM_CONTEXTMENU && (HWND)w == h) {
        // Could suppress default context menu here if needed
        // Можно подавить контекстное меню по умолчанию здесь, если нужно
    }

    // Handle window destruction - critical cleanup
    // Обработка уничтожения окна - критическая очистка
    if (m == WM_NCDESTROY) {
        // Restore original window procedure before destruction
        // Восстановление оригинальной оконной процедуры перед уничтожением
        if (s_oldPE) SetWindowLong(h, GWL_WNDPROC, (LONG)s_oldPE);
        
        // Clear state variables
        // Очистка переменных состояния
        s_oldPE = NULL; 
        s_hwndPE = NULL;
    }

    // Forward all other messages to original window procedure
    // Пересылка всех остальных сообщений оригинальной оконной процедуре
    return CallWindowProc(s_oldPE ? s_oldPE : DefWindowProc, h, m, w, l);
}

/*******************************************************************************
 * EXPORTED FUNCTIONS
 * ЭКСПОРТИРУЕМЫЕ ФУНКЦИИ
 ******************************************************************************/

/*******************************************************************************
 * PeZones_SetInstance
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Called by the plugin host to provide the module instance handle. Currently
 * used to trigger menu building.
 * 
 * Вызывается хостом плагина для предоставления дескриптора экземпляра модуля.
 * В данный момент используется для запуска создания меню.
 * 
 * PARAMETERS / ПАРАМЕТРЫ:
 * (unused) - Instance handle (not currently used)
 *            Дескриптор экземпляра (в данный момент не используется)
 * 
 * NOTES / ПРИМЕЧАНИЯ:
 * This function could be called multiple times, e.g., when language changes
 * and menus need to be rebuilt.
 * 
 * Эта функция может вызываться несколько раз, например, при изменении языка
 * и необходимости пересоздания меню.
 ******************************************************************************/
extern "C" void PeZones_SetInstance(void*) {
    // Build/rebuild all zone menus
    // Создание/пересоздание всех меню зон
    BuildMenus();
}

/*******************************************************************************
 * PeZones_Init
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Initializes the PE zones module by finding the Playlist Editor window and
 * installing the window procedure subclass.
 * 
 * Инициализирует модуль зон PE, находя окно редактора плейлистов и
 * устанавливая подмену оконной процедуры.
 * 
 * INITIALIZATION PROCESS / ПРОЦЕСС ИНИЦИАЛИЗАЦИИ:
 * 1. Find main Winamp window
 * 2. Request PE window handle via IPC
 * 3. Fallback to FindWindow if IPC fails
 * 4. Subclass PE window if found and not already subclassed
 * 
 * 1. Найти главное окно Winamp
 * 2. Запросить дескриптор окна PE через IPC
 * 3. Использовать FindWindow, если IPC не сработал
 * 4. Подменить окно PE, если найдено и ещё не подменено
 * 
 * NOTES / ПРИМЕЧАНИЯ:
 * The function is safe to call multiple times - it only subclasses once.
 * 
 * Функцию безопасно вызывать несколько раз - она подменяет только один раз.
 ******************************************************************************/
extern "C" void PeZones_Init(void) {
    // Find main Winamp window
    // Поиск главного окна Winamp
    HWND wa = FindWindowA("Winamp v1.x", NULL);
    if (!wa) return;  // Can't proceed without main window / Невозможно продолжить без главного окна
    
    // Try to get PE window handle via IPC (preferred method)
    // Попытка получить дескриптор окна PE через IPC (предпочтительный метод)
    HWND pe = (HWND)SendMessage(wa, WM_WA_IPC, IPC_GETWND_PE, IPC_GETWND);
    
    // Fallback to FindWindow if IPC didn't return a valid handle
    // Использовать FindWindow, если IPC не вернул допустимый дескриптор
    if (!pe) pe = FindWindowA("Winamp PE", 0);
    
    // Subclass PE window if found and not already subclassed
    // Подмена окна PE, если найдено и ещё не подменено
    if (pe && !s_oldPE) {
        s_hwndPE = pe;  // Cache PE window handle / Кэшировать дескриптор окна PE
        s_oldPE = (WNDPROC)SetWindowLong(pe, GWL_WNDPROC, (LONG)PEProc);  // Install subclass / Установить подмену
    }
}

/*******************************************************************************
 * PeZones_Quit
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Cleans up the PE zones module by destroying menus and restoring the
 * original window procedure.
 * 
 * Очищает модуль зон PE, уничтожая меню и восстанавливая оригинальную
 * оконную процедуру.
 * 
 * CLEANUP PROCESS / ПРОЦЕСС ОЧИСТКИ:
 * 1. Destroy all created menus to prevent memory leaks
 * 2. Restore original PE window procedure if it was subclassed
 * 
 * 1. Уничтожить все созданные меню для предотвращения утечек памяти
 * 2. Восстановить оригинальную оконную процедуру PE, если она была подменена
 * 
 * IMPORTANCE / ВАЖНОСТЬ:
 * CRITICAL: This function must be called before module unload to prevent
 * crashes. Failing to restore the window procedure will cause Winamp to
 * call unloaded code.
 * 
 * КРИТИЧНО: Эта функция должна быть вызвана перед выгрузкой модуля для
 * предотвращения крашей. Неспособность восстановить оконную процедуру
 * приведёт к тому, что Winamp попытается вызвать выгруженный код.
 ******************************************************************************/
extern "C" void PeZones_Quit(void) {
    // Destroy all menus to prevent memory leaks
    // Уничтожение всех меню для предотвращения утечек памяти
    for(int i=0; i<5; i++) 
        if(s_menus[i]) { 
            DestroyMenu(s_menus[i]); 
            s_menus[i]=0; 
        }
    
    // Restore original window procedure if we subclassed it
    // Восстановление оригинальной оконной процедуры, если мы её подменили
    if (s_hwndPE && s_oldPE) 
        SetWindowLong(s_hwndPE, GWL_WNDPROC, (LONG)s_oldPE);
}