#include "stdafx.h"
#include "FoxWriting.h"
#include "CodePage.h"

#include <fstream>
#include <sstream>

static void DebugLog(const char* fmt, ...)
{
    char buf[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    std::ofstream log("FW_debug.log", std::ios::app);
    if (log.is_open()) {
        log << buf << std::endl;
    }
}

std::unordered_map<int, FontInfo*> g_fontMap;
int g_fontCount = 0;
FontInfo* g_currentFont = NULL;
int g_halign = 0;
int g_valign = 0;
float g_lineSpacing = 0.0f;
bool g_pixelAlign = false;
Gdiplus::GdiplusStartupInput g_gdiplusStartupInput;
ULONG_PTR g_gdiplusToken = 0;
HWND g_gameWindow = NULL;
int g_drawColor1 = 0xFFFFFF;
int g_drawColor2 = 0xFFFFFF;
double g_drawAlpha = 1.0;

static UINT g_codePage = CP_ACP;
static int g_viewWidth = 800;
static int g_viewHeight = 600;

static HWND FindGameWindow()
{
    DWORD pid = GetCurrentProcessId();

    // Scan all top-level windows; pick the visible one with largest client area.
    // The game runs in TRunnerForm (an owned window of TApplication), which has
    // non-zero client area vs TApplication itself which may be 0x0 in IDE mode.
    HWND bestWnd = NULL;
    LONG bestArea = 0;
    HWND hwnd = NULL;
    do {
        hwnd = FindWindowExW(NULL, hwnd, NULL, NULL);
        DWORD wpid;
        GetWindowThreadProcessId(hwnd, &wpid);
        if (wpid != pid || !IsWindowVisible(hwnd)) continue;
        RECT rc;
        GetClientRect(hwnd, &rc);
        LONG area = rc.right * rc.bottom;
        if (area > bestArea) {
            bestArea = area;
            bestWnd = hwnd;
        }
    } while (hwnd);

    DebugLog("FindGameWindow: best hwnd=%p area=%ld", bestWnd, bestArea);
    return bestWnd;
}

static std::wstring AnsiToWide(LPCSTR input)
{
    if (!input || !*input) return L"";
    int len = MultiByteToWideChar(g_codePage, 0, input, -1, NULL, 0);
    if (len <= 0) {
        // Fallback to ACP
        len = MultiByteToWideChar(CP_ACP, 0, input, -1, NULL, 0);
        if (len <= 0) return L"";
    }
    std::wstring result(len - 1, L'\0');
    MultiByteToWideChar(g_codePage, 0, input, -1, &result[0], len);
    return result;
}

static HWND g_overlayWnd = NULL;
static WNDPROC g_origGameWndProc = NULL;

static void UpdateOverlayPosition();

static LRESULT CALLBACK GameWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (msg == WM_SIZE) {
        if (wParam == SIZE_MINIMIZED) {
            if (g_overlayWnd) ShowWindow(g_overlayWnd, SW_HIDE);
        } else if (g_overlayWnd) {
            ShowWindow(g_overlayWnd, SW_SHOWNA);
            UpdateOverlayPosition();
        }
    }
    if (msg == WM_WINDOWPOSCHANGED || msg == WM_MOVE || msg == WM_SIZE) {
        UpdateOverlayPosition();
    }
    return CallWindowProcW(g_origGameWndProc, hwnd, msg, wParam, lParam);
}

static LRESULT CALLBACK OverlayWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

static void EnsureOverlayWindow()
{
    if (g_overlayWnd && IsWindow(g_overlayWnd)) return;
    if (!g_gameWindow || !IsWindow(g_gameWindow)) return;

    WNDCLASSW wc = {};
    wc.lpfnWndProc = OverlayWndProc;
    wc.hInstance = GetModuleHandleW(NULL);
    wc.lpszClassName = L"FWOverlay";
    RegisterClassW(&wc);

    g_overlayWnd = CreateWindowExW(
        WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW,
        L"FWOverlay", L"", WS_POPUP,
        0, 0, 800, 600, NULL, NULL, wc.hInstance, NULL);
    DebugLog("Overlay window created: %p", g_overlayWnd);
}

