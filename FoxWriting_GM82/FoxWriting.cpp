#include "stdafx.h"
#include "FoxWriting.h"
#include "CodePage.h"
#include "d3d9_mini.h"

#include <fstream>
#include <sstream>
#include <cstring>

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
static float g_screenDpiX = 96.0f;
static float g_screenDpiY = 96.0f;

static const int RENDER_SCALE = 4;

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

static std::vector<std::wstring> SplitLines(const std::wstring& text)
{
    std::wstring s = text;
    size_t pos = 0;
    while ((pos = s.find(L"\r\n", 0)) != std::wstring::npos) {
        s.replace(pos, 2, L"\n");
    }
    std::replace(s.begin(), s.end(), L'\r', L'\n');

    std::vector<std::wstring> lines;
    std::wstringstream ss(s);
    std::wstring line;
    while (std::getline(ss, line, L'\n')) {
        lines.push_back(line);
    }
    if (lines.empty()) lines.push_back(L"");
    return lines;
}

static Gdiplus::TextRenderingHint GetDrawHint(Gdiplus::Font* font)
{
    if (!font) return Gdiplus::TextRenderingHintAntiAliasGridFit;
    return (font->GetSize() <= 20.0f)
        ? Gdiplus::TextRenderingHintClearTypeGridFit
        : Gdiplus::TextRenderingHintAntiAliasGridFit;
}

static Gdiplus::TextRenderingHint GetMeasureHint(Gdiplus::Font* font)
{
    if (!font) return Gdiplus::TextRenderingHintAntiAliasGridFit;
    return (font->GetSize() <= 15.0f)
        ? Gdiplus::TextRenderingHintClearTypeGridFit
        : Gdiplus::TextRenderingHintAntiAliasGridFit;
}

struct TextDrawItem {
    std::wstring text;
    float x, y;
    int color;
    float alpha;
    Gdiplus::Font* font;
    int halign, valign;
    float xOffset, yOffset;
    float lineSpacing;
    bool pixelAlign, doStroke;
};
static std::vector<TextDrawItem> g_textQueue;

// ── Direct3D 9 backbuffer render ─────────────────────────────
// Reads the D3D9 device pointer from the gm82dx9 extension's
// storage location (fixed address 0x6886a8, no ASLR in gm82vp.exe),
// then hooks IDirect3DDevice9::Present[vtable 17] to render text
// via GetBackBuffer → GDI+ Bitmap → D3D9 texture quad right before
// Present submits.

static IDirect3DDevice9* g_d3d9Device = NULL;

typedef HRESULT (__stdcall *Present9_t)(IDirect3DDevice9*, const RECT*, const RECT*, HWND, const RGNDATA*);
static Present9_t g_realPresent9 = NULL;

// GDI+ Bitmap -> D3D9 texture pipeline. Text is rasterized by GDI+ into an
// in-memory ARGB bitmap, uploaded to a D3D9 texture, then drawn as a
// full-screen textured quad in the Present hook. This stays entirely inside
// the D3D pipeline and avoids GetDC / GetRenderTargetData / StretchRect on
// D3D surfaces -- calls that fail or produce no output on some GPU drivers
// (e.g. Intel integrated graphics under Windows 11 24H2).
static IDirect3DTexture9* g_textTexture = NULL; // D3DPOOL_DEFAULT, A8R8G8B8
static IDirect3DVertexDeclaration9* g_quadDecl = NULL; // XYZRHW + TEX1
static int g_textTexW = 0;
static int g_textTexH = 0;

#define SAFE_RELEASE(p) do { if (p) { (p)->Release(); (p) = NULL; } } while(0)

