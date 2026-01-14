/*******************************************************************************
 * WINAMP MEDIA LIBRARY ICONS TINT MODULE
 * МОДУЛЬ ТОНИРОВАНИЯ ИКОНОК МЕДИАТЕКИ WINAMP
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Automatically tints TreeView icons in Winamp's Media Library to match the
 * current text color and background. This ensures icons remain visible and
 * aesthetically consistent across different color schemes, themes, and skins.
 * 
 * Автоматически тонирует иконки TreeView в библиотеке Winamp под текущий
 * цвет текста и фона. Гарантирует, что иконки остаются видимыми и эстетически
 * согласованными в различных цветовых схемах, темах и скинах.
 * 
 * HOW IT WORKS / КАК ЭТО РАБОТАЕТ:
 * 
 * 1. ICON EXTRACTION / ИЗВЛЕЧЕНИЕ ИКОНОК:
 *    - Finds Media Library TreeView window
 *    - Creates baseline copies of all original icons
 *    - Stores them for repeated re-tinting
 * 
 *    - Находит окно TreeView библиотеки
 *    - Создаёт baseline копии всех оригинальных иконок
 *    - Хранит их для повторного перетонирования
 * 
 * 2. COLOR SAMPLING / СЭМПЛИРОВАНИЕ ЦВЕТА:
 *    - Reads TreeView text color (TVM_GETTEXTCOLOR)
 *    - Samples background color by rendering small area
 *    - Handles gradient/themed backgrounds correctly
 * 
 *    - Читает цвет текста TreeView (TVM_GETTEXTCOLOR)
 *    - Сэмплирует цвет фона отрисовкой маленькой области
 *    - Правильно обрабатывает градиентные/тематические фоны
 * 
 * 3. TINTING ALGORITHM / АЛГОРИТМ ТОНИРОВАНИЯ:
 *    - Converts each icon to 32bpp DIB
 *    - Calculates pixel luminance (luma = 0.3R + 0.59G + 0.11B)
 *    - Blends between background and text color based on luma
 *    - Dark pixels > closer to background, light pixels > closer to text
 * 
 *    - Преобразует каждую иконку в 32bpp DIB
 *    - Вычисляет яркость пикселя (luma = 0.3R + 0.59G + 0.11B)
 *    - Смешивает между фоном и текстом на основе luma
 *    - Тёмные пиксели > ближе к фону, светлые пиксели > ближе к тексту
 * 
 * 4. EVENT HANDLING / ОБРАБОТКА СОБЫТИЙ:
 *    - Subclasses TreeView window
 *    - Monitors TVM_SETTEXTCOLOR, TVM_SETIMAGELIST
 *    - Responds to WM_SYSCOLORCHANGE, WM_THEMECHANGED
 *    - Re-tints automatically when colors change
 * 
 *    - Субклассирует окно TreeView
 *    - Отслеживает TVM_SETTEXTCOLOR, TVM_SETIMAGELIST
 *    - Реагирует на WM_SYSCOLORCHANGE, WM_THEMECHANGED
 *    - Автоматически перетонирует при изменении цветов
 * 
 * WHY BASELINE COPIES / ЗАЧЕМ BASELINE КОПИИ:
 * Re-tinting already-tinted icons causes quality degradation. We store pristine
 * originals and always tint from baseline > final, never final > final.
 * 
 * Повторное тонирование уже тонированных иконок вызывает ухудшение качества.
 * Храним чистые оригиналы и всегда тонируем baseline > финал, никогда финал > финал.
 * 
 * COMPATIBILITY / СОВМЕСТИМОСТЬ:
 * - Win98: Basic DIB and icon operations only
 * - WinXP: Full theme support with WM_PRINTCLIENT
 * - Win11: Works with modern themes and high DPI
 * 
 * - Win98: Только базовые операции DIB и иконок
 * - WinXP: Полная поддержка тем с WM_PRINTCLIENT
 * - Win11: Работает с современными темами и высоким DPI
 * 
 ******************************************************************************/

// mod_ml_icons.cpp
// VS2003 / ANSI. Win98/XP/Win11-safe.

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>
#include <process.h>
#include <math.h>

#pragma comment(lib, "comctl32.lib")

/*******************************************************************************
 * EXPORTED FUNCTIONS
 * ЭКСПОРТИРУЕМЫЕ ФУНКЦИИ
 ******************************************************************************/

extern "C" BOOL ML_IconsTint_Start(HINSTANCE hInst);
extern "C" void ML_IconsTint_Stop(void);
extern "C" void ML_IconsTint_ForceOnce(COLORREF forceColor);

/*******************************************************************************
 * CONSTANT DEFINITIONS
 * ОПРЕДЕЛЕНИЯ КОНСТАНТ
 ******************************************************************************/

#ifndef ARRAYSIZE
#define ARRAYSIZE(a) (sizeof(a)/sizeof((a)[0]))
#endif

// TreeView message constants / Константы сообщений TreeView
#ifndef TV_FIRST
#define TV_FIRST 0x1100
#endif
#ifndef TVM_GETIMAGELIST
#define TVM_GETIMAGELIST (TV_FIRST + 8)   // Get image list / Получить список изображений
#endif
#ifndef TVM_SETIMAGELIST
#define TVM_SETIMAGELIST (TV_FIRST + 9)   // Set image list / Установить список изображений
#endif
#ifndef TVM_GETTEXTCOLOR
#define TVM_GETTEXTCOLOR (TV_FIRST + 32)  // Get text color / Получить цвет текста
#endif
#ifndef TVM_SETTEXTCOLOR
#define TVM_SETTEXTCOLOR (TV_FIRST + 30)  // Set text color / Установить цвет текста
#endif
#ifndef TVM_GETBKCOLOR
#define TVM_GETBKCOLOR   (TV_FIRST + 31)  // Get background color / Получить цвет фона
#endif

// System messages / Системные сообщения
#ifndef WM_THEMECHANGED
#define WM_THEMECHANGED  0x031A  // Theme changed notification / Уведомление об изменении темы
#endif
#ifndef WM_PRINTCLIENT
#define WM_PRINTCLIENT   0x0318  // Print client area / Печать клиентской области
#endif

// Print client flags / Флаги печати клиента
#ifndef PRF_CLIENT
#define PRF_CHECKVISIBLE 0x00000001
#define PRF_NONCLIENT    0x00000002
#define PRF_CLIENT       0x00000004  // Print client area / Печать клиентской области
#define PRF_ERASEBKGND   0x00000008  // Erase background / Стереть фон
#define PRF_CHILDREN     0x00000010
#define PRF_OWNED        0x00000020
#endif

