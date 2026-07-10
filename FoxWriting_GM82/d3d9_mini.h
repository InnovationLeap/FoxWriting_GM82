#pragma once
#include <windows.h>

// ============================================================
// Minimal D3D9 type definitions for FoxWriting_GM81
// Uses C++ pure-virtual classes (same vtable layout as real
// D3D9 interfaces) so that ptr->Method() syntax works.
// Extended: full device vtable, textures, vertex decl, all
// constants needed for GDI+ Bitmap -> D3D9 texture quad pipeline.
// ============================================================

#ifdef __cplusplus

#define D3D_SDK_VERSION 32
#define D3D_OK         0
#define D3DADAPTER_DEFAULT 0
#define D3DERR_DEVICELOST     0x887608A2L
#define D3DERR_DEVICENOTRESET 0x887608A3L

// ── Basic enums ──────────────────────────────────────────
typedef enum _D3DFORMAT {
    D3DFMT_UNKNOWN=0, D3DFMT_R8G8B8=20, D3DFMT_A8R8G8B8=21, D3DFMT_X8R8G8B8=22,
    D3DFMT_R5G6B5=23, D3DFMT_A1R5G5B5=25, D3DFMT_A4R4G4B4=26,
    D3DFMT_A8=28,
    D3DFMT_DXT1=0x31545844, D3DFMT_DXT3=0x33545844, D3DFMT_DXT5=0x35545844,
    D3DFMT_FORCE_DWORD=0x7FFFFFFF
} D3DFORMAT;

typedef enum _D3DRESOURCETYPE {
    D3DRTYPE_SURFACE=1, D3DRTYPE_TEXTURE=3, D3DRTYPE_FORCE_DWORD=0x7FFFFFFF
} D3DRESOURCETYPE;

typedef enum _D3DMULTISAMPLE_TYPE {
    D3DMULTISAMPLE_NONE=0, D3DMULTISAMPLE_FORCE_DWORD=0x7FFFFFFF
} D3DMULTISAMPLE_TYPE;

typedef enum _D3DSWAPEFFECT {
    D3DSWAPEFFECT_DISCARD=1, D3DSWAPEFFECT_FLIP=2, D3DSWAPEFFECT_COPY=3,
    D3DSWAPEFFECT_FORCE_DWORD=0x7FFFFFFF
} D3DSWAPEFFECT;

typedef enum _D3DDEVTYPE {
    D3DDEVTYPE_HAL=1, D3DDEVTYPE_REF=2, D3DDEVTYPE_FORCE_DWORD=0x7FFFFFFF
} D3DDEVTYPE;

typedef enum _D3DPOOL {
    D3DPOOL_DEFAULT=0, D3DPOOL_MANAGED=1, D3DPOOL_SYSTEMMEM=2, D3DPOOL_SCRATCH=3,
    D3DPOOL_FORCE_DWORD=0x7FFFFFFF
} D3DPOOL;

typedef enum _D3DBACKBUFFER_TYPE {
    D3DBACKBUFFER_TYPE_MONO=0, D3DBACKBUFFER_TYPE_LEFT=1, D3DBACKBUFFER_TYPE_RIGHT=2,
    D3DBACKBUFFER_TYPE_FORCE_DWORD=0x7FFFFFFF
} D3DBACKBUFFER_TYPE;

typedef enum _D3DBLEND {
    D3DBLEND_ZERO=1, D3DBLEND_ONE=2, D3DBLEND_SRCCOLOR=3, D3DBLEND_INVSRCCOLOR=4,
    D3DBLEND_SRCALPHA=5, D3DBLEND_INVSRCALPHA=6, D3DBLEND_DESTALPHA=7,
    D3DBLEND_INVDESTALPHA=8, D3DBLEND_DESTCOLOR=9, D3DBLEND_INVDESTCOLOR=10,
    D3DBLEND_SRCALPHASAT=11, D3DBLEND_BOTHSRCALPHA=12, D3DBLEND_BOTHINVSRCALPHA=13,
    D3DBLEND_BLENDFACTOR=14, D3DBLEND_INVBLENDFACTOR=15
} D3DBLEND;

