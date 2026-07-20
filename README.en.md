# FoxWriting_GM82

> Language / 语言: [中文](README.md)

A **FoxWriting text-rendering plugin** for GameMaker 8.2 (Community Edition) — a recompiled branch targeting GM8.2.

The original FoxWriting plugin stopped working on GM8.1+ due to GMAPI changes. This repository is a rewrite/compilation based on a modified source tree, keeping the original exported function names and GML calling interface intact, while replacing only the underlying rendering implementation to fit the GM8.2 + gm82dx9 (D3D9) environment.

> The DLL filename is fixed as **`FoxWriting_GM82.dll`** and is hard-coded in the GML script
> `fw_init.gml` (referenced via `external_define`). When switching branches, you only replace this DLL — no GML changes are required.

---

## Current approach (`solution/d3d-hook` branch — the active branch)

Text is composited into the game frame via **GDI+ rasterization → upload to a D3D9 texture → draw a full-screen transparent quad inside the Present hook**. The whole pipeline stays inside the D3D pipeline, so there is no flicker.

Core flow (`FoxWriting_GM82/FoxWriting.cpp`):

1. **`FWInit`** starts GDI+ and locates the game window via `FindGameWindow()`, then calls `TryFindD3D9Device()`.
2. **`TryFindD3D9Device`**:
   - Checks that `d3d9.dll` is loaded (`GetModuleHandleA("d3d9.dll")`).
   - Reads the D3D9 device pointer directly from the fixed address **`0x6886a8`**, where the gm82dx9 extension stores it
     (`gm82vp.exe` has ASLR disabled, so the address is constant).
   - Validates the pointer with `AddRef/Release`, and wraps the read in SEH (`__try/__except`) for exception safety.
   - Uses `HookPresentVtable` to replace **vtable[17]** of `IDirect3DDevice9::Present` with
     `Present9Hook` (the old code mistakenly used vtable[15] = `GetNumberOfSwapChains`, which was a bug).
3. The **`FWDrawText*`** family of functions do not draw immediately; instead they enqueue the text into `g_textQueue`.
4. **`Present9Hook`**: before the original Present, it calls `RenderTextToBackbuffer`:
   - `GetBackBuffer` obtains the back-buffer size;
   - GDI+ renders the queued text into an in-memory `Bitmap` (A8R8G8B8, with black stroke);
   - `LockRect(DISCARD)` copies the bitmap into the dynamic D3D9 texture `g_textTexture`;
   - `DrawTextQuad` overlays this texture as a quad using full-screen alpha blending
     (`ONE / INVSRCALPHA`, since GDI+ 32bpp ARGB is premultiplied alpha);
     all touched device state is restored after drawing.
5. The queue is cleared, then the original Present is called to submit the frame.

This "GDI+ → texture → quad" pipeline avoids calling `GetDC` / `GetRenderTargetData` / `StretchRect`
on D3D surfaces — those calls fail or produce no output on some GPU drivers (e.g. Intel integrated
graphics under Windows 11 24H2).

**Fallback path**: if no D3D9 device is found, `DrawGdiText` draws directly with `GetDC(g_gameWindow)` + GDI+
(which may flicker); `FWPaint` also triggers this fallback.

---

## Branch strategy

This repository has 4 branches, and **all branches share the same set of GML scripts**
(`fw_*.gml`, `o_textDrawer.gml`). When switching, you only replace `FoxWriting_GM82.dll`:

| Branch                  | Rendering approach              | Notes                                                          |
| ----------------------- | ------------------------------- | -------------------------------------------------------------- |
| `solution/d3d-hook`     | D3D9 back-buffer texture compose | **Current active branch** (described above)                    |
| `solution/gdi-direct`   | GDI+ direct to game-window DC    | `GetDC(g_gameWindow)` + GDI+ `DrawString`, relies on DWM compositing |
| `solution/gdi-simple`   | Minimal GDI direct draw          | Same as gdi-direct, but drops scaling / stroke / subclassing / debug logging |
| `master`                | Baseline                        | Historical baseline branch                                     |

```bash
cd FoxWriting_GM82
git checkout solution/d3d-hook      # current default branch
# Open FoxWriting_GM82.sln with VS2022, build Release / Win32
# The resulting FoxWriting_GM82.dll is placed at the repo root
```

---

## Build requirements

- **Visual Studio 2022**, toolset `v143`.
- Target platform **Win32 (32-bit)** — GameMaker 8.x is a 32-bit process, so the DLL must be 32-bit.
- **No .NET dependency** — pure native C++ (GDI+ / D3D9).
- Configuration: **Release**.
- Output: `FoxWriting_GM82.dll` (exported functions are listed in `.def`).
- Debug logging is disabled by default (`g_debugLogEnabled = false`). Set it to `true` for troubleshooting,
  which writes `FW_debug.log`.

---

## Deployment

1. Copy the built `FoxWriting_GM82.dll` into the game executable's directory.
2. On the GML side it is invoked via `external_define` in `fw_init.gml` (the DLL name is already hard-coded).

---

## Source

This is a rewrite based on a modified FoxWriting source tree. Upstream repository:

- **Upstream (modified)**: <https://github.com/InnovationLeap/FoxWriting-master>