static void UpdateOverlayPosition()
{
    if (!g_overlayWnd || !IsWindow(g_overlayWnd)) return;
    if (!g_gameWindow || !IsWindow(g_gameWindow)) return;
    RECT rc;
    GetClientRect(g_gameWindow, &rc);
    POINT pt = { 0, 0 };
    ClientToScreen(g_gameWindow, &pt);
    SetWindowPos(g_overlayWnd, HWND_TOPMOST, pt.x, pt.y,
        rc.right - rc.left, rc.bottom - rc.top,
        SWP_NOACTIVATE | SWP_NOCOPYBITS);
}

DOUBLE WINAPI FWSetViewSize(DOUBLE w, DOUBLE h)
{
    g_viewWidth = (int)w;
    g_viewHeight = (int)h;
    DebugLog("FWSetViewSize(%d, %d)", g_viewWidth, g_viewHeight);
    return TRUE;
}

DOUBLE WINAPI FWInit(DOUBLE sprite, DOUBLE argList)
{
    DebugLog("===== FWInit(%f, %f) =====", sprite, argList);
    wchar_t cwd[512];
    GetCurrentDirectoryW(512, cwd);
    DebugLog("CWD: '%ls'", cwd);
    Gdiplus::GdiplusStartup(&g_gdiplusToken, &g_gdiplusStartupInput, NULL);
    DebugLog("GDI+ started: token=0x%lx", g_gdiplusToken);
    g_gameWindow = FindGameWindow();
    DebugLog("Found game window: %p (class=%s)", g_gameWindow, g_gameWindow ? "?" : "null");
    if (g_gameWindow) {
        g_origGameWndProc = (WNDPROC)SetWindowLongPtrW(g_gameWindow, GWLP_WNDPROC, (LONG_PTR)GameWndProc);
        DebugLog("Subclassed game window: origProc=%p", g_origGameWndProc);
    }
    EnsureOverlayWindow();
    g_fontCount = 0;
    g_currentFont = NULL;
    g_halign = 0;
    g_valign = 0;
    g_lineSpacing = 0.0f;
    g_pixelAlign = false;
    return TRUE;
}

DOUBLE WINAPI FWReleaseCache()
{
    return TRUE;
}

DOUBLE WINAPI FWCleanup()
{
    if (g_origGameWndProc && g_gameWindow && IsWindow(g_gameWindow)) {
        SetWindowLongPtrW(g_gameWindow, GWLP_WNDPROC, (LONG_PTR)g_origGameWndProc);
        DebugLog("Restored original game WndProc");
        g_origGameWndProc = NULL;
    }
    g_currentFont = NULL;
    for (auto& pair : g_fontMap) {
        FontInfo* fi = pair.second;
        if (fi) {
            delete fi->font;
            delete fi->family;
            delete fi->pfc;
            delete fi;
        }
    }
    g_fontMap.clear();
    g_fontCount = 0;
    if (g_gdiplusToken) {
        Gdiplus::GdiplusShutdown(g_gdiplusToken);
        g_gdiplusToken = 0;
    }
    return TRUE;
}

DOUBLE WINAPI FWSetEncoding(LPCSTR CPName)
{
    if (!CPName) { DebugLog("FWSetEncoding: null name"); return FALSE; }
    CodePageEntry* cp = g_codePages;
    while (cp->code_page != 0) {
        if (strcmp(CPName, cp->name) == 0) {
            g_codePage = cp->code_page;
            DebugLog("FWSetEncoding('%s') = %u", CPName, g_codePage);
            return TRUE;
        }
        cp++;
    }
    DebugLog("FWSetEncoding('%s'): NOT FOUND", CPName);
    return FALSE;
}

