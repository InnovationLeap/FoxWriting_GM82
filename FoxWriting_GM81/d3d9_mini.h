#pragma once
#include <windows.h>

// ============================================================
// Minimal D3D9 type definitions for FoxWriting_GM81
// ============================================================

#define D3D_SDK_VERSION 32
#define D3D_OK         0
#define D3DADAPTER_DEFAULT 0

#define D3DERR_DEVICELOST     0x887608A2L
#define D3DERR_DEVICENOTRESET 0x887608A3L

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
    D3DBACKBUFFER_TYPE_MONO=0,
    D3DBACKBUFFER_TYPE_LEFT=1,
    D3DBACKBUFFER_TYPE_RIGHT=2,
    D3DBACKBUFFER_TYPE_FORCE_DWORD=0x7FFFFFFF
} D3DBACKBUFFER_TYPE;

#define D3DCREATE_SOFTWARE_VERTEXPROCESSING 0x00000020L
#define D3DCREATE_HARDWARE_VERTEXPROCESSING 0x00000040L

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
    D3DFORMAT Format; D3DRESOURCETYPE Type; DWORD Usage; D3DPOOL Pool; UINT Size;
    D3DMULTISAMPLE_TYPE MultiSampleType; DWORD MultiSampleQuality; UINT Width, Height;
} D3DSURFACE_DESC;

typedef struct D3DLOCKED_RECT { INT Pitch; void* pBits; } D3DLOCKED_RECT;

// Forward declarations
struct IDirect3D9;
struct IDirect3DDevice9;
struct IDirect3DSurface9;

// ============================================================
// IDirect3D9
// ============================================================
typedef struct IDirect3D9Vtbl {
    HRESULT (__stdcall *QueryInterface)(IDirect3D9*, const IID&, void**);
    ULONG   (__stdcall *AddRef)(IDirect3D9*);
    ULONG   (__stdcall *Release)(IDirect3D9*);
    void*   _pad03; // RegisterSoftwareDevice
    void*   _pad04; // GetAdapterCount
    void*   _pad05; // GetAdapterIdentifier
    void*   _pad06; // GetAdapterModeCount
    void*   _pad07; // EnumAdapterModes
    void*   _pad08; // GetAdapterDisplayMode
    void*   _pad09; // CheckDeviceType
    void*   _pad10; // CheckDeviceFormat
    void*   _pad11; // CheckDeviceMultiSampleType
    void*   _pad12; // CheckDepthStencilMatch
    void*   _pad13; // CheckDeviceFormatConversion
    void*   _pad14; // GetDeviceCaps
    void*   _pad15; // GetAdapterMonitor
    HRESULT (__stdcall *CreateDevice)(IDirect3D9*, UINT, D3DDEVTYPE, HWND, DWORD, D3DPRESENT_PARAMETERS*, IDirect3DDevice9**);
} IDirect3D9Vtbl;

struct IDirect3D9 { IDirect3D9Vtbl* lpVtbl; };

// ============================================================
// IDirect3DDevice9 (partial)
// ============================================================
typedef struct IDirect3DDevice9Vtbl {
    HRESULT (__stdcall *QueryInterface)(IDirect3DDevice9*, const IID&, void**);
    ULONG   (__stdcall *AddRef)(IDirect3DDevice9*);
    ULONG   (__stdcall *Release)(IDirect3DDevice9*);
    HRESULT (__stdcall *TestCooperativeLevel)(IDirect3DDevice9*);
    UINT    (__stdcall *GetAvailableTextureMem)(IDirect3DDevice9*);
    HRESULT (__stdcall *EvictManagedResources)(IDirect3DDevice9*);
    HRESULT (__stdcall *GetDirect3D)(IDirect3DDevice9*, IDirect3D9**);
    HRESULT (__stdcall *GetDeviceCaps)(IDirect3DDevice9*, void*);
    HRESULT (__stdcall *GetDisplayMode)(IDirect3DDevice9*, UINT, void*);
    HRESULT (__stdcall *GetCreationParameters)(IDirect3DDevice9*, D3DDEVICE_CREATION_PARAMETERS*);
    HRESULT (__stdcall *SetCursorProperties)(IDirect3DDevice9*, UINT, UINT, IDirect3DSurface9*);
    void    (__stdcall *SetCursorPosition)(IDirect3DDevice9*, INT, INT, DWORD);
    BOOL    (__stdcall *ShowCursor)(IDirect3DDevice9*, BOOL);
    HRESULT (__stdcall *CreateAdditionalSwapChain)(IDirect3DDevice9*, D3DPRESENT_PARAMETERS*, void*);
    HRESULT (__stdcall *GetSwapChain)(IDirect3DDevice9*, UINT, void*);
    UINT    (__stdcall *GetNumberOfSwapChains)(IDirect3DDevice9*);
    HRESULT (__stdcall *Reset)(IDirect3DDevice9*, D3DPRESENT_PARAMETERS*);
    HRESULT (__stdcall *Present)(IDirect3DDevice9*, const RECT*, const RECT*, HWND, const RGNDATA*);
    HRESULT (__stdcall *GetBackBuffer)(IDirect3DDevice9*, UINT, UINT, D3DBACKBUFFER_TYPE, IDirect3DSurface9**);
} IDirect3DDevice9Vtbl;

struct IDirect3DDevice9 { IDirect3DDevice9Vtbl* lpVtbl; };

// ============================================================
// IDirect3DSurface9 (partial)
// ============================================================
typedef struct IDirect3DSurface9Vtbl {
    HRESULT (__stdcall *QueryInterface)(IDirect3DSurface9*, const IID&, void**);
    ULONG   (__stdcall *AddRef)(IDirect3DSurface9*);
    ULONG   (__stdcall *Release)(IDirect3DSurface9*);
    void*   _pad03_14[12];
    HRESULT (__stdcall *GetDC)(IDirect3DSurface9*, HDC*);
    HRESULT (__stdcall *ReleaseDC)(IDirect3DSurface9*, HDC);
} IDirect3DSurface9Vtbl;

struct IDirect3DSurface9 { IDirect3DSurface9Vtbl* lpVtbl; };
