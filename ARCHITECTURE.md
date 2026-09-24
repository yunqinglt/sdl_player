# sdl-player 架构说明与重构分析

## 0. 2026-09-23 仓库拆分说明

平台无关 UI 已拆为独立仓库 `yunqinglt/treelike_ui`。在
`yunqinglt/stm32_in_c` 的标准布局中，本仓库位于 `tools/sdl-player`，UI 仓库位于
兄弟目录 `tools/treelike-ui`；本仓库的 CMake 优先使用该本地 checkout，单独 clone 时
则 FetchContent 固定的 UI 提交。本文后续的 `player_conf.h`、`ui/...` 和
`tests/ui_core_tests.c` 均指 `treelike_ui` 仓库内路径，其余路径属于本仓库。

拆分只改变仓库和构建边界，没有改写 C/C++ 源码。下文对 2026-08-28 重构的分析仍然
有效；涉及旧主仓库提交 ID 的位置同时给出拆分后可复核的提交。

## 1. 分析范围与结论

本文分析原主仓库 2026-08-28 的提交
`f25d3c146e1a219ea500a48633a19d7a06a26847`；它在本仓库保留历史中对应
`48d54e6`（`Updated SDL player.`），并与原提交的直接父提交
`a088a24c3f9303c8df53b3484ac348eadc287824` 比较。该提交在
`tools/sdl-player` 中涉及 34 个文件，约新增 1855 行、删除 2676 行。

旧文件已经从工作树删除。本文标注“旧版”的行号时，内容可用下面的命令复核：

```sh
git show 48d54e6^:main.c
git show 48d54e6^:display.c
git show 48d54e6^:drawer.c
```

这次提交不是简单地改名或搬文件，而是一次分层重构。新版可以概括为：

> 以 `UiSurface` 作为跨平台 framebuffer 契约，把纯像素/UI 代码、演示应用、
> SDL 平台资源和 PC 主循环分开，再由 CMake target 固化依赖方向。

它现在是一个“可移植 framebuffer/UI 核心 + SDL 模拟器 + 程序生成演示”的小型分层
应用。虽然目录名叫 `sdl-player`，当前并没有媒体文件解码、音频、音视频同步或资源加载
流水线，因此不应把它理解成传统意义上的音视频播放器。它更准确的定位是嵌入式屏幕和
软件渲染效果的 PC 模拟器。

新版已经解决了若干确定的正确性、内存和构建问题，也建立了比旧版清楚得多的模块边界；
但 retained UI、通用输入、多个脏矩形、effects 生命周期等仍是明确的后续工作。

## 2. 总体架构

### 2.1 编译目标和依赖方向

当前 CMake target 关系如下：

```text
                         +----------------------+
                         |   sdl-player / main  |
                         | PC 组合入口与帧循环   |
                         +----------+-----------+
                                    |
                         links      |      links
                      +-------------+-------------+
                      |                           |
                      v                           v
              +---------------+          +----------------+
              |   demo_core   |          |  sdl_platform  |
              | app/demo.c    |          | SDL 平台适配器  |
              | esp32_effects |          +-------+--------+
              +-------+-------+                  |
                      |                          | PRIVATE
                      |                          v
                      |                       SDL2
                      |
                      +-------------+-------------+
                                    |
                                    v
                             +-------------+
                             | treelike_ui |
                             | UiSurface   |
                             | UiBuffer/UI |
                             +------+------+
                                    ^
                                    |
                         treelike-ui-tests

          treelike_ui/ui/experimental/*：不进入上述正式构建图
```

对应定义分布在两个仓库的 `CMakeLists.txt`：

- `treelike_ui::treelike_ui` 编译 `ui/ui_surface.c`、`ui/ui_drawer.c`、
  `ui/ui_object_raw.c` 和 `ui/buffer_font_render.c`；启用 Win32/GDI outline backend 时
  另加入 `ui/buffer_font_win32.c`。
- `demo_core` 编译 `app/demo.c` 和 `esp32_effects.c`，依赖 `treelike_ui`。
- `sdl_platform` 编译 `platform/sdl/sdl_display.c`，依赖 `treelike_ui`，并把 SDL2
  保持为私有依赖。
- `sdl-player` 可执行文件只编译 `main.c`，负责把应用与平台后端组合起来。
- `treelike-ui-tests` 由 UI 仓库定义，只链接 `treelike_ui`，测试二进制不链接 SDL。

这是一种“分层 + 平台 Adapter”的设计。它还不是完整的 Ports and Adapters：
`main.c` 仍直接调用 `SdlDisplay` API，并没有抽象的 `DisplayBackend` vtable。当前分层为
MCU 提供了“不引入 SDL 的源码复用边界”，但移植时仍需编写平台后端和组合入口；若目标
不允许动态分配，还要替换 `ui_drawer`/`ui_object_raw` 的 `malloc/calloc` 策略，effects
也有额外限制。

### 2.2 模块职责

| 模块 | 主要职责 | 所有权/边界 |
| --- | --- | --- |
| `treelike_ui: player_conf.h` | 屏幕默认尺寸、`pixel_t`、颜色、日志和 FPS 编译配置 | 不包含 SDL 类型；像素格式会影响所有模块的 ABI |
| `treelike_ui: ui/ui_types.h` | `UiRect` 及交集、并集、空矩形判断 | 统一 UI、裁剪和 dirty 的几何语义 |
| `treelike_ui: ui/ui_surface.*` | framebuffer、stride、像素内存所有权、blit、脏区 | 新架构最核心的跨平台数据契约 |
| `treelike_ui: ui/ui_drawer.*` | framebuffer 子视图树、dirty 传播和渲染调度 | 不创建离屏像素层，所有节点共享同一个 surface |
| `treelike_ui: ui/ui_object_raw.*` | 直接写 `UiBuffer` 的回调、临近分组和调试控件 | 使用调用方拥有的 context；不是 retained `UiObject` 基类 |
| `treelike_ui: ui/buffer_font_render.*` | 点阵、stroke/vector 及可选 TrueType outline 的加载、测量、光栅化与 draw callback | 只写 `UiBuffer`；不是 `UiObject` 派生类；TTF 与 ASCII `TLFNT1` 使用不同 parser |
| `app/demo.*` | 9 个 phase 的状态与分派 | 借用一个 `UiSurface *`，不拥有 SDL 资源 |
| `esp32_effects.*` | plasma/tunnel/moire/fire/bounce 纯软件像素效果 | 无 SDL 依赖，但仍使用进程级静态状态和内存 |
| `platform/sdl/sdl_display.*` | SDL 初始化、窗口、renderer、texture、输入、时钟和 present | 唯一正式包含 `<SDL.h>` 的实现模块 |
| `main.c` | 参数解析、事件循环、帧调度、FPS、错误退出 | PC composition root，不再实现具体画面 |
| `treelike_ui: ui/experimental/` | object/event/animation 与固定块池草案 | 明确不进入正式 target，不能视为现有运行时能力 |