// TreeView macro wrappers / Макрос-обёртки TreeView
#ifndef TreeView_GetImageList
#define TreeView_GetImageList(hwnd, iImage) \
    (HIMAGELIST)SendMessageA((hwnd), TVM_GETIMAGELIST, (WPARAM)(iImage), 0)
#endif
#ifndef TreeView_GetTextColor
#define TreeView_GetTextColor(hwnd) \
    (COLORREF)SendMessageA((hwnd), TVM_GETTEXTCOLOR, 0, 0)
#endif
#ifndef TreeView_GetBkColor
#define TreeView_GetBkColor(hwnd) \
    (COLORREF)SendMessageA((hwnd), TVM_GETBKCOLOR, 0, 0)
#endif

/*******************************************************************************
 * UTILITY FUNCTIONS: COLOR MATH
 * УТИЛИТЫ: МАТЕМАТИКА ЦВЕТА
 ******************************************************************************/

/*******************************************************************************
 * Clamp8
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Clamps an integer value to 0-255 range (valid byte range).
 * 
 * Ограничивает целочисленное значение диапазоном 0-255 (допустимый диапазон байта).
 ******************************************************************************/
static __inline int Clamp8(int v) { 
    if (v<0) return 0; 
    if (v>255) return 255; 
    return v; 
}

/*******************************************************************************
 * RGB_From_Channels
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Creates COLORREF from separate R, G, B channels with clamping.
 * 
 * Создаёт COLORREF из отдельных каналов R, G, B с ограничением.
 ******************************************************************************/
static COLORREF RGB_From_Channels(int r, int g, int b)
{
    return RGB((BYTE)Clamp8(r), (BYTE)Clamp8(g), (BYTE)Clamp8(b));
}

/*******************************************************************************
 * Lerp8
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Linear interpolation between two 8-bit values.
 * 
 * Линейная интерполяция между двумя 8-битными значениями.
 * 
 * PARAMETERS / ПАРАМЕТРЫ:
 * base   - Starting value / Начальное значение
 * target - Ending value / Конечное значение
 * t255   - Interpolation factor (0-255) / Фактор интерполяции (0-255)
 * 
 * RETURNS / ВОЗВРАЩАЕТ:
 * Interpolated value: base when t255=0, target when t255=255
 * Интерполированное значение: base при t255=0, target при t255=255
 * 
 * ALGORITHM / АЛГОРИТМ:
 * result = (base * (255 - t) + target * t + 127) / 255
 * The +127 provides rounding instead of truncation.
 * 
 * результат = (base * (255 - t) + target * t + 127) / 255
 * +127 обеспечивает округление вместо усечения.
 ******************************************************************************/
static __inline int Lerp8(int base, int target, int t255)
{
    return ( (base*(255 - t255)) + (target*t255) + 127 ) / 255;
}

/*******************************************************************************
 * DIB HELPER FUNCTIONS
 * ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ DIB
 ******************************************************************************/

/*******************************************************************************
 * CreateDIB32
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Creates a 32-bit DIB section with direct pixel access.
 * 
 * Создаёт 32-битную DIB-секцию с прямым доступом к пикселям.
 * 
 * PARAMETERS / ПАРАМЕТРЫ:
 * W      - Width in pixels / Ширина в пикселях
 * H      - Height in pixels / Высота в пикселях
 * ppBits - Output: pointer to pixel buffer / Выход: указатель на буфер пикселей
 * 
 * RETURNS / ВОЗВРАЩАЕТ:
 * Bitmap handle or NULL on failure
 * Дескриптор битмапа или NULL при неудаче
 * 
 * PIXEL FORMAT / ФОРМАТ ПИКСЕЛЕЙ:
 * 32bpp BGRA (or BGRX): Blue in low byte, alpha/unused in high byte
 * Each pixel is a DWORD: 0xAARRGGBB
 * 
 * 32bpp BGRA (или BGRX): Синий в младшем байте, альфа/не используется в старшем
 * Каждый пиксель - DWORD: 0xAARRGGBB
 ******************************************************************************/
static HBITMAP CreateDIB32(int W, int H, void** ppBits)
{
    if (W<=0 || H<=0 || !ppBits) return NULL;
    
    // Setup BITMAPINFO structure / Настроить структуру BITMAPINFO
    BITMAPINFO bi; 
    ZeroMemory(&bi, sizeof(bi));
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = W; 
    bi.bmiHeader.biHeight = -H;  // Negative for top-down DIB / Отрицательное для DIB сверху вниз
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    
    // Create DIB section / Создать DIB-секцию
    HDC hdc = GetDC(NULL);
    void* bits = NULL;
    HBITMAP h = CreateDIBSection(hdc, &bi, DIB_RGB_COLORS, &bits, NULL, 0);
    ReleaseDC(NULL, hdc);
    
    if (!h) return NULL;
    *ppBits = bits; 
    return h;
}

/*******************************************************************************
 * CreateDIB32FromBitmap
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Converts any bitmap to a 32-bit DIB for pixel manipulation.
 * 
 * Преобразует любой битмап в 32-битный DIB для манипуляции пикселями.
 * 
 * PARAMETERS / ПАРАМЕТРЫ:
 * hbmSrc - Source bitmap (any format) / Исходный битмап (любой формат)
 * pW     - Output: width / Выход: ширина
 * pH     - Output: height / Выход: высота
 * ppBits - Output: pixel buffer / Выход: буфер пикселей
 * 
 * RETURNS / ВОЗВРАЩАЕТ:
 * 32bpp DIB handle or NULL
 * Дескриптор 32bpp DIB или NULL
 ******************************************************************************/
static HBITMAP CreateDIB32FromBitmap(HBITMAP hbmSrc, int* pW, int* pH, void** ppBits)
{
    if(!hbmSrc || !pW || !pH || !ppBits) return NULL;
    
    // Get source bitmap info / Получить информацию об исходном битмапе
    BITMAP bm; 
    ZeroMemory(&bm, sizeof(bm)); 
    if(!GetObject(hbmSrc, sizeof(bm), &bm)) return NULL;
    
    *pW = bm.bmWidth; 
    *pH = bm.bmHeight; 
    void* bits = NULL;
    
    // Create 32bpp DIB / Создать 32bpp DIB
    HBITMAP hbm32 = CreateDIB32(*pW, *pH, &bits); 
    if(!hbm32 || !bits) { 
        if (hbm32) DeleteObject(hbm32); 
        return NULL; 
    }
    
    // Copy source bitmap to DIB / Копировать исходный битмап в DIB
    HDC hdc = GetDC(NULL); 
    HDC src = CreateCompatibleDC(hdc);
    HDC dst = CreateCompatibleDC(hdc);
    
    HGDIOBJ o1 = SelectObject(src, hbmSrc);
    HGDIOBJ o2 = SelectObject(dst, hbm32);
    BitBlt(dst, 0, 0, *pW, *pH, src, 0, 0, SRCCOPY);
    SelectObject(src, o1); 
    SelectObject(dst, o2);
    
    DeleteDC(src); 
    DeleteDC(dst); 
    ReleaseDC(NULL, hdc);
    
    *ppBits = bits; 
    return hbm32;
}

