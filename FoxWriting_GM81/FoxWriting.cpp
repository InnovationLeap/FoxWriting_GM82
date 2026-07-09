// FoxWriting.cpp – overlay rendering matching original FoxWriting visual style
// Uses GDI+ text path with 8-offset black stroke + gradient fill on persistent DIB.

#include "stdafx.h"
#include <windows.h>
#include <gdiplus.h>
#pragma comment(lib, "gdiplus.lib")
#include <stdio.h>
#include <stdarg.h>
#include <vector>
#include <string>

using namespace Gdiplus;

// ============================================================
// Debug log
// ============================================================
static void DebugLog(const char* fmt, ...)
{
    char buf[512];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    OutputDebugStringA("[FoxWriting] ");
    OutputDebugStringA(buf);
    OutputDebugStringA("\n");

    FILE* f = fopen("FW_debug.log", "a");
    if (f) { fprintf(f, "[FoxWriting] %s\n", buf); fclose(f); }
}

// ============================================================
// Globals
// ============================================================
static HWND    g_gameWindow  = NULL;
static HWND    g_overlay     = NULL;
static bool    g_overlayFailed = false;
static ULONG_PTR g_gdiplusToken = 0;
static DWORD   g_mainThreadId = 0;
static PrivateFontCollection* g_pfc = NULL;

// Persistent DIB
static HBITMAP g_hDib   = NULL;
static HDC     g_hdcMem = NULL;
static void*   g_bits   = NULL;
static int     g_dibW   = 0;
static int     g_dibH   = 0;
static int     g_xdpi   = 96;
static int     g_ydpi   = 96;

// Track GDI-loaded fonts
static std::vector<std::wstring> g_gdiFonts;

// Font storage
struct FontInfo {
    std::wstring familyName;
    float size;
    bool bold;
    bool italic;
    bool stroke;
};
static std::vector<FontInfo> g_fonts;

// Current text settings
static int     g_curFontId   = 0;
static int     g_halign      = 0;
static int     g_valign      = 0;
static int     g_lineSpacing = 0;

// Colors (set by FWSetTextColor)
static int     g_curColor1    = 0xFFFFFFFF;
static int     g_curColor2    = 0xFFFFFFFF;
static float   g_curAlpha     = 1.0f;

// Game window subclass
static bool    g_gameMinimized = false;
static WNDPROC g_origGameProc  = NULL;
static int     g_overlayRetry  = 0;

// ============================================================
// Utility
// ============================================================
static std::wstring AnsiToWide(LPCSTR str)
{
    if (!str || !*str) return L"";
    int len = MultiByteToWideChar(CP_UTF8, 0, str, -1, NULL, 0);
    if (len <= 0) return L"";
    std::wstring wstr(len - 1, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, str, -1, &wstr[0], len);
    return wstr;
}

static float Pt2Px(float pt) { return pt * g_ydpi / 72.0f; }

// ============================================================
// Game window subclass
// ============================================================
static LRESULT CALLBACK GameWindowProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_SIZE:
        if (wp == SIZE_MINIMIZED) {
            g_gameMinimized = true;
            if (g_overlay) ShowWindow(g_overlay, SW_HIDE);
        } else if (wp == SIZE_RESTORED || wp == SIZE_MAXIMIZED) {
            g_gameMinimized = false;
            if (g_overlay) {
                RECT cr; GetClientRect(hwnd, &cr);
                POINT pt = {0,0}; ClientToScreen(hwnd, &pt);
                SetWindowPos(g_overlay, HWND_TOPMOST,
                             pt.x, pt.y, cr.right, cr.bottom,
                             SWP_NOACTIVATE | SWP_SHOWWINDOW);
            }
        }
        break;
    case WM_WINDOWPOSCHANGED: {
        WINDOWPOS* wpos = (WINDOWPOS*)lp;
        if (!(wpos->flags & SWP_NOMOVE) || !(wpos->flags & SWP_NOSIZE)) {
            if (g_overlay && !g_gameMinimized && IsWindowVisible(hwnd)) {
                RECT cr; GetClientRect(hwnd, &cr);
                POINT pt = {0,0}; ClientToScreen(hwnd, &pt);
                SetWindowPos(g_overlay, HWND_TOPMOST,
                             pt.x, pt.y, cr.right, cr.bottom, SWP_NOACTIVATE);
            }
        }
        break;
    }
    case WM_DESTROY:
        if (g_overlay) { DestroyWindow(g_overlay); g_overlay = NULL; }
        break;
    }
    return CallWindowProcA(g_origGameProc, hwnd, msg, wp, lp);
}