typedef enum _D3DZBUFFERTYPE {
    D3DZB_FALSE=0, D3DZB_TRUE=1, D3DZB_USEW=2, D3DZB_FORCE_DWORD=0x7FFFFFFF
} D3DZBUFFERTYPE;

typedef enum _D3DCULL {
    D3DCULL_NONE=1, D3DCULL_CW=2, D3DCULL_CCW=3, D3DCULL_FORCE_DWORD=0x7FFFFFFF
} D3DCULL;

typedef enum _D3DPRIMITIVETYPE {
    D3DPT_POINTLIST=1, D3DPT_LINELIST=2, D3DPT_LINESTRIP=3,
    D3DPT_TRIANGLELIST=4, D3DPT_TRIANGLESTRIP=5, D3DPT_TRIANGLEFAN=6,
    D3DPT_FORCE_DWORD=0x7FFFFFFF
} D3DPRIMITIVETYPE;

typedef enum _D3DTEXTUREADDRESS {
    D3DTADDRESS_WRAP=1, D3DTADDRESS_MIRROR=2, D3DTADDRESS_CLAMP=3,
    D3DTADDRESS_BORDER=4, D3DTADDRESS_MIRRORONCE=5,
    D3DTADDRESS_FORCE_DWORD=0x7FFFFFFF
} D3DTEXTUREADDRESS;

typedef enum _D3DTEXTUREFILTERTYPE {
    D3DTEXF_NONE=0, D3DTEXF_POINT=1, D3DTEXF_LINEAR=2,
    D3DTEXF_ANISOTROPIC=3, D3DTEXF_FORCE_DWORD=0x7FFFFFFF
} D3DTEXTUREFILTERTYPE;

typedef enum _D3DTEXTUREOP {
    D3DTOP_DISABLE=1, D3DTOP_SELECTARG1=2, D3DTOP_SELECTARG2=3,
    D3DTOP_MODULATE=4, D3DTOP_MODULATE2X=5, D3DTOP_MODULATE4X=6,
    D3DTOP_ADD=7, D3DTOP_ADDSIGNED=8, D3DTOP_ADDSIGNED2X=9,
    D3DTOP_SUBTRACT=10, D3DTOP_ADDSMOOTH=11,
    D3DTOP_BLENDDIFFUSEALPHA=12, D3DTOP_BLENDTEXTUREALPHA=13,
    D3DTOP_BLENDFACTORALPHA=14, D3DTOP_BLENDTEXTUREALPHAPM=15,
    D3DTOP_BLENDCURRENTALPHA=16,
    D3DTOP_PREMODULATE=17, D3DTOP_MODULATEALPHA_ADDCOLOR=18,
    D3DTOP_MODULATECOLOR_ADDALPHA=19,
    D3DTOP_MODULATEINVALPHA_ADDCOLOR=20,
    D3DTOP_MODULATEINVCOLOR_ADDALPHA=21,
    D3DTOP_BUMPENVMAP=22, D3DTOP_BUMPENVMAPLUMINANCE=23,
    D3DTOP_DOTPRODUCT3=24, D3DTOP_MULTIPLYADD=25, D3DTOP_LERP=26,
    D3DTOP_FORCE_DWORD=0x7FFFFFFF
} D3DTEXTUREOP;

typedef enum _D3DTEXTURESTAGESTATETYPE {
    D3DTSS_COLOROP=1, D3DTSS_COLORARG1=2, D3DTSS_COLORARG2=3,
    D3DTSS_ALPHAOP=4, D3DTSS_ALPHAARG1=5, D3DTSS_ALPHAARG2=6,
    D3DTSS_FORCE_DWORD=0x7FFFFFFF
} D3DTEXTURESTAGESTATETYPE;