## 3. 核心契约：UiSurface

### 3.1 为什么它是新架构的中心

旧版的核心对象是 `Display`，它同时持有 framebuffer、dirty 状态和 SDL
window/renderer/texture。新版把 CPU 像素面抽成 `UiSurface`：

```c
typedef struct {
    pixel_t *pixels;
    int width;
    int height;
    int stride;
    bool owns_pixels;
    bool has_dirty;
    UiRect dirty;
} UiSurface;
```

见 `ui/ui_surface.h:10-18`。应用和 UI 向 `pixels` 写入，dirty 描述需要提交的范围；
SDL 后端只消费这个契约。由此，像素生成不再需要知道窗口、texture 或 SDL 的矩形类型。

### 3.2 两种像素内存生命周期

`UiSurface` 支持两种创建方式：

- `ui_surface_create()` 分配紧密排列的 framebuffer，设置 `stride == width`，并在分配前
  检查像素数和字节数乘法溢出，见 `ui_surface.c:12-38`。
- `ui_surface_wrap()` 包装调用方拥有的像素内存，允许 `stride > width`，不接管所有权，
  见 `ui_surface.c:41-56`。
- `ui_surface_destroy()` 只释放 `owns_pixels == true` 的内存，随后把结构清零，见
  `ui_surface.c:59-64`。

`wrap()` 是面向 MCU 的关键扩展点：CPU 可寻址、按行线性排列的静态 SRAM/PSRAM
framebuffer，或 CPU 可访问的 DMA backing memory，可以被描述成同一接口。它本身不处理
cache coherency、异步 DMA 生命周期、像素格式转换或非线性显存。还要注意，目前 effects
仍假定紧密排列和编译期固定尺寸，所以这一能力对 `ui_core` 成立，对 effects 尚未完整
成立。

### 3.3 blit 与裁剪

`ui_surface_blit()` 接收源宽、高和源 stride。目标左上角位于屏幕外时，它同步调整
`source_x/source_y`，再按目标 surface 裁剪实际复制宽高，最后逐行 `memcpy`。见
`ui_surface.c:77-117`。

这个接口特意修复了旧 `Display_draw()` 的错误语义：裁剪目标坐标不能只改变目标矩形，
还必须在源图上跳过被裁掉的列/行，并保留原来的源行距。

### 3.4 dirty 协议

dirty 是 UI 核心和显示后端之间的提交协议：

```text
写 framebuffer
    |
    +-- fill --------------------------> 整屏 dirty
    +-- blit --------------------------> 实际复制矩形 dirty
    +-- UiBuffer callback -------------> 节点绝对矩形 dirty
    +-- 直接写 pixels -----------------> 调用方必须显式 mark dirty
                                              |
                                              v
                                      UiSurface.dirty
                                              |
                                              v
                                      platform present
                                              |
                                   仅成功后 clear_dirty
```

`ui_surface_mark_dirty()` 会先把输入矩形裁到 surface 内，再与已有 dirty 取包围矩形，见
`ui_surface.c:120-133`。因此多个修改最终只有一个最小包围矩形，而不是多个独立矩形。

直接操作 `surface->pixels` 时，调用方必须主动标脏。棋盘和渐变完成后调用
`ui_surface_mark_all_dirty()`；effects 直接写完整 framebuffer 后也做同样处理，见
`app/demo.c:38-82,211-229`。这个约束目前靠 API 使用者遵守，并没有写访问器强制保证。

## 4. UiBuffer 树和控件渲染

### 4.1 UiBuffer 不是独立 framebuffer

`UiBuffer` 这个名字容易让人误以为每个控件拥有离屏缓存。实际上它是父 surface
上的矩形视图：

- 根节点的 `pixels` 直接指向 `surface->pixels`，见 `ui_drawer.c:29-43`。
- 子节点只分配节点结构；其像素指针是父指针加 `y * stride + x`，见
  `ui_drawer.c:45-69`。
- 所有层级复用同一个 stride 和同一块像素存储。
- `ui_buffer_destroy_tree()` 只释放节点，不释放 surface 像素，见
  `ui_drawer.c:72-84`。

所以这棵树提供的是局部坐标、空间层次、遍历剪枝和 dirty 传播，而不是 texture layer、
alpha layer 或合成器。

### 4.2 两级脏状态与遍历

每个节点有两个状态：

- `is_dirty`：节点自身的 draw callback 需要执行。
- `child_dirty`：至少一个后代需要处理，但本节点自身不一定要重画。

`ui_buffer_mark_dirty()` 设置自身 `is_dirty`，再沿父链只设置 `child_dirty`，见
`ui_drawer.c:86-97`。`render_node()` 使用深度优先顺序：

1. 当前分支既无自身 dirty 也无子树 dirty 时直接返回。
2. 自身 dirty 时调用 `draw(buffer, context)`。
3. 计算节点在根 surface 中的绝对矩形并合入 surface dirty。
4. 递归子节点。
5. 清除本次处理过的 dirty 状态。

见 `ui_drawer.c:99-120`。这使 UI 节点失效与最终屏幕提交形成一条连续链路，不再需要
外部代码猜测控件写了哪一块 framebuffer。

当前规则仍不是完整的 retained-mode invalidation：父节点重画可能覆盖子节点区域，
但“父 dirty”不会自动令所有干净子节点重画。正式示例的分组容器没有业务 draw
callback，暂时不会触发这一矛盾；启用 `--ui-tree-debug` 时看到的红框也不是容器的普通
callback，而是整棵树正常渲染后的调试 overlay。将来保留跨帧树时仍必须先定义父子覆盖
和失效传播规则。