static void SubclassGameWindow()
{
    if (!g_gameWindow || g_origGameProc) return;
    g_origGameProc = (WNDPROC)SetWindowLongPtrA(g_gameWindow, GWLP_WNDPROC, (LONG_PTR)GameWindowProc);
}

// ============================================================
// Overlay window proc
// ============================================================
static LRESULT CALLBACK OverlayWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_ERASEBKGND: return 1;
    case WM_NCDESTROY:
        if (hwnd == g_overlay) { g_overlay = NULL; g_overlayFailed = true; }
        break;
    }
    return DefWindowProcA(hwnd, msg, wp, lp);
}

// ============================================================
// Persistent DIB management
// ============================================================
static bool EnsureDib(int w, int h)
{
    if (g_hDib && g_dibW == w && g_dibH == h) return true;
    if (g_hDib) { DeleteObject(g_hDib); g_hDib = NULL; }
    if (g_hdcMem) { DeleteDC(g_hdcMem); g_hdcMem = NULL; }
    g_bits = NULL;
    if (w <= 0 || h <= 0) return false;
    g_hdcMem = CreateCompatibleDC(NULL);
    if (!g_hdcMem) return false;
    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = w;
    bmi.bmiHeader.biHeight = -h;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;
    g_hDib = CreateDIBSection(g_hdcMem, &bmi, DIB_RGB_COLORS, &g_bits, NULL, 0);
    if (!g_hDib) { DeleteDC(g_hdcMem); g_hdcMem = NULL; return false; }
    SelectObject(g_hdcMem, g_hDib);
    g_dibW = w; g_dibH = h;
    return true;
}

static void ClearDib()
{
    if (g_bits && g_dibW > 0 && g_dibH > 0)
        memset(g_bits, 0, (size_t)g_dibW * g_dibH * 4);
}

static void PresentDib()
{
    if (!g_overlay || !g_hDib || !g_hdcMem) return;
    SIZE sz = { g_dibW, g_dibH };
    POINT zero = {0,0};
    BLENDFUNCTION blend = { AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };
    UpdateLayeredWindow(g_overlay, NULL, NULL, &sz, g_hdcMem, &zero, 0, &blend, ULW_ALPHA);
}

