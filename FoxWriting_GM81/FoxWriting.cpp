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
static WNDPROC g_origWndProc = NULL;
static HDC g_memDC = NULL;
static HBITMAP g_memDib = NULL;
static void* g_memBits = NULL;
static int g_memW = 0;
static int g_memH = 0;

static void EnsureAlphaDIB(int w, int h)
{
    if (g_memDib && g_memW >= w && g_memH >= h) return;
    if (g_memDib) { DeleteObject(g_memDib); g_memDib = NULL; }
    if (g_memDC) { DeleteDC(g_memDC); g_memDC = NULL; }
    g_memBits = NULL;
    HDC screenDC = GetDC(NULL);
    g_memDC = CreateCompatibleDC(screenDC);
    BITMAPINFO bmi = {0};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = w;
    bmi.bmiHeader.biHeight = -h;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;
    g_memDib = CreateDIBSection(screenDC, &bmi, DIB_RGB_COLORS, &g_memBits, NULL, 0);
    ReleaseDC(NULL, screenDC);
    if (g_memDC && g_memDib) {
        SelectObject(g_memDC, g_memDib);
        g_memW = w;
        g_memH = h;
    }
}

static LRESULT CALLBACK WindowSubclass(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg) {
        case WM_ERASEBKGND:
            return TRUE;
    }
    return CallWindowProcW(g_origWndProc, hwnd, msg, wParam, lParam);
}

static HWND FindGameWindow()
{
    DWORD pid = GetCurrentProcessId();

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
        len = MultiByteToWideChar(CP_ACP, 0, input, -1, NULL, 0);
        if (len <= 0) return L"";
    }
    std::wstring result(len - 1, L'\0');
    MultiByteToWideChar(g_codePage, 0, input, -1, &result[0], len);
    return result;
}

// ── Scaling ────────────────────────────────────────────────
// Returns the uniform scale factor from view coords -> client pixels.
// Also outputs the client area size.
static float GetViewScale(int* outClientW, int* outClientH)
{
    if (!g_gameWindow || !IsWindow(g_gameWindow)) {
        g_gameWindow = FindGameWindow();
    }
    RECT rc = {0, 0, g_viewWidth, g_viewHeight};
    if (g_gameWindow) {
        GetClientRect(g_gameWindow, &rc);
    }
    int cw = max(1, rc.right - rc.left);
    int ch = max(1, rc.bottom - rc.top);
    if (outClientW) *outClientW = cw;
    if (outClientH) *outClientH = ch;
    float sx = (float)cw / (float)max(1, g_viewWidth);
    float sy = (float)ch / (float)max(1, g_viewHeight);
    return min(sx, sy);
}

// ── Multi-line helpers ─────────────────────────────────────
// Split a wide string on '\n' and return lines.
// Each line is trimmed of trailing '\r'.
static std::vector<std::wstring> SplitLines(const std::wstring& text)
{
    std::vector<std::wstring> lines;
    std::wstringstream ss(text);
    std::wstring line;
    while (std::getline(ss, line, L'\n')) {
        if (!line.empty() && line.back() == L'\r')
            line.pop_back();
        lines.push_back(line);
    }
    if (lines.empty()) lines.push_back(L"");
    return lines;
}

static Gdiplus::Font* GetGdiFont();