/*******************************************************************************
 * ExtractAndMask
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Extracts the AND mask from an icon's mask bitmap. Icons store XOR and AND
 * masks together; this function extracts just the AND part.
 * 
 * Извлекает маску AND из битмапа маски иконки. Иконки хранят маски XOR и AND
 * вместе; эта функция извлекает только часть AND.
 * 
 * ICON MASK FORMAT / ФОРМАТ МАСКИ ИКОНКИ:
 * Classic icons store mask as double-height 1bpp bitmap:
 * - Top half: AND mask (transparency)
 * - Bottom half: XOR mask (color)
 * 
 * Классические иконки хранят маску как битмап двойной высоты 1bpp:
 * - Верхняя половина: маска AND (прозрачность)
 * - Нижняя половина: маска XOR (цвет)
 ******************************************************************************/
static HBITMAP ExtractAndMask(HBITMAP hbmMask)
{
    if (!hbmMask) return NULL;
    
    BITMAP bm; 
    ZeroMemory(&bm, sizeof(bm)); 
    if (!GetObject(hbmMask, sizeof(bm), &bm)) return NULL;
    
    int W = bm.bmWidth;
    int fullH = bm.bmHeight;
    
    // For 1bpp double-height mask, extract top half
    // Для 1bpp маски двойной высоты извлечь верхнюю половину
    int andH = (bm.bmBitsPixel==1 && (fullH%2)==0) ? (fullH/2) : fullH;
    
    // Create 1bpp bitmap for AND mask / Создать 1bpp битмап для маски AND
    HBITMAP hOut = CreateBitmap(W, andH, 1, 1, NULL);
    if (!hOut) return NULL;
    
    // Copy AND mask / Копировать маску AND
    HDC hdc = GetDC(NULL); 
    HDC src = CreateCompatibleDC(hdc);
    HDC dst = CreateCompatibleDC(hdc);
    
    HGDIOBJ o1 = SelectObject(src, hbmMask);
    HGDIOBJ o2 = SelectObject(dst, hOut);
    BitBlt(dst, 0, 0, W, andH, src, 0, 0, SRCCOPY);
    SelectObject(src, o1); 
    SelectObject(dst, o2);
    
    DeleteDC(src); 
    DeleteDC(dst); 
    ReleaseDC(NULL, hdc);
    
    return hOut;
}

/*******************************************************************************
 * BACKGROUND SAMPLING
 * СЭМПЛИРОВАНИЕ ФОНА
 ******************************************************************************/

/*******************************************************************************
 * AverageTopStrip32
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Calculates average color of a horizontal strip of pixels.
 * 
 * Вычисляет средний цвет горизонтальной полосы пикселей.
 * 
 * PARAMETERS / ПАРАМЕТРЫ:
 * px     - Pointer to 32bpp pixel buffer / Указатель на буфер 32bpp пикселей
 * W      - Width / Ширина
 * H      - Height / Высота
 * stripH - Height of strip to average / Высота полосы для усреднения
 * 
 * RETURNS / ВОЗВРАЩАЕТ:
 * Average RGB color of the strip
 * Средний RGB-цвет полосы
 ******************************************************************************/
static __inline COLORREF AverageTopStrip32(const DWORD* px, int W, int H, int stripH)
{
    if (!px || W<=0 || H<=0) return RGB(255, 255, 255);
    if (stripH <= 0) stripH = 1;
    if (stripH > H)  stripH = H;
    
    // Accumulate color channels / Накапливать цветовые каналы
    unsigned long sr=0, sg=0, sb=0, cnt=0;
    for (int y=0; y<stripH; ++y) {
        const DWORD* row = px + y*W;
        for (int x=0; x<W; ++x) {
            DWORD v = row[x];
            BYTE b = (BYTE)(v & 0xFF);
            BYTE g = (BYTE)((v >> 8) & 0xFF);
            BYTE r = (BYTE)((v >> 16) & 0xFF);
            sr+=r; sg+=g; sb+=b; ++cnt;
        }
    }
    
    if (!cnt) return RGB(255, 255, 255);
    return RGB((BYTE)(sr/cnt), (BYTE)(sg/cnt), (BYTE)(sb/cnt));
}

/*******************************************************************************
 * SampleTreeBackground
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Samples the actual rendered background color of a TreeView. This is necessary
 * because TreeView backgrounds can be gradients, themes, or parent-painted.
 * Simply reading TVM_GETBKCOLOR is insufficient.
 * 
 * Сэмплирует фактический отрисованный цвет фона TreeView. Необходимо, потому
 * что фоны TreeView могут быть градиентами, темами или отрисованы родителем.
 * Простого чтения TVM_GETBKCOLOR недостаточно.
 * 
 * ALGORITHM / АЛГОРИТМ:
 * 1. Create small offscreen bitmap (64x32)
 * 2. Render TreeView background into it
 * 3. Average top strip of pixels
 * 4. Return averaged color
 * 
 * 1. Создать маленький offscreen битмап (64x32)
 * 2. Отрисовать фон TreeView в него
 * 3. Усреднить верхнюю полосу пикселей
 * 4. Вернуть усреднённый цвет
 * 
 * RENDERING STRATEGY / СТРАТЕГИЯ ОТРИСОВКИ:
 * - If TVM_GETBKCOLOR valid: use solid brush
 * - Else: try parent WM_PRINTCLIENT (for themed backgrounds)
 * - Then: TreeView WM_ERASEBKGND
 * - Fallback: system window color
 * 
 * - Если TVM_GETBKCOLOR допустим: использовать сплошную кисть
 * - Иначе: попытаться родительский WM_PRINTCLIENT (для тематических фонов)
 * - Затем: TreeView WM_ERASEBKGND
 * - Резерв: системный цвет окна
 ******************************************************************************/