// ============================================================
// Render text onto DIB – matches original FoxWriting style
// ============================================================
static void RenderText(int fontId, const std::wstring& text,
                        float x, float y, int ha, int va,
                        int color1, int color2, float alpha)
{
    if (text.empty()) return;
    if (fontId < 0 || fontId >= (int)g_fonts.size()) return;
    if (!g_bits || !g_hdcMem) return;

    const FontInfo& fi = g_fonts[fontId];

    Graphics g(g_hdcMem);
    g.SetPageUnit(UnitPixel);
    g.SetSmoothingMode(SmoothingModeHighQuality);
    g.SetTextRenderingHint(fi.size <= 20
        ? TextRenderingHintClearTypeGridFit
        : TextRenderingHintAntiAliasGridFit);
    g.SetPixelOffsetMode(PixelOffsetModeHighQuality);
    g.SetCompositingMode(CompositingModeSourceOver);

    FontFamily fontFamily(fi.familyName.c_str(), g_pfc);
    if (fontFamily.GetLastStatus() != Ok) return;

    int style = FontStyleRegular;
    if (fi.bold)   style |= FontStyleBold;
    if (fi.italic) style |= FontStyleItalic;

    // Original FoxWriting uses UnitPoint (pt), converts to world via Pt2Px
    float fontSize = fi.size;
    Font font(&fontFamily, fontSize, style, UnitPoint);
    if (font.GetLastStatus() != Ok) return;

    StringFormat sf;
    sf.SetAlignment(StringAlignmentNear);
    sf.SetLineAlignment(StringAlignmentNear);
    sf.SetFormatFlags(StringFormatFlagsNoWrap);

    // Measure
    RectF bounds;
    PointF origin(0, 0);
    g.MeasureString(text.c_str(), (INT)text.length(), &font, origin, &sf, &bounds);

    float dx = 0, dy = 0;
    if (ha == 1) dx = -bounds.Width / 2;
    else if (ha == 2) dx = -bounds.Width;
    if (va == 1) dy = -bounds.Height / 2;
    else if (va == 2) dy = -bounds.Height;

    PointF pt(x + dx, y + dy);

    // Build text path
    GraphicsPath path;
    path.AddString(text.c_str(), (INT)text.length(), &fontFamily, style, (REAL)Pt2Px(fontSize), pt, &sf);

    BYTE a = (BYTE)(alpha * 255.0f);
    BYTE r1 = (BYTE)((color1 >> 16) & 0xFF);
    BYTE g1 = (BYTE)((color1 >> 8) & 0xFF);
    BYTE b1 = (BYTE)(color1 & 0xFF);
    BYTE r2 = (BYTE)((color2 >> 16) & 0xFF);
    BYTE g2 = (BYTE)((color2 >> 8) & 0xFF);
    BYTE b2 = (BYTE)(color2 & 0xFF);

    if (fi.stroke) {
        // Stroke = draw black text at 8 offset positions (1px)
        Color black(a, 0, 0, 0);
        SolidBrush blackBrush(black);
        int offsets[8][2] = {
            {1,1},{1,0},{1,-1},{0,1},{0,-1},{-1,1},{-1,0},{-1,-1}
        };
        for (int i = 0; i < 8; i++) {
            Graphics g2(g_hdcMem);
            g2.SetPageUnit(UnitPixel);
            g2.SetSmoothingMode(SmoothingModeHighQuality);
            g2.SetTextRenderingHint(fi.size <= 20
                ? TextRenderingHintClearTypeGridFit
                : TextRenderingHintAntiAliasGridFit);
            g2.SetPixelOffsetMode(PixelOffsetModeHighQuality);
            g2.SetCompositingMode(CompositingModeSourceOver);
            g2.TranslateTransform(offsets[i][0], offsets[i][1]);
            g2.FillPath(&blackBrush, &path);
        }
    }

    // Fill = vertical gradient matching original c1(top)/c2(bottom)
    Color col1(a, r1, g1, b1);
    Color col2(a, r2, g2, b2);
    RectF bounds2;
    path.GetBounds(&bounds2, NULL, NULL);
    LinearGradientBrush fillBrush(
        PointF(bounds2.X, bounds2.Y),
        PointF(bounds2.X, bounds2.Y + bounds2.Height),
        col1, col2);

    g.FillPath(&fillBrush, &path);
}

// ============================================================
// Overlay creation
// ============================================================
static void EnsureOverlay()
{
    if (!g_gameWindow || g_overlay || g_overlayFailed) return;

    RECT cr; GetClientRect(g_gameWindow, &cr);
    int w = cr.right, h = cr.bottom;
    if (w <= 0 || h <= 0) { w = 640; h = 480; }

    WNDCLASSEXA wc = {};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = OverlayWndProc;
    wc.hInstance = GetModuleHandleA(NULL);
    wc.lpszClassName = "FoxWritingOverlay";
    wc.hbrBackground = (HBRUSH)GetStockObject(NULL_BRUSH);
    wc.hCursor = LoadCursorA(NULL, (LPCSTR)IDC_ARROW);
    RegisterClassExA(&wc);

    POINT o = {0,0}; ClientToScreen(g_gameWindow, &o);
    SetLastError(0);
    g_overlay = CreateWindowExA(
        WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW,
        "FoxWritingOverlay", "", WS_POPUP | WS_VISIBLE,
        o.x, o.y, w, h, NULL, NULL, GetModuleHandleA(NULL), NULL);

    if (g_overlay) {
        g_overlayFailed = false;
        SetWindowPos(g_overlay, HWND_TOPMOST, 0,0,0,0, SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE);
        SubclassGameWindow();
        EnsureDib(w, h);
    } else {
        g_overlayFailed = true;
    }
}

