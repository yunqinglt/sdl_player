# sdl-player

该工具是在 Windows/Linux PC 上模拟嵌入式 framebuffer 的 SDL2 虚拟屏幕。
平台无关的 UI 渲染层已经拆为独立仓库
[`yunqinglt/treelike_ui`](https://github.com/yunqinglt/treelike_ui)，同一套像素、
脏区和控件绘制代码可以交给 MCU 屏幕驱动使用。

## 获取源码

单独使用播放器时：

```sh
git clone git@github.com:yunqinglt/sdl_player.git
cd sdl_player
```

在 `yunqinglt/stm32_in_c` 主仓库中使用时，播放器和 UI 是两个同级子模块：

```sh
git submodule update --init --recursive
cd tools/sdl-player
```

## 构建与运行

Linux 推荐先安装系统 SDL2：

```sh
sudo apt install cmake ninja-build libsdl2-dev
cmake --preset linux-debug
cmake --build --preset linux-debug
ctest --preset linux-debug
./out/build/linux-debug/sdl-player
```

Windows 可选 Visual Studio 2019 或 2022：

```powershell
cmake --preset windows-vs2022
cmake --build --preset windows-vs2022-debug
ctest --preset windows-vs2022-debug
.\out\build\windows-vs2022\Debug\sdl-player.exe
```

在主仓库布局中，CMake 会优先使用兄弟目录 `../treelike-ui`。单独 clone
`sdl_player` 时，CMake 从 GitHub 获取并固定到已验证的 `treelike_ui` 提交；也可通过
`SDL_PLAYER_TREELIKE_UI_SOURCE_DIR` 显式指定本地 checkout。像素格式由播放器配置并以
PUBLIC compile definition 传播，保证两边的 `pixel_t` ABI 一致。

CMake 会优先使用系统、vcpkg 或 MSYS2 提供的 `SDL2::SDL2`。找不到时，
默认通过 FetchContent 下载 SDL 2.30.11 并参与构建；离线环境可设置
`SDL2_DIR`，也可用 `-DSDL_PLAYER_FETCH_SDL2=OFF` 禁止下载并获得明确报错。
Windows 动态库会在构建后复制到 exe 旁边，不再依赖固定磁盘路径或架构目录。

运行时按 `SPACE` 切换画面，按 `ESC` 退出。自动化冒烟测试可以使用：

```sh
SDL_VIDEODRIVER=dummy ./out/build/linux-debug/sdl-player \
  --phase 3 --frames 10 --ui-tree-debug
```

`--phase` 可选 0 到 8；前四项覆盖 framebuffer、脏区与 UI 树，后五项是
从 ESP32 示例移植的纯软件效果。

### UI Tree 调试

`--ui-tree-debug` 专门观察 phase 3 的临近分组。参数可以和任意 phase 一起解析，
但只有 phase 3 会产生下面的调试行为：

- 两个或更多控件形成临近组时，在普通控件全部画完后，以 overlay 方式补画该组的
  完整红色外框；单独控件没有组外框。
- 组边界是各成员经过根 surface 裁剪后的紧包围矩形，不是向外扩张 40 像素的
  threshold halo。
- 两个矩形只有在水平和垂直两个轴上的 gap 都严格小于 40 时才直接临近；任一轴的
  gap 恰好等于 40 都不会直接成组。
- 分组使用并查集求传递闭包，所以 A 临近 B、B 临近 C 时，A、B、C 属于同一组，
  即使 A 与 C 本身不满足直接临近条件。
- 命令行在进入 phase 3 时输出一次 `snapshot`，此后只在成员拓扑发生 `join`、
  `split` 或 `regroup` 时输出 `UI-TREE` 日志。控件仅移动、组成员不变时不会逐帧刷屏。

默认运动轨迹在第 8 tick 首次合并控件 0 和 1；对应输出片段为：

```text
[INFO] UI-TREE tick=8 event=join groups=2 threshold=40
[INFO] UI-TREE group=0 kind=proximity members=[0,1] bounds=(112,104 454x306)
```

Windows PowerShell 的等价无头运行方式是：

```powershell
$env:SDL_VIDEODRIVER = 'dummy'
.\out\build\windows-vs2022\Debug\sdl-player.exe `
  --phase 3 --frames 10 --ui-tree-debug
Remove-Item Env:SDL_VIDEODRIVER
```

dummy video driver 不显示窗口，适合通过 `UI-TREE` 输出验证自动成组；要直接观察红框，
去掉 `SDL_VIDEODRIVER=dummy` 后正常启动即可。

## 结构与依赖边界

```text
main.c (PC 入口与帧循环)
  ├─ app/demo.c + esp32_effects.c     无平台的示例状态与画面生成
  │    └─ ../treelike-ui/             独立仓库与 CMake target
  │         └─ ui/
  │              ├─ ui_surface.*      framebuffer、裁剪、脏矩形
  │              └─ ui_drawer.*       控件 buffer 树与基础 widget 绘制
  └─ platform/sdl/sdl_display.*       Windows/Linux 窗口、事件、时钟、texture
       ├─ treelike_ui                 只读取 framebuffer/脏区接口
       └─ SDL2                        唯一允许包含 SDL.h 的模块
```

构建使用 `treelike_ui::treelike_ui`、`demo_core`、`sdl_platform` 三个独立
library target。UI 单元测试归属 `treelike_ui` 仓库且不链接 SDL；播放器仓库保留
命令行和 SDL 无头测试。旧的
`Display` 同时拥有 framebuffer 和 SDL 对象、`drawer` 又读取 Display 内部字段的
双向耦合已经移除。

`../treelike-ui/ui/experimental/` 保留了原先残缺草稿里的 object tree、event、animation 和
固定块池方向，但不加入正式构建；正式化之前仍需统一生命周期和调度模型。

## 已发现的后续工作

1. 当前脏区是所有修改区域的包围矩形。两个相距很远的小控件会导致中间区域也
   被上传；下一步应改为固定 tile bitset 或小型脏矩形列表，并设置“超过面积阈值
   就整屏刷新”的退化策略。
2. UI 示例每帧重建临时 buffer 树。实际 UI 应采用 retained tree，只在布局或层级
   改变时重建，并把控件自身的 dirty 状态跨帧保存。
3. 高层 `UiObject` 的事件捕获/冒泡、焦点、z-order、裁剪栈和动画 scheduler 尚未
   定型；这也是 `treelike_ui` 的 `ui/experimental` 暂不进入公共 API 的原因。
4. ESP32 effects 仍使用进程级静态内存，缺少初始化失败返回值与 shutdown；若它们
   要成为库 API，应改为显式 context 生命周期。
5. CI 还应覆盖 Windows MSVC 构建及 RGB565/RGB888 矩阵，并由真实 Windows runner
   持续验证工具链、路径和动态库部署。
6. 输入层目前只映射 ESC/SPACE。要测试触摸 UI，应定义平台无关的 pointer/key
   event，再由 SDL、实体按键和触摸驱动分别转换。