static COLORREF SampleTreeBackground(HWND hTree, COLORREF fallback)
{
    // Get client area size / Получить размер клиентской области
    RECT rc; 
    if (!GetClientRect(hTree, &rc)) return fallback;
    int W = rc.right - rc.left; 
    if (W<=0) return fallback;
    int H = rc.bottom - rc.top; 
    if (H<=0) return fallback;
    
    // Limit sample size for performance / Ограничить размер образца для производительности
    if (W>64) W=64; 
    if (H>32) H=32;

    // Create offscreen DIB / Создать offscreen DIB
    void* bits = NULL; 
    HBITMAP hbm = CreateDIB32(W, H, &bits);
    if (!hbm || !bits) { 
        if (hbm) DeleteObject(hbm); 
        return fallback; 
    }

    HDC hdc = CreateCompatibleDC(NULL); 
    if (!hdc) { 
        DeleteObject(hbm); 
        return fallback; 
    }
    HGDIOBJ old = SelectObject(hdc, hbm);

    // Try to get TreeView background color / Попытаться получить цвет фона TreeView
    COLORREF tvbk = TreeView_GetBkColor(hTree);
    if (tvbk != (COLORREF)-1 && tvbk != CLR_NONE && tvbk != CLR_DEFAULT) {
        // Valid solid color / Допустимый сплошной цвет
        HBRUSH br = CreateSolidBrush(tvbk);
        RECT r2 = {0, 0, W, H}; 
        FillRect(hdc, &r2, br); 
        DeleteObject(br);
    } else {
        // Complex background: render it / Сложный фон: отрисовать его
        HWND parent = GetParent(hTree);
        BOOL drawn = FALSE;
        
        if (parent && IsWindow(parent)) {
            // Try parent background brush / Попытаться кисть фона родителя
            HBRUSH br = (HBRUSH)(LONG_PTR)GetClassLongPtr(parent, GCLP_HBRBACKGROUND);
            RECT r2 = {0, 0, W, H};
            if (br) FillRect(hdc, &r2, br);
            
            // Parent WM_PRINTCLIENT (for themed backgrounds)
            // Родительский WM_PRINTCLIENT (для тематических фонов)
            SendMessageA(parent, WM_PRINTCLIENT, (WPARAM)hdc, PRF_CLIENT|PRF_ERASEBKGND);
            drawn = TRUE;
        }
        
        // TreeView WM_ERASEBKGND / TreeView WM_ERASEBKGND
        SendMessageA(hTree, WM_ERASEBKGND, (WPARAM)hdc, 0);
        
        // Fallback to system window color / Резерв - системный цвет окна
        if (!drawn) {
            HBRUSH br2 = CreateSolidBrush(GetSysColor(COLOR_WINDOW));
            RECT r3 = {0, 0, W, H}; 
            FillRect(hdc, &r3, br2); 
            DeleteObject(br2);
        }
    }

    // Average top strip / Усреднить верхнюю полосу
    COLORREF avg = AverageTopStrip32((const DWORD*)bits, W, H, 4);

    SelectObject(hdc, old); 
    DeleteDC(hdc); 
    DeleteObject(hbm);
    
    return avg ? avg : fallback;
}

/*******************************************************************************
 * GLOBAL STATE
 * ГЛОБАЛЬНОЕ СОСТОЯНИЕ
 ******************************************************************************/

// Window class names / Имена классов окон
static const char* CLS_WINAMP_MAIN = "Winamp v1.x";
static const char* CLS_WINAMP_GEN  = "Winamp Gen";

// Worker thread / Рабочий поток
static HANDLE   g_thr = NULL;
static volatile LONG g_thrRun = 0;

// TreeView subclass / Субкласс TreeView
static HWND     g_hwndTree    = NULL;
static WNDPROC  g_oldTreeProc = NULL;

// Last tint state / Последнее состояние тонирования
static HIMAGELIST g_himlLast  = NULL;
static COLORREF   g_lastTxt   = (COLORREF)0xFFFFFFFF;

// Force color override / Принудительное переопределение цвета
static COLORREF   g_forceClr  = 0;
static BOOL       g_forceOnce = FALSE;

// Recursion guard / Защита от рекурсии
static BOOL       g_inRetint  = FALSE;

// Baseline icon storage / Хранилище baseline иконок
static HICON*  g_origIcons = NULL;
static int     g_origCount = 0;

/*******************************************************************************
 * WINDOW FINDING HELPERS
 * ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ ПОИСКА ОКОН
 ******************************************************************************/

/*******************************************************************************
 * IsOurProcessWindow
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Checks if window belongs to current process.
 * 
 * Проверяет, принадлежит ли окно текущему процессу.
 ******************************************************************************/
static BOOL IsOurProcessWindow(HWND h) { 
    DWORD pid=0; 
    GetWindowThreadProcessId(h, &pid); 
    return pid == GetCurrentProcessId(); 
}

/*******************************************************************************
 * IsWinampAncestor
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Checks if window has Winamp window as ancestor.
 * 
 * Проверяет, имеет ли окно окно Winamp в качестве предка.
 ******************************************************************************/
static BOOL IsWinampAncestor(HWND h)
{
    HWND p = h; 
    char cls[64];
    while (p) { 
        cls[0] = 0; 
        GetClassNameA(p, cls, ARRAYSIZE(cls)); 
        if (!lstrcmpiA(cls, CLS_WINAMP_GEN) || !lstrcmpiA(cls, CLS_WINAMP_MAIN)) 
            return TRUE; 
        p = GetParent(p); 
    }
    return FALSE;
}

/*******************************************************************************
 * EnumFindTreeProc
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * EnumChildWindows callback to find TreeView control.
 * 
 * Функция обратного вызова EnumChildWindows для поиска контрола TreeView.
 ******************************************************************************/
static BOOL CALLBACK EnumFindTreeProc(HWND h, LPARAM lp)
{
    char cls[64] = {0}; 
    GetClassNameA(h, cls, ARRAYSIZE(cls));
    
    if (!lstrcmpiA(cls, "SysTreeView32") && 
        IsOurProcessWindow(h) && 
        IsWinampAncestor(h)) { 
        *(HWND*)lp = h; 
        return FALSE;  // Stop enumeration / Остановить перечисление
    }
    
    EnumChildWindows(h, EnumFindTreeProc, lp); 
    return TRUE;
}

/*******************************************************************************
 * FindMLTree
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Finds Media Library TreeView window by searching all Winamp windows.
 * 
 * Находит окно TreeView библиотеки, ища во всех окнах Winamp.
 * 
 * SEARCH STRATEGY / СТРАТЕГИЯ ПОИСКА:
 * 1. Search all "Winamp Gen" windows (Media Library)
 * 2. Search main "Winamp v1.x" window
 * 3. Recursively enumerate children looking for SysTreeView32
 * 
 * 1. Искать все окна "Winamp Gen" (Библиотека)
 * 2. Искать главное окно "Winamp v1.x"
 * 3. Рекурсивно перечислять детей в поиске SysTreeView32
 ******************************************************************************/