static void SyncOverlay()
{
    if (!g_overlay || !g_gameWindow) return;
    RECT cr; GetClientRect(g_gameWindow, &cr);
    if (cr.right <= 0 || cr.bottom <= 0) return;
    POINT pt = {0,0}; ClientToScreen(g_gameWindow, &pt);
    SetWindowPos(g_overlay, HWND_TOPMOST, pt.x, pt.y, cr.right, cr.bottom, SWP_NOACTIVATE);
    if (cr.right != g_dibW || cr.bottom != g_dibH) EnsureDib(cr.right, cr.bottom);
}

// ============================================================
// Exported functions – signatures match original FoxWriting
// ============================================================

DOUBLE WINAPI FWInit(DOUBLE w, DOUBLE h)
{
    DebugLog("===== FWInit(%f, %f) =====", w, h);
    GdiplusStartupInput gsi;
    GdiplusStartup(&g_gdiplusToken, &gsi, NULL);
    g_pfc = new PrivateFontCollection();
    HDC dc = GetDC(0);
    g_xdpi = GetDeviceCaps(dc, LOGPIXELSX);
    g_ydpi = GetDeviceCaps(dc, LOGPIXELSY);
    ReleaseDC(0, dc);

    g_gameWindow = FindWindowA("GameMaker", NULL);
    if (!g_gameWindow) g_gameWindow = FindWindowA("GMS_Window", NULL);
    if (!g_gameWindow) g_gameWindow = FindWindowA("Mf2MainClassTh", NULL);
    if (g_gameWindow) {
        DWORD pid; GetWindowThreadProcessId(g_gameWindow, &pid);
        if (pid != GetCurrentProcessId()) g_gameWindow = NULL;
    }
    if (!g_gameWindow) {
        EnumWindows([](HWND hwnd, LPARAM lp) -> BOOL {
            DWORD pid; GetWindowThreadProcessId(hwnd, &pid);
            if (pid == GetCurrentProcessId() && IsWindowVisible(hwnd)) {
                *(HWND*)lp = hwnd; return FALSE;
            }
            return TRUE;
        }, (LPARAM)&g_gameWindow);
    }
    char wc[256]="";
    if (g_gameWindow) GetClassNameA(g_gameWindow, wc, 256);
    DebugLog("FWInit: hwnd=%p class=%s dpi=%dx%d", g_gameWindow, wc, g_xdpi, g_ydpi);

    g_fonts.clear();
    g_curColor1 = g_curColor2 = 0xFFFFFFFF;
    g_curAlpha = 1.0f;
    g_overlay = NULL;
    g_overlayFailed = false;
    return TRUE;
}

DOUBLE WINAPI FWReleaseCache()
{
    ClearDib();
    return TRUE;
}

DOUBLE WINAPI FWCleanup()
{
    if (g_hDib) { DeleteObject(g_hDib); g_hDib = NULL; g_bits = NULL; }
    if (g_hdcMem) { DeleteDC(g_hdcMem); g_hdcMem = NULL; }
    g_dibW = g_dibH = 0;
    g_fonts.clear();
    if (g_overlay) { DestroyWindow(g_overlay); g_overlay = NULL; }
    if (g_gameWindow && g_origGameProc) {
        SetWindowLongPtrA(g_gameWindow, GWLP_WNDPROC, (LONG_PTR)g_origGameProc);
        g_origGameProc = NULL;
    }
    for (auto& f : g_gdiFonts) RemoveFontResourceExW(f.c_str(), FR_PRIVATE, 0);
    g_gdiFonts.clear();
    delete g_pfc; g_pfc = NULL;
    if (g_gdiplusToken) { GdiplusShutdown(g_gdiplusToken); g_gdiplusToken = 0; }
    return TRUE;
}

DOUBLE WINAPI FWSetEncoding(LPCSTR name)  { return TRUE; }
DOUBLE WINAPI FWSetEncodingEx(DOUBLE cp)  { return TRUE; }