DOUBLE WINAPI FWSetEncodingEx(DOUBLE codePage)
{
    UINT cp = (UINT)codePage;
    if (cp != 0 && IsValidCodePage(cp) == 0) return FALSE;
    g_codePage = cp;
    return TRUE;
}

DOUBLE WINAPI FWAddFont(LPCSTR name, DOUBLE pt, DOUBLE style)
{
    if (pt <= 0) return -1;
    std::wstring wname = AnsiToWide(name);
    Gdiplus::FontFamily* family = new Gdiplus::FontFamily(wname.c_str());
    if (!family->IsAvailable()) {
        delete family;
        return -1;
    }
    int gdiStyle = (int)style & 0x03; // strip FoxWriting stroke flag (0x4)
    bool stroke = ((int)style & 0x04) != 0;
    Gdiplus::Font* font = new Gdiplus::Font(family, (Gdiplus::REAL)pt,
        gdiStyle, Gdiplus::UnitPoint);
    FontInfo* fi = new FontInfo();
    fi->font = font;
    fi->family = family;
    fi->pfc = NULL;
    fi->style = gdiStyle;
    fi->xOffset = 0;
    fi->yOffset = 0;
    fi->fromFile = false;
    fi->stroke = stroke;
    int index = g_fontCount++;
    g_fontMap[index] = fi;
    return index;
}

DOUBLE WINAPI FWAddFontFromFile(LPCSTR ttf, DOUBLE pt, DOUBLE style)
{
    DebugLog("FWAddFontFromFile('%s', %f, %f)", ttf ? ttf : "null", pt, style);
    if (pt <= 0) { DebugLog("FWAddFontFromFile: pt<=0"); return -1; }
    std::wstring wpath = AnsiToWide(ttf);
    Gdiplus::Font* font = NULL;
    Gdiplus::FontFamily* family = NULL;

    int gdiStyle = (int)style & 0x03; // strip FoxWriting stroke flag (0x4)
    bool stroke = ((int)style & 0x04) != 0;

    Gdiplus::PrivateFontCollection* pfc = new Gdiplus::PrivateFontCollection();
    DebugLog("FWAddFontFromFile: wpath='%ls'", wpath.c_str());
    pfc->AddFontFile(wpath.c_str());
    DebugLog("FWAddFontFromFile: AddFontFile done");
    int found = 0;
    {
        Gdiplus::FontFamily families[1];
        pfc->GetFamilies(1, families, &found);
        if (found > 0 && families[0].IsAvailable()) {
            WCHAR familyName[LF_FACESIZE];
            families[0].GetFamilyName(familyName);
            family = new Gdiplus::FontFamily(familyName, pfc);
            if (family && family->IsAvailable()) {
                font = new Gdiplus::Font(family, (Gdiplus::REAL)pt,
                    gdiStyle, Gdiplus::UnitPoint);
            }
        }
    }

    if (!font) {
        // Try direct path
        DebugLog("FWAddFontFromFile: trying direct path");
        font = new Gdiplus::Font(wpath.c_str(), (Gdiplus::REAL)pt,
            gdiStyle, Gdiplus::UnitPoint);
        if (font && font->GetLastStatus() == Gdiplus::Ok) {
            family = NULL; // Font owns the family reference
            DebugLog("FWAddFontFromFile: direct path OK");
        } else {
            Gdiplus::Status st = font ? font->GetLastStatus() : Gdiplus::NotImplemented;
            DebugLog("FWAddFontFromFile: direct path FAILED status=%d", st);
            delete font;
            delete pfc;
            return -1;
        }
    } else {
        DebugLog("FWAddFontFromFile: PrivateFontCollection path SUCCESS");
    }

    FontInfo* fi = new FontInfo();
    fi->font = font;
    fi->family = family;
    fi->pfc = pfc;
    fi->style = gdiStyle;
    fi->xOffset = 0;
    fi->yOffset = 0;
    fi->fromFile = true;
    fi->stroke = stroke;
    int index = g_fontCount++;
    g_fontMap[index] = fi;
    DebugLog("FWAddFontFromFile -> index=%d (font=%p family=%p)", index, font, family);
    return index;
}