static HWND FindMLTree()
{
    HWND tree = NULL;
    
    // Search all Media Library windows / Искать все окна библиотеки
    for (HWND gen = FindWindowA(CLS_WINAMP_GEN, NULL); 
         gen; 
         gen = FindWindowExA(NULL, gen, CLS_WINAMP_GEN, NULL)) {
        if (!IsOurProcessWindow(gen)) continue;
        EnumChildWindows(gen, EnumFindTreeProc, (LPARAM)&tree);
        if (tree) return tree;
    }
    
    // Search main Winamp window / Искать главное окно Winamp
    HWND wa = FindWindowA(CLS_WINAMP_MAIN, NULL);
    if (wa && IsOurProcessWindow(wa)) { 
        EnumChildWindows(wa, EnumFindTreeProc, (LPARAM)&tree); 
        if (tree) return tree; 
    }
    
    return NULL;
}

/*******************************************************************************
 * BASELINE ICON MANAGEMENT
 * УПРАВЛЕНИЕ BASELINE ИКОНКАМИ
 ******************************************************************************/

/*******************************************************************************
 * FreeBaseline
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Frees all stored baseline icon copies.
 * 
 * Освобождает все сохранённые копии baseline иконок.
 ******************************************************************************/
static void FreeBaseline()
{
    if (g_origIcons) { 
        for (int i=0; i<g_origCount; ++i) 
            if (g_origIcons[i]) 
                DestroyIcon(g_origIcons[i]); 
        HeapFree(GetProcessHeap(), 0, g_origIcons); 
    }
    g_origIcons = NULL; 
    g_origCount = 0;
}

/*******************************************************************************
 * BuildBaseline
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Creates baseline copies of all icons in an image list. These pristine copies
 * are used as source for all future re-tinting operations.
 * 
 * Создаёт baseline копии всех иконок в списке изображений. Эти чистые копии
 * используются как источник для всех будущих операций повторного тонирования.
 * 
 * WHY NEEDED / ЗАЧЕМ НУЖНО:
 * Re-tinting already-tinted icons causes cumulative quality loss. By always
 * tinting from baseline > final, we maintain quality.
 * 
 * Повторное тонирование уже тонированных иконок вызывает накопительную потерю
 * качества. Всегда тонируя из baseline > финал, мы поддерживаем качество.
 * 
 * PARAMETERS / ПАРАМЕТРЫ:
 * himl - Image list to copy from / Список изображений для копирования
 * 
 * RETURNS / ВОЗВРАЩАЕТ:
 * TRUE on success, FALSE on failure
 * TRUE при успехе, FALSE при неудаче
 ******************************************************************************/
static BOOL BuildBaseline(HIMAGELIST himl)
{
    FreeBaseline(); 
    if (!himl) return FALSE;
    
    int n = ImageList_GetImageCount(himl); 
    if (n<=0 || n>4096) return FALSE;  // Sanity check / Проверка разумности
    
    // Allocate icon array / Выделить массив иконок
    g_origIcons = (HICON*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(HICON)*n); 
    if (!g_origIcons) return FALSE;
    g_origCount = n;
    
    // Copy all icons / Копировать все иконки
    for (int i=0; i<n; ++i) { 
        HICON h = ImageList_GetIcon(himl, i, ILD_NORMAL); 
        g_origIcons[i] = h ? CopyIcon(h) : NULL; 
        if (h) DestroyIcon(h); 
    }
    
    return TRUE;
}

/*******************************************************************************
 * ICON TINTING CORE ALGORITHM
 * ОСНОВНОЙ АЛГОРИТМ ТОНИРОВАНИЯ ИКОНОК
 ******************************************************************************/

/*******************************************************************************
 * TintIcon_BlendPlusColor
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Tints an icon by blending between background and text color based on pixel
 * luminance. This is the core tinting algorithm.
 * 
 * Тонирует иконку смешиванием между фоном и текстом на основе яркости пикселя.
 * Это основной алгоритм тонирования.
 * 
 * PARAMETERS / ПАРАМЕТРЫ:
 * hIn - Input icon to tint / Входная иконка для тонирования
 * txt - Text color (target for bright pixels) / Цвет текста (цель для ярких пикселей)
 * bk  - Background color (target for dark pixels) / Цвет фона (цель для тёмных пикселей)
 * 
 * RETURNS / ВОЗВРАЩАЕТ:
 * Tinted icon handle or NULL on failure
 * Дескриптор тонированной иконки или NULL при неудаче
 * 
 * ALGORITHM / АЛГОРИТМ:
 * 1. Extract icon to 32bpp DIB
 * 2. For each pixel:
 *    - Calculate luminance: luma = 0.3R + 0.59G + 0.11B
 *    - Use luma as blend factor (0-255)
 *    - Interpolate: newColor = lerp(background, text, luma)
 * 3. Create new icon from tinted DIB
 * 
 * 1. Извлечь иконку в 32bpp DIB
 * 2. Для каждого пикселя:
 *    - Вычислить яркость: luma = 0.3R + 0.59G + 0.11B
 *    - Использовать luma как фактор смешивания (0-255)
 *    - Интерполировать: новыйЦвет = lerp(фон, текст, luma)
 * 3. Создать новую иконку из тонированного DIB
 * 
 * WHY LUMINANCE / ПОЧЕМУ ЯРКОСТЬ:
 * Human eye is most sensitive to green, moderately to red, least to blue.
 * Standard formula: 0.30R + 0.59G + 0.11B approximates perceived brightness.
 * 
 * Человеческий глаз наиболее чувствителен к зелёному, умеренно к красному,
 * меньше всего к синему. Стандартная формула: 0.30R + 0.59G + 0.11B
 * аппроксимирует воспринимаемую яркость.
 ******************************************************************************/