### 4.3 从平面控件到空间分组树

应用先用 `UiControl` 描述一组平面控件：

```c
typedef struct {
    UiRect bounds;
    UiDrawCallback draw;
    void *context;
} UiControl;
```

`ui_build_render_tree()` 的工作过程是：

1. 两两判断矩形在给定阈值内是否“邻近”。
2. 使用并查集计算邻近关系的传递闭包。
3. 把每个控件裁到根 surface，并求每组包围矩形。
4. 单控件组直接挂到根；多控件组先创建一个容器，再以组内相对坐标创建叶节点。
5. 为所有叶节点安装 draw/context 并标脏。

见 `ui_object_raw.c` 的 `ui_build_render_tree_debug()`。当前算法时间复杂度约为 `O(n²)`，
适合演示中的三个控件，
并不适合直接扩展到很大的动态 UI。分组容器也不拥有独立像素缓存；它只是空间和 dirty
遍历层次，不会自动改善 surface 只有一个 dirty 包围矩形的问题。

这里的“邻近”是二维条件：两个矩形在水平轴和垂直轴上的间隔都必须严格小于
`grouping_threshold`。phase 3 使用阈值 40，因此任一轴的 gap 等于 40 时不会直接合并；
重叠或接触的轴向 gap 记为 0。并查集保留传递关系：若 A 临近 B、B 临近 C，三者会进入
同一组，A 与 C 不需要直接临近。多控件组的 bounds 是所有成员经过根 surface 裁剪后的
矩形并集，也就是成员的紧包围矩形；阈值只参与判定，不会在 bounds 外生成 40 像素 halo。

### 4.4 临近组的调试观察与覆盖层

`--ui-tree-debug` 把“建树时的结构观察”和“渲染后的视觉标记”组合起来，但没有让
`ui_core` 承担命令行输出职责：

1. 调试建树接口在成功完成树后，通过 callback 暴露一次只读 snapshot，包括裁剪后控件
   bounds、控件到组的映射、组 bounds/size 和阈值；这些数组只在 callback 期间有效。
   完全被 root surface 裁掉的控件映射为“无渲染组”，不计入可见组数，也不创建节点。
2. 对其余可见控件，并查集根会归一化为组内最小的可见源控件下标，给日志和比较提供
   稳定组 ID；因此完全离屏且下标更小的控件不会占用可见组的 ID。
3. `Demo` 消费 snapshot，并用成员映射而非不断变化的坐标识别拓扑。进入 phase 3 先输出
   `snapshot`，随后只在 `join`、`split` 或成员重排但组数不变的 `regroup` 时输出
   `UI-TREE` 日志；单纯移动不会产生逐帧坐标噪声。
4. builder 会在创建合成分组节点时显式把该 `UiBuffer` 标记为 group container。普通树
   先按父到子的顺序渲染，之后调试 overlay 以 post-order 只处理带该标记的节点，在其
   绝对 bounds 上补画完整红框并标脏。因此红框位于叶控件之上，不会被靠近边界的控件
   覆盖；overlay 不依赖“是否有多个子节点”等树形状猜测，普通多子节点容器、根节点和
   直接挂根的单控件都不会被误画框。

这个开关可以被命令行统一解析，但只在 phase 3 生效。关闭开关时，分组容器仍是不可见的
空间节点，正常像素输出不受影响。该功能恢复的是临近渲染区的可观察能力，并非逐像素复制
旧 fuzzy demo 的配色：旧 fuzzy 容器使用绿色边框和深绿色填充，当前调试契约明确采用
无填充的红色外框。

### 4.5 Widget 扩展方式

低层 widget 使用统一回调：

```c
typedef void (*UiDrawCallback)(UiBuffer *buffer, void *context);
```

raw 层已经提供调试边框、调试分组、圆角矩形、图片拷贝和 RGB565/RGB888 像素混合，
实现集中在 `ui_object_raw.c`。新增低层 widget 时不需要修改树遍历器，只需要：

1. 定义稳定生命周期的 context。
2. 在 `buffer->width/height` 范围内绘制。
3. 通过 `ui_buffer_mark_dirty()` 触发渲染。

目前树构建器会裁剪越界 `UiControl.bounds`，但不会把“左/上被裁掉多少”传给回调。
对图片或带局部坐标内容的控件，这会把内容原点移动到裁后左上角，而不是真正保留源偏移。
因此 `UiSurface` 的 blit 裁剪已经正确，控件级裁剪语义仍需补充 clip/source-offset 信息。

### 4.6 字体渲染服务不是对象层级

`buffer_font_render` 沿用同一个 `UiDrawCallback` 接缝，但它是无 SDL 的渲染服务，不是
`UiObject` 或 Label 的派生类。内置字体分成两种：

- 5×7、1-bpp ROM 点阵字体，适合小尺寸和资源受限目标；
- 字形源由几何线段组成、按每个目标字号重新变换和光栅化的 scalable stroke/vector
  font。它不是 TrueType outline 引擎，不负责解析 TTF/OTF 文件。

Windows 构建启用 Win32/GDI backend 并导出 `TREELIKE_UI_HAS_TRUETYPE` 时，还会增加
独立的真实 TrueType outline path backend。它与第二项的线段字形并存，选择或回退引擎
时不能把 TTF 文件交给 `TLFNT1` parser，也不能把 stroke font 宣称为 TrueType。

`font_type_t` 描述字体，`text_renderer_t` 是初始化后的只读绑定；调用方拥有
`UiTextRenderContext`，其中保存文本、局部原点、目标像素高度、笔画宽度、间距、颜色和
透明/不透明背景策略。`buffer_font_draw()` 适配 callback 的 `void` 返回约束，把状态和
测量结果回写到 context。直接绑定方式如下：

```c
text_renderer_t renderer;
UiTextRenderContext text = {
    .renderer = &renderer,
    .text = "VECTOR",
    .pixel_height = 31,
    .stroke_width = 3,
    .color = COLOR_YELLOW,
    .opaque = false
};

buffer_font_renderer_init(&renderer, &ui_font_vector_stroke);
UiControl control = {
    .bounds = {410, 280, 180, 90},
    .draw = buffer_font_draw,
    .context = &text
};
```