DOUBLE WINAPI FWDeleteFont(DOUBLE font)
{
    int idx = (int)font;
    auto it = g_fontMap.find(idx);
    if (it == g_fontMap.end()) return FALSE;
    FontInfo* fi = it->second;
    if (g_currentFont == fi) g_currentFont = NULL;
    delete fi->font;
    delete fi->family;
    delete fi->pfc;
    delete fi;
    g_fontMap.erase(it);
    return TRUE;
}

DOUBLE WINAPI FWPreloadFont(DOUBLE font, DOUBLE from, DOUBLE to)
{
    return TRUE;
}

DOUBLE WINAPI FWSetFontOffset(DOUBLE font, DOUBLE xOffset, DOUBLE yOffset)
{
    int idx = (int)font;
    auto it = g_fontMap.find(idx);
    if (it == g_fontMap.end()) return FALSE;
    it->second->xOffset = (float)xOffset;
    it->second->yOffset = (float)yOffset;
    return TRUE;
}

DOUBLE WINAPI FWSetFont(DOUBLE font)
{
    int idx = (int)font;
    auto it = g_fontMap.find(idx);
    if (it == g_fontMap.end()) { DebugLog("FWSetFont(%d): NOT FOUND", idx); return FALSE; }
    g_currentFont = it->second;
    DebugLog("FWSetFont(%d) -> font=%p family=%p", idx, g_currentFont->font, g_currentFont->family);
    return TRUE;
}

DOUBLE WINAPI FWSetHAlign(DOUBLE align)
{
    g_halign = (int)align;
    return TRUE;
}

DOUBLE WINAPI FWSetVAlign(DOUBLE align)
{
    g_valign = (int)align;
    return TRUE;
}

DOUBLE WINAPI FWEnablePixelAlignment(DOUBLE enable)
{
    g_pixelAlign = (int)enable != 0;
    return TRUE;
}

DOUBLE WINAPI FWSetLineSpacing(DOUBLE sep)
{
    g_lineSpacing = (float)sep;
    return TRUE;
}

static Gdiplus::Font* GetGdiFont()
{
    if (!g_currentFont || !g_currentFont->font) return NULL;
    return g_currentFont->font;
}

DOUBLE WINAPI FWStringWidth(LPCSTR str)
{
    Gdiplus::Font* font = GetGdiFont();
    if (!font) return 0;
    std::wstring wstr = AnsiToWide(str);
    if (wstr.empty()) return 0;

    HDC hdc = GetDC(NULL);
    Gdiplus::Graphics g(hdc);
    g.SetPageUnit(Gdiplus::UnitPixel);
    Gdiplus::RectF layout(0, 0, 0, 0);
    Gdiplus::RectF bounds;
    g.MeasureString(wstr.c_str(), -1, font, Gdiplus::PointF(0, 0), &bounds);
    ReleaseDC(NULL, hdc);
    return bounds.Width;
}

DOUBLE WINAPI FWStringHeight(LPCSTR str)
{
    Gdiplus::Font* font = GetGdiFont();
    if (!font) return 0;
    std::wstring wstr = AnsiToWide(str);
    if (wstr.empty()) return 0;

    HDC hdc = GetDC(NULL);
    Gdiplus::Graphics g(hdc);
    g.SetPageUnit(Gdiplus::UnitPixel);
    Gdiplus::RectF layout(0, 0, 0, 0);
    Gdiplus::RectF bounds;
    g.MeasureString(wstr.c_str(), -1, font, Gdiplus::PointF(0, 0), &bounds);
    ReleaseDC(NULL, hdc);
    return bounds.Height;
}

DOUBLE WINAPI FWStringWidthEx(LPCSTR str, DOUBLE sep, DOUBLE w)
{
    return FWStringWidth(str);
}

DOUBLE WINAPI FWStringHeightEx(LPCSTR str, DOUBLE sep, DOUBLE w)
{
    return FWStringHeight(str);
}