typedef enum _D3DSAMPLERSTATETYPE {
    D3DSAMP_ADDRESSU=1, D3DSAMP_ADDRESSV=2, D3DSAMP_ADDRESSW=3,
    D3DSAMP_BORDERCOLOR=4, D3DSAMP_MAGFILTER=5, D3DSAMP_MINFILTER=6,
    D3DSAMP_MIPFILTER=7, D3DSAMP_MIPMAPLODBIAS=8, D3DSAMP_MAXMIPLEVEL=9,
    D3DSAMP_MAXANISOTROPY=10, D3DSAMP_SRGBTEXTURE=11, D3DSAMP_ELEMENTINDEX=12,
    D3DSAMP_DMAPOFFSET=13, D3DSAMP_FORCE_DWORD=0x7FFFFFFF
} D3DSAMPLERSTATETYPE;

// ── Vertex declaration ────────────────────────────────────
typedef enum _D3DDECLUSAGE {
    D3DDECLUSAGE_POSITION=0, D3DDECLUSAGE_BLENDWEIGHT=1, D3DDECLUSAGE_BLENDINDICES=2,
    D3DDECLUSAGE_NORMAL=3, D3DDECLUSAGE_PSIZE=4, D3DDECLUSAGE_TEXCOORD=5,
    D3DDECLUSAGE_TANGENT=6, D3DDECLUSAGE_BINORMAL=7, D3DDECLUSAGE_TESSFACTOR=8,
    D3DDECLUSAGE_POSITIONT=9, D3DDECLUSAGE_COLOR=10, D3DDECLUSAGE_FOG=11,
    D3DDECLUSAGE_DEPTH=12, D3DDECLUSAGE_SAMPLE=13
} D3DDECLUSAGE;

typedef enum _D3DDECLMETHOD {
    D3DDECLMETHOD_DEFAULT=0, D3DDECLMETHOD_PARTIALU=1, D3DDECLMETHOD_PARTIALV=2,
    D3DDECLMETHOD_CROSSUV=3, D3DDECLMETHOD_UV=4,
    D3DDECLMETHOD_LOOKUP=5, D3DDECLMETHOD_LOOKUPPRESAMPLED=6
} D3DDECLMETHOD;

typedef enum _D3DDECLTYPE {
    D3DDECLTYPE_FLOAT1=0, D3DDECLTYPE_FLOAT2=1, D3DDECLTYPE_FLOAT3=2,
    D3DDECLTYPE_FLOAT4=3, D3DDECLTYPE_D3DCOLOR=4, D3DDECLTYPE_UBYTE4=5,
    D3DDECLTYPE_SHORT2=6, D3DDECLTYPE_SHORT4=7,
    D3DDECLTYPE_UNUSED=17
} D3DDECLTYPE;

typedef struct _D3DVERTEXELEMENT9 {
    WORD Stream;
    WORD Offset;
    BYTE Type;
    BYTE Method;
    BYTE Usage;
    BYTE UsageIndex;
} D3DVERTEXELEMENT9;

#define D3DDECL_END() {0xFF,0,D3DDECLTYPE_UNUSED,0,0,0}