除内置字体外，`buffer_font_load()` 可以把 `font_type_t.path` 指向的 ASCII `TLFNT1`
文件解析进调用方提供的 `UiFontStorage`；启用 TrueType feature 时，同一入口也可识别
真实 TTF 并交给 outline backend。font、storage 和 path 必须比 renderer 活得更久，
`buffer_font_unload()` 负责显式失效和清零。`TLFNT1` 分支用十六进制行位图或
“线段表 + glyph mask”表达字形，和 TTF parser 保持严格边界。
`font_type_t.addr` 为 SPI Flash 端口保留，本阶段 addr-only 来源返回
`UI_FONT_STATUS_UNSUPPORTED_SOURCE`，不会把整数地址当成本机指针解引用。当前文本路径支持
换行与未知字符 fallback，但不承担 shaping、双向排版或完整 Unicode 字库。

## 5. 应用层：Demo 状态机与 effects

### 5.1 Demo 的职责

`Demo` 保存借用的 `UiSurface *`、phase、帧 tick 和局部弹跳矩形状态，见
`app/demo.h:10-20`。它不包含 SDL 类型。

`demo_set_phase()` 是统一状态转换入口：规范化 phase、重置 tick 和局部弹跳状态、进入
对应 effect，并清空 surface。`demo_next_phase()` 只在此基础上前进一步。见
`app/demo.c:182-200`。

9 个 phase 的实际行为如下：

| phase | 名称 | 写入方式 | dirty 范围 |
| ---: | --- | --- | --- |
| 0 | checkerboard | 直接逐像素生成棋盘 | 整屏 |
| 1 | gradient | 直接逐像素生成渐变 | 整屏 |
| 2 | partial update | 擦除旧矩形，再绘制新矩形 | 切入后的首帧整屏；后续为两个矩形的包围框 |
| 3 | platform-free UI tree | 构造三个移动文字控件的临时 UiBuffer 树 | 当前演示先清背景，所以仍是整屏 |
| 4 | plasma | effects 直接写 surface 像素 | 整屏 |
| 5 | tunnel | effects 直接写 surface 像素 | 整屏 |
| 6 | moire | effects 直接写 surface 像素 | 整屏 |
| 7 | fire | effects 直接写 surface 像素 | 整屏 |
| 8 | bouncing sprite | 程序生成的渐变矩形 | 整屏 |

名称表和分派见 `app/demo.c:202-230`。

### 5.2 phase 3 的真实成熟度

phase 3 每帧执行以下流程：

```text
整屏填背景
  -> 更新三个 MovingBox
  -> 首次进入时从 build tree 加载位图字体和可选 TTF
  -> TTF 失败时回退 TLFNT1 stroke，再失败回退内置 stroke
  -> 在栈上构造 UiControl[] 和字体 renderer/context
  -> 把 buffer_font_draw 直接绑定到三个 UiControl.draw
  -> 分配根节点
  -> 并查集分组并分配临时节点
  -> 可选：把结构 snapshot 交给拓扑观察器
  -> render 普通树
  -> 可选：post-render 红色临近组 overlay
  -> 释放整棵临时树
```

支持字体 API 的 UI target 下，三个透明背景控件分别显示白色点阵 `BITMAP`、黄色
31 px `VECTOR` 和青色点阵 `TREE`。`VECTOR` 在 TrueType feature 与 Fantasque path
同时存在时使用真实 outline，否则使用可缩放 stroke fallback。它们继续使用原来的
bounds、速度和 40 px 阈值，所以 tick 1 snapshot、tick 8 join、tick 10 split 的拓扑
轨迹不变；改变的只是叶节点的 draw/context，不是分组算法或对象继承关系。

两份 `TLFNT1` demo 字体在 configure 时通过 `configure_file(COPYONLY)` 进入 build
tree；父工程提供 `SDL_PLAYER_TRUETYPE_FONT_PATH` 时，Fantasque TTF 也复制到同一资源
目录。编译给 demo 的是对应绝对 build 路径，因此运行时 cwd 无关。路径字体及其
caller-owned storage 以进程期静态对象保存，只在首次使用时解析；打开或校验失败按上述
顺序回退，进程正常退出时通过 `buffer_font_unload()` 释放平台字体资源。

为使 `sdl_player` 独立 checkout 仍可链接其固定的旧 UI 提交 `72e3060`，上述路径由
`TREELIKE_UI_HAS_FONT_RENDER` 选择；旧 target 不导出该 feature macro 时保留圆角控件
回退。这里不使用 `__has_include`，能力声明来自链接 target 的 PUBLIC usage requirement。

因此 phase 3 展示的是“只通过 `UiSurface` 完成 UI 绘制”的调用路径
和临时树的绘制结果；源码依赖隔离的更强证据来自独立 CMake target 和 core test。
它还不是 retained UI。因为每帧先 `ui_surface_fill()`，本 phase 也没有实际获得局部
dirty 上传收益。

命令行启用 `--ui-tree-debug` 后，红框只标记包含至少两个控件的临近组。日志只在初始
snapshot 或拓扑变化时产生；重新进入 phase 3 会使旧 snapshot 失效并输出新的初始状态。
Linux/CI 可用下面的有限帧无头命令覆盖一次自动 join：

```sh
SDL_VIDEODRIVER=dummy ./out/build/linux-debug/sdl-player \
  --phase 3 --frames 10 --ui-tree-debug
```

### 5.3 ESP32 effects 的边界

effects 接收 `pixel_t *` 和应用 tick，不再调用 SDL 时钟或 LCD API，因此在源码依赖上属于
`demo_core`。`demo_init()` 会无条件调用 `Effects_init()`，所以即使只运行 phase 0 一帧，
启动时也会立即尝试分配所有 effect heap，而不是进入 phase 4 到 8 时才按需分配。

默认 800×600 下，它的一次性 heap 大致包括：

- tunnel distance map：480,000 字节；
- tunnel angle map：480,000 字节；
- fire buffer：480,000 字节；
- 合计 1,440,000 字节，约 1.44 MB，不含主 framebuffer 和 SDL texture。

主 framebuffer 在 RGB565 下为 960,000 字节，在 RGB888 下为 1,920,000 字节。因此
CPU 侧的 framebuffer + effect heap 基线分别约为 2.40 MB 和 3.36 MB，尚未计入 SDL
texture、window/renderer 资源以及 phase 3 的临时节点分配。