static bool EnsureTextTexture(IDirect3DDevice9* device, int w, int h)
{
    if (g_textTexture && g_textTexW == w && g_textTexH == h)
        return true;

    SAFE_RELEASE(g_textTexture);

    // D3DUSAGE_DYNAMIC so we can LockRect(DISCARD) each frame for updates.
    HRESULT hr = device->CreateTexture(w, h, 1, D3DUSAGE_DYNAMIC, D3DFMT_A8R8G8B8,
        D3DPOOL_DEFAULT, &g_textTexture, NULL);
    if (FAILED(hr)) {
        DebugLog("EnsureTextTexture: %dx%d failed hr=%08X", w, h, hr);
        return false;
    }

    g_textTexW = w;
    g_textTexH = h;

    if (!g_quadDecl) {
        D3DVERTEXELEMENT9 elts[] = {
            { 0, 0,  D3DDECLTYPE_FLOAT4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITIONT, 0 },
            { 0, 16, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD,  0 },
            D3DDECL_END()
        };
        if (FAILED(device->CreateVertexDeclaration(elts, &g_quadDecl)))
            DebugLog("EnsureTextTexture: CreateVertexDeclaration failed");
    }

    DebugLog("EnsureTextTexture: %dx%d tex=%p", w, h, g_textTexture);
    return true;
}

