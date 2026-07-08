#include "stdafx.h"
#include "FoxWriting.h"
#include "CodePage.h"

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

static HWND FindGameWindow()
{
    HWND hwnd = GetForegroundWindow();
    if (hwnd) {
        wchar_t cls[256];
        GetClassNameW(hwnd, cls, 256);
        if (wcsstr(cls, L"GMAKER") || wcsstr(cls, L"GameMaker") || wcsstr(cls, L"GM")) {
            return hwnd;
        }
    }
    return FindWindowW(L"GMAKER", NULL);
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

DOUBLE WINAPI FWInit(DOUBLE sprite, DOUBLE argList)
{
    Gdiplus::GdiplusStartup(&g_gdiplusToken, &g_gdiplusStartupInput, NULL);
    g_gameWindow = FindGameWindow();
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
    g_currentFont = NULL;
    for (auto& pair : g_fontMap) {
        FontInfo* fi = pair.second;
        if (fi) {
            delete fi->font;
            delete fi->family;
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
    if (!CPName) return FALSE;
    CodePageEntry* cp = g_codePages;
    while (cp->code_page != 0) {
        if (strcmp(CPName, cp->name) == 0) {
            g_codePage = cp->code_page;
            return TRUE;
        }
        cp++;
    }
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
    Gdiplus::Font* font = new Gdiplus::Font(family, (Gdiplus::REAL)pt,
        (INT)style, Gdiplus::UnitPixel);
    FontInfo* fi = new FontInfo();
    fi->font = font;
    fi->family = family;
    fi->style = (int)style;
    fi->xOffset = 0;
    fi->yOffset = 0;
    fi->fromFile = false;
    int index = g_fontCount++;
    g_fontMap[index] = fi;
    return index;
}

DOUBLE WINAPI FWAddFontFromFile(LPCSTR ttf, DOUBLE pt, DOUBLE style)
{
    if (pt <= 0) return -1;
    std::wstring wpath = AnsiToWide(ttf);
    Gdiplus::Font* font = NULL;
    Gdiplus::FontFamily* family = NULL;

    Gdiplus::PrivateFontCollection* pfc = new Gdiplus::PrivateFontCollection();
    pfc->AddFontFile(wpath.c_str());
    int found = 0;
    WCHAR familyName[LF_FACESIZE];
    pfc->GetFamilies(1, NULL, &found); // just check count
    if (found > 0) {
        Gdiplus::FontFamily tempFamily;
        pfc->GetFamilies(1, &tempFamily, &found);
        if (found > 0 && tempFamily.IsAvailable()) {
            // Get the family name
            tempFamily.GetFamilyName(familyName);
            family = new Gdiplus::FontFamily(familyName, pfc);
            if (family && family->IsAvailable()) {
                font = new Gdiplus::Font(family, (Gdiplus::REAL)pt,
                    (INT)style, Gdiplus::UnitPixel);
            }
        }
    }

    if (!font) {
        // Try direct path
        font = new Gdiplus::Font(wpath.c_str(), (Gdiplus::REAL)pt,
            (INT)style, Gdiplus::UnitPixel);
        if (font && font->GetLastStatus() == Gdiplus::Ok) {
            family = NULL; // Font owns the family reference
        } else {
            delete font;
            delete pfc;
            return -1;
        }
    }

    FontInfo* fi = new FontInfo();
    fi->font = font;
    fi->family = family;
    fi->style = (int)style;
    fi->xOffset = 0;
    fi->yOffset = 0;
    fi->fromFile = true;
    int index = g_fontCount++;
    g_fontMap[index] = fi;
    delete pfc;
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
    if (it == g_fontMap.end()) return FALSE;
    g_currentFont = it->second;
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

// Draw text using GDI+ to screen DC
// In windowed mode D3D, GDI drawing to the game window DC appears on top of D3D
static void DrawGdiText(int x, int y, LPCSTR str, int color, double alpha)
{
    Gdiplus::Font* font = GetGdiFont();
    if (!font || !str || !*str) return;

    std::wstring wstr = AnsiToWide(str);
    if (wstr.empty()) return;

    // Get the game window DC for overlay drawing
    if (!g_gameWindow || !IsWindow(g_gameWindow)) {
        g_gameWindow = FindGameWindow();
    }
    if (!g_gameWindow || !IsWindow(g_gameWindow)) return;

    HDC hdc = GetDC(g_gameWindow);
    if (!hdc) return;

    Gdiplus::Graphics g(hdc);
    g.SetPageUnit(Gdiplus::UnitPixel);
    g.SetSmoothingMode(Gdiplus::SmoothingModeNone);

    BYTE a = (BYTE)(alpha * 255.0f);
    BYTE r = (color >> 16) & 0xFF;
    BYTE gr = (color >> 8) & 0xFF;
    BYTE b = color & 0xFF;
    Gdiplus::Color textColor(a, r, gr, b);

    float fsize = font->GetSize();
    float fx = (float)x + (g_currentFont ? g_currentFont->xOffset : 0);
    float fy = (float)y + (g_currentFont ? g_currentFont->yOffset : 0);

    if (g_pixelAlign) {
        fx = floorf(fx);
        fy = floorf(fy);
    }

    // Measure
    Gdiplus::RectF layout(0, 0, 0, 0);
    Gdiplus::RectF bounds;
    g.MeasureString(wstr.c_str(), -1, font, Gdiplus::PointF(0, 0), &bounds);

    // Apply alignment
    Gdiplus::PointF pt(fx, fy);
    if (g_valign == 1) { // middle
        pt.Y = fy - bounds.Height / 2.0f;
    } else if (g_valign == 2) { // bottom
        pt.Y = fy - bounds.Height;
    }
    if (g_halign == 1) { // center
        pt.X = fx - bounds.Width / 2.0f;
    } else if (g_halign == 2) { // right
        pt.X = fx - bounds.Width;
    }

    // Draw with GDI+
    Gdiplus::SolidBrush brush(textColor);
    g.DrawString(wstr.c_str(), -1, font, pt, &brush);

    ReleaseDC(g_gameWindow, hdc);
}

DOUBLE WINAPI FWDrawText(DOUBLE x, DOUBLE y, LPCSTR str)
{
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