新版不再像旧 `main.c` 那样额外分配一整屏中转 block；effects 直接写 `UiSurface` 的
framebuffer。但它仍有重要限制：

- LUT、map、fire buffer 和 bounce 状态都是进程级 static，不能自然支持多实例或并发。
- `Effects_init()` 返回 `void`；malloc 失败不可上报，也没有 `Effects_shutdown()`。display
  销毁不会释放这些内存；短进程退出时由 OS 回收，但库化或重复创建/销毁 Demo 时生命周期
  是不完整的。
- 如果 tunnel 两张 map 只成功一张，成功的分配无法回收，初始化又会被标为完成。
- tunnel/fire 分配失败时对应的 effect render 函数提前返回，但 `demo_render()` 仍报告成功
  并标整屏 dirty。
- fire buffer 在 malloc 后不会立即清零，只在 `Effects_enter(EFFECT_FIRE)` 时清零；当前
  Demo 的 phase 转换路径满足这个前提，但调用者若只按头文件执行 init 后直接 render fire，
  会读取未初始化状态。
- 所有效果都使用编译期 `SCREEN_WIDTH/SCREEN_HEIGHT` 和紧密一维索引，不尊重任意
  `UiSurface.width/height/stride`。
- bounce 固定为 60×60；若把编译期屏幕改得更小，位置修正仍可能产生负索引。
- 初始化调用 `srand(12345)`，fire 使用进程全局 `rand()`；这会改写宿主程序的随机数状态，
  也是不可重入、非线程安全的跨模块副作用。

此外，phase 3 的 `MovingBox boxes[]` 是函数内 static，而不是 `Demo` 成员；多个 `Demo`
实例会共享状态，`demo_set_phase()` 也不会真正重置这些 box。这说明应用状态的显式
context 化只完成了一部分。

## 6. SDL 平台适配层

### 6.1 真正的不透明平台对象

`platform/sdl/sdl_display.h` 只声明：

```c
typedef struct SdlDisplay SdlDisplay;
```

SDL window、renderer、texture 和内部 `UiSurface` 只在
`platform/sdl/sdl_display.c:10-16` 的私有结构中出现。整个正式源码中，只有该 `.c`
文件包含 `<SDL.h>`。

创建过程见 `sdl_display.c:36-103`：

1. 初始化 SDL video/timer 子系统。
2. 创建 CPU `UiSurface`。
3. 创建可调整大小的窗口。
4. 优先请求 accelerated + VSync renderer，失败则回退 software renderer。
5. 设置 logical size。
6. 创建与 `pixel_t` 对应的 streaming texture。

所有已检查的资源创建步骤失败时，都会回收已经创建的成员。当前
`SDL_RenderSetLogicalSize()` 的返回值被忽略，不在这项保证内。销毁按 texture、renderer、
window、surface 的逆向顺序执行，再按 `owns_sdl` 标志退出子系统，见
`sdl_display.c:27-34,89,106-111`。

### 6.2 present 的数据路径

`sdl_display_present()` 见 `sdl_display.c:118-145`：

```text
UiSurface.dirty
    -> 转换为私有 SDL_Rect
    -> 计算 dirty 首像素地址和完整 surface pitch
    -> SDL_UpdateTexture：只上传 dirty 子矩形
    -> SDL_RenderClear / SDL_RenderCopy：复制整张 texture
    -> SDL_RenderPresent
    -> 成功后清 UiSurface dirty
```

局部 dirty 优化的是 CPU framebuffer 到 SDL texture 的上传；texture 到窗口的渲染仍是
整张 texture。update 或已检查的 render 操作失败时函数返回 `false`，dirty 保留，主循环
会返回失败。旧版即使 texture update 已记录失败也会继续并清 dirty，render 错误则完全
没有检查。

### 6.3 输入和时间

SDL 输入当前被压缩成：

- `SDL_DISPLAY_EVENT_NONE`；
- `SDL_DISPLAY_EVENT_QUIT`：窗口关闭或 ESC；
- `SDL_DISPLAY_EVENT_NEXT_DEMO`：SPACE。

这样 `main.c` 不再认识 `SDL_Keycode`，但 `NEXT_DEMO` 已经是应用动作，不是可复用的通用
输入事件。鼠标、触摸、普通键盘、焦点、捕获/冒泡都还没有正式模型。

其余 SDL event（包括 resize/expose）会被轮询函数直接丢弃。当前所有 phase 每帧都会产生
dirty，通常仍会重绘；未来 retained UI 可能出现“surface 无 dirty，于是 present 直接返回，
窗口 expose 却未重绘”的组合问题。

帧循环使用 SDL tick 统计 FPS 和补 sleep；`Demo.tick` 则只是每次 render 增加 1，
并非 wall-clock delta。发生掉帧时动画会随帧率变慢。`sdl_display_microseconds()` 已提供但
当前未使用。手工限帧采用整数毫秒 `1000 / TARGET_FPS`，属于粗粒度 pacing。加速 renderer
同时请求了 VSync，因此实际 pacing 受 VSync 和手工 sleep 中较慢的一方约束；回退到软件
renderer 后则没有这层 VSync 保证。

## 7. 主循环和端到端运行链路

新版 `main.c` 从旧版约 553 行缩到 154 行，主要工作是组合与调度：

1. 解析 `--phase`、`--frames` 和 `--ui-tree-debug`；`--frames` 限制运行帧数，可配合
   `SDL_VIDEODRIVER=dummy` 做 headless/CI 有限帧 smoke test。调试开关传给 Demo，
   非 phase 3 时保持 armed 但不产生红框或拓扑日志。
2. 创建 `SdlDisplay`。
3. 把 display 内部 surface 借给 `Demo`。
4. 每帧拉取抽象事件。
5. 调用 `demo_render()` 写 CPU framebuffer 和 dirty。
6. 调用 `sdl_display_present()` 消费 dirty。
7. 统计 FPS、补帧延时。
8. 销毁 display，并根据渲染结果返回成功或失败。

完整主循环见 `main.c:66-153`。双向信息流可以总结为：

```text
控制流：SDL event -> SdlDisplayEvent -> main -> Demo phase/state

像素流：Demo/effect/widget -> UiSurface.pixels -> dirty
                                      |
                                      v
                         SDL texture 局部上传 -> window

时间流：SDL clock -> main 的 FPS/pacing
         frame count -> Demo/effect animation tick
```