// ── Core draw function ─────────────────────────────────────
static void DrawGdiText(int x, int y, LPCSTR str, int color, double alpha)
{
    Gdiplus::Font* font = GetGdiFont();
    if (!font) return;

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

    if (IsIconic(g_gameWindow)) return;

    HDC winDC = GetDC(g_gameWindow);
    if (!winDC) return;

    int clientW, clientH;
    float scale = GetViewScale(&clientW, &clientH);

    EnsureAlphaDIB(clientW, clientH);
    if (!g_memDib || !g_memBits) { ReleaseDC(g_gameWindow, winDC); return; }

    // Clear to transparent
    memset(g_memBits, 0, (size_t)clientW * clientH * 4);

    {
        Gdiplus::Graphics g(g_memDC);
        g.SetPageUnit(Gdiplus::UnitPixel);
        g.SetSmoothingMode(Gdiplus::SmoothingModeHighQuality);

        g.SetTextRenderingHint(Gdiplus::TextRenderingHintAntiAliasGridFit);
        g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHighQuality);

        float offsetX = 0, offsetY = 0;
        float scaledViewW = (float)g_viewWidth * scale;
        float scaledViewH = (float)g_viewHeight * scale;
        if (g_halign == 1) offsetX = ((float)clientW - scaledViewW) / 2.0f;
        else if (g_halign == 2) offsetX = (float)clientW - scaledViewW;
        if (g_valign == 1) offsetY = ((float)clientH - scaledViewH) / 2.0f;
        else if (g_valign == 2) offsetY = (float)clientH - scaledViewH;

        Gdiplus::Matrix matrix;
        matrix.Translate(offsetX, offsetY);
        matrix.Scale(scale, scale);
        g.SetTransform(&matrix);

        if (!empty) {
            BYTE a = (BYTE)(alpha * 255.0f);
            BYTE r = (color >> 16) & 0xFF;
            BYTE gr = (color >> 8) & 0xFF;
            BYTE b = color & 0xFF;
            Gdiplus::Color textColor(a, r, gr, b);

            float fx = (float)x + (g_currentFont ? g_currentFont->xOffset : 0);
            float fy = (float)y + (g_currentFont ? g_currentFont->yOffset : 0);
            if (g_pixelAlign) { fx = floorf(fx); fy = floorf(fy); }

            auto lines = SplitLines(wstr);
            if (!lines.empty()) {
                Gdiplus::RectF bounds;
                g.MeasureString(lines[0].c_str(), -1, font, Gdiplus::PointF(0, 0), &bounds);
                float lineH = bounds.Height + g_lineSpacing;
                float totalH = (float)lines.size() * lineH - g_lineSpacing;

                float drawY = fy;
                if (g_valign == 1) drawY = fy - totalH / 2.0f;
                else if (g_valign == 2) drawY = fy - totalH;

                bool doStroke = g_currentFont && g_currentFont->stroke;

                for (size_t i = 0; i < lines.size(); i++) {
                    float lineX = fx;
                    float lineY = drawY + (float)i * lineH;

                    if (g_halign != 0) {
                        Gdiplus::RectF lineBounds;
                        g.MeasureString(lines[i].c_str(), -1, font,
                            Gdiplus::PointF(0, 0), &lineBounds);
                        if (g_halign == 1) lineX = fx - lineBounds.Width / 2.0f;
                        else if (g_halign == 2) lineX = fx - lineBounds.Width;
                    }

                    if (g_pixelAlign) { lineX = floorf(lineX); lineY = floorf(lineY); }

                    if (doStroke) {
                        Gdiplus::SolidBrush strokeBrush(Gdiplus::Color(255, 0, 0, 0));
                        for (int ox = -1; ox <= 1; ox++) {
                            for (int oy = -1; oy <= 1; oy++) {
                                if (ox == 0 && oy == 0) continue;
                                g.DrawString(lines[i].c_str(), -1, font,
                                    Gdiplus::PointF(lineX + (float)ox, lineY + (float)oy),
                                    &strokeBrush);
                            }
                        }
                    }

                    Gdiplus::SolidBrush textBrush(textColor);
                    g.DrawString(lines[i].c_str(), -1, font,
                        Gdiplus::PointF(lineX, lineY), &textBrush);
                }
            }
        }
    }

    BLENDFUNCTION blend = {0};
    blend.BlendOp = AC_SRC_OVER;
    blend.SourceConstantAlpha = 255;
    blend.AlphaFormat = AC_SRC_ALPHA;
    AlphaBlend(winDC, 0, 0, clientW, clientH, g_memDC, 0, 0, clientW, clientH, blend);

    ReleaseDC(g_gameWindow, winDC);
}

static Gdiplus::Font* GetGdiFont()
{
    if (!g_currentFont || !g_currentFont->font) return NULL;
    return g_currentFont->font;
}

// ── Exported functions ─────────────────────────────────────