// Draw the text texture as a full-screen alpha-blended quad over the
// backbuffer. All touched device state is saved and restored afterwards.
static void DrawTextQuad(IDirect3DDevice9* device, IDirect3DSurface9* backbuf, int w, int h)
{
    struct TV { float x, y, z, rhw, u, v; };
    TV v[4] = {
        { 0,        0,        0, 1, 0, 0 },
        { (float)w, 0,        0, 1, 1, 0 },
        { 0,        (float)h, 0, 1, 0, 1 },
        { (float)w, (float)h, 0, 1, 1, 1 },
    };

    DWORD sZEnable, sZWrite, sABlend, sSrc, sDst, sCull, sLight, sFog, sATest, sScissor;
    device->GetRenderState(D3DRS_ZENABLE, &sZEnable);
    device->GetRenderState(D3DRS_ZWRITEENABLE, &sZWrite);
    device->GetRenderState(D3DRS_ALPHABLENDENABLE, &sABlend);
    device->GetRenderState(D3DRS_SRCBLEND, &sSrc);
    device->GetRenderState(D3DRS_DESTBLEND, &sDst);
    device->GetRenderState(D3DRS_CULLMODE, &sCull);
    device->GetRenderState(D3DRS_LIGHTING, &sLight);
    device->GetRenderState(D3DRS_FOGENABLE, &sFog);
    device->GetRenderState(D3DRS_ALPHATESTENABLE, &sATest);
    device->GetRenderState(D3DRS_SCISSORTESTENABLE, &sScissor);

    DWORD sColorOp, sColorA1, sColorA2, sAlphaOp, sAlphaA1, sAlphaA2;
    device->GetTextureStageState(0, D3DTSS_COLOROP, &sColorOp);
    device->GetTextureStageState(0, D3DTSS_COLORARG1, &sColorA1);
    device->GetTextureStageState(0, D3DTSS_COLORARG2, &sColorA2);
    device->GetTextureStageState(0, D3DTSS_ALPHAOP, &sAlphaOp);
    device->GetTextureStageState(0, D3DTSS_ALPHAARG1, &sAlphaA1);
    device->GetTextureStageState(0, D3DTSS_ALPHAARG2, &sAlphaA2);

    DWORD sMag, sMin, sAddrU, sAddrV;
    device->GetSamplerState(0, D3DSAMP_MAGFILTER, &sMag);
    device->GetSamplerState(0, D3DSAMP_MINFILTER, &sMin);
    device->GetSamplerState(0, D3DSAMP_ADDRESSU, &sAddrU);
    device->GetSamplerState(0, D3DSAMP_ADDRESSV, &sAddrV);

    IDirect3DBaseTexture9* sTex = NULL;
    device->GetTexture(0, &sTex);
    IDirect3DVertexDeclaration9* sDecl = NULL;
    device->GetVertexDeclaration(&sDecl);
    IDirect3DSurface9* sRT = NULL;
    device->GetRenderTarget(0, &sRT);

    device->SetRenderState(D3DRS_ZENABLE, D3DZB_FALSE);
    device->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
    device->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
    // GDI+ 32bppARGB is premultiplied alpha -> use ONE / INVSRCALPHA.
    device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_ONE);
    device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
    device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
    device->SetRenderState(D3DRS_LIGHTING, FALSE);
    device->SetRenderState(D3DRS_FOGENABLE, FALSE);
    device->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
    device->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);

    device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
    device->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
    device->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_CURRENT);
    device->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
    device->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
    device->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_CURRENT);

    device->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
    device->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
    device->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
    device->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);

    device->SetTexture(0, (IDirect3DBaseTexture9*)g_textTexture);
    device->SetRenderTarget(0, backbuf);
    device->SetVertexDeclaration(g_quadDecl);
    device->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, v, sizeof(TV));

    // Restore previous state.
    device->SetRenderTarget(0, sRT);
    if (sRT) sRT->Release();
    device->SetTexture(0, sTex);
    if (sTex) sTex->Release();
    device->SetVertexDeclaration(sDecl);
    if (sDecl) sDecl->Release();

    device->SetTextureStageState(0, D3DTSS_COLOROP, sColorOp);
    device->SetTextureStageState(0, D3DTSS_COLORARG1, sColorA1);
    device->SetTextureStageState(0, D3DTSS_COLORARG2, sColorA2);
    device->SetTextureStageState(0, D3DTSS_ALPHAOP, sAlphaOp);
    device->SetTextureStageState(0, D3DTSS_ALPHAARG1, sAlphaA1);
    device->SetTextureStageState(0, D3DTSS_ALPHAARG2, sAlphaA2);

    device->SetSamplerState(0, D3DSAMP_MAGFILTER, sMag);
    device->SetSamplerState(0, D3DSAMP_MINFILTER, sMin);
    device->SetSamplerState(0, D3DSAMP_ADDRESSU, sAddrU);
    device->SetSamplerState(0, D3DSAMP_ADDRESSV, sAddrV);

    device->SetRenderState(D3DRS_ZENABLE, sZEnable);
    device->SetRenderState(D3DRS_ZWRITEENABLE, sZWrite);
    device->SetRenderState(D3DRS_ALPHABLENDENABLE, sABlend);
    device->SetRenderState(D3DRS_SRCBLEND, sSrc);
    device->SetRenderState(D3DRS_DESTBLEND, sDst);
    device->SetRenderState(D3DRS_CULLMODE, sCull);
    device->SetRenderState(D3DRS_LIGHTING, sLight);
    device->SetRenderState(D3DRS_FOGENABLE, sFog);
    device->SetRenderState(D3DRS_ALPHATESTENABLE, sATest);
    device->SetRenderState(D3DRS_SCISSORTESTENABLE, sScissor);
}