// Draw text using GDI+ to a transparent overlay window on top of the game window
// This avoids D3D overwriting GDI content
static void DrawGdiText(int x, int y, LPCSTR str, int color, double alpha)
{
    Gdiplus::Font* font = GetGdiFont();
    if (!font) { DebugLog("DrawGdiText: no font"); return; }

    bool empty = (!str || !*str);
    std::wstring wstr;
    if (!empty) {
        wstr = AnsiToWide(str);
        if (wstr.empty()) empty = true;
    }

    if (!g_gameWindow || !IsWindow(g_gameWindow)) {
        g_gameWindow = FindGameWindow();
    }
    if (!g_gameWindow || !IsWindow(g_gameWindow)) return;

    EnsureOverlayWindow();
    if (!g_overlayWnd || !IsWindow(g_overlayWnd)) return;

    UpdateOverlayPosition();

    RECT rc;
    GetClientRect(g_gameWindow, &rc);
    if (rc.right <= 0 || rc.bottom <= 0) return;

    int w = rc.right, h = rc.bottom;
    POINT screenPt = {0, 0};
    ClientToScreen(g_gameWindow, &screenPt);

    HDC screenDC = GetDC(NULL);
    if (!screenDC) return;

    // Create 32-bit ARGB DIBSection for per-pixel alpha
    BITMAPV5HEADER bi = {};
    bi.bV5Size = sizeof(BITMAPV5HEADER);
    bi.bV5Width = w;
    bi.bV5Height = -h; // top-down
    bi.bV5Planes = 1;
    bi.bV5BitCount = 32;
    bi.bV5Compression = BI_RGB;
    bi.bV5AlphaMask = 0xFF000000;

    void* bits = NULL;
    HBITMAP dib = CreateDIBSection(screenDC, (BITMAPINFO*)&bi, DIB_RGB_COLORS, &bits, NULL, 0);
    if (!dib) { ReleaseDC(NULL, screenDC); return; }

    HDC memdc = CreateCompatibleDC(screenDC);
    HBITMAP oldbmp = (HBITMAP)SelectObject(memdc, dib);

    // Wrap DIB with Gdiplus::Bitmap for alpha-aware drawing
    Gdiplus::Bitmap gdiBmp(w, h, 4 * w, PixelFormat32bppARGB, (BYTE*)bits);
    Gdiplus::Graphics g(&gdiBmp);
    g.SetPageUnit(Gdiplus::UnitPixel);
    g.SetSmoothingMode(Gdiplus::SmoothingModeHighQuality);
    // Match original FoxWriting: ClearType for small fonts, anti-alias for large
    Gdiplus::REAL fontSize = font->GetSize();
    g.SetTextRenderingHint(fontSize <= 20.0f
        ? Gdiplus::TextRenderingHintClearTypeGridFit
        : Gdiplus::TextRenderingHintAntiAliasGridFit);
    g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHighQuality);

    // Clear to transparent
    g.Clear(Gdiplus::Color(0, 0, 0, 0));

    if (!empty) {
        BYTE a = (BYTE)(alpha * 255.0f);
        BYTE r = (color >> 16) & 0xFF;
        BYTE gr = (color >> 8) & 0xFF;
        BYTE b = color & 0xFF;

        // Scale coordinates from game view to window client (for window maximize etc.)
        float scaleX = (float)w / (float)g_viewWidth;
        float scaleY = (float)h / (float)g_viewHeight;
        g.ScaleTransform(scaleX, scaleY, Gdiplus::MatrixOrderAppend);

        float fx = (float)x + (g_currentFont ? g_currentFont->xOffset : 0);
        float fy = (float)y + (g_currentFont ? g_currentFont->yOffset : 0);
        if (g_pixelAlign) { fx = floorf(fx); fy = floorf(fy); }

        Gdiplus::RectF bounds;
        g.MeasureString(wstr.c_str(), -1, font, Gdiplus::PointF(0, 0), &bounds);

        Gdiplus::PointF pt(fx, fy);
        if (g_valign == 1) pt.Y = fy - bounds.Height / 2.0f;
        else if (g_valign == 2) pt.Y = fy - bounds.Height;
        if (g_halign == 1) pt.X = fx - bounds.Width / 2.0f;
        else if (g_halign == 2) pt.X = fx - bounds.Width;

        // Draw outline (stroke) if enabled: 8-dir offset in fully opaque black
        // Scale the 1px offset by inverse scale so stroke stays 1px in screen space
        float invScaleX = 1.0f / scaleX;
        float invScaleY = 1.0f / scaleY;
        bool doStroke = g_currentFont && g_currentFont->stroke;
        if (doStroke) {
            Gdiplus::SolidBrush strokeBrush(Gdiplus::Color(255, 0, 0, 0));
            for (int ox = -1; ox <= 1; ox++) {
                for (int oy = -1; oy <= 1; oy++) {
                    if (ox == 0 && oy == 0) continue;
                    g.DrawString(wstr.c_str(), -1, font,
                        Gdiplus::PointF(pt.X + (float)ox * invScaleX, pt.Y + (float)oy * invScaleY),
                        &strokeBrush);
                }
            }
        }

        // Draw main text
        Gdiplus::SolidBrush textBrush(Gdiplus::Color(a, r, gr, b));
        g.DrawString(wstr.c_str(), -1, font, pt, &textBrush);
    }

    // Update layered window with per-pixel alpha
    SIZE winSize = {w, h};
    POINT zero = {0, 0};
    BLENDFUNCTION blend = {AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
    UpdateLayeredWindow(g_overlayWnd, screenDC, &screenPt, &winSize, memdc, &zero, 0, &blend, ULW_ALPHA);

    // Show the overlay window on first use
    ShowWindow(g_overlayWnd, SW_SHOWNA);

    SelectObject(memdc, oldbmp);
    DeleteObject(dib);
    DeleteDC(memdc);
    ReleaseDC(NULL, screenDC);

    DebugLog("DrawGdiText: updated overlay at (%d,%d) size=%dx%d stroke=%d",
        screenPt.x, screenPt.y, w, h, g_currentFont ? g_currentFont->stroke : 0);
}

