# sdl-player

该工具是在 Windows/Linux PC 上模拟嵌入式 framebuffer 的 SDL2 虚拟屏幕。
UI 渲染层不依赖 SDL，因此同一套像素、脏区和控件绘制代码可以交给 MCU
屏幕驱动使用。

## 构建与运行

Linux 推荐先安装系统 SDL2：

```sh
sudo apt install cmake ninja-build libsdl2-dev
cd tools/sdl-player
cmake --preset linux-debug
cmake --build --preset linux-debug
ctest --preset linux-debug
./out/build/linux-debug/sdl-player
```

Windows 可选 Visual Studio 2019 或 2022：

```powershell
Set-Location tools/sdl-player
cmake --preset windows-vs2022
cmake --build --preset windows-vs2022-debug
ctest --preset windows-vs2022-debug
.\out\build\windows-vs2022\Debug\sdl-player.exe
```

CMake 会优先使用系统、vcpkg 或 MSYS2 提供的 `SDL2::SDL2`。找不到时，
默认通过 FetchContent 下载 SDL 2.30.11 并参与构建；离线环境可设置
`SDL2_DIR`，也可用 `-DSDL_PLAYER_FETCH_SDL2=OFF` 禁止下载并获得明确报错。
Windows 动态库会在构建后复制到 exe 旁边，不再依赖固定磁盘路径或架构目录。

运行时按 `SPACE` 切换画面，按 `ESC` 退出。自动化冒烟测试可以使用：

```sh
SDL_VIDEODRIVER=dummy ./out/build/linux-debug/sdl-player --phase 3 --frames 10
```

`--phase` 可选 0 到 8；前四项覆盖 framebuffer、脏区与 UI 树，后五项是
从 ESP32 示例移植的纯软件效果。

## 结构与依赖边界

```text
main.c (PC 入口与帧循环)
  ├─ app/demo.c + esp32_effects.c     无平台的示例状态与画面生成
  │    └─ ui/
  │         ├─ ui_surface.*           framebuffer、裁剪、脏矩形
  │         └─ ui_drawer.*            控件 buffer 树与基础 widget 绘制
  └─ platform/sdl/sdl_display.*       Windows/Linux 窗口、事件、时钟、texture
       ├─ ui_surface.h                只读取 framebuffer/脏区接口
       └─ SDL2                        唯一允许包含 SDL.h 的模块
```

构建也使用三个独立 target：`ui_core`、`demo_core`、`sdl_platform`。因此
`ui_core` 的单元测试不链接 SDL，能直接检查平台隔离是否被破坏。旧的
`Display` 同时拥有 framebuffer 和 SDL 对象、`drawer` 又读取 Display 内部字段的
双向耦合已经移除。

`ui/experimental/` 保留了原先残缺草稿里的 object tree、event、animation 和
固定块池方向，但不加入正式构建；正式化之前仍需统一生命周期和调度模型。

## 已发现的后续工作

1. 当前脏区是所有修改区域的包围矩形。两个相距很远的小控件会导致中间区域也
   被上传；下一步应改为固定 tile bitset 或小型脏矩形列表，并设置“超过面积阈值
   就整屏刷新”的退化策略。
2. UI 示例每帧重建临时 buffer 树。实际 UI 应采用 retained tree，只在布局或层级
   改变时重建，并把控件自身的 dirty 状态跨帧保存。
3. 高层 `UiObject` 的事件捕获/冒泡、焦点、z-order、裁剪栈和动画 scheduler 尚未
   定型；这也是 `ui/experimental` 暂不进入公共 API 的原因。
4. ESP32 effects 仍使用进程级静态内存，缺少初始化失败返回值与 shutdown；若它们
   要成为库 API，应改为显式 context 生命周期。
5. 还应在 CI 中增加 Windows MSVC 构建，以及 RGB565/RGB888 的矩阵测试。本次仅在
   Linux 实际运行，Windows 路径已消除硬编码但仍需要真实 Windows runner 验证。
6. 输入层目前只映射 ESC/SPACE。要测试触摸 UI，应定义平台无关的 pointer/key
   event，再由 SDL、实体按键和触摸驱动分别转换。