// Draw queued text items onto a Gdiplus::Graphics target (either an HDC or
// an in-memory Bitmap). Separate function avoids C2712 (__try with C++ dtors).
static void RenderQueueOnGraphics(Gdiplus::Graphics& g, float pageScale = 1.0f)
{
    g.SetPageUnit(Gdiplus::UnitPixel);
    g.SetSmoothingMode(Gdiplus::SmoothingModeHighSpeed);
    g.SetTextContrast(4);
    g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeNone);

    Gdiplus::StringFormat typoFmt(Gdiplus::StringFormat::GenericTypographic());
    typoFmt.SetAlignment(Gdiplus::StringAlignmentNear);
    typoFmt.SetLineAlignment(Gdiplus::StringAlignmentNear);

    for (size_t i = 0; i < g_textQueue.size(); i++) {
        const TextDrawItem& item = g_textQueue[i];
        if (item.text.empty() || !item.font) continue;

        g.SetTextRenderingHint(GetDrawHint(item.font));

        BYTE a = (BYTE)(item.alpha * 255.0f);
        BYTE r = (item.color >> 16) & 0xFF;
        BYTE gr = (item.color >> 8) & 0xFF;
        BYTE b = item.color & 0xFF;
        Gdiplus::Color textColor(a, r, gr, b);

        float fx = (item.x + item.xOffset) * pageScale;
        float fy = (item.y + item.yOffset) * pageScale;
        if (item.pixelAlign) { fx = floorf(fx); fy = floorf(fy); }

        float scaledSpacing = item.lineSpacing * pageScale;

        auto lines = SplitLines(item.text);
        if (lines.empty()) continue;

        Gdiplus::RectF bounds;
        g.MeasureString(lines[0].c_str(), -1, item.font,
            Gdiplus::PointF(0, 0), &typoFmt, &bounds);
        float lineH = bounds.Height + scaledSpacing;
        float totalH = (float)lines.size() * lineH - scaledSpacing;

        float drawY = fy;
        if (item.valign == 1) drawY = fy - totalH / 2.0f;
        else if (item.valign == 2) drawY = fy - totalH;

        for (size_t li = 0; li < lines.size(); li++) {
            float lineX = fx;
            float lineY = drawY + (float)li * lineH;

            if (item.halign != 0) {
                Gdiplus::RectF lb;
                g.MeasureString(lines[li].c_str(), -1, item.font,
                    Gdiplus::PointF(0, 0), &typoFmt, &lb);
                if (item.halign == 1) lineX = fx - lb.Width / 2.0f;
                else if (item.halign == 2) lineX = fx - lb.Width;
            }

            if (item.pixelAlign) { lineX = floorf(lineX); lineY = floorf(lineY); }

            if (item.doStroke) {
                Gdiplus::SolidBrush strokeBrush(Gdiplus::Color(255, 0, 0, 0));
                for (int ox = -1; ox <= 1; ox++) {
                    for (int oy = -1; oy <= 1; oy++) {
                        if (ox == 0 && oy == 0) continue;
                        g.DrawString(lines[li].c_str(), -1, item.font,
                            Gdiplus::PointF(lineX + (float)ox * pageScale, lineY + (float)oy * pageScale),
                            &typoFmt, &strokeBrush);
                    }
                }
            }

            Gdiplus::SolidBrush textBrush(textColor);
            g.DrawString(lines[li].c_str(), -1, item.font,
                Gdiplus::PointF(lineX, lineY), &typoFmt, &textBrush);
        }
    }
}

// Thin wrapper for the legacy window-DC fallback (FWPaint).
static void RenderQueueOnDC(HDC hdc)
{
    Gdiplus::Graphics g(hdc);
    RenderQueueOnGraphics(g);
}

// Separate function to avoid C2712 (__try with C++ dtors in same function).
static void RenderTextToBackbuffer(IDirect3DDevice9* self)
{
    IDirect3DSurface9* backbuf = NULL;
    if (SUCCEEDED(self->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &backbuf)) && backbuf) {
        D3DSURFACE_DESC desc;
        backbuf->GetDesc(&desc);
        int w = (int)desc.Width;
        int h = (int)desc.Height;

        int tw = w * RENDER_SCALE;
        int th = h * RENDER_SCALE;
        if (w > 0 && h > 0 && EnsureTextTexture(self, tw, th)) {
            Gdiplus::Bitmap bmp(tw, th, PixelFormat32bppARGB);
            bmp.SetResolution(g_screenDpiX * RENDER_SCALE, g_screenDpiY * RENDER_SCALE);
            {
                Gdiplus::Graphics g(&bmp);
                g.Clear(Gdiplus::Color(0, 0, 0, 0));
                RenderQueueOnGraphics(g, (float)RENDER_SCALE);
            }

            Gdiplus::BitmapData bd;
            Gdiplus::Rect rect(0, 0, tw, th);
            if (bmp.LockBits(&rect, Gdiplus::ImageLockModeRead,
                    PixelFormat32bppARGB, &bd) == Gdiplus::Ok) {
                D3DLOCKED_RECT lr;
                if (SUCCEEDED(g_textTexture->LockRect(0, &lr, NULL, D3DLOCK_DISCARD))) {
                    BYTE* src = (BYTE*)bd.Scan0;
                    BYTE* dst = (BYTE*)lr.pBits;
                    SIZE_T rowBytes = (SIZE_T)tw * 4;
                    for (int y = 0; y < th; y++)
                        memcpy(dst + (SIZE_T)y * lr.Pitch,
                               src + (SIZE_T)y * bd.Stride, rowBytes);
                    g_textTexture->UnlockRect(0);
                    DrawTextQuad(self, backbuf, w, h);
                }
                bmp.UnlockBits(&bd);
            }
        }
        backbuf->Release();
    }
}

