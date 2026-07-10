#pragma once
#include <windows.h>

typedef DWORD D3DCOLOR;

typedef struct D3DMATRIX { float m[4][4]; } D3DMATRIX;
typedef struct D3DRECT { LONG x1,y1,x2,y2; } D3DRECT;

typedef enum _D3DFORMAT {
    D3DFMT_UNKNOWN=0, D3DFMT_R8G8B8=20, D3DFMT_A8R8G8B8=21, D3DFMT_X8R8G8B8=22,
    D3DFMT_R5G6B5=23, D3DFMT_A1R5G5B5=25, D3DFMT_A4R4G4B4=26,
    D3DFMT_A8=28, D3DFMT_DXT1=0x31545844, D3DFMT_DXT3=0x33545844, D3DFMT_DXT5=0x35545844,
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

typedef enum _D3DPRIMITIVETYPE {
    D3DPT_TRIANGLESTRIP=5, D3DPT_FORCE_DWORD=0x7FFFFFFF
} D3DPRIMITIVETYPE;

typedef enum _D3DTRANSFORMSTATETYPE {
    D3DTS_VIEW=2, D3DTS_PROJECTION=3, D3DTS_WORLD=256, D3DTS_FORCE_DWORD=0x7FFFFFFF
} D3DTRANSFORMSTATETYPE;

typedef enum _D3DRENDERSTATETYPE {
    D3DRS_ZENABLE=7, D3DRS_FILLMODE=8, D3DRS_SHADEMODE=9, D3DRS_ZWRITEENABLE=14,
    D3DRS_ALPHATESTENABLE=15, D3DRS_SRCBLEND=19, D3DRS_DESTBLEND=20,
    D3DRS_CULLMODE=22, D3DRS_ALPHABLENDENABLE=27, D3DRS_FOGENABLE=28,
    D3DRS_SPECULARENABLE=29, D3DRS_LIGHTING=137, D3DRS_STENCILENABLE=52,
    D3DRS_COLORVERTEX=43, D3DRS_ALPHAFUNC=25, D3DRS_ALPHAREF=24,
    D3DRS_FORCE_DWORD=0x7FFFFFFF
} D3DRENDERSTATETYPE;

typedef enum _D3DTEXTURESTAGESTATETYPE {
    D3DTSS_COLOROP=1, D3DTSS_COLORARG1=2, D3DTSS_COLORARG2=3,
    D3DTSS_ALPHAOP=4, D3DTSS_ALPHAARG1=5, D3DTSS_ALPHAARG2=6,
    D3DTSS_MINFILTER=17, D3DTSS_MAGFILTER=16, D3DTSS_MIPFILTER=18,
    D3DTSS_FORCE_DWORD=0x7FFFFFFF
} D3DTEXTURESTAGESTATETYPE;

typedef enum _D3DTEXTUREOP {
    D3DTOP_SELECTARG1=2, D3DTOP_FORCE_DWORD=0x7FFFFFFF
} D3DTEXTUREOP;

#define D3DTA_TEXTURE 0x02
#define D3DTA_DIFFUSE 0x00
#define D3DTA_CURRENT 0x01

typedef enum _D3DTEXTUREFILTERTYPE {
    D3DTEXF_NONE=0, D3DTEXF_POINT=1, D3DTEXF_LINEAR=2, D3DTEXF_FORCE_DWORD=0x7FFFFFFF
} D3DTEXTUREFILTERTYPE;

typedef enum _D3DSHADEMODE {
    D3DSHADE_FLAT=1, D3DSHADE_GOURAUD=2, D3DSHADE_FORCE_DWORD=0x7FFFFFFF
} D3DSHADEMODE;

typedef enum _D3DSTATEBLOCKTYPE {
    D3DSBT_ALL=1, D3DSBT_PIXELSTATE=2, D3DSBT_VERTEXSTATE=3, D3DSBT_FORCE_DWORD=0x7FFFFFFF
} D3DSTATEBLOCKTYPE;

typedef enum _D3DCULL {
    D3DCULL_NONE=1, D3DCULL_CW=2, D3DCULL_CCW=3, D3DCULL_FORCE_DWORD=0x7FFFFFFF
} D3DCULL;

typedef enum _D3DBLEND {
    D3DBLEND_ZERO=1, D3DBLEND_ONE=2, D3DBLEND_SRCALPHA=5, D3DBLEND_INVSRCALPHA=6,
    D3DBLEND_FORCE_DWORD=0x7FFFFFFF
} D3DBLEND;

// ============================================================
// Structs
// ============================================================
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

typedef struct D3DDISPLAYMODE { UINT Width,Height,RefreshRate; D3DFORMAT Format; } D3DDISPLAYMODE;
typedef struct D3DDEVICE_CREATION_PARAMETERS { UINT AdapterOrdinal; D3DDEVTYPE DeviceType; HWND hFocusWindow; DWORD BehaviorFlags; } D3DDEVICE_CREATION_PARAMETERS;

typedef struct D3DSURFACE_DESC {
    D3DFORMAT Format; D3DRESOURCETYPE Type; DWORD Usage; D3DPOOL Pool; UINT Size;
    D3DMULTISAMPLE_TYPE MultiSampleType; DWORD MultiSampleQuality; UINT Width, Height;
} D3DSURFACE_DESC;

typedef struct D3DLOCKED_RECT { INT Pitch; void* pBits; } D3DLOCKED_RECT;

// ============================================================
// Constants
// ============================================================
#define D3DFVF_XYZ     0x0002
#define D3DFVF_DIFFUSE 0x0040
#define D3DFVF_TEX1    0x0100

#define D3DCREATE_SOFTWARE_VERTEXPROCESSING 0x00000020L
#define D3DCREATE_HARDWARE_VERTEXPROCESSING 0x00000040L

#define D3D_SDK_VERSION 32
#define D3DBACKBUFFER_TYPE_MONO 0
#define D3DADAPTER_DEFAULT 0

// ============================================================
// Forward declarations
// ============================================================
struct IDirect3D8;
struct IDirect3DDevice8;
struct IDirect3DTexture8;
struct IDirect3DSurface8;

// ============================================================
// IDirect3D8
// ============================================================
typedef struct IDirect3D8Vtbl {
    HRESULT (__stdcall *QueryInterface)(IDirect3D8*, const IID&, void**);
    ULONG   (__stdcall *AddRef)(IDirect3D8*);
    ULONG   (__stdcall *Release)(IDirect3D8*);
    HRESULT (__stdcall *RegisterSoftwareDevice)(IDirect3D8*, void*);
    UINT    (__stdcall *GetAdapterCount)(IDirect3D8*);
    HRESULT (__stdcall *GetAdapterIdentifier)(IDirect3D8*, UINT, DWORD, void*);
    UINT    (__stdcall *GetAdapterModeCount)(IDirect3D8*, UINT, D3DFORMAT);
    HRESULT (__stdcall *EnumAdapterModes)(IDirect3D8*, UINT, UINT, void*);
    HRESULT (__stdcall *GetAdapterDisplayMode)(IDirect3D8*, UINT, D3DDISPLAYMODE*);
    HRESULT (__stdcall *CheckDeviceType)(IDirect3D8*, UINT, D3DDEVTYPE, D3DFORMAT, D3DFORMAT, BOOL);
    HRESULT (__stdcall *CheckDeviceFormat)(IDirect3D8*, UINT, D3DDEVTYPE, D3DFORMAT, DWORD, D3DRESOURCETYPE, D3DFORMAT);
    HRESULT (__stdcall *CheckDeviceMultiSampleType)(IDirect3D8*, UINT, D3DDEVTYPE, D3DFORMAT, BOOL, D3DMULTISAMPLE_TYPE);
    HRESULT (__stdcall *CheckDepthStencilMatch)(IDirect3D8*, UINT, D3DDEVTYPE, D3DFORMAT, D3DFORMAT, D3DFORMAT);
    HRESULT (__stdcall *GetDeviceCaps)(IDirect3D8*, UINT, D3DDEVTYPE, void*);
    HMONITOR(__stdcall *GetAdapterMonitor)(IDirect3D8*, UINT);
    HRESULT (__stdcall *CreateDevice)(IDirect3D8*, UINT, D3DDEVTYPE, HWND, DWORD, D3DPRESENT_PARAMETERS*, IDirect3DDevice8**);
} IDirect3D8Vtbl;

struct IDirect3D8 { IDirect3D8Vtbl* lpVtbl; };

// ============================================================
// IDirect3DDevice8
// ============================================================
typedef struct IDirect3DDevice8Vtbl {
    HRESULT (__stdcall *QueryInterface)(IDirect3DDevice8*, const IID&, void**);
    ULONG   (__stdcall *AddRef)(IDirect3DDevice8*);
    ULONG   (__stdcall *Release)(IDirect3DDevice8*);
    HRESULT (__stdcall *TestCooperativeLevel)(IDirect3DDevice8*);
    UINT    (__stdcall *GetAvailableTextureMem)(IDirect3DDevice8*);
    HRESULT (__stdcall *ResourceManagerDiscardBytes)(IDirect3DDevice8*, DWORD);
    HRESULT (__stdcall *GetDirect3D)(IDirect3DDevice8*, IDirect3D8**);
    HRESULT (__stdcall *GetDeviceCaps)(IDirect3DDevice8*, void*);
    HRESULT (__stdcall *GetDisplayMode)(IDirect3DDevice8*, D3DDISPLAYMODE*);
    HRESULT (__stdcall *GetCreationParameters)(IDirect3DDevice8*, D3DDEVICE_CREATION_PARAMETERS*);
    HRESULT (__stdcall *SetCursorProperties)(IDirect3DDevice8*, UINT, UINT, void*);
    void    (__stdcall *SetCursorPosition)(IDirect3DDevice8*, int, int, DWORD);
    BOOL    (__stdcall *ShowCursor)(IDirect3DDevice8*, BOOL);
    HRESULT (__stdcall *CreateAdditionalSwapChain)(IDirect3DDevice8*, D3DPRESENT_PARAMETERS*, void*);
    HRESULT (__stdcall *Reset)(IDirect3DDevice8*, D3DPRESENT_PARAMETERS*);
    HRESULT (__stdcall *Present)(IDirect3DDevice8*, const RECT*, const RECT*, HWND, const RGNDATA*);
    HRESULT (__stdcall *GetBackBuffer)(IDirect3DDevice8*, UINT, DWORD, IDirect3DSurface8**);
    HRESULT (__stdcall *GetRasterStatus)(IDirect3DDevice8*, void*);
    void    (__stdcall *SetGammaRamp)(IDirect3DDevice8*, DWORD, void*);
    void    (__stdcall *GetGammaRamp)(IDirect3DDevice8*, void*);
    HRESULT (__stdcall *CreateTexture)(IDirect3DDevice8*, UINT, UINT, UINT, DWORD, D3DFORMAT, D3DPOOL, IDirect3DTexture8**);
    // 21-33 omitted (not needed)
    void*   _pad21to33[13];
    HRESULT (__stdcall *BeginScene)(IDirect3DDevice8*);
    HRESULT (__stdcall *EndScene)(IDirect3DDevice8*);
    HRESULT (__stdcall *Clear)(IDirect3DDevice8*, DWORD, const D3DRECT*, DWORD, D3DCOLOR, float, DWORD);
    HRESULT (__stdcall *SetTransform)(IDirect3DDevice8*, D3DTRANSFORMSTATETYPE, const D3DMATRIX*);
    // 38-49 omitted
    void*   _pad38to49[12];
    HRESULT (__stdcall *SetRenderState)(IDirect3DDevice8*, D3DRENDERSTATETYPE, DWORD);
    HRESULT (__stdcall *GetRenderState)(IDirect3DDevice8*, D3DRENDERSTATETYPE, DWORD*);
    HRESULT (__stdcall *BeginStateBlock)(IDirect3DDevice8*);
    HRESULT (__stdcall *EndStateBlock)(IDirect3DDevice8*, DWORD*);
    HRESULT (__stdcall *ApplyStateBlock)(IDirect3DDevice8*, DWORD);
    HRESULT (__stdcall *DeleteStateBlock)(IDirect3DDevice8*, DWORD);
    HRESULT (__stdcall *CreateStateBlock)(IDirect3DDevice8*, DWORD, DWORD*);
    // 57-58 omitted
    void*   _pad57to58[2];
    HRESULT (__stdcall *GetTexture)(IDirect3DDevice8*, DWORD, IDirect3DTexture8**);
    HRESULT (__stdcall *SetTexture)(IDirect3DDevice8*, DWORD, IDirect3DTexture8*);
    HRESULT (__stdcall *GetTextureStageState)(IDirect3DDevice8*, DWORD, D3DTEXTURESTAGESTATETYPE, DWORD*);
    HRESULT (__stdcall *SetTextureStageState)(IDirect3DDevice8*, DWORD, D3DTEXTURESTAGESTATETYPE, DWORD);
    // 63-76 omitted
    void*   _pad63to76[14];
    HRESULT (__stdcall *DrawPrimitiveUP)(IDirect3DDevice8*, D3DPRIMITIVETYPE, UINT, const void*, UINT);
    // 78-83 omitted
    void*   _pad78to83[6];
    HRESULT (__stdcall *SetFVF)(IDirect3DDevice8*, DWORD);
    // 85+ omitted
} IDirect3DDevice8Vtbl;

struct IDirect3DDevice8 { IDirect3DDevice8Vtbl* lpVtbl; };

// ============================================================
// IDirect3DTexture8
// ============================================================
typedef struct IDirect3DTexture8Vtbl {
    HRESULT (__stdcall *QueryInterface)(IDirect3DTexture8*, const IID&, void**);
    ULONG   (__stdcall *AddRef)(IDirect3DTexture8*);
    ULONG   (__stdcall *Release)(IDirect3DTexture8*);
    HRESULT (__stdcall *GetDevice)(IDirect3DTexture8*, IDirect3DDevice8**);
    // 4-9 omitted
    void*   _pad4to9[6];
    DWORD   (__stdcall *GetLevelCount)(IDirect3DTexture8*);
    HRESULT (__stdcall *GetLevelDesc)(IDirect3DTexture8*, UINT, D3DSURFACE_DESC*);
    HRESULT (__stdcall *LockRect)(IDirect3DTexture8*, UINT, D3DLOCKED_RECT*, const RECT*, DWORD);
    HRESULT (__stdcall *UnlockRect)(IDirect3DTexture8*, UINT);
    HRESULT (__stdcall *GetSurfaceLevel)(IDirect3DTexture8*, UINT, IDirect3DSurface8**);
} IDirect3DTexture8Vtbl;

struct IDirect3DTexture8 { IDirect3DTexture8Vtbl* lpVtbl; };

// ============================================================
// IDirect3DSurface8
// ============================================================
typedef struct IDirect3DSurface8Vtbl {
    HRESULT (__stdcall *QueryInterface)(IDirect3DSurface8*, const IID&, void**);
    ULONG   (__stdcall *AddRef)(IDirect3DSurface8*);
    ULONG   (__stdcall *Release)(IDirect3DSurface8*);
    HRESULT (__stdcall *GetDevice)(IDirect3DSurface8*, IDirect3DDevice8**);
    // 4-10 omitted
    void*   _pad4to10[7];
    HRESULT (__stdcall *GetDesc)(IDirect3DSurface8*, D3DSURFACE_DESC*);
    HRESULT (__stdcall *LockRect)(IDirect3DSurface8*, D3DLOCKED_RECT*, const RECT*, DWORD);
    HRESULT (__stdcall *UnlockRect)(IDirect3DSurface8*);
} IDirect3DSurface8Vtbl;

struct IDirect3DSurface8 { IDirect3DSurface8Vtbl* lpVtbl; };

// ============================================================
// GUIDs
// ============================================================
DEFINE_GUID(IID_IDirect3DDevice8, 0x7385E5DF, 0x8FE8, 0x41D5, 0x86, 0xB6, 0xD7, 0xB4, 0x85, 0x47, 0xB6, 0xCF);

// ============================================================
// Direct3DCreate8
// ============================================================
typedef IDirect3D8* (WINAPI *D3D8CREATE8)(UINT SDKVersion);