本项目源码没有显式创建线程；应用级事件、更新、绘制和 present 都在主循环串行执行。
SDL 或图形驱动内部是否使用线程不属于本项目保证。

## 8. 配置、ABI 与构建设计

### 8.1 像素格式

`player_conf.h` 支持 RGB565 和用 32 位存储的 RGB888：

- RGB565：`pixel_t == uint16_t`；
- RGB888：`pixel_t == uint32_t`。

CMake 通过 `SDL_PLAYER_PIXEL_FORMAT` 校验取值，再传给 UI 仓库的
`TREELIKE_UI_PIXEL_FORMAT`；`treelike_ui` 把选中的宏作为 `PUBLIC` compile
definition。`pixel_t` 会改变结构体 ABI、stride
字节数和 texture 格式，因此必须传播给 demo、platform 和 executable；这里使用 PUBLIC
是正确且重要的设计。

SDL texture 常量不再出现在公共配置中，而由 `sdl_display.c:18-25` 私有映射。

### 8.2 UI 与 SDL 依赖发现

UI 依赖按以下顺序解析：父工程已存在的 `treelike_ui::treelike_ui` target、
`SDL_PLAYER_TREELIKE_UI_SOURCE_DIR` 显式路径、主仓库的 `../treelike-ui` 兄弟目录，
最后是固定到完整 commit ID 的 GitHub FetchContent。这样主仓库使用两个被 gitlink
分别锁定的本地子模块，独立 clone 也能复现同一 UI 版本。

SDL2 依次尝试：

1. SDL2 config package；
2. CMake `FindSDL2` module；
3. 兼容只提供旧变量的 Find module；
4. 默认通过 FetchContent 获取 SDL 2.30.11。

离线环境可设置 `SDL_PLAYER_FETCH_SDL2=OFF` 并提供 `SDL2_DIR`；找不到依赖时会给出明确
错误。当选中的动态 target 恰为 `SDL2::SDL2` 时，Windows DLL 从 imported target 的实际
位置复制，不再拼接固定 x86/x64 路径；旧变量兼容分支不会自动复制，静态 target 则不需要
DLL。见 `CMakeLists.txt:16-62,107-116`。

`CMakePresets.json` 提供 Linux Ninja、VS2019 x64、VS2022 x64 及对应 build/test preset，
不再写死某台机器的编译器绝对路径。

### 8.3 测试边界

`treelike_ui` 仓库的 `tests/ui_core_tests.c` 当前覆盖：

- 矩形交集和并集；
- 负目标坐标 blit 的正确源偏移；
- dirty 合并与 surface 边界裁剪；
- 不链接 SDL 的 UiBuffer 建树和绘制；
- 调试 snapshot 的可见组元数据、稳定组 ID，以及完全离屏控件的“无渲染组”语义；
- gap 恰好等于阈值时不分组，以及并查集的传递成组；
- post-render 红色 group overlay，包括完整边框、叶控件内容保留、显式 container 标记，
  以及单控件或普通多子节点容器不被误画框。
- 5×7 golden mask、2× 点阵缩放、UTF-8 fallback/换行、任意 stride 与四边裁剪；
- stroke/vector 在多个非固定字号下的 coverage、锚点和 measure/render 一致性；
- `TLFNT1` path load/unload、addr-only 拒绝；启用 Win32 backend 时还覆盖 Fantasque
  outline 的 metrics、抗锯齿像素、大小写、裁剪和资源生命周期。

CTest 还定义了以下 CLI smoke：`--help` 暴露 `--ui-tree-debug`；SDL dummy driver 下
首帧出现 `snapshot`、10 帧内出现 `join` 和 `split`；默认关闭调试时不出现 `UI-TREE`。
当 target 导出 `TREELIKE_UI_HAS_TRUETYPE=1` 且 CMake 提供 TTF 路径时，还要求输出
`UI-FONT source=truetype family=Fantasque Sans Mono`，从应用层证明没有静默 fallback。
这些测试的 timeout 均为 10 秒；dummy driver 的无头结果仍不能替代真实窗口交互检查。

这组测试能对“UI target 不应链接 SDL”提供回归保护，但不能单凭链接关系禁止只引用 SDL
头类型却不调用符号的源码依赖。拆分后的 UI 仓库可以在没有 SDL 时独立配置、构建和运行
core tests。当前还没有 effects、分配失败、任意 stride、输入、SDL 生命周期或
RGB565/RGB888 矩阵的完整
覆盖；现有 10 帧 CLI smoke 也不能替代真实窗口、真实 Windows runner 和平台生命周期测试。

## 9. 旧代码存在的问题

以下把“确定缺陷”“潜伏风险”和“架构债务”分开，避免把尚未触发的风险描述成已经发生
的崩溃。

### 9.1 已有代码可以直接证明的缺陷

#### 9.1.1 五个 effects 实际不可达

旧 `main.c:8-11,23-24` 宣称有 9 个 phase；但 SPACE 在旧 `main.c:435` 使用
`(phase + 1) % 4`，switch 在 `:444-528` 也只有 0 到 3，`Effects_init()` 还在
`:372-373` 被注释。于是 `esp32_effects.c` 虽被 CMake 编译，phase 4 到 8 永远不会执行。

新版用 `DEMO_PHASE_COUNT == 9`、`demo_set_phase()/demo_next_phase()` 和统一 dispatcher
打通全部 phase，见 `app/demo.h:10` 和 `app/demo.c:170-230`。

#### 9.1.2 活动演示每帧泄漏内存

旧 phase 3 每帧调用 `run_smart_container_frame()`，在旧 `main.c:180` 用 `calloc` 分配
三个 `ui_control_t`，但函数末尾只 `free_buffer_tree(root)`，没有 `free(controls)`。
未启用的 fuzzy demo 还在每帧 `calloc` style，同样不释放且不检查分配结果。

新版把 controls 放在栈上，把 style 设为 static const；并查集的临时数组则集中在
`ui_build_render_tree()` 的 `done` 路径释放。

#### 9.1.3 旧局部弹跳存在确定的周期性 framebuffer 越界写（高严重性）

