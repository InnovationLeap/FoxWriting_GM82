#ifdef FOXWRITING_EXPORTS
#define FOXWRITING_API __declspec(dllexport)
#else
#define FOXWRITING_API __declspec(dllimport)
#endif

#ifndef __FOXWRITING_H__
#define __FOXWRITING_H__

#include <vector>
#include <string>

struct FontInfo {
    Gdiplus::Font* font;
    Gdiplus::FontFamily* family;
    Gdiplus::PrivateFontCollection* pfc;
    std::wstring filePath;
    int style;
    float xOffset;
    float yOffset;
    bool fromFile;
    bool stroke;
    float spaceWidth; // cached space character width, -1 = not computed
};


extern std::unordered_map<int, FontInfo*> g_fontMap;
extern int g_fontCount;
extern FontInfo* g_currentFont;
extern int g_halign;
extern int g_valign;
extern float g_lineSpacing;
extern bool g_pixelAlign;
extern Gdiplus::GdiplusStartupInput g_gdiplusStartupInput;
extern ULONG_PTR g_gdiplusToken;
extern HWND g_gameWindow;
extern int g_drawColor1;
extern int g_drawColor2;
extern double g_drawAlpha;
extern HDC g_lastDC;

extern "C"
{
    FOXWRITING_API DOUBLE WINAPI FWInit(DOUBLE sprite, DOUBLE argList);
    FOXWRITING_API DOUBLE WINAPI FWReleaseCache();
    FOXWRITING_API DOUBLE WINAPI FWCleanup();
    FOXWRITING_API DOUBLE WINAPI FWSetEncoding(LPCSTR CPName);
    FOXWRITING_API DOUBLE WINAPI FWSetEncodingEx(DOUBLE codePage);
    FOXWRITING_API DOUBLE WINAPI FWAddFont(LPCSTR name, DOUBLE pt, DOUBLE style);
    FOXWRITING_API DOUBLE WINAPI FWAddFontFromFile(LPCSTR ttf, DOUBLE pt, DOUBLE style);
    FOXWRITING_API DOUBLE WINAPI FWDeleteFont(DOUBLE font);
    FOXWRITING_API DOUBLE WINAPI FWPreloadFont(DOUBLE font, DOUBLE from, DOUBLE to);
    FOXWRITING_API DOUBLE WINAPI FWSetFontOffset(DOUBLE font, DOUBLE xOffset, DOUBLE yOffset);
    FOXWRITING_API DOUBLE WINAPI FWSetFont(DOUBLE font);
    FOXWRITING_API DOUBLE WINAPI FWSetHAlign(DOUBLE align);
    FOXWRITING_API DOUBLE WINAPI FWSetVAlign(DOUBLE align);
    FOXWRITING_API DOUBLE WINAPI FWEnablePixelAlignment(DOUBLE enable);
    FOXWRITING_API DOUBLE WINAPI FWSetLineSpacing(DOUBLE sep);
    FOXWRITING_API DOUBLE WINAPI FWStringWidth(LPCSTR str);
    FOXWRITING_API DOUBLE WINAPI FWStringHeight(LPCSTR str);
    FOXWRITING_API DOUBLE WINAPI FWStringWidthEx(LPCSTR str, DOUBLE sep, DOUBLE w);
    FOXWRITING_API DOUBLE WINAPI FWStringHeightEx(LPCSTR str, DOUBLE sep, DOUBLE w);
    FOXWRITING_API DOUBLE WINAPI FWDrawText(DOUBLE x, DOUBLE y, LPCSTR str);
    FOXWRITING_API DOUBLE WINAPI FWDrawTextEx(DOUBLE x, DOUBLE y, LPCSTR str);
    FOXWRITING_API DOUBLE WINAPI FWDrawTextTransformed(DOUBLE x, DOUBLE y, LPCSTR str);
    FOXWRITING_API DOUBLE WINAPI FWDrawTextTransformedEx(DOUBLE x, DOUBLE y, LPCSTR str);
    FOXWRITING_API DOUBLE WINAPI FWDrawTextColor(DOUBLE x, DOUBLE y, LPCSTR str);
    FOXWRITING_API DOUBLE WINAPI FWDrawTextColorEx(DOUBLE x, DOUBLE y, LPCSTR str);
    FOXWRITING_API DOUBLE WINAPI FWDrawTextTransformedColor(DOUBLE x, DOUBLE y, LPCSTR str);
    FOXWRITING_API DOUBLE WINAPI FWDrawTextTransformedColorEx(DOUBLE x, DOUBLE y, LPCSTR str);
    FOXWRITING_API DOUBLE WINAPI FWPaint();
    FOXWRITING_API DOUBLE WINAPI FWWindowResized();
    FOXWRITING_API DOUBLE WINAPI FWSetViewSize(DOUBLE w, DOUBLE h);
};

#endif