DOUBLE WINAPI FWAddFont(LPCSTR name, DOUBLE pt, DOUBLE style)
{
    std::wstring wname = AnsiToWide(name);
    int s = (int)style;
    FontInfo fi;
    fi.familyName = wname;
    fi.size = (float)pt;
    fi.bold   = (s & 1) != 0;
    fi.italic = (s & 2) != 0;
    fi.stroke = (s & 4) != 0;
    g_fonts.push_back(fi);
    DebugLog("FWAddFont: %ls pt=%.1f style=%d(b%d i%d s%d) id=%d",
             wname.c_str(), fi.size, s, fi.bold, fi.italic, fi.stroke, (int)g_fonts.size()-1);
    return (DOUBLE)(g_fonts.size() - 1);
}

DOUBLE WINAPI FWAddFontFromFile(LPCSTR ttf, DOUBLE pt, DOUBLE style)
{
    if (!ttf || !*ttf) return -1;

    char abs[MAX_PATH];
    GetFullPathNameA(ttf, MAX_PATH, abs, NULL);
    WCHAR wpath[MAX_PATH];
    MultiByteToWideChar(CP_UTF8, 0, abs, -1, wpath, MAX_PATH);

    std::wstring fam;

    if (g_pfc) {
        Status st = g_pfc->AddFontFile(wpath);
        if (st == Ok) {
            int total = g_pfc->GetFamilyCount();
            if (total > 0) {
                FontFamily* ff = new FontFamily[total];
                int found = 0;
                g_pfc->GetFamilies(total, ff, &found);
                WCHAR fn[LF_FACESIZE];
                ff[total-1].GetFamilyName(fn);
                fam = fn;
                delete[] ff;
            }
        }
    }

    if (fam.empty()) {
        int added = AddFontResourceExW(wpath, FR_PRIVATE, 0);
        if (added > 0) {
            g_gdiFonts.push_back(wpath);
            InstalledFontCollection ifc;
            int total = ifc.GetFamilyCount();
            if (total > 0) {
                FontFamily* ff = new FontFamily[total > 0 ? total : 1];
                int found = 0;
                ifc.GetFamilies(total, ff, &found);
                WCHAR fn[LF_FACESIZE] = {0};
                if (found > 0) { ff[found-1].GetFamilyName(fn); fam = fn; }
                delete[] ff;
            }
            if (fam.empty()) {
                std::wstring fn = wpath;
                size_t bs = fn.find_last_of(L'\\');
                if (bs != std::wstring::npos) fn = fn.substr(bs+1);
                size_t dot = fn.find_last_of(L'.');
                if (dot != std::wstring::npos) fn = fn.substr(0, dot);
                fam = fn;
            }
        }
    }

    if (fam.empty()) return -1;

    int s = (int)style;
    FontInfo fi;
    fi.familyName = fam;
    fi.size = (float)pt;
    fi.bold   = (s & 1) != 0;
    fi.italic = (s & 2) != 0;
    fi.stroke = (s & 4) != 0;
    g_fonts.push_back(fi);
    DebugLog("FWAddFontFromFile: %ls pt=%.1f style=%d(b%d i%d s%d) -> id=%d",
             fam.c_str(), pt, s, fi.bold, fi.italic, fi.stroke, (int)g_fonts.size()-1);
    return (DOUBLE)(g_fonts.size() - 1);
}

DOUBLE WINAPI FWDeleteFont(DOUBLE fontId)
{
    int id = (int)fontId;
    if (id >= 0 && id < (int)g_fonts.size()) g_fonts[id].familyName = L"";
    return TRUE;
}

DOUBLE WINAPI FWPreloadFont(DOUBLE fontId, DOUBLE from, DOUBLE to) { return TRUE; }
DOUBLE WINAPI FWSetFontOffset(DOUBLE fontId, DOUBLE x, DOUBLE y)  { return TRUE; }

DOUBLE WINAPI FWSetFont(DOUBLE fontId)
{
    g_curFontId = (int)fontId;
    return TRUE;
}

DOUBLE WINAPI FWSetHAlign(DOUBLE align)     { g_halign = (int)align; return TRUE; }
DOUBLE WINAPI FWSetVAlign(DOUBLE align)     { g_valign = (int)align; return TRUE; }
DOUBLE WINAPI FWEnablePixelAlignment(DOUBLE) { return TRUE; }
DOUBLE WINAPI FWSetLineSpacing(DOUBLE sep)  { g_lineSpacing = (int)sep; return TRUE; }