static HICON TintIcon_BlendPlusColor(HICON hIn, COLORREF txt, COLORREF bk)
{
    if(!hIn) return NULL;

    // Get icon info / Получить информацию об иконке
    ICONINFO ii; 
    ZeroMemory(&ii, sizeof(ii));
    if(!GetIconInfo(hIn, &ii)) return NULL;

    int W=0, H=0; 
    void* pBits = NULL;
    HBITMAP hbm32 = NULL; 
    HBITMAP hMaskOut = NULL;

    if (ii.hbmColor) {
        // Color icon: convert to 32bpp / Цветная иконка: преобразовать в 32bpp
        hbm32 = CreateDIB32FromBitmap(ii.hbmColor, &W, &H, &pBits);
        hMaskOut = ii.hbmMask ? ii.hbmMask : NULL;
    } else {
        // Monochrome icon: render to 32bpp / Монохромная иконка: отрисовать в 32bpp
        BITMAP bm; 
        ZeroMemory(&bm, sizeof(bm));
        if (ii.hbmMask && GetObject(ii.hbmMask, sizeof(bm), &bm)) { 
            W = bm.bmWidth; 
            H = (bm.bmBitsPixel==1 && (bm.bmHeight%2)==0) ? (bm.bmHeight/2) : bm.bmHeight; 
        } else { 
            W = 16; 
            H = 16; 
        }
        
        hbm32 = CreateDIB32(W, H, &pBits);
        if (hbm32) {
            HDC hdc = GetDC(NULL); 
            HDC mem = CreateCompatibleDC(hdc);
            HGDIOBJ o = SelectObject(mem, hbm32);
            PatBlt(mem, 0, 0, W, H, BLACKNESS);
            DrawIconEx(mem, 0, 0, hIn, W, H, 0, NULL, DI_NORMAL);
            SelectObject(mem, o); 
            DeleteDC(mem); 
            ReleaseDC(NULL, hdc);
        }
        hMaskOut = ExtractAndMask(ii.hbmMask);
    }

    if(!hbm32 || !pBits) {
        // Cleanup on failure / Очистка при неудаче
        if (ii.hbmColor) DeleteObject(ii.hbmColor);
        if (ii.hbmMask)  DeleteObject(ii.hbmMask);
        if (hMaskOut && hMaskOut!=ii.hbmMask) DeleteObject(hMaskOut);
        if (hbm32) DeleteObject(hbm32);
        return NULL;
    }

    // Extract color channels / Извлечь цветовые каналы
    int rTxt = (int)(txt & 0xFF);
    int gTxt = (int)((txt >> 8) & 0xFF);
    int bTxt = (int)((txt >> 16) & 0xFF);

    int rBk  = (int)(bk  & 0xFF);
    int gBk  = (int)((bk  >> 8) & 0xFF);
    int bBk  = (int)((bk  >> 16) & 0xFF);

    // Tint each pixel / Тонировать каждый пиксель
    DWORD* px = (DWORD*)pBits; 
    const int N = W*H;
    
    for (int i=0; i<N; ++i) {
        BYTE* pb = (BYTE*)&px[i] + 0; // B
        BYTE* pg = (BYTE*)&px[i] + 1; // G
        BYTE* pr = (BYTE*)&px[i] + 2; // R

        // Calculate luminance (perceived brightness)
        // Вычислить яркость (воспринимаемую яркость)
        int luma = ( (int)(*pr)*30 + (int)(*pg)*59 + (int)(*pb)*11 ) / 100;
        int t = luma;  // Blend factor 0-255 / Фактор смешивания 0-255

        // Interpolate between background and text
        // Интерполировать между фоном и текстом
        int rr = Lerp8(rBk, rTxt, t);
        int gg = Lerp8(gBk, gTxt, t);
        int bb = Lerp8(bBk, bTxt, t);

        *pr = (BYTE)rr;
        *pg = (BYTE)gg;
        *pb = (BYTE)bb;
    }

    // Create new icon from tinted DIB / Создать новую иконку из тонированного DIB
    ICONINFO io; 
    ZeroMemory(&io, sizeof(io));
    io.fIcon = TRUE; 
    io.hbmColor = hbm32; 
    io.hbmMask = hMaskOut ? hMaskOut : ii.hbmMask;
    HICON hOut = CreateIconIndirect(&io);

    // Cleanup / Очистка
    if (ii.hbmColor) DeleteObject(ii.hbmColor);
    if (ii.hbmMask)  DeleteObject(ii.hbmMask);
    if (hMaskOut && hMaskOut!=ii.hbmMask) DeleteObject(hMaskOut);
    if (hbm32) DeleteObject(hbm32);

    return hOut;
}

/*******************************************************************************
 * RetintImageList
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Re-tints all icons in an image list to match current TreeView colors.
 * 
 * Перетонирует все иконки в списке изображений под текущие цвета TreeView.
 * 
 * PARAMETERS / ПАРАМЕТРЫ:
 * hTree - TreeView window / Окно TreeView
 * himl  - Image list to tint / Список изображений для тонирования
 * txt   - Text color / Цвет текста
 * 
 * ALGORITHM / АЛГОРИТМ:
 * 1. Sample background color (handles gradients/themes)
 * 2. Smart fallback if sampling fails
 * 3. Rebuild baseline if needed
 * 4. Tint each icon from baseline
 * 5. Replace in image list
 * 6. Redraw TreeView
 * 
 * 1. Сэмплировать цвет фона (обрабатывает градиенты/темы)
 * 2. Умный резервный вариант при неудаче сэмплирования
 * 3. Перестроить baseline при необходимости
 * 4. Тонировать каждую иконку из baseline
 * 5. Заменить в списке изображений
 * 6. Перерисовать TreeView
 * 
 * SMART FALLBACK / УМНЫЙ РЕЗЕРВНЫЙ ВАРИАНТ:
 * If text is very bright (luma > 128), assume dark background.
 * If sampled background is white and text very bright, force black background.
 * 
 * Если текст очень яркий (luma > 128), предположить тёмный фон.
 * Если сэмплированный фон белый и текст очень яркий, принудительно чёрный фон.
 ******************************************************************************/