旧 phase 2 的可见矩形大小是 `rect_w × rect_h`，但实际擦除和 blit 的临时块是
`(rect_w + 4) × (rect_h + 4)`，见旧 `main.c:390-396`。位置反弹却只用较小的
`rect_w/rect_h` 判断边界，见 `:498-502`。当矩形到达右边或下边时，下一帧在
`:484-493` 直接擦除 `sub_w × sub_h`，没有裁剪，会跨行，底边时还会越过 framebuffer
末尾。这不是仅有理论可能：初始 `y=0, dy=2`，约第 225 帧到达 `y=450`，下一帧会按
`y=450..603` 擦除；800×600 framebuffer 的 `y=600..603` 已经越界，RGB565 下最远约
写出 6 KB。到达右边界时，每行还会额外写 4 个像素并污染下一行。

新版 phase 2 使用 `fill_surface_rect()`，每次写入都先与 surface 求交，并把位置钳制到
与实际绘制尺寸一致的边界，见 `app/demo.c:84-131`。

#### 9.1.4 Display_draw 的负坐标裁剪复制错误

旧 `display.c:185-206` 只把目标负坐标钳到 0，却仍从 `block[0]` 开始读，并把裁后的
`region_w` 当作源 stride。例如把 4×3 源贴到 x=-2，正确结果应从每行第 3 个像素开始，
旧实现却从第 1 个像素开始并每 2 个像素换行。

新版 `ui_surface_blit()` 显式接收源 stride，保留 source offset；
`tests/ui_core_tests.c:28-49` 对 x=-2 的像素结果和 dirty 矩形做了精确断言。

#### 9.1.5 显示失败被吞掉并丢失 dirty

旧 `Display_present()` 返回 `void`。旧 `display.c:309-322` 在 `SDL_UpdateTexture` 失败后
只打印日志，仍继续 render，并无条件清除 dirty；`SDL_RenderClear/Copy` 的错误完全未检查。

新版 `sdl_display_present()` 返回 `bool`，update 或已检查的 render 步骤失败都会保留
dirty 并向主循环上报；只有完整成功后才清 dirty，见 `sdl_display.c:118-145` 和
`main.c:117-123`。

#### 9.1.6 干净 checkout 无法完成配置/构建

旧 CMake 有两项本机假设和一项仓库完整性错误：

- `CMakeLists.txt:13-28` 固定使用源码树下 `SDL2-2.30.4/lib/x64|x86` 的 Windows
  `.lib/.dll`，但仓库 `.gitignore` 又排除了整个目录。
- `CMakeLists.txt:44-46` 引用 `54A99503218B6635058DAE67B9FF007B_resized.c`，该文件不在
  父提交的 Git tree 中。
- 旧 `CMakePresets.json:14-15` 把 `cl.exe` 写死到某台机器的 `D:/Microsoft Visual
  Studio/...`。

所以旧版依赖一个“恰好准备好的开发机目录”；干净 checkout 会因缺失源文件而无法完成
CMake 配置，Linux 依赖路径也不成立。新版改为 package/FetchContent，移除 phase 3 对
未入库图片源文件和符号的依赖，改用程序绘制控件，并提供跨平台 preset。需要区分的是，
程序化的 bounce effect 在父提交中已经存在；本次重构只是通过新 dispatcher 让它可达。

#### 9.1.7 加速 renderer 不可用时直接退出

旧 `display.c:106-118` 只请求 accelerated + VSync renderer，失败就销毁退出。新版先
尝试加速 renderer，再回退 software renderer；结合 `--frames` 和 SDL dummy driver，
才具备自动化 smoke test 的基础。

### 9.2 潜伏缺陷与静默腐化

- 旧 `drawer.c:170-188` 没有清 `child_dirty`；那行代码被注释。当前演示每帧销毁树，
  所以主要表现为通用 API 的潜伏性能问题；一旦保留树，祖先会永久重复遍历。新版在
  `render_node()` 末尾清状态。
- 旧 `build_smart_render_tree()` 使用多组固定 `[64]` 数组，却不验证 `count <= 64`，见
  旧 `drawer.c:332-378`。当前只传 3，不是现行崩溃；但 API 对更大输入会越界。新版按
  `size_t count` 动态分配。
- 旧 `add_drawer.h` 有非法的函数指针参数和未完成表达式；旧
  `new_extract/index_block.c` 把像素块写成指针数组，又在 `pixel_t **` 与 `pixel_t *`
  之间错误赋值。这些草稿没有进入 CMake，因此不会破坏当时的 executable，却形成
  “仓库中看似存在、实际不可编译”的静默腐化。新版把原有意图重写、整理为语法完整的
  草案，隔离到 `ui/experimental/`，并明确声明暂不进入正式 API。

### 9.3 架构债务

| 旧版问题 | 直接后果 | 新版处理 |
| --- | --- | --- |
| `Display` 公共头公开 SDL window/renderer/texture、framebuffer、`SDL_Rect`，同时又声称自己 opaque | 抽象名不副实，UI 无法独立于 SDL | `SdlDisplay` 真正不透明；framebuffer/dirty 下沉为 `UiSurface` |
| `drawer.c` 直接读取 `disp->width/height/framebuffer` | UI 直接依赖混合了 SDL 细节的 Display，无法在不引入 SDL 头/依赖时独立复用 | UI 只依赖 `UiSurface`；`<SDL.h>`、SDL 具体类型/API 只在平台 `.c` 中出现 |
| 旧 `main.c` 同时处理 SDL、Timer、FPS、像素算法、demo、UI 树和内存 | 553 行入口承担过多变化原因 | main 只做组合和调度，demo/platform/UI 各自成层 |
| UI 节点 dirty 与 Display dirty 没有连接，phase 3 最后只能 `Display_markFullDirty()` | 树的局部失效不能驱动局部上传 | 节点 render 已能把绝对 bounds 合入 surface dirty；当前 phase 3 因整屏 fill 仍未获得局部收益 |
| `player_conf.h` 暴露 SDL pixel 常量，且格式冲突时没有明确报错 | 公共配置携带平台知识，像素 ABI 容易不一致 | 配置只定义 `pixel_t`；SDL 格式私有映射；CMake 校验并 PUBLIC 传播 |
| display、drawer、add_drawer、new_extract 存在多套矩形/树/块模型 | 概念重复，正式与草稿边界不清 | 正式 API 收敛到 `ui/`，未定方向隔离到 `experimental/` |
| 没有 CTest、有限帧运行或独立 target | 裁剪、依赖泄漏和不可达 phase 长期隐藏 | 新增 core tests、`--phase/--frames`、分层 targets 和 warnings |