// ── Render states ─────────────────────────────────────────
typedef enum _D3DRENDERSTATETYPE {
    D3DRS_ZENABLE=7, D3DRS_FILLMODE=8, D3DRS_SHADEMODE=9,
    D3DRS_ZWRITEENABLE=14, D3DRS_ALPHATESTENABLE=15, D3DRS_LASTPIXEL=16,
    D3DRS_SRCBLEND=19, D3DRS_DESTBLEND=20, D3DRS_CULLMODE=22,
    D3DRS_ZFUNC=23, D3DRS_ALPHAREF=24, D3DRS_ALPHAFUNC=25,
    D3DRS_DITHERENABLE=26, D3DRS_ALPHABLENDENABLE=27, D3DRS_FOGENABLE=28,
    D3DRS_SPECULARENABLE=29, D3DRS_FOGCOLOR=34, D3DRS_FOGTABLEMODE=35,
    D3DRS_FOGSTART=36, D3DRS_FOGEND=37, D3DRS_FOGDENSITY=38,
    D3DRS_STENCILENABLE=52, D3DRS_STENCILFAIL=53, D3DRS_STENCILZFAIL=54,
    D3DRS_STENCILPASS=55, D3DRS_STENCILFUNC=56, D3DRS_STENCILREF=57,
    D3DRS_STENCILMASK=58, D3DRS_STENCILWRITEMASK=59,
    D3DRS_TEXTUREFACTOR=60,
    D3DRS_CLIPPING=136, D3DRS_LIGHTING=137, D3DRS_AMBIENT=139,
    D3DRS_FOGVERTEXMODE=140, D3DRS_COLORVERTEX=141, D3DRS_LOCALVIEWER=142,
    D3DRS_NORMALIZENORMALS=143, D3DRS_DIFFUSEMATERIALSOURCE=145,
    D3DRS_SPECULARMATERIALSOURCE=146, D3DRS_AMBIENTMATERIALSOURCE=147,
    D3DRS_EMISSIVEMATERIALSOURCE=148, D3DRS_VERTEXBLEND=151,
    D3DRS_CLIPPLANEENABLE=152,
    D3DRS_POINTSIZE=154, D3DRS_POINTSIZE_MIN=155, D3DRS_POINTSPRITEENABLE=156,
    D3DRS_POINTSCALEENABLE=157, D3DRS_MULTISAMPLEANTIALIAS=161,
    D3DRS_MULTISAMPLEMASK=162,
    D3DRS_INDEXEDVERTEXBLENDENABLE=167, D3DRS_COLORWRITEENABLE=168,
    D3DRS_TWEENFACTOR=170, D3DRS_BLENDOP=171, D3DRS_POSITIONDEGREE=172,
    D3DRS_SCISSORTESTENABLE=174, D3DRS_SLOPESCALEDEPTHBIAS=175,
    D3DRS_ANTIALIASEDLINEENABLE=176,
    D3DRS_FORCE_DWORD=0x7FFFFFFF
} D3DRENDERSTATETYPE;

// ── Texture argument macros ───────────────────────────────
#define D3DTA_CURRENT           0x00000001
#define D3DTA_TEXTURE           0x00000002

// ── Usage / Lock flags ────────────────────────────────────
#define D3DUSAGE_DYNAMIC        (0x00000200L)
#define D3DLOCK_DISCARD          0x00002000L

// ── Creation flags ────────────────────────────────────────
#define D3DCREATE_SOFTWARE_VERTEXPROCESSING 0x00000020L
#define D3DCREATE_HARDWARE_VERTEXPROCESSING 0x00000040L

// ── Structs ───────────────────────────────────────────────
typedef struct D3DPRESENT_PARAMETERS {
    UINT BackBufferWidth, BackBufferHeight;
    D3DFORMAT BackBufferFormat;
    UINT BackBufferCount;
    D3DMULTISAMPLE_TYPE MultiSampleType;
    DWORD MultiSampleQuality;
    D3DSWAPEFFECT SwapEffect;
    HWND hDeviceWindow;
    BOOL Windowed;
    BOOL EnableAutoDepthStencil;
    D3DFORMAT AutoDepthStencilFormat;
    DWORD Flags;
    UINT FullScreen_RefreshRateInHz;
    UINT FullScreen_PresentationInterval;
} D3DPRESENT_PARAMETERS;

typedef struct D3DDEVICE_CREATION_PARAMETERS {
    UINT AdapterOrdinal;
    D3DDEVTYPE DeviceType;
    HWND hFocusWindow;
    DWORD BehaviorFlags;
} D3DDEVICE_CREATION_PARAMETERS;