// Present hook — calls RenderTextToBackbuffer inside SEH, then original Present.
static HRESULT __stdcall Present9Hook(IDirect3DDevice9* self, const RECT* pSourceRect, const RECT* pDestRect, HWND hDestWindowOverride, const RGNDATA* pDirtyRegion)
{
    if (!g_textQueue.empty() && self && (!g_gameWindow || !IsIconic(g_gameWindow))) {
        __try {
            RenderTextToBackbuffer(self);
        } __except(EXCEPTION_EXECUTE_HANDLER) {
            DebugLog("Present9Hook: exception");
        }
        g_textQueue.clear();
    }

    return g_realPresent9(self, pSourceRect, pDestRect, hDestWindowOverride, pDirtyRegion);
}

static bool HookPresentVtable(IDirect3DDevice9* device)
{
    DWORD_PTR* vtbl = *(DWORD_PTR**)device;
    DWORD oldProtect;
    if (!VirtualProtect(&vtbl[17], sizeof(DWORD_PTR), PAGE_EXECUTE_READWRITE, &oldProtect)) {
        DebugLog("HookPresentVtable: VirtualProtect failed");
        return false;
    }
    g_realPresent9 = (Present9_t)vtbl[17];
    vtbl[17] = (DWORD_PTR)Present9Hook;
    VirtualProtect(&vtbl[17], sizeof(DWORD_PTR), oldProtect, &oldProtect);
    DebugLog("HookPresentVtable: patched vt[17] old=%p", g_realPresent9);
    return true;
}

// Verify a D3D9 device by trying AddRef/Release — the only 100% reliable test.
static bool IsValidD3D9Device(IDirect3DDevice9* device)
{
    if (!device) return false;

    __try {
        ULONG ref = device->AddRef();
        device->Release();
        DebugLog("IsValidD3D9Device: device=%p AddRef=%u OK", device, ref);
        return true;
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        DebugLog("IsValidD3D9Device: device=%p crashed — rejecting", device);
        return false;
    }
}

// Try to find an already-existing D3D9 device (created by the gm82dx9
// DirectX9 extension which runs in DllMain before FWInit).
static bool TryFindD3D9Device()
{
    HMODULE hD3D9 = GetModuleHandleA("d3d9.dll");
    if (!hD3D9) {
        DebugLog("TryFindD3D9Device: d3d9.dll not loaded");
        return false;
    }

    // gm82dx9 stores the game's D3D9 device pointer at fixed address
    // 0x6886a8 (gm82vp.exe has no ASLR).
    IDirect3DDevice9* device = NULL;
    __try {
        device = *(IDirect3DDevice9**)0x6886a8;
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        DebugLog("TryFindD3D9Device: access violation reading 0x6886a8");
        return false;
    }

    if (!IsValidD3D9Device(device)) {
        DebugLog("TryFindD3D9Device: pointer @0x6886a8 = %p invalid", device);
        return false;
    }

    g_d3d9Device = device;
    DebugLog("TryFindD3D9Device: found device=%p", device);
    return HookPresentVtable(device);
}