DOUBLE WINAPI FWSetViewSize(DOUBLE w, DOUBLE h)
{
    g_viewWidth = max(1, (int)w);
    g_viewHeight = max(1, (int)h);
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
    DebugLog("Found game window: %p", g_gameWindow);
    g_fontCount = 0;
    g_currentFont = NULL;
    g_halign = 0;
    g_valign = 0;
    g_lineSpacing = 0.0f;
    g_pixelAlign = false;

    // Subclass window to suppress background erase (reduces flicker)
    g_origWndProc = (WNDPROC)SetWindowLongPtrW(g_gameWindow, GWLP_WNDPROC, (LONG_PTR)WindowSubclass);

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
            delete fi->pfc;
            delete fi;
        }
    }
    g_fontMap.clear();
    g_fontCount = 0;

    // Restore original wndproc
    if (g_origWndProc && g_gameWindow) {
        SetWindowLongPtrW(g_gameWindow, GWLP_WNDPROC, (LONG_PTR)g_origWndProc);
        g_origWndProc = NULL;
    }

    // Free double-buffer resources
    if (g_memDib) { DeleteObject(g_memDib); g_memDib = NULL; g_memBits = NULL; }
    if (g_memDC) { DeleteDC(g_memDC); g_memDC = NULL; }
    g_memW = g_memH = 0;

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
    int gdiStyle = (int)style & 0x03;
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

    int gdiStyle = (int)style & 0x03;
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
        DebugLog("FWAddFontFromFile: trying direct path");
        font = new Gdiplus::Font(wpath.c_str(), (Gdiplus::REAL)pt,
            gdiStyle, Gdiplus::UnitPoint);
        if (font && font->GetLastStatus() == Gdiplus::Ok) {
            family = NULL;
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

DOUBLE WINAPI FWStringWidth(LPCSTR str)
{
    Gdiplus::Font* font = GetGdiFont();
    if (!font) return 0;
    std::wstring wstr = AnsiToWide(str);
    if (wstr.empty()) return 0;

    HDC hdc = GetDC(NULL);
    Gdiplus::Graphics g(hdc);
    g.SetPageUnit(Gdiplus::UnitPixel);
    Gdiplus::RectF bounds;
    g.MeasureString(wstr.c_str(), -1, font, Gdiplus::PointF(0, 0), &bounds);
    ReleaseDC(NULL, hdc);
    float scale = GetViewScale(NULL, NULL);
    return bounds.Width * scale;
}

DOUBLE WINAPI FWStringHeight(LPCSTR str)
{
    Gdiplus::Font* font = GetGdiFont();
    if (!font) return 0;
    std::wstring wstr = AnsiToWide(str);
    if (wstr.empty()) return 0;

    auto lines = SplitLines(wstr);
    if (lines.empty()) return 0;

    HDC hdc = GetDC(NULL);
    Gdiplus::Graphics g(hdc);
    g.SetPageUnit(Gdiplus::UnitPixel);
    Gdiplus::RectF bounds;
    g.MeasureString(lines[0].c_str(), -1, font, Gdiplus::PointF(0, 0), &bounds);
    float lineH = bounds.Height + g_lineSpacing;
    ReleaseDC(NULL, hdc);
    float scale = GetViewScale(NULL, NULL);
    return ((float)lines.size() * lineH - g_lineSpacing) * scale;
}

DOUBLE WINAPI FWStringWidthEx(LPCSTR str, DOUBLE sep, DOUBLE w)
{
    if (!GetGdiFont()) return FWStringWidth(str);
    std::wstring wstr = AnsiToWide(str);
    if (wstr.empty()) return 0;

    float scale = GetViewScale(NULL, NULL);
    Gdiplus::REAL wrapW = (Gdiplus::REAL)w;
    if (scale > 0.001f) wrapW = (Gdiplus::REAL)(w / scale);

    HDC hdc = GetDC(NULL);
    Gdiplus::Graphics g(hdc);
    g.SetPageUnit(Gdiplus::UnitPixel);

    Gdiplus::StringFormat fmt;
    Gdiplus::RectF layout(0, 0, wrapW, 10000);
    Gdiplus::RectF bounds;
    g.MeasureString(wstr.c_str(), -1, GetGdiFont(), layout, &fmt, &bounds);
    ReleaseDC(NULL, hdc);
    return bounds.Width * scale;
}

DOUBLE WINAPI FWStringHeightEx(LPCSTR str, DOUBLE sep, DOUBLE w)
{
    if (!GetGdiFont()) return FWStringHeight(str);
    std::wstring wstr = AnsiToWide(str);
    if (wstr.empty()) return 0;

    float scale = GetViewScale(NULL, NULL);
    Gdiplus::REAL wrapW = (Gdiplus::REAL)w;
    if (scale > 0.001f) wrapW = (Gdiplus::REAL)(w / scale);

    HDC hdc = GetDC(NULL);
    Gdiplus::Graphics g(hdc);
    g.SetPageUnit(Gdiplus::UnitPixel);

    Gdiplus::StringFormat fmt;
    Gdiplus::RectF layout(0, 0, wrapW, 10000);
    Gdiplus::RectF bounds;
    g.MeasureString(wstr.c_str(), -1, GetGdiFont(), layout, &fmt, &bounds);
    ReleaseDC(NULL, hdc);
    return bounds.Height * scale;
}

// ── Draw text variants ─────────────────────────────────────

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