typedef struct D3DSURFACE_DESC {
    D3DFORMAT Format; D3DRESOURCETYPE Type; DWORD Usage; D3DPOOL Pool;
    D3DMULTISAMPLE_TYPE MultiSampleType; DWORD MultiSampleQuality; UINT Width; UINT Height;
} D3DSURFACE_DESC;

typedef struct D3DLOCKED_RECT { INT Pitch; void* pBits; } D3DLOCKED_RECT;

// ============================================================
// Pure-virtual C++ classes (same vtable layout as real D3D9)
// ============================================================

struct IDirect3D9;
struct IDirect3DDevice9;
struct IDirect3DSurface9;
struct IDirect3DBaseTexture9;
struct IDirect3DTexture9;
struct IDirect3DVertexDeclaration9;

// ── IDirect3D9 ──────────────────────────────────────────
struct IDirect3D9 {
    virtual HRESULT __stdcall QueryInterface(const IID&, void**) = 0;
    virtual ULONG   __stdcall AddRef() = 0;
    virtual ULONG   __stdcall Release() = 0;
    virtual void*   _pad03() = 0;
    virtual void*   _pad04() = 0;
    virtual void*   _pad05() = 0;
    virtual void*   _pad06() = 0;
    virtual void*   _pad07() = 0;
    virtual void*   _pad08() = 0;
    virtual void*   _pad09() = 0;
    virtual void*   _pad10() = 0;
    virtual void*   _pad11() = 0;
    virtual void*   _pad12() = 0;
    virtual void*   _pad13() = 0;
    virtual void*   _pad14() = 0;
    virtual void*   _pad15() = 0;
    virtual HRESULT __stdcall CreateDevice(UINT, D3DDEVTYPE, HWND, DWORD, D3DPRESENT_PARAMETERS*, IDirect3DDevice9**) = 0;
};