// ── FWSetViewSize ──────────────────────────────────────────

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
    Gdiplus::GdiplusStartup(&g_gdiplusToken, &g_gdiplusStartupInput, NULL);
    {
        HDC hdc = GetDC(NULL);
        g_screenDpiX = (float)GetDeviceCaps(hdc, LOGPIXELSX);
        g_screenDpiY = (float)GetDeviceCaps(hdc, LOGPIXELSY);
        ReleaseDC(NULL, hdc);
        DebugLog("Screen DPI: %.0f x %.0f", g_screenDpiX, g_screenDpiY);
    }
    g_gameWindow = FindGameWindow();
    DebugLog("Found game window: %p", g_gameWindow);
    g_fontCount = 0;
    g_currentFont = NULL;
    g_halign = 0;
    g_valign = 0;
    g_lineSpacing = 0.0f;
    g_pixelAlign = false;
    TryFindD3D9Device();
    return TRUE;
}

DOUBLE WINAPI FWReleaseCache()
{
    g_textQueue.clear();
    return TRUE;
}

DOUBLE WINAPI FWCleanup()
{
    g_textQueue.clear();
    SAFE_RELEASE(g_textTexture);
    SAFE_RELEASE(g_quadDecl);
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
    g.SetSmoothingMode(Gdiplus::SmoothingModeHighQuality);
    g.SetTextRenderingHint(GetMeasureHint(font));
    Gdiplus::StringFormat fmt(Gdiplus::StringFormat::GenericTypographic());
    fmt.SetAlignment(Gdiplus::StringAlignmentNear);
    fmt.SetLineAlignment(Gdiplus::StringAlignmentNear);
    Gdiplus::RectF bounds;
    g.MeasureString(wstr.c_str(), -1, font, Gdiplus::PointF(0, 0), &fmt, &bounds);
    ReleaseDC(NULL, hdc);
    return bounds.Width;
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
    g.SetSmoothingMode(Gdiplus::SmoothingModeHighQuality);
    g.SetTextRenderingHint(GetMeasureHint(font));
    Gdiplus::StringFormat fmt(Gdiplus::StringFormat::GenericTypographic());
    fmt.SetAlignment(Gdiplus::StringAlignmentNear);
    fmt.SetLineAlignment(Gdiplus::StringAlignmentNear);
    Gdiplus::RectF bounds;
    g.MeasureString(lines[0].c_str(), -1, font, Gdiplus::PointF(0, 0), &fmt, &bounds);
    float lineH = bounds.Height + g_lineSpacing;
    ReleaseDC(NULL, hdc);
    return (float)lines.size() * lineH - g_lineSpacing;
}

DOUBLE WINAPI FWStringWidthEx(LPCSTR str, DOUBLE sep, DOUBLE w)
{
    if (!GetGdiFont()) return FWStringWidth(str);
    std::wstring wstr = AnsiToWide(str);
    if (wstr.empty()) return 0;

    HDC hdc = GetDC(NULL);
    Gdiplus::Graphics g(hdc);
    g.SetPageUnit(Gdiplus::UnitPixel);
    g.SetSmoothingMode(Gdiplus::SmoothingModeHighQuality);
    g.SetTextRenderingHint(GetMeasureHint(GetGdiFont()));

    Gdiplus::StringFormat fmt(Gdiplus::StringFormat::GenericTypographic());
    fmt.SetAlignment(Gdiplus::StringAlignmentNear);
    fmt.SetLineAlignment(Gdiplus::StringAlignmentNear);
    Gdiplus::RectF layout(0, 0, (Gdiplus::REAL)w, 10000);
    Gdiplus::RectF bounds;
    g.MeasureString(wstr.c_str(), -1, GetGdiFont(), layout, &fmt, &bounds);
    ReleaseDC(NULL, hdc);
    return bounds.Width;
}

