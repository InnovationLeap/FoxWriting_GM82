# FoxWriting_GM82

> 语言 / Language: [English](README.en.md)

GameMaker 8.2（社区版）可用的 **FoxWriting 文字渲染插件**（GM8.2 重编译分支）。

原版 FoxWriting 插件在 GM8.1+ 因 GMAPI 变更无法使用，本仓库基于修改版源码重写并编译，保持原有导出函数名与 GML 调用接口不变，
仅替换底层渲染实现以适配 GM8.2 + gm82dx9（D3D9）环境。

> DLL 文件名固定为 **`FoxWriting_GM82.dll`**，该名称硬编码在 GML 脚本
> `fw_init.gml` 中（`external_define` 引用），切换分支时只替换此 DLL，无需改动任何 GML。

---

## 当前方案（`solution/d3d-hook` 分支，活跃分支）

文字通过 **GDI+ 栅格化 → 上传到 D3D9 纹理 → 在 Present 钩子中绘制全屏透明四边形**
的方式合成到游戏画面，全程处于 D3D 管线内，无闪烁。

核心流程（`FoxWriting_GM82/FoxWriting.cpp`）：

1. **`FWInit`** 启动 GDI+，通过 `FindGameWindow()` 找到游戏窗口，
   随后调用 `TryFindD3D9Device()`。
2. **`TryFindD3D9Device`**：
   - 检查 `d3d9.dll` 已加载（`GetModuleHandleA("d3d9.dll")`）。
   - 直接读取 gm82dx9 扩展存放在固定地址 **`0x6886a8`** 的 D3D9 设备指针
     （`gm82vp.exe` 关闭了 ASLR，地址恒定）。
   - 用 `AddRef/Release` 验证指针有效性，再用 SEH（`__try/__except`）包裹读取，异常安全。
   - 通过 `HookPresentVtable` 将 `IDirect3DDevice9::Present` 的 **vtable[17]** 替换为
     `Present9Hook`（旧代码误用 vtable[15]=GetNumberOfSwapChains，是个 bug）。
3. **`FWDrawText*`** 系列函数不直接绘制，而是把文字放入 `g_textQueue` 队列。
4. **`Present9Hook`**：在原始 Present 之前，调用 `RenderTextToBackbuffer`：
   - `GetBackBuffer` 取得后备缓冲尺寸；
   - 用 GDI+ 把队列文字渲染到一块内存 `Bitmap`（A8R8G8B8，含黑色描边）；
   - `LockRect(DISCARD)` 把位图拷入动态 D3D9 纹理 `g_textTexture`；
   - `DrawTextQuad` 以全屏透明混合（`ONE / INVSRCALPHA`，GDI+ 32bpp ARGB 为预乘 alpha）
     把这个纹理作为 Quad 叠在画面上；所有被改动的设备状态在绘制后恢复。
5. 队列清空，再调用原始 Present 提交。

这种“GDI+ → 纹理 → Quad”的管线避免了在 D3D 表面上调用 `GetDC` /
`GetRenderTargetData` / `StretchRect`（这些在部分 GPU 驱动——如 Win11 24H2 的 Intel
核显——上会失败或无输出）。

**回退路径**：若找不到 D3D9 设备，`DrawGdiText` 直接用 `GetDC(g_gameWindow)` + GDI+ 绘制
（可能闪烁），`FWPaint` 也会触发该回退。

---

## 分支策略

本仓库有 4 个分支，**所有分支共用同一套 GML 脚本**（`fw_*.gml`、`o_textDrawer.gml`），
切换时只替换 `FoxWriting_GM82.dll`：

| 分支                    | 渲染方案                | 说明                                                  |
| --------------------- | ------------------- | --------------------------------------------------- |
| `solution/d3d-hook`   | D3D9 背缓冲 GetDC/纹理合成 | **当前活跃分支**（上文描述）                                    |
| `solution/gdi-direct` | GDI+ 直画游戏窗口 DC      | `GetDC(g_gameWindow)` + GDI+ `DrawString`，利用 DWM 合成 |
| `solution/gdi-simple` | 最简 GDI 直画           | 同 gdi-direct，但去掉缩放/描边/子类化/调试日志                      |
| `master`              | 基线                  | 历史基线分支                                              |

```bash
cd FoxWriting_GM82
git checkout solution/d3d-hook      # 当前默认分支
# 用 VS2022 打开 FoxWriting_GM82.sln，编译 Release / Win32
# 生成的 FoxWriting_GM82.dll 位于仓库根目录
```

---

## 构建要求

- **Visual Studio 2022**，工具集 `v143`。
- 目标平台 **Win32（32 位）** —— GameMaker 8.x 是 32 位进程，DLL 必须 32 位。
- **无 .NET 依赖**，纯原生 C++（GDI+ / D3D9）。
- 配置：**Release**。
- 产物：`FoxWriting_GM82.dll`（导出函数见 `.def`）。
- 调试日志默认关闭（`g_debugLogEnabled = false`）。需要排查时置为 `true`
  会输出 `FW_debug.log`。

---

## 部署

1. 把编译出的 `FoxWriting_GM82.dll` 复制到游戏可执行文件所在目录。
2. GML 侧通过 `fw_init.gml` 的 `external_define` 调用（DLL 名已硬编码）。

---

## 来源

基于修改版 FoxWriting 源码重写，上游仓库地址：

- **上游（修改版）**：<https://github.com/InnovationLeap/FoxWriting-master>