// ── IDirect3DDevice9 ─────────────────────────────────────
struct IDirect3DDevice9 {
    virtual HRESULT __stdcall QueryInterface(const IID&, void**) = 0;
    virtual ULONG   __stdcall AddRef() = 0;
    virtual ULONG   __stdcall Release() = 0;
    virtual HRESULT __stdcall TestCooperativeLevel() = 0;
    virtual UINT    __stdcall GetAvailableTextureMem() = 0;
    virtual HRESULT __stdcall EvictManagedResources() = 0;
    virtual HRESULT __stdcall GetDirect3D(IDirect3D9**) = 0;
    virtual HRESULT __stdcall GetDeviceCaps(void*) = 0;
    virtual HRESULT __stdcall GetDisplayMode(UINT, void*) = 0;
    virtual HRESULT __stdcall GetCreationParameters(D3DDEVICE_CREATION_PARAMETERS*) = 0;
    virtual HRESULT __stdcall SetCursorProperties(UINT, UINT, IDirect3DSurface9*) = 0;
    virtual void    __stdcall SetCursorPosition(INT, INT, DWORD) = 0;
    virtual BOOL    __stdcall ShowCursor(BOOL) = 0;
    virtual HRESULT __stdcall CreateAdditionalSwapChain(D3DPRESENT_PARAMETERS*, void*) = 0;
    virtual HRESULT __stdcall GetSwapChain(UINT, void*) = 0;
    virtual UINT    __stdcall GetNumberOfSwapChains() = 0;
    virtual HRESULT __stdcall Reset(D3DPRESENT_PARAMETERS*) = 0;
    virtual HRESULT __stdcall Present(const RECT*, const RECT*, HWND, const RGNDATA*) = 0;
    virtual HRESULT __stdcall GetBackBuffer(UINT, UINT, D3DBACKBUFFER_TYPE, IDirect3DSurface9**) = 0;
    // vt[19]..vt[22] pads
    virtual void* _pad19() = 0;
    virtual void* _pad20() = 0;
    virtual void* _pad21() = 0;
    virtual void* _pad22() = 0;
    // vt[23] CreateTexture
    virtual HRESULT __stdcall CreateTexture(UINT, UINT, UINT, DWORD, D3DFORMAT, D3DPOOL, IDirect3DTexture9**, HANDLE*) = 0;
    // vt[24]..vt[36] pads
    virtual void* _pad24() = 0;
    virtual void* _pad25() = 0;
    virtual void* _pad26() = 0;
    virtual void* _pad27() = 0;
    virtual void* _pad28() = 0;
    virtual void* _pad29() = 0;
    virtual void* _pad30() = 0;
    virtual void* _pad31() = 0;
    virtual void* _pad32() = 0;
    virtual void* _pad33() = 0;
    virtual void* _pad34() = 0;
    virtual void* _pad35() = 0;
    virtual void* _pad36() = 0;
    // vt[37] SetRenderTarget
    virtual HRESULT __stdcall SetRenderTarget(DWORD, IDirect3DSurface9*) = 0;
    // vt[38] GetRenderTarget
    virtual HRESULT __stdcall GetRenderTarget(DWORD, IDirect3DSurface9**) = 0;
    // vt[39]..vt[56] pads
    virtual void* _pad39() = 0; virtual void* _pad40() = 0; virtual void* _pad41() = 0;
    virtual void* _pad42() = 0; virtual void* _pad43() = 0; virtual void* _pad44() = 0;
    virtual void* _pad45() = 0; virtual void* _pad46() = 0; virtual void* _pad47() = 0;
    virtual void* _pad48() = 0; virtual void* _pad49() = 0; virtual void* _pad50() = 0;
    virtual void* _pad51() = 0; virtual void* _pad52() = 0; virtual void* _pad53() = 0;
    virtual void* _pad54() = 0; virtual void* _pad55() = 0; virtual void* _pad56() = 0;
    // vt[57] SetRenderState
    virtual HRESULT __stdcall SetRenderState(D3DRENDERSTATETYPE, DWORD) = 0;
    // vt[58] GetRenderState
    virtual HRESULT __stdcall GetRenderState(D3DRENDERSTATETYPE, DWORD*) = 0;
    // vt[59]..vt[63] pads
    virtual void* _pad59() = 0; virtual void* _pad60() = 0; virtual void* _pad61() = 0;
    virtual void* _pad62() = 0; virtual void* _pad63() = 0;
    // vt[64] GetTexture
    virtual HRESULT __stdcall GetTexture(DWORD, IDirect3DBaseTexture9**) = 0;
    // vt[65] SetTexture
    virtual HRESULT __stdcall SetTexture(DWORD, IDirect3DBaseTexture9*) = 0;
    // vt[66] GetTextureStageState
    virtual HRESULT __stdcall GetTextureStageState(DWORD, D3DTEXTURESTAGESTATETYPE, DWORD*) = 0;
    // vt[67] SetTextureStageState
    virtual HRESULT __stdcall SetTextureStageState(DWORD, D3DTEXTURESTAGESTATETYPE, DWORD) = 0;
    // vt[68] GetSamplerState
    virtual HRESULT __stdcall GetSamplerState(DWORD, D3DSAMPLERSTATETYPE, DWORD*) = 0;
    // vt[69] SetSamplerState
    virtual HRESULT __stdcall SetSamplerState(DWORD, D3DSAMPLERSTATETYPE, DWORD) = 0;
    // vt[70]..vt[82] pads
    virtual void* _pad70() = 0; virtual void* _pad71() = 0; virtual void* _pad72() = 0;
    virtual void* _pad73() = 0; virtual void* _pad74() = 0; virtual void* _pad75() = 0;
    virtual void* _pad76() = 0; virtual void* _pad77() = 0; virtual void* _pad78() = 0;
    virtual void* _pad79() = 0; virtual void* _pad80() = 0; virtual void* _pad81() = 0;
    virtual void* _pad82() = 0;
    // vt[83] DrawPrimitiveUP
    virtual HRESULT __stdcall DrawPrimitiveUP(D3DPRIMITIVETYPE, UINT, const void*, UINT) = 0;
    // vt[84]..vt[85] pads
    virtual void* _pad84() = 0; virtual void* _pad85() = 0;
    // vt[86] CreateVertexDeclaration
    virtual HRESULT __stdcall CreateVertexDeclaration(const D3DVERTEXELEMENT9*, IDirect3DVertexDeclaration9**) = 0;
    // vt[87] SetVertexDeclaration
    virtual HRESULT __stdcall SetVertexDeclaration(IDirect3DVertexDeclaration9*) = 0;
    // vt[88] GetVertexDeclaration
    virtual HRESULT __stdcall GetVertexDeclaration(IDirect3DVertexDeclaration9**) = 0;
};