static void RetintImageList(HWND hTree, HIMAGELIST himl, COLORREF txt)
{
    if(!hTree || !himl) return;

    // Calculate text luminance / Вычислить яркость текста
    int rT = (int)(txt & 0xFF);
    int gT = (int)((txt >> 8) & 0xFF);
    int bT = (int)((txt >> 16) & 0xFF);
    int txtLuma = (rT*30 + gT*59 + bT*11)/100;

    // Smart fallback for dark themes / Умный резервный вариант для тёмных тем
    COLORREF smartFallback = (txtLuma > 128) ? RGB(0,0,0) : GetSysColor(COLOR_WINDOW);

    // Get TreeView background / Получить фон TreeView
    COLORREF bk = TreeView_GetBkColor(hTree);
    if (bk==(COLORREF)-1 || bk==CLR_NONE || bk==CLR_DEFAULT) {
        bk = smartFallback;
    }

    // Sample actual rendered background / Сэмплировать фактический отрисованный фон
    COLORREF sampled = SampleTreeBackground(hTree, bk);
    
    // Override white background for very bright text / Переопределить белый фон для очень яркого текста
    if (sampled == GetSysColor(COLOR_WINDOW) && txtLuma > 160) {
         sampled = RGB(0,0,0);
    }

    if (sampled != bk) bk = sampled;

    __try {
        int count = ImageList_GetImageCount(himl);
        
        // Rebuild baseline if needed / Перестроить baseline при необходимости
        if (!g_origIcons || g_origCount != count) 
            BuildBaseline(himl);

        g_inRetint = TRUE;  // Set recursion guard / Установить защиту от рекурсии
        
        int n = ImageList_GetImageCount(himl);
        for (int i=0; i<n; ++i) {
            // Get baseline icon / Получить baseline иконку
            HICON hSrc = (g_origIcons && i<g_origCount) ? g_origIcons[i] : NULL;
            BOOL tempSrc = FALSE;
            
            // Fallback: extract from image list / Резерв: извлечь из списка изображений
            if (!hSrc) { 
                HICON tmp = ImageList_GetIcon(himl, i, ILD_NORMAL); 
                if (tmp) { 
                    hSrc = tmp; 
                    tempSrc = TRUE; 
                } 
            }
            if (!hSrc) continue;

            // Tint icon / Тонировать иконку
            HICON hTint = TintIcon_BlendPlusColor(hSrc, txt, bk);
            if (hTint) { 
                ImageList_ReplaceIcon(himl, i, hTint); 
                DestroyIcon(hTint); 
            }
            
            if (tempSrc && hSrc) DestroyIcon(hSrc);
        }
        
        // Redraw TreeView / Перерисовать TreeView
        RedrawWindow(hTree, NULL, NULL, RDW_INVALIDATE|RDW_ERASE|RDW_FRAME);
        
        g_inRetint = FALSE;  // Clear recursion guard / Очистить защиту от рекурсии
    }
    __except(EXCEPTION_EXECUTE_HANDLER) {
        g_inRetint = FALSE;
    }
}

/*******************************************************************************
 * GetTreeColorEffective
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Gets effective text color for TreeView, handling defaults and forced colors.
 * 
 * Получает эффективный цвет текста для TreeView, обрабатывая значения по
 * умолчанию и принудительные цвета.
 ******************************************************************************/
static COLORREF GetTreeColorEffective(HWND hTree)
{
    COLORREF txt = TreeView_GetTextColor(hTree);
    if (txt==(COLORREF)-1 || txt==CLR_DEFAULT) 
        txt = GetSysColor(COLOR_WINDOWTEXT);
    
    // Force color override (one-time) / Принудительное переопределение цвета (одноразовое)
    if (g_forceOnce) { 
        txt = g_forceClr; 
        g_forceOnce = FALSE; 
    }
    
    return txt;
}

/*******************************************************************************
 * TREEVIEW SUBCLASS
 * СУБКЛАСС TREEVIEW
 ******************************************************************************/

/*******************************************************************************
 * CallPrev
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Calls previous window procedure.
 * 
 * Вызывает предыдущую процедуру окна.
 ******************************************************************************/
static LRESULT CallPrev(HWND h, UINT m, WPARAM w, LPARAM l)
{
    if (g_oldTreeProc) 
        return CallWindowProcA(g_oldTreeProc, h, m, w, l);
    return DefWindowProcA(h, m, w, l);
}

/*******************************************************************************
 * Tree_SubclassProc
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Subclassed TreeView window procedure. Intercepts messages that affect icon
 * appearance and triggers re-tinting.
 * 
 * Процедура субклассированного окна TreeView. Перехватывает сообщения, влияющие
 * на внешний вид иконок, и запускает повторное тонирование.
 * 
 * INTERCEPTED MESSAGES / ПЕРЕХВАЧЕННЫЕ СООБЩЕНИЯ:
 * 
 * TVM_SETIMAGELIST:
 *   Called when image list is changed. We rebuild baseline and re-tint.
 *   Вызывается при изменении списка изображений. Перестраиваем baseline и перетонируем.
 * 
 * TVM_SETTEXTCOLOR:
 *   Called when text color changes. We re-tint with new color.
 *   Вызывается при изменении цвета текста. Перетонируем новым цветом.
 * 
 * WM_SYSCOLORCHANGE / WM_THEMECHANGED:
 *   System color scheme or theme changed. Re-sample background and re-tint.
 *   Изменилась цветовая схема системы или тема. Пересэмплировать фон и перетонировать.
 * 
 * WM_NCDESTROY:
 *   Window being destroyed. Cleanup and unsubclass.
 *   Окно уничтожается. Очистка и снятие субкласса.
 ******************************************************************************/
static LRESULT CALLBACK Tree_SubclassProc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    switch(m) {
    case TVM_SETIMAGELIST: {
        LRESULT r = CallPrev(h, m, w, l);
        if (!g_inRetint) {
            HIMAGELIST himl = TreeView_GetImageList(h, TVSIL_NORMAL);
            if (himl) { 
                g_himlLast = himl; 
                BuildBaseline(himl); 
                COLORREF txt = GetTreeColorEffective(h); 
                g_lastTxt = txt; 
                RetintImageList(h, himl, txt); 
            }
        }
        return r;
    }
    
    case TVM_SETTEXTCOLOR: {
        LRESULT r = CallPrev(h, m, w, l);
        if (!g_inRetint) {
            HIMAGELIST himl = TreeView_GetImageList(h, TVSIL_NORMAL);
            if (himl) {
                COLORREF txt = (COLORREF)l;
                if (txt==(COLORREF)-1 || txt==CLR_DEFAULT) 
                    txt = GetTreeColorEffective(h);
                if (txt != g_lastTxt || himl != g_himlLast) { 
                    g_lastTxt = txt; 
                    g_himlLast = himl; 
                    RetintImageList(h, himl, txt); 
                }
            }
        }
        return r;
    }
    
    case WM_SYSCOLORCHANGE:
    case WM_THEMECHANGED: {
        LRESULT r = CallPrev(h, m, w, l);
        if (!g_inRetint) {
            HIMAGELIST himl = TreeView_GetImageList(h, TVSIL_NORMAL);
            if (himl) { 
                COLORREF txt = GetTreeColorEffective(h); 
                if (txt != g_lastTxt || himl != g_himlLast) { 
                    g_lastTxt = txt; 
                    g_himlLast = himl; 
                    RetintImageList(h, himl, txt); 
                } 
            }
        }
        return r;
    }
    
    case WM_NCDESTROY: {
        // Cleanup and unsubclass / Очистка и снятие субкласса
        WNDPROC old = g_oldTreeProc;
        SetWindowLongA(h, GWL_WNDPROC, (LONG)old);
        g_oldTreeProc = NULL; 
        g_hwndTree = NULL; 
        g_himlLast = NULL; 
        g_lastTxt = (COLORREF)0xFFFFFFFF;
        FreeBaseline();
        return old ? CallWindowProcA(old, h, m, w, l) : DefWindowProcA(h, m, w, l);
    }}
    
    return CallPrev(h, m, w, l);
}