DOUBLE WINAPI FWSetTextColor(DOUBLE c1, DOUBLE c2, DOUBLE alpha)
{
    g_curColor1 = (int)c1;
    g_curColor2 = (int)c2;
    g_curAlpha = (float)alpha;
    return TRUE;
}

DOUBLE WINAPI FWStringWidth(LPCSTR str)
{
    return (DOUBLE)AnsiToWide(str).length() * 10.0;
}
DOUBLE WINAPI FWStringHeight(LPCSTR str) { return 16.0; }
DOUBLE WINAPI FWStringWidthEx(LPCSTR str, DOUBLE sep, DOUBLE w)  { return FWStringWidth(str); }
DOUBLE WINAPI FWStringHeightEx(LPCSTR str, DOUBLE sep, DOUBLE w) { return FWStringHeight(str); }

DOUBLE WINAPI FWDrawText(DOUBLE x, DOUBLE y, LPCSTR str)
{
    if (g_gameWindow && !IsWindow(g_gameWindow)) {
        if (g_overlay) { DestroyWindow(g_overlay); g_overlay = NULL; }
        if (g_hDib) { DeleteObject(g_hDib); g_hDib = NULL; g_bits = NULL; }
        if (g_hdcMem) { DeleteDC(g_hdcMem); g_hdcMem = NULL; }
        g_dibW = g_dibH = 0;
        g_gameWindow = NULL;
    }
    if (!g_gameWindow) {
        EnumWindows([](HWND hwnd, LPARAM lp) -> BOOL {
            DWORD pid; GetWindowThreadProcessId(hwnd, &pid);
            if (pid == GetCurrentProcessId() && IsWindowVisible(hwnd)) {
                *(HWND*)lp = hwnd; return FALSE;
            }
            return TRUE;
        }, (LPARAM)&g_gameWindow);
    }
    if (!g_gameWindow) return FALSE;

    if (!g_overlay) {
        if (g_overlayFailed) {
            if (++g_overlayRetry < 120) return FALSE;
            g_overlayRetry = 0; g_overlayFailed = false;
        }
        EnsureOverlay();
    }
    if (!g_overlay) return FALSE;

    SyncOverlay();

    std::wstring text = AnsiToWide(str);
    if (text.empty()) return TRUE;

    RenderText(g_curFontId, text, (float)x, (float)y,
               g_halign, g_valign, g_curColor1, g_curColor2, g_curAlpha);
    PresentDib();
    return TRUE;
}

DOUBLE WINAPI FWDrawTextEx(DOUBLE x, DOUBLE y, LPCSTR str)                      { return FWDrawText(x,y,str); }
DOUBLE WINAPI FWDrawTextTransformed(DOUBLE x, DOUBLE y, LPCSTR str)             { return FWDrawText(x,y,str); }
DOUBLE WINAPI FWDrawTextTransformedEx(DOUBLE x, DOUBLE y, LPCSTR str)           { return FWDrawText(x,y,str); }
DOUBLE WINAPI FWDrawTextColor(DOUBLE x, DOUBLE y, LPCSTR str)                   { return FWDrawText(x,y,str); }
DOUBLE WINAPI FWDrawTextColorEx(DOUBLE x, DOUBLE y, LPCSTR str)                 { return FWDrawText(x,y,str); }
DOUBLE WINAPI FWDrawTextTransformedColor(DOUBLE x, DOUBLE y, LPCSTR str)        { return FWDrawText(x,y,str); }
DOUBLE WINAPI FWDrawTextTransformedColorEx(DOUBLE x, DOUBLE y, LPCSTR str)      { return FWDrawText(x,y,str); }

DOUBLE WINAPI FWPaint()
{
    ClearDib();
    return TRUE;
}

DOUBLE WINAPI FWWindowResized()
{
    SyncOverlay();
    return TRUE;
}

DOUBLE WINAPI FWSetViewSize(DOUBLE w, DOUBLE h) { return TRUE; }