// ── IDirect3DSurface9 ────────────────────────────────────
// vt[0..2] IUnknown, vt[3..10] IDirect3DResource9 (8 pads),
// vt[11] GetContainer, vt[12] GetDesc, vt[13] LockRect, vt[14] UnlockRect,
// vt[15] GetDC, vt[16] ReleaseDC
struct IDirect3DSurface9 {
    virtual HRESULT __stdcall QueryInterface(const IID&, void**) = 0;
    virtual ULONG   __stdcall AddRef() = 0;
    virtual ULONG   __stdcall Release() = 0;
    virtual void* _pad03() = 0; virtual void* _pad04() = 0;
    virtual void* _pad05() = 0; virtual void* _pad06() = 0;
    virtual void* _pad07() = 0; virtual void* _pad08() = 0;
    virtual void* _pad09() = 0; virtual void* _pad10() = 0;
    virtual void* _pad11() = 0;
    virtual HRESULT __stdcall GetDesc(D3DSURFACE_DESC*) = 0;
    virtual void* _pad13() = 0; virtual void* _pad14() = 0;
    virtual HRESULT __stdcall GetDC(HDC*) = 0;
    virtual HRESULT __stdcall ReleaseDC(HDC) = 0;
};

// ── IDirect3DBaseTexture9 ──────────────────────────────────
struct IDirect3DBaseTexture9 {
    virtual HRESULT __stdcall QueryInterface(const IID&, void**) = 0;
    virtual ULONG   __stdcall AddRef() = 0;
    virtual ULONG   __stdcall Release() = 0;
};

// ── IDirect3DTexture9 ──────────────────────────────────────
struct IDirect3DTexture9 {
    virtual HRESULT __stdcall QueryInterface(const IID&, void**) = 0;
    virtual ULONG   __stdcall AddRef() = 0;
    virtual ULONG   __stdcall Release() = 0;
    // IDirect3DBaseTexture9: vt[3]..vt[18]
    virtual void* _pad03() = 0; virtual void* _pad04() = 0; virtual void* _pad05() = 0;
    virtual void* _pad06() = 0; virtual void* _pad07() = 0; virtual void* _pad08() = 0;
    virtual void* _pad09() = 0; virtual void* _pad10() = 0; virtual void* _pad11() = 0;
    virtual void* _pad12() = 0; virtual void* _pad13() = 0; virtual void* _pad14() = 0;
    virtual void* _pad15() = 0; virtual void* _pad16() = 0; virtual void* _pad17() = 0;
    virtual void* _pad18() = 0;
    virtual HRESULT __stdcall LockRect(UINT, D3DLOCKED_RECT*, const RECT*, DWORD) = 0;
    virtual HRESULT __stdcall UnlockRect(UINT) = 0;
    virtual HRESULT __stdcall AddDirtyRect(const RECT*) = 0;
};

// ── IDirect3DVertexDeclaration9 ───────────────────────────
struct IDirect3DVertexDeclaration9 {
    virtual HRESULT __stdcall QueryInterface(const IID&, void**) = 0;
    virtual ULONG   __stdcall AddRef() = 0;
    virtual ULONG   __stdcall Release() = 0;
};

#endif // __cplusplus