/*******************************************************************************
 * INITIALIZATION
 * ИНИЦИАЛИЗАЦИЯ
 ******************************************************************************/

/*******************************************************************************
 * HookTreeOnce
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Finds and subclasses the Media Library TreeView (once).
 * 
 * Находит и субклассирует TreeView библиотеки (один раз).
 ******************************************************************************/
static void HookTreeOnce()
{
    // Already hooked? / Уже захучен?
    if (g_hwndTree && IsWindow(g_hwndTree) && g_oldTreeProc) 
        return;
    
    // Find TreeView / Найти TreeView
    HWND tree = FindMLTree(); 
    if (!tree) return;
    
    // Verify it's a TreeView / Проверить, что это TreeView
    char cls[64] = {0}; 
    GetClassNameA(tree, cls, ARRAYSIZE(cls)); 
    if (lstrcmpiA(cls, "SysTreeView32") != 0) return;

    // Install subclass / Установить субкласс
    g_hwndTree = tree;
    g_oldTreeProc = (WNDPROC)SetWindowLongA(g_hwndTree, GWL_WNDPROC, (LONG)Tree_SubclassProc);

    // Initial tint / Начальное тонирование
    HIMAGELIST himl = TreeView_GetImageList(g_hwndTree, TVSIL_NORMAL);
    if (himl) { 
        g_himlLast = himl; 
        BuildBaseline(himl); 
        COLORREF txt = GetTreeColorEffective(g_hwndTree); 
        g_lastTxt = txt; 
        RetintImageList(g_hwndTree, himl, txt); 
    }
}

/*******************************************************************************
 * WorkerProc
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Worker thread that waits for Winamp to initialize, then hooks TreeView.
 * 
 * Рабочий поток, который ждёт инициализации Winamp, затем хукает TreeView.
 * 
 * PROCESS / ПРОЦЕСС:
 * 1. Wait for Winamp main window to appear and become visible (20s max)
 * 2. Wait for Media Library TreeView to appear (20s max)
 * 3. Hook TreeView when found
 * 
 * 1. Ждать появления и видимости главного окна Winamp (макс. 20с)
 * 2. Ждать появления TreeView библиотеки (макс. 20с)
 * 3. Захукать TreeView при обнаружении
 ******************************************************************************/
static unsigned __stdcall WorkerProc(void*)
{
    __try {
        // Wait for Winamp window to be visible / Ждать видимости окна Winamp
        for (int i=0; i<200; ++i) { 
            HWND wa = FindWindowA(CLS_WINAMP_MAIN, NULL); 
            if (wa && IsWindowVisible(wa)) break; 
            Sleep(100); 
        }
        
        // Wait for TreeView to appear / Ждать появления TreeView
        for (int i=0; i<200; ++i) { 
            HookTreeOnce(); 
            if (g_hwndTree) break; 
            Sleep(100); 
        }
    } __except(EXCEPTION_EXECUTE_HANDLER) {}
    
    InterlockedExchange(&g_thrRun, 0); 
    _endthreadex(0); 
    return 0;
}

/*******************************************************************************
 * EXPORTED PUBLIC API
 * ЭКСПОРТИРУЕМЫЙ ПУБЛИЧНЫЙ API
 ******************************************************************************/

/*******************************************************************************
 * ML_IconsTint_Start
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Starts the icon tinting module. Creates worker thread to find and hook TreeView.
 * 
 * Запускает модуль тонирования иконок. Создаёт рабочий поток для поиска и
 * хукания TreeView.
 * 
 * PARAMETERS / ПАРАМЕТРЫ:
 * hInst - Module instance (unused) / Экземпляр модуля (не используется)
 * 
 * RETURNS / ВОЗВРАЩАЕТ:
 * TRUE on success, FALSE on failure
 * TRUE при успехе, FALSE при неудаче
 ******************************************************************************/
extern "C" BOOL ML_IconsTint_Start(HINSTANCE /*hInst*/)
{
    if (InterlockedExchange(&g_thrRun, 1) == 0) {
        unsigned tid = 0; 
        g_thr = (HANDLE)_beginthreadex(NULL, 0, WorkerProc, NULL, 0, &tid);
        if (!g_thr) { 
            InterlockedExchange(&g_thrRun, 0); 
            return FALSE; 
        }
    }
    return TRUE;
}

/*******************************************************************************
 * ML_IconsTint_Stop
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Stops the icon tinting module. Waits for worker thread, unsubclasses TreeView,
 * and frees all resources.
 * 
 * Останавливает модуль тонирования иконок. Ждёт рабочий поток, снимает субкласс
 * TreeView и освобождает все ресурсы.
 ******************************************************************************/
extern "C" void ML_IconsTint_Stop(void)
{
    // Signal thread to stop / Сигнализировать потоку остановиться
    LONG run = InterlockedExchange(&g_thrRun, 0);
    if (run && g_thr) 
        WaitForSingleObject(g_thr, 1000);
    
    if (g_thr) { 
        CloseHandle(g_thr); 
        g_thr = NULL; 
    }
    
    // Unsubclass TreeView / Снять субкласс TreeView
    if (g_hwndTree && g_oldTreeProc) 
        SetWindowLongA(g_hwndTree, GWL_WNDPROC, (LONG)g_oldTreeProc);
    
    g_oldTreeProc = NULL; 
    g_hwndTree = NULL; 
    g_himlLast = NULL; 
    g_lastTxt = (COLORREF)0xFFFFFFFF; 
    
    FreeBaseline();
}

/*******************************************************************************
 * ML_IconsTint_ForceOnce
 * 
 * PURPOSE / НАЗНАЧЕНИЕ:
 * Forces a one-time color override for the next tint operation.
 * Useful for programmatic color changes.
 * 
 * Принудительно переопределяет цвет один раз для следующей операции тонирования.
 * Полезно для программных изменений цвета.
 * 
 * PARAMETERS / ПАРАМЕТРЫ:
 * forceColor - Color to use instead of TreeView text color
 *              Цвет для использования вместо цвета текста TreeView
 ******************************************************************************/
extern "C" void ML_IconsTint_ForceOnce(COLORREF forceColor) { 
    g_forceClr = forceColor; 
    g_forceOnce = TRUE; 
}