DOUBLE WINAPI FWStringHeightEx(LPCSTR str, DOUBLE sep, DOUBLE w)
{
    if (!GetGdiFont()) return FWStringHeight(str);
    std::wstring wstr = AnsiToWide(str);
    if (wstr.empty()) return 0;

    HDC hdc = GetDC(NULL);
    Gdiplus::Graphics g(hdc);
    g.SetPageUnit(Gdiplus::UnitPixel);
    g.SetSmoothingMode(Gdiplus::SmoothingModeHighQuality);
    g.SetTextRenderingHint(GetMeasureHint(GetGdiFont()));

    Gdiplus::StringFormat fmt(Gdiplus::StringFormat::GenericTypographic());
    fmt.SetAlignment(Gdiplus::StringAlignmentNear);
    fmt.SetLineAlignment(Gdiplus::StringAlignmentNear);
    Gdiplus::RectF layout(0, 0, (Gdiplus::REAL)w, 10000);
    Gdiplus::RectF bounds;
    g.MeasureString(wstr.c_str(), -1, GetGdiFont(), layout, &fmt, &bounds);
    ReleaseDC(NULL, hdc);
    return bounds.Height;
}

// Queue text items for D3D Present hook, or render directly if no D3D9 device.
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

    // Lazy init: try to find D3D9 device once per frame if missing
    if (!g_d3d9Device) {
        TryFindD3D9Device();
    }

    // If D3D9 is available, queue for Present-hook render (no-flicker).
    // Otherwise fall back to direct GDI+ window DC render (may flicker).
    if (g_d3d9Device) {
        TextDrawItem item;
        item.text = wstr;
        item.x = (float)x;
        item.y = (float)y;
        item.color = color;
        item.alpha = (float)alpha;
        item.font = font;
        item.halign = g_halign;
        item.valign = g_valign;
        item.xOffset = g_currentFont ? g_currentFont->xOffset : 0;
        item.yOffset = g_currentFont ? g_currentFont->yOffset : 0;
        item.lineSpacing = g_lineSpacing;
        item.pixelAlign = g_pixelAlign;
        item.doStroke = g_currentFont ? g_currentFont->stroke : false;
        g_textQueue.push_back(item);
    } else if (!IsIconic(g_gameWindow)) {
        // Instant fallback: draw directly to game window DC
        HDC hdc = GetDC(g_gameWindow);
        if (hdc) {
            // Build a temporary queue with just this item
            TextDrawItem tmp;
            tmp.text = wstr;
            tmp.x = (float)x;
            tmp.y = (float)y;
            tmp.color = color;
            tmp.alpha = (float)alpha;
            tmp.font = font;
            tmp.halign = g_halign;
            tmp.valign = g_valign;
            tmp.xOffset = g_currentFont ? g_currentFont->xOffset : 0;
            tmp.yOffset = g_currentFont ? g_currentFont->yOffset : 0;
            tmp.lineSpacing = g_lineSpacing;
            tmp.pixelAlign = g_pixelAlign;
            tmp.doStroke = g_currentFont ? g_currentFont->stroke : false;
            g_textQueue.push_back(tmp);
            RenderQueueOnDC(hdc);
            g_textQueue.clear();
            ReleaseDC(g_gameWindow, hdc);
        }
    }
}

DOUBLE WINAPI FWDrawText(DOUBLE x, DOUBLE y, LPCSTR str)
{
    if (str && *str)
        DebugLog("FWDrawText(%f, %f, '%s')", x, y, str);
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
    if (!g_d3d9Device && !g_textQueue.empty() && g_gameWindow && IsWindow(g_gameWindow)) {
        HDC hdc = GetDC(g_gameWindow);
        if (hdc) {
            RenderQueueOnDC(hdc);
            ReleaseDC(g_gameWindow, hdc);
        }
        g_textQueue.clear();
    }
    return TRUE;
}

DOUBLE WINAPI FWWindowResized()
{
    return TRUE;
}