## 10. 新架构仍未解决的问题

以下不能描述成“本次已经完成”：

1. **dirty 仍是单一包围矩形。** 两个相距很远的小控件会把中间区域一起上传；需要
   小型矩形列表或 tile bitset，并设置面积阈值退化成全屏。
2. **UI 树仍是每帧重建。** phase 3 先全屏填充，再反复进行节点和并查集 heap 分配；
   retained tree、跨帧 widget dirty 和稳定布局尚未落地。
3. **父子重画、z-order 和 clip 规则未定型。** 共享 framebuffer 不是自动合成模型；
   越界控件也缺少 callback source offset。
4. **effects 生命周期不完整。** 需要显式 `EffectsContext`、失败返回值、shutdown，以及
   width/height/stride 参数；当前实现不可重入且对小屏有风险。
5. **输入只完成了最小映射。** 还没有平台无关的 pointer/key event、焦点、捕获/冒泡。
6. **动画以帧计数而非时间推进。** 掉帧会减慢动画，尚未使用 delta time 或 scheduler。
7. **平台抽象还不是统一端口。** `main.c` 直接认识 `SdlDisplay`；若确实需要同一个入口
   动态切换 SDL、实体 LCD 或测试后端，可再定义 display/event/clock port。
8. **测试矩阵仍不完整。** UI 已可脱离 SDL 独立配置和测试，但 RGB888、effects、任意
   stride 和平台层仍缺少持续集成矩阵。
9. **`ui/experimental` 只是方向声明。** object event、animation、block pool 虽已能阅读，
   但没有生命周期、调度和失败策略，不应被业务代码依赖。
10. **建树失败不是事务式的。** `ui_build_render_tree()` 分配失败时可能已经向 root 挂入
    部分节点；当前 Demo 会立即 destroy root，所以现有链路安全，但通用 retained API
    需要约定“失败后必须销毁”或实现完整回滚。

## 11. 推荐的下一步顺序

按风险和架构依赖，建议这样推进：

1. 先把 effects 改成显式 context：`init(width,height,stride) -> bool`、`render()`、
   `destroy()`，并为分配失败、任意 stride、小尺寸写测试。
2. 明确 `UiBuffer` 的 clip 与父子失效契约，特别是父重画是否使整个子树 dirty，以及
   部分裁剪控件如何保留局部内容原点。
3. 把 phase 3 改成持久树，让位置/样式改变触发真正的局部重画，再根据测量结果选择
   dirty rect list 或 tile bitset。
4. 在 `treelike_ui` 与 `sdl_player` 两仓的 CI 中建立 RGB565/RGB888、Linux/Windows
   矩阵，并验证播放器固定的 UI 提交仍兼容。
5. 需要交互式 UI 后，再正式化平台无关事件和 animation scheduler；不要直接把
   `ui/experimental` 的草案当成稳定 ABI。

## 12. 本次审阅的验证结果

- 已确认目标提交就是当前 `HEAD`，对比基线为其直接父提交。
- 已检查完整文件清单和提交 diff，并逐项回看旧 `display`、`drawer`、`timer`、`main`、
  CMake 和未入构建草稿。
- 使用现有 ARM GCC，以 C11、`-Wall -Wextra -Wpedantic`，分别在 RGB565 和 RGB888 宏下
  把 `ui_surface`、`ui_drawer`、`demo`、`esp32_effects`、`main` 和 core test 这些非 SDL
  translation unit 逐个编译为目标文件，均无警告或编译错误。该检查不包含
  `sdl_display.c`，也没有进行 SDL 链接、运行 executable 或执行 CTest。
- 本机没有 Visual Studio 2022 generator，`cmake --preset windows-vs2022` 在生成器检测
  阶段停止，因此本次没有宣称 Windows SDL executable 或 CTest 已实际运行。该结果是
  审阅环境限制，不是已证明的源码构建失败。

## 13. 2026-09-23 拆仓验证

- `treelike_ui` 使用 Visual Studio 2026 / MSVC 19.51 分别以 RGB565、RGB888 独立配置、
  构建并运行 `treelike-ui-tests`，两组均为 1/1 通过。
- `sdl_player` 在没有兄弟目录的独立 checkout 中，通过 FetchContent 获取固定的
  `treelike_ui` commit，并使用本机 SDL2 package 完成 Debug 构建；CTest 6/6 通过，
  包括 UI core test、`--help` 与四个 SDL dummy-driver 冒烟测试。
- 这次验证没有改写 C/C++ 源码，也没有进行真实窗口交互或嵌入式硬件测试。MSVC 仍报告
  `esp32_effects.*` 的既有代码页/窄化转换警告；本地 SDL2 package 还报告其最低 CMake
  版本声明的弃用警告，均不是本次拆仓引入的构建失败。

## 14. 2026-09-24 字体 demo 验证

- Visual Studio 2026 / MSVC 19.51 下，当前 UI、Win32/GDI TrueType backend 和
  Fantasque Regular 分别以 RGB565、RGB888 构建；两套 CTest 均为 7/7，通过专用
  `sdl-player-truetype-font` 证明 demo 实际选择 TTF，而不是静默回退。
- RGB565 的 SDL dummy 预览从项目外 cwd 运行 10 帧，依次输出 tick 1 snapshot、tick 8
  join、tick 10 split，并在首次绘制时输出稳定的 `UI-FONT source=truetype` 日志；说明
  build-tree 绝对资源路径和原有控件轨迹同时有效。
- 固定旧 UI 提交 `72e3060` 由隔离的本地 checkout 提供给 standalone `sdl_player`，构建
  和原有 6/6 CTest 通过，证明没有字体 feature macro 时圆角兼容分支仍可用。GitHub
  FetchContent 的首次在线拉取因连接重置失败，因此这里不把网络下载本身标为已验证。
- 仍未进行真实窗口视觉检查。构建只出现 `esp32_effects.*` 的既有代码页/窄化警告和 SDL
  CMake 兼容性弃用警告；字体 demo 源码没有新增 MSVC 警告。