DOUBLE WINAPI FWDrawText(DOUBLE x, DOUBLE y, LPCSTR str)
{
    DebugLog("FWDrawText(%f, %f, '%s')", x, y, str ? str : "null");
    DrawGdiText((int)x, (int)y, str, g_drawColor1, g_drawAlpha);
    return TRUE;
}

DOUBLE WINAPI FWDrawTextEx(DOUBLE x, DOUBLE y, LPCSTR str)
{
    return FWDrawText(x, y, str);
}

DOUBLE WINAPI FWDrawTextTransformed(DOUBLE x, DOUBLE y, LPCSTR str)
{
    return FWDrawText(x, y, str);
}

DOUBLE WINAPI FWDrawTextTransformedEx(DOUBLE x, DOUBLE y, LPCSTR str)
{
    return FWDrawText(x, y, str);
}

DOUBLE WINAPI FWDrawTextColor(DOUBLE x, DOUBLE y, LPCSTR str)
{
    return FWDrawText(x, y, str);
}

DOUBLE WINAPI FWDrawTextColorEx(DOUBLE x, DOUBLE y, LPCSTR str)
{
    return FWDrawText(x, y, str);
}

DOUBLE WINAPI FWDrawTextTransformedColor(DOUBLE x, DOUBLE y, LPCSTR str)
{
    return FWDrawText(x, y, str);
}

DOUBLE WINAPI FWDrawTextTransformedColorEx(DOUBLE x, DOUBLE y, LPCSTR str)
{
    return FWDrawText(x, y, str);
}

DOUBLE WINAPI FWPaint()
{
    return TRUE;
}

DOUBLE WINAPI FWWindowResized()
{
    return TRUE;
}
