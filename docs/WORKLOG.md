# MEngine 整理重构 —— 执行日志（WORKLOG）

> 逐条记录：日期 / 分支 / commit / 做了什么 / 如何验证 / 遇到的问题与解决 / 下一步。
> 配套总计划见 [DEV-PLAN.md](./DEV-PLAN.md)。每条以“执行记录”为单位，新记录加在最上方。

---

## 2026-09-12 — 2D 收尾：精灵平铺（方形格）+ 精灵动画示例（qoguldsd.png）+ 修编辑器断言

- **问题 1：2D 视口里的“背景方块网格不是方的”**。原因不是网格几何，而是 `New 2D Scene` 的示例背景
  是一张**被拉伸的精灵**：`Transform.scale = (18, 11)` 而贴图是 32×32 的棋盘 → 棋盘格被拉成 4.5×2.75
  单位（≈1.64:1）的矩形。2D 里“平铺”本来就是缺的能力（旧 `Sprite2D` 有 `tiling_factor`，重构时丢了）。
  - **新增精灵平铺**：`GetSpriteQuad(uv_rect, flip, tiling)` 现在把平铺烘进 UV（`u1 = u0 + (u1-u0)*tiling`，
    翻转与平铺互不干扰），四边形的缓存键加上 tiling；采样器本来就是 `GL_REPEAT`，所以平铺是零额外状态的。
    `SpriteComponent` 增 `tiling` 字段 + `SetTiledSize(world_size, pixels_per_unit = 32)`：按贴图自身
    尺寸换算重复次数（32×32 贴图 @32px/unit = 1 单位重复一次），这样**纹素永远是方的**——不管精灵多大、
    视口什么宽高比。序列化加 `"tiling"`；Sprite 检查器加 `Tiling` 滑条 + `Tile at 32 px/unit` 一键按钮。
  - 示例背景改用**真实资产** `textures/checkerboard.png`（64×64，1 单位 = 32px）平铺 30×18：既方形又能
    随场景保存/重新打开原样复现（原来的程序化棋盘贴图没有路径，存盘后重开会变成纯色块）。
- **问题 2：sprite 动画**。`assets/textures/qoguldsd.png` 是一张 6 帧横向走路循环（198×32，每帧 33×32）。
  `New 2D Scene` 的示例场景现在直接带一个**动画精灵**：
  `SpriteSheet{6, 1}` + `SpriteAnimationComponent{fps 8, loop}` + `fit_pixels(32)`（不拉伸帧像素），
  另加一个静态精灵做层序对照。`SpriteAnimationComponent` 本来就已存在（`StepSimulation` 里推进），
  这次补上**编辑态预览**：Edit 模式下 2D 场景每帧调 `Scene::UpdateSpriteAnimations(dt)`——2D 场景是“看着排”
  的，走路循环只在 Play 模式下动就没法用。Play 模式仍然走 `StepSimulation`（不重复推进）。
- **问题 3（顺手修掉的崩溃）**：编辑器在 debug 下偶尔 `Assertion failed: size_arg.x != 0.0f &&
  size_arg.y != 0.0f`（`imgui_widgets.cpp:763`，即 `InvisibleButton`），随后**弹出断言对话框卡住进程**——
  之前“`--scene` 打开时编辑器卡死”的真凶就是它。三个拖放热区把 `GetContentRegionAvail().x` 直接当宽度
  传给 `InvisibleButton`，而窗口在“隐藏/自适应尺寸”的那几帧里该值确实是 0（ImGui 只有在窗口尺寸算完之后
  才会设 `SkipItems`）。改为统一的 `FullWidthDropZone(id, height)` 助手，宽度取下限 1px。
- **验证**：
  - 像素级测量 2D 视口截图里的棋盘节距：水平 12px / 垂直 12px（修改前是 4.5×2.75 单位 ≈ 1.64:1 的矩形格），
    即**方格已方**。
  - 动画：同场景隔帧截两张（第 8 帧 vs 第 36 帧）做差异，变化像素恰好落在狐狸精灵所在的 51×50 px 区域
    （2550 px，0.18%），证明编辑态确实在动。
  - 场景往返：保存后 `.scene` 里 `Backdrop.tiling = [30,18]`（贴图 `textures/checkerboard.png`）、
    `Fox.sprite.texture = "textures/qoguldsd.png"` + `sprite_animation{columns 6, rows 1, fps 8, loop}`，
    重新打开一致。
  - 回归：`sandbox2d` 截图与改动前一致（默认 `tiling = (1,1)` 时 UV 计算是恒等变换）；clang debug 全量构建
    零警告。
- **下一步**：`uv_rect` + 平铺组合目前是“重复选中的那一帧”（图集平铺需要 shader 端按子矩形取模）；
  其余同前（tilemap/图集工具、2D 物理、2D 相机组件、精灵锚点）。

---

## 2026-09-12 — 2D 支持：场景维度 + 独立 2D 渲染路径 + 编辑器 2D 视口（sandbox2d）

- **问题/目标**：引擎此前只有 3D（PBR/阴影/IBL/后处理）主路径，旧的 `Sprite2D`/`AnimatedSprite2D` 与
  `RenderPipeline`/`RenderPass`/`RenderSprite` 那套 2D 代码在 3D 化时被搁置。目标：**引擎同时支持 2D 和 3D**，
  拆出 `sandbox2d` / `sandbox3d`，补齐常见 2D 组件（sprite / 贴图 / 精灵动画），编辑器也能编辑 2D 场景。
- **关键设计（对齐主流引擎，而不是“3D 相机换成正交”）**：第一版实现是把 2D 场景当成「正交相机的 3D 场景」，
  仍然走 HDR + tone mapping + 深度/SSAO/阴影阶段；实测与用户反馈都指出这**不是真的 2D**（颜色被 toner 改变、
  多付了整个 3D 阶段的开销、绘制顺序靠距离）。最终改为：
  - **场景带维度**：`SceneDimension::Scene2D/Scene3D`（`scene.hpp`），随 `.scene` 的 `"dimension"` 字段持久化；
    旧文件缺字段时按「主相机是否正交」推断（向后兼容）。`SetDimension(Scene2D)` 顺带应用 2D 渲染默认值
    （关天空盒/SSAO/TAA/Bloom/God Rays/IBL，背景纯黑则换深蓝灰）——因为 2D 路径**根本不执行**这些阶段。
    `Is2D()` 决定渲染路径 / 编辑器面板 / Launch 目标。
  - **独立 2D 渲染路径**（`Scene::Render2D` + `Renderer::Begin2DScene/DrawSprites2D/End2DScene`）：
    绑定目标 FBO → 设 viewport（0×0 = 取窗口 framebuffer 尺寸）→ 清屏 → **关深度测试/写、关剔除、开 alpha 混合**
    → 收集 `SpriteComponent` 按 `sorting_layer → order_in_layer → 世界 z`（painter）排序 → 相邻且
    quad/材质内容相同的合并成一次 `DrawIndexedInstanced` → 恢复状态。**没有**阴影/点光阴影/SSAO/HDR FBO/
    天空盒/后处理；片元着色器只做 `texel * base_color_factor`（不做 tone mapping/gamma），所以 2D 画面像素级等于美术图。
  - **`RenderFromPrimaryCamera` 是唯一入口**：2D → `Render2D`，3D → `RenderMeshes`（编辑器 Play / sandbox 共用）。
    3D 场景里的精灵仍然照旧走 3D 主 pass 的半透明通道（HDR + 按层/深度排序），两种模式可以混用。
  - **组件与资源**：新增 `render/sprite.{hpp,cpp}`（`SpriteSheet` 网格 → `FrameRect`；按 `uv_rect`+翻转缓存
    单位四边形；按贴图+tint 缓存 unlit 混合材质）、`SpriteComponent`（texture/color/uv_rect/size/flip/
    sorting_layer/order_in_layer + `SetSheetFrame`/`SetWholeTexture`/`FitPixels`）、`SpriteAnimationComponent`
    （sheet/first_frame/frame_count/fps/loop/ping_pong/playing/time/frame，`Scene::StepSimulation` 末尾统一
    `UpdateSpriteAnimations(dt)` 写回 `uv_rect`）；新增 `assets/shaders/sprite_vert.glsl` / `sprite_frag.glsl`
    （沿用位置/法线/UV + 实例矩阵 location 3..6 的引擎惯例，所以 2D 与 3D 共用 Mesh/实例化路径）。
  - **序列化**：实体写 `"sprite"`（贴图相对路径 / tint / uv_rect / size / flip / sorting layer / order）与
    `"sprite_animation"`；顶层写 `"dimension"`。
  - **编辑器**：不是「3D 视口里加 2D 开关」，而是 **2D 场景配一个独立视口面板 `2D Viewport`**
    （`ShowImGui2DViewport`）：工具栏 = Play/Stop + **Sprite** 新建 + Frame All + Launch + 操作提示；
    中键平移 / 滚轮缩放（无轨道/飞行）；从内容浏览器**拖入图片 = 用该贴图新建精灵**；ImGuizmo 走正交模式；
    Info 面板的 Editor Camera 在 2D 下显示 `Center (X/Y)` + `Zoom size`。View 菜单两个面板都在，
    中央 dock 区共用，`Is2DView()` 决定显示哪个。File → New 2D Scene 新建（自动建正交主相机 + 示例精灵）。
    视口/2D 视口工具条的子窗口宽度做了下限钳制（新窗口首帧内容区为 0 会触发 ImGui 断言 —— 实测踩到，
    `imgui_widgets.cpp:763` 的 `size_arg.x != 0`）。
  - **sandbox 拆分**：`sandbox/src/sandbox_2d.{hpp,cpp}`（程序化贴图 + 800 块地砖 + 玩家走行走动画 + 7 个
    不同步宝石 + 正交相机跟随）与改名后的 `sandbox_3d.*`；`sandbox/CMakeLists.txt` 两个目标。`Sandbox2D`
    在 `BuildDemoScene` 里 `SetDimension(Scene2D)`，所以它跑的就是真 2D 通道；`--scene <path>` 打开的场景
    由文件里的 `dimension` 决定（自动兼容 3D 场景）。
- **删除**：`render_pipeline.{hpp,cpp}`、`render_pass.{hpp,cpp}`、`core/command.hpp`（2D 时代的绘制抽象，
  已无使用者）、`Sprite2D`/`AnimatedSprite2D`/`AABB`/`Circle` 旧组件、`RenderSprite` 系列、纹理的子区域
  (`SetSubTexture`/`h_frames_`/`v_frames_`) 一并移除。
- **验证**：
  - `sandbox2d --frames 120 --hidden --capture-frame 100`：全屏铺满地砖 + 中央角色 + 周围宝石
    （内容 bbox = 整个 1600×900 帧），日志无 ERROR/WARN；渲染统计为**两次 draw call / 两个实例批次 / 4 个三角形**
    （地砖 800 张 + 其余 = 只有 2 个批次），无 shadow/point/SSAO/main/post 耗时（2D 不跑）。
  - 编辑器：`MENGINE_EDITOR_SELFTEST_2D_SAVE=<path>` 新建 2D 场景并保存 → 日志
    `dimension=2D sprites=2  primary_camera=1 skybox=off 2d_view=yes`；再用 `--scene <该文件>` 打开 →
    `Opened scene file ... (4 entities, 2D)`，中央区域显示 `2D Viewport` 面板，Render Stats 2 draw call。
  - 3D 回归：`sandbox3d`（Cyborg 场景）截图正常；把 2D 场景的文件改成 `dimension=3d` + 透视相机后
    用 `sandbox3d --scene` 打开，精灵仍通过 3D 半透明通道正确绘制（HDR 后处理路径未被破坏）；`voxel`
    截图正常（区块/水/树）；编辑器默认 3D 演示场景不受影响。
  - clang debug 全量构建（engine/editor/sandbox2d/sandbox3d/voxel/examples）零警告；MSVC debug 同样零警告
    （顺手修掉一条 `C4456` 变量遮蔽——3D 半透明批处理里的 `batch` 改名为 `sprite_batch`）。
- **下一步**：2D 侧可继续做的：tilemap/图集导入与九宫格、2D 物理（Jolt 只有 3D 体，2D 需要平面约束或另接 2D 求解器）、
  2D 摄像机组件（跟随/边界/缩放）、精灵锚点（pivot）；渲染侧：2D 批处理改为全局按材质分组（现在只在连续区间内合并）。

---

## 2026-09-12 — voxel 性能：区块加载与渲染解耦（Minecraft 式异步区块流水线 + 已加载区块常驻）

- **问题**：`voxel_app` 把「生成 + 网格化 + GPU 上传 + 建实体」全放在帧循环里同步做，而且玩家每跨过一个 16 格区块边界就**整个重建**半径 5 的 121 个区块（实测该调用耗时 ≈1.0 s，期间渲染完全停住）。此外旧网格器每个方块都走一次 `unordered_map` 查找（每区块约 6.1 万次）。
- **做法（只改 `voxel/`，engine 未动，仍只用公共 API）**：
  - `voxel_world`：世界改为**线程安全**——chunk map 加 `mutex`，生成放在锁外（生成是只依赖 seed 的纯函数，可并行），只有插入串行化；新增纯函数 `TerrainHeight()`（不生成、不加锁）与 `GenerateChunkData()`；新增 `ChunkSnapshot`（本区块 16×16×40 + 四个 XZ 邻居各一圈边界，一次加锁拷贝）——网格化改为读快照，**锁内零哈希查找**；`PrepareChunk` / `BuildChunkMesh(World&,...)` 被 `BuildChunkMesh(ChunkSnapshot&,...)` 取代；新增 `UnloadChunk` 释放远处数据。
  - **新增 `voxel_streamer.{hpp,cpp}`**：`ChunkStreamer` = 工作线程池（cores-1，上限 4，`MENGINE_VOXEL_WORKERS=<n>` 可覆盖）+ 任务队列 + 结果队列。任务按**距离环由近到远**入队；破坏/放置走 `RequestRemeshWithNeighbours` **插队**。每个 mesh 结果带 per-chunk 请求 id，过期结果（编辑前生成）在 `Drain` 中丢弃；离开半径的区块用 cancel 标记，worker 直接跳过不做无用功；未上传结果上限 24，避免 worker 抢跑整个世界的 GPU 上传。**`Mesh::Create` + 建/删实体只在渲染线程**（GL 要求）。
  - `voxel_app`：跨区块时只调 `SetCenter`（纯记账，不生成、不建 mesh）；每帧 `UploadFinishedChunks` 有**每帧预算 3 ms / 最多 4 个**（至少 1 个保证进度）；`tiles_` 从 `vector` 改为按区块 key 的 `unordered_map`，跨界不再清空重建；物理用 `GroundReady`（玩家脚下 3×3 已出 mesh）门控，出生/传送/掉出世界时不会穿进未加载区域；出生点搜索改用纯 `TerrainHeight`（原来最坏会同步生成 441 个区块，现在 0 个）；启动 `PrewarmSpawn` 只等玩家周围 3×3（≈50 ms，相当于 Loading），其余后台流式补上。
- **验证**：
  - 远距传送（`MENGINE_VOXEL_DEBUG_CAM="400,26,400,60,-10"`）触发 121 区块全量重载，新增的每帧统计窗口输出 `avg 1.10 ms worst 5.43 ms`（该窗口含整轮加载爆发；旧实现同场景是 ≈1.0 s/帧），之后稳定在 `avg 0.002 ms worst 0.003 ms`，`121 tiles/121 tracked/0 jobs in flight`（无抖动、无重复入队）。
  - A/B 截图回归（同 seed、同 debug cam、同 `--capture-frame 200`）：`tools/ppm_diff.py`（新增）平均通道差 **0.004/255**，差异是沿高对比边缘的 1–2 级噪声 + 约 29 个像素（物理门控使落点相差几毫秒导致的半透明排序差异），**没有任何区块形状/缺面差异**。
  - clang debug 零警告；`--frames 240` / `--frames 600 --hidden` 长跑稳定。
- **追加（同日，第二轮：已加载区块常驻 + 消掉跨区块抖动）**：
  - **双环流式（MC 的 render distance vs simulation distance）**：`ChunkStreamer::SetCenter(center, load_radius, keep_radius)`——内环（默认 5）主动生成/网格化，外环（默认 8，`MENGINE_VOXEL_KEEP`，最小 load+2）为**常驻环**：区块保留体素数据 + 实体 + mesh 并且**继续渲染**，只是不再更新；走出 keep 环才回收。看回去/走回去时地形不会消失也不需要重新生成（resident 区块重新进入 load 环直接复用，连 mesh job 都不发）。
  - **实体池**：回收的 tile 不再 `Scene::DestroyEntity`，而是摘掉 `MeshComponent`（释放 GPU buffer）后实体进 `free_tiles_` 池，新进入的区块从池里取实体复用。原因：实测 `Scene::DestroyEntity` ≈ **1.3 ms/实体**（引擎记账 + debug 构建的逐行日志），而释放 mesh ≈ 0.1 ms、重挂 `MeshComponent` 几乎免费。
  - **每帧共享预算**：上传（≤3 个）与回收（≤8 个）共用 4 ms deadline，每帧至少推进一个，爆发分摊到多帧。
  - 验证（debug，`MENGINE_VOXEL_AUTOWALK=14` 沿 +X 飞 2400 帧 ≈ 280 格，每 16 格一次跨区）：`worst update 0.4 / upload ≤5 / retire ≤1.4 ms`，`avg ≈0.15 ms/frame`，稳定在 `184 tiles/184 tracked (63 resident)/0 jobs`；对比：改造前跨一次区 **75 ms**（旧同步实现 **≈1.0 s**）。相机朝后（yaw 150）截图可见刚从身后飞过的地形；`MENGINE_VOXEL_KEEP=7` 与 `=12` 同帧截图差异 50.3%（平均 27.6/255），证明常驻区块确实参与渲染。
  - release 构建（windows-clang-release）编译零警告并且可运行；MSVC debug 同上。
- **下一步**：真机试玩确认手感（行走/飞行时区块弹出的节奏、挖矿后重网格化的延迟），可选：视距/常驻半径做成运行时可调（面板或键盘），以及把「已探索区域永久保留 + 存盘」做成引擎级能力。

---

## 2026-09-06 — 视差映射 Parallax Occlusion Mapping（POM，D 项）

- **engine 材质**：`Material` 增 height map 槽 + `height_scale` 因子；`Renderer::DrawMeshInstanced` 把高度图绑到纹理单元 15（避开 shadow/IBL/SSAO/点光阴影 8..11 等），逐 draw 上传 `height_scale`；合批比较加 height map + scale，视差/非视差材质不会误合并。commit `576d9f2`。
- **shader（pbr/blinn 引擎路径）**：LO 5.3 风格 POM——沿切线空间视线对高度场分层 ray-march + 层间线性插值；用导数法几何 TBN（无需新顶点属性）。仅在有高度图且 `height_scale>0` 的 lit pass 生效；unlit/发光材质不受影响（`blinn_lo` 单色精确路径不动）。
- **序列化**：`"height"` + `"height_scale"` 往返（MaterialToJson/FromJson/ApplyMaterialJson）。
- **editor**：共享 `DrawMaterialEditor` 增 Height 缩略图行 + Height Scale 滑条（0..0.2）——Mesh 与 Model 每部位都能用。
- 验证：`bricks2`(diffuse+normal+disp) 平面 headless 两张截图，视差开/关 mean abs diff≈3.2/255、开时砖块明显浮出；`example_ex_6_2_2_ibl_specular` 输出不变。
- 下一步：其余引擎增强 / 技术债清理（重复抽象/死码/命名）。

---

## 2026-09-06 — 单实体多材质模型组件（ModelComponent，C 项）

- **engine 类型正规化**：`ObjModel`/`ObjModelPart` 更名通用 `Model`/`ModelPart`（每 part = 独立 mesh + 材质 + 材质名）。`LoadObjModel` 通过 **MeshLibrary**（此前死代码，现正式接入）按 `"路径|材质组"` 缓存 part 网格 → 同一模型多处导入/多实体共享同一份 GPU 网格，可实例化合批。commit `8b89821`。
- **ModelComponent（ECS）**：单实体携带多材质模型；`Scene::RenderMeshes` 把 parts 在**同一实体 Transform** 下展开为渲染项（阴影 / SSAO / 主 pass / 视锥剔除 / 合批与 MeshComponent 完全一致）。
- **序列化**：实体写 `"model"`（source + 每 part 材质覆盖）；读时 `LoadObjModel` 重建 `.mtl` 材质再用 `ApplyMaterialJson` 叠加已存因子/贴图——保留 spec/反射贴图、shininess 等未序列化通道。旧存档“根 + 每材质子实体”写法仍可加载。
- **editor**：多材质 OBJ 拖入 → **单个实体** + ModelComponent（不再是“根 + 每材质子实体”），自动整体取景；Properties 新增 Model 检查器（part 下拉逐部位编辑材质，复用从 Mesh 抽取的 `DrawMaterialEditor`）；Duplicate 一并复制。
- 验证：nanosuit 以 6 part 单实体 headless 渲染正常（六部位各贴各图）；`example_ex_6_2_2_ibl_specular` pbr 输出不变。
- 下一步：D（引擎视差/反射等能力增强）与剩余技术债清理。

---

## 2026-09-06 — 引擎多方向光（带阴影主光 + 最多 4 个无阴影补光）

- **engine 多方向光**：`DirectionalLightComponent` 实体可多个——第一个是**带阴影主光**（沿用原 shadow-map 路径），其余变成额外的无阴影方向光（补光/彩色填充）。renderer 增 `directional_extras_`（`Set/Clear/GetDirectionalExtras`，上限 `kMaxDirectionalExtras=4`）；`SyncLightComponents` 每帧收集全部方向光实体、推导方向后拆分主光 + extras。
- **着色器**：`pbr_frag` / `blinn_frag`（引擎路径）声明 `MAX_DIR_EXTRA 4` + `dir_extra_count/dir_extra_dir[]/dir_extra_color[]` 数组 uniform；主方向光块之后叠加无阴影补光（pbr 加 BRDF 项、blinn 加 diffuse+spec 项）。upload 放在 per-pass uniform cache 守卫内（同 shader 只传一次）。LO 精确 `blinn_lo` **保持单方向光不动**。
- **editor**：Lighting 面板方向光区改为列出**全部**方向光实体（点击选中），首个标注“Primary（cast shadows）”，其余为无阴影补光；无实体时仍显示 Scene Sun 兜底 + “Add Directional Light”。光源示意图本就可为每个方向光实体各画一个太阳盘+箭头，天然支持补光瞄准。
- 回归：editor / `example_ex_6_2_2_ibl_specular`（pbr）编译运行通过，headless 30 帧截图与改造前一致（`dir_extra_count=0` 默认不改变渲染）；LO exe、默认 PBR/ACES 编辑器场景不受影响。
- 下一步：C 已完成（见下条）；后续 D（引擎视差/反射增强）。

---

## 2026-09-06 — 多材质模型 + OBJ spec/反射资源 + 点光软阴影 + 默认调暗

- **点光软阴影（PCF）**：`pbr/blinn` 前向着色器把点阴影改为切线空间 5×5 PCF（`point_shadow_size` 上传 cube 面分辨率）→ Rendering 面板 `Shadow PCF Radius` 可调软硬。commit `1fc4cd8`。
- **多材质 OBJ 支持（nanosuit）**：`ModelLoader::LoadObjModel()` 按 `usemtl` 把几何拆成多 part（`ObjModel`/`ObjModelPart`，各 part 单独子网格+对应 `.mtl newmtl` 材质）；修复旧 `LoadObjMaterial` 现在真正解析第一个 newmtl 块。editor 拖入多材质 OBJ → 根实体 + 每 part 一个子实体（各自 mesh+材质、自动整体取景）。
- **OBJ spec/反射资源**：`.mtl` 现读 `map_Ks`(spec)、`map_Ka`(equirect 反射)、`Ns`；`Material` 增 reflection map 槽（renderer slot 14）；pbr 采样 spec 贴图驱动 F0（介质 `0.04+spec*0.5`、金属直接用颜色）、并按反射方向 equirect 采样 `map_Ka` 做 baked 反射；带 spec 的 part 默认转低粗糙介质(rough≈0.4)。commit `6c9bf93`。
- **editor**：多材质 OBJ 以“根 + 每材质子实体”导入（nanosuit 六部位各贴各图）；默认 PBR/ACES 演示调暗（IBL 0.35→0.18、sun 1.6→1.35/amb 0.02）。commit `a5f71a3`。
- 回归：engine/editor 编译通过；单材质 OBJ（backpack 等）仍走原 .mtl/文件名启发式路径，行为不变。

---

## 2026-09-06 — 编辑器通用引擎光照化 + 灯光视觉/操控重构

- **editor 深色主题**：ImGui 整体换成 VS Code 风中性炭灰 + 蓝强调（`SetupImGuiStyle` 全量重写；Log 等级文字/过滤钮、Content Browser hover、提示色同步适配深色）。
- **场景树遍历安全**：`ShowImGuiScene` 改为先对实体列表**快照**再绘制，杜绝右键 Delete/Duplicate/Create Child 在遍历中改 `entities_` 引起的迭代器失效崩溃。
- **光源视口示意图**（`DrawLightGizmos`，Edit 常显）：Directional＝太阳圆盘＋方向箭头、Point＝三环灯泡线框＋光晕短线、Spot＝外锥角线框锥体；位置跟实体 Transform、方向同旋转。
- **灯光方向 = 实体 Rotation**（engine scene）：实体灯传播/照射方向＝世界旋转后的局部 -Z；`SyncLightComponents` 每帧推导并写回 `light.direction`；新建 Directional/Spot 默认 pitch -90°（朝下）；Properties 去掉旧独立 Direction 字段（旋转 gizmo 调方向 + 只读显示）。
- **方向光删除不再残留（engine）**：Scene 增基准 `authored_directional_light_`（SetLight/Lighting 面板/序列化都读写它）；无 DirectionalLightComponent 实体时每帧用基准恢复 renderer → 删实体光立即消失。
- **默认场景 → 通用引擎光照（PBR + ACES）**：`CreateEngineDemo`（地板 + Box + 金属球 + 太阳阴影 + 暖点光(开阴影) + 冷点光补光，带 HDR 发光灯泡 bloom）。不再用 LO 精确模式；LO 精确展示由 `examples` LO exe 负责，editor 旧的 LO 展示厅 `CreateLightingDemo` 删除（此前“默认=LO bloom 展示厅”的记录不再适用）。
- **Lighting 面板 ECS 化**：删除遗留非 ECS `point_lights_` 列表与每帧 renderer 同步；面板改为列出/新增/选中 Directional/Spot/Point **实体灯** + 无方向光实体时的 Scene Sun 兜底；点光默认 `casts_shadow=true`。
- 提交：`6212d27`(engine) `81f3c8f`(editor) `905f83b`(Lighting 面板重构)。

---

## 2026-09-06 — `.mtl` 解析（ModelLoader::LoadObjMaterial）+ editor OBJ 导入优先用 .mtl

- **engine**：`ModelLoader::LoadObjMaterial(obj)` 解析 OBJ 引用的 sidecar `.mtl`（首个材质）：
  `Kd` base color（无 `map_Kd` 时）、`map_Kd` albedo（标 sRGB）、`map_Bump/map_Kn/norm` 法线、
  `map_Ks` 高光；自动跳过 `-bm` 等选项 token；纹理用 `Texture::Create`（OBJ 翻转向，与现有路径一致）。
  返回 `nullptr` 时调用方可回退文件名启发式。多材质 `usemtl`（需 Model 重构拆分）暂返回首个材质。
- **editor**：`LoadModelAsset` 拖入 `.obj` 时优先用 `.mtl`（如 backpack），无 `.mtl` 再回退文件名约定。
  技术债“OBJ .mtl 未解析”部分解决（多材质拆分为后续 Model 项）。

---

## 2026-09-06 — editor 默认场景换成 ex_5_6(HDR/bloom) 风 + 运行时换天空盒

- **默认场景 → Lighting demo**（`CreateLightingDemo`，复刻 ex_5_6 = LO 7.bloom）：木地板 + container 木箱
  + 4 个 HDR 点光（1/d²，色 5/10/15 等）各自带发光立方体，暗室 + LO HDR 色调 + bloom（threshold 1，
  strength 1）；每个 HDR 灯是实体（`PointLightComponent` + unlit 发光盒），拖实体光与发光一起动。
  原物理 demo（CreatePhysicsDemo）保留但不再是默认。
- **NewScene 重置友好基线**：新空场景不再继承上一个暗 LO 场景的设置——恢复默认太阳/IBL 0.6/ACES/
  skybox 开/bloom 柔和等。
- **运行时换环境（engine）**：`Renderer::Scene::SetEnvironmentHdr(path, flip)`——运行时重建 Skybox
  （env cubemap/irradiance/prefilter，BRDF 保留），并同步 Application 静态。
- **Editor Rendering 面板**：新增环境 .hdr 拖放槽（拖图片文件换天空盒/IBL）+ “Flip V(glTF)” 开关。
- 引擎默认方向光 color 2.5 → 1.0（默认不再刺眼）。
- 提交含：engine(scene/renderer/light)、editor(editor.cpp/hpp)、docs。

---

## 2026-09-06 — editor 默认场景：发光小正方体 + 调暗环境光 + bloom

- 默认编辑器场景（`CreatePhysicsDemo`）四角加入 4 个 **HDR 发光小正方体**（unlit PBR、base_color>1
  直出 → bloom 提亮发光），每个实体同时挂 **`PointLightComponent`**（演示 Light 组件化：拖实体/用
  Gizmo 移动，小方块与其点光一起动），颜色 暖/绿/蓝/白。
- 初始化调暗：IBL 0.8 → **0.35**；显式 `SetBloomEnabled(true)` + `SetBloomThreshold(1.0f)`，
  让发光源在较暗环境下清晰泛光（editor 打开即可看到 bloom 效果）。

---

## 2026-09-06 — Light 组件化 v2：方向光实体 + 灯光组件实体级序列化

- **方向光实体**：`DirectionalLightComponent`（ECS）。场景存在该组件时每帧把（首个）组件拷入
  renderer 方向光；renderer 仍只支持 1 个方向光（多方向光需 shader 数组，未做）。Editor Create 菜单
  新增 Directional Light 实体、Add Component/Properties 可编辑（Direction/Color/ambient/diffuse/specular）；
  Lighting 面板检测到方向光实体时提示改用实体编辑，避免每帧被覆盖。
- **灯光组件实体级序列化**：scene_serializer 为实体 JSON 加 `directional_light`/`point_light`/
  `spot_light` 完整字段（含 LO ambient/diffuse/specular、lo_attenuation、cutoff 等）并读写；SaveScene
  在存在组件灯时**跳过**旧的顶层灯光数组（旧文件无组件仍按旧路径读回）；Play 快照同构。
- 回归：engine/editor 编译通过；组件灯+实体→Gizmo/层级/保存加载闭环完成。多方向光/面板整合为后续项。

---

## 2026-09-06 — Light 组件化 v1：PointLight/SpotLight 变 ECS 组件（editor 可建可编辑）

- **引擎**：新增 `PointLightComponent` / `SpotLightComponent`（component.hpp，含 render/light.hpp）。
  `Scene::SyncLightComponents()`（RenderMeshes 每帧开头调用）：只要场景存在带灯光组件的实体，就从这些
  实体**重建** renderer 的点/聚光灯表（位置取实体世界 Transform）；无组件时旧 `AddPointLight`/`AddSpotLight`
  列表 API 完全不变（回归：ex_6_1_1 / ex_2_6 / ex_5_8 / ex_6_2_2 编译+运行正常）。
- **Editor**：Create 菜单新增 Point Light / Spot Light 实体；Add Component 弹出项 + Properties 可编辑
  颜色/强度/半径/阴影/LO 衰减（点光）与锥角/方向（聚光）。灯的实体可被 Gizmo 移动/层级管理。
- 方向光仍为场景级单一 `DirectionalLight`（未组件化）；灯光组件实体级序列化待做（暂以 renderer 灯光数组落盘）。
- 文档：scene.md（组件表+说明）、status.md（技术债更新）。

---

## 2026-09-06 — 深度整理启动：文档同步 + 死码清理 + Editor 渲染选项接入

- **阶段 A 后续（整理/增强）**：
  - **B-1 文档同步**：`status.md` 补 M6(LO 移植期)；`rendering.md`/`architecture.md` 纠过时说法
    （2D/无剔除/无 BRDF LUT）；纠正 4 处“序列化未实现”（其实 engine+editor 早已实现）。
  - **B-2 技术债**：删除死代码 `render_context.hpp`（无人引用、无实现）。
  - **Editor**：`Rendering` 面板接入 **Tone Mapping**(ACES/Linear/LO-HDR/Reinhard)、**Skybox**、
    **Background**（关天空盒时）、**IBL Specular**；引擎补 `Renderer/Scene::IsReinhardTone`、
    `Scene::IsIblSpecular`。
  - **序列化持久化**：`scene_serializer` 补存 skybox/background/linear_output/lo_hdr/reinhard/ibl_specular
    （SaveScene + LoadScene + RestorePlaySnapshot）。

---

## 2026-09-06 — 补完 LO 必要端口：IBL 2.1.2/2.2.1 + model_loading

- 用户确认"先补完再整理"：把剩余必要的 LO 端口补齐，随后进入项目深度整理/engine+editor 增强。
- **引擎新增 `Scene::SetIblSpecular(bool)` / `Renderer`（默认开）**：pbr_frag 加 `u_ibl_specular`，可只保留漫反射 IBL（irradiance），复刻 LO 2.1.2 的"镜面 IBL 之前"状态。
- **`ex_6_2_1_ibl_irradiance`**（LO 6.pbr/2.1.2）：7×7 红球阵 + newport_loft + 4×300 灯，`SetIblSpecular(false)` → 金属球偏暗、只反射直射光高光。
- **`ex_6_2_2_ibl_specular`**（LO 6.pbr/2.2.1）：同场景 + 全 IBL（prefilter + split-sum BRDF LUT）→ 金属球反射房间。与 2.1.2 并排即为 LO 加镜面 IBL 的前后对照。
- **`ex_3_1_model_loading`**（LO 3.model_loading/1）：backpack.obj + diffuse，`SetUnlit` + `LoScene` 线性输出直出贴图（同 LO 3.1.fs 无光照）；OBJ 原始尺寸很大 → 自动居中归一化取景。
- 均抓帧验证；更新 PORTING.md。

---

## 2026-09-06 — 修 Cerberus 渲染：GLB 只内嵌 albedo，其余贴图在 sidecar 文件里

- **现象**：枪渲染成一片浅灰/塑料感，无贴图细节。
- **根因**：FBX2glTF 转出的 `Cerberus_LP.glb` 只内嵌了 `Cerberus_A.tga`（mime `image/unknown`，材质只有 baseColor，metallic/roughness 是标量），**metallic / roughness / normal / AO 四张在 `Textures/` sidecar 文件里没进 GLB** → 材质日志 `albedo=set mr=NULL normal=NULL ao=NULL`。
- **验证**：unlit 直接输出原始 albedo 一帧，可见铜喷口+细节 → UV 与 albedo 正常，缺的是 M/R/N/AO。
- **修复**：新增 `examples::PbrSidecarTextured(albedo, normal, roughness, metallic, ao)`（example_helpers.hpp）：用 stb 按 **glTF UV 不翻转**读 sidecar 图，把 R/M 两张贴图打包成引擎 MR（G=roughness、B=metallic），albedo 标记 sRGB 解码、factor=1 由贴图驱动。`ex_6_2_cerberus` 与 `ex_model_viewer` 改用它（两者同是 Cerberus）。
- capture：两处均恢复深色金属 + 金/铜细节的完整 PBR 观感。
- 注：Cerberus 原始 TGA(4096² 每张 35–48MB)+GLB(47MB) 体积大，不入库。

---

## 2026-09-06 — 把 Cerberus 放进 LO 6.2 的 newport_loft IBL 场景（ex_6_2_cerberus）

- **用户**：把新加的 model 放到 pbr 6.2 的场景下（参考图=悬浮在客厅的斜置枪，背景虚化）。
- **ex_6_2_cerberus**：同 6.2.2 环境配置（newport_loft env+flip、4×300 直射光、IBL=1、Reinhard、无 bloom/godrays/TAA/SSAO），主体为 Cerberus glTF：自归一化后斜置（绕 Z ~38°）悬浮在房间里；相机可 WASD 飞行/轨道/滚轮。capture 与参考图一致观感（清晰模型+虚化客厅）。
- 6.2.2 的 LO 球体材质图保持独立不受影响。

---

## 2026-09-06 — example 相机可移动（WASD 飞行）+ Cerberus 模型 viewer

- **用户**：① 6.2.2 球仍有点不一样（后续再对）；② 加了个新模型让加载进去；③ 让摄像头能移动。
- **相机移动**：ExampleApp host 增加 WASD（前/后/左/右平移轨道目标）+ 空格/左Ctrl 上/下，配合右键轨道与滚轮缩放 → 所有 example 都能像 LO 那样飞行查看（WASD 用的是按当前 yaw/pitch 计算的 view 轴向）。
- **新模型**：用户加入 `assets/models/Cerberus_by_Andrew_Maximov/`（FBX+TGA）。引擎只支持 OBJ/glTF → 下载 **FBX2glTF 0.9.7**（`build/fbx2gltf/FBX2glTF.exe`）把 `Cerberus_LP.FBX` 转成 `Cerberus_LP.glb`（47MB，贴图内嵌）。
- **ex_model_viewer**（新示例）：通用模型查看器，加载该 GLB + glTF PBR 材质，自归一化居中（同 sandbox helmet 做法），环境 IBL + 方向光；飞行/轨道/缩放相机可用。27.4k verts/100k idx，capture 正常渲染。
- **注意**：Cerberus 原始目录约 300MB（TGA/psd 等）留在本地**未提交**（否则 repo/build 拷贝会巨量膨胀；每个 example 的 POST_BUILD 会整目录拷贝 assets，建议后续对“大模型目录”做排除或只留 glb）。

---

## 2026-09-06 — 引擎 PBR 新增 split-sum BRDF LUT（对齐 LO 2.2 的 IBL 镜面高光）

- **用户**：6.2.2 球体偏亮/偏鲜艳、和背景不像同一图层；问是亮度还是颜色映射。逐项比对 LO 2.2.2.pbr.fs 后：直射/材质/albedo 解码/tone 全一致，唯 IBL 镜面不同——LO `prefiltered*(F*brdf.x+brdf.y)`（split-sum LUT），引擎之前 `prefiltered*F_ibl`（无 LUT）→ IBL 镜面能量分布不对（金属球偏亮/鲜艳），另背景在引擎里也走 tone+gamma（LO 背景 raw）。
- **引擎**：新增 BRDF LUT：`assets/shaders/brdf_frag.glsl`（LO 2.2 brdf.fs，1024 样本积分 NdotV×roughness→512 RG16F）+ manifest `brdf`（复用 post_vert 全屏三角形）；`Skybox` 生成 LUT（`GenerateBRDF`/`BindBRDF`，RG16F 2D、fullscreen VAO）；Renderer 绑到 slot13；`pbr_frag` spec IBL 改为 LO 形式 `prefiltered*(F_ibl*brdf.x+brdf.y)*specular_intensity`。
- **验证**：6.2.2 capture 正常；ex_2_1/ex_2_2_basic hidden 冒烟 exit=0（走 IBL 的 PBR 场景无回归）。球体 IBL 高光能量现与 LO 一致。

---

## 2026-09-06 — 修复 ex_6_2_2 天空盒上下颠倒 + 球/背景不同图层

- **用户反馈**：① 天空盒上下颠倒；② 不同材质球感觉和背景不是一个图层。
- **根因**：引擎 Skybox 加载 HDR 默认**不翻转**，而 LO 的 2.x demo 都是 `stbi_set_flip_vertically_on_load(true)` → newport_loft 在引擎里上下颠倒（默认 kloppenheim 恰好未被注意）。环境一倒，球上的 IBL 反射（房间/天花灯）与背景互相矛盾 → “不像同一图层”。
- **引擎**：新增 `Application::SetEnvironmentHdrFlip(bool)`（默认 false，不破坏默认环境）+ Skybox ctor `flip_equirect` 参数（loadf 前 stbi flip true、之后复位）。Renderer 从 Application 静态读 flip。
- **ex_6_2_2**：设 `SetEnvironmentHdrPath(newport_loft)` + `SetEnvironmentHdrFlip(true)`。
- **验证**：capture 正常：客厅沙发/书架/木地板/窗户光朝上；金球内可见房间与天花灯反射，与背景同一环境 → 两个问题都消失。

---

## 2026-09-06 — LO 6.pbr/2.2.2 IBL specular_textured + 引擎：可换环境 HDR + pbr albedo sRGB 解码

- **引擎**：① `Application::SetEnvironmentHdrPath`（静态，Renderer 初始化时用）→ 每个 exe 可选自己的 IBL/天空盒环境（默认仍 kloppenheim）；② `pbr_frag` 新增 `u_albedo_srgb` 解码（albedo=pow(2.2)，uniform 原来已上传，材质 `SetAlbedoSRGB(true)` 开启）——对齐 LO 贴图 PBR 的 albedo 处理。
- **ex_6_2_2_ibl_specular_textured** ← LO 6.pbr/2.2.2：newport_loft.hdr 环境 + 5 颗材质球（rusted_iron/gold/grass/plastic/wall，x=-5..3 y0 z2）+ LO 4×300 直射灯 + IBL；引擎 pbr（irradiance+prefiltered 简化 specular，无 BRDF LUT）+ Reinhard + 环境当背景。观感=LO 经典 5 材质球图。
- **ex_6_1_2 修复**：查 LO 1.2.pbr.fs 确认 **albedo 也是 pow(2.2) 解码** → 之前 6.1.2 不解码正是“锈灰/不红”根因；现开 `SetAlbedoSRGB(true)` → 锈呈棕黑锈蚀而非灰。
- **验证**：capture：6.2.2 = 新波特客厅背景 + 金/草/塑料/砖墙/锈铁球；6.1.2 锈球棕黑不灰。
- 2.2.1（无贴图金属/粗糙矩阵 IBL）未做，可续。

---

## 2026-09-06 — ex_6.1 观感再对齐：滚轮缩放 + 去掉环境光（锈/红“花、灰”主因是天空盒环境光染色）

- **用户反馈**：① 红球仍稍鲜艳；② 锈铁球锈处已不反光，但相比 LO 不自然、锈不红、颜色“花”；③ 想要滚轮放大缩小查看。
- **滚轮缩放**：example host（`ExampleApp`）注册 GLFW 滚轮回调，`OnUpdate` 里按 LO 方式调 **FOV**（滚轮上=缩小 fov 放大，clamp [5,70]）→ 所有 example 都能像 LO 一样滚轮放大看细节（LO 默认相机离得近，很多“花/灰”是远景纹理缩小的观感，放大后再判断更公平）。
- **去环境光**：ex_6_1_1/6_1_2 之前为近似 LO 的 `0.03*albedo` 开了微弱 IBL，但引擎 ambient 来自**天空盒 irradiance**（不是 LO 的纯平 0.03）→ 给锈/红叠了一层灰/蓝环境色 → 不红、灰、花。现改为 **IBL 关 (0)**：只剩纯直射 + Reinhard（LO 1.x 本无环境）。
- 参数核对结论：ex_6_1_1 红球 albedo 0.5、4×300、1/d²、metallic=行/7、roughness=列/7、Reinhard+gamma 均与 LO 1.1.pbr.fs 一致；ex_6_1_2 albedo/normal/mr/ao raw + Reinhard 与 LO 1.2 一致（metallic/rough 已逐像素）。
- 待你放大后再确认红/锈颜色，如仍差可再调（如曝光/直射能量/贴图解码口径）。

---

## 2026-09-06 — ex_6.1 PBR 观感对齐：引擎加 Reinhard 色调 + 合并 MR 贴图（修锈迹反光）

- **用户反馈**：① 纯色红球太亮，希望与 LO 参数一致；② 锈铁球的“锈”不该反光但看着太亮。
- **根因**：① 引擎默认 composite 是 ACES+gamma，LO 6.pbr 1.1/1.2 用 **Reinhard** `color/(color+1)`+gamma → 偏亮/饱和；② 之前引擎没有 separate metallic/roughness，我以标量 metal 0.9/rough 0.6 近似 → 整球金属，锈区也反光。
- **引擎**：composite 新增 **Reinhard 色调** `u_reinhard_tone`（`SetReinhardTone`，scene→renderer→post 链路照抄 lo_hdr_tone）；默认关，其它场景不变。
- **素材/工具**：新增 `tools/make_pbr_mr.ps1`：把 LO 分开的 `metallic.png`/`roughness.png` 灰度合成引擎的合并 MR 贴图（R=1、G=roughness、B=metallic）→ 每材质生成 `assets/textures/pbr/<mat>/mr.png`（gold/grass/plastic/rusted_iron/wall 共 5 张）。
- **ex_6_1_1**：`SetReinhardTone(true)` + 微弱 IBL ambient（≈LO 0.03*albedo）+ 小线性背景使 tone 后呈 LO 深 clear。
- **ex_6_1_2**：`RustedIron()` 改用 albedo/normal/**mr.png**/ao（factors=1，金属/粗糙逐像素）→ 锈区低金属高粗糙哑光、裸金属才反光。
- **验证**：capture：6.1.1 暗底红球+白高光（Reinhard）；6.1.2 锈铁球呈现“石状粗糙 + 局部金属亮斑”而非整体反光。
- 之前 ACES 背景 0.1→0.36 中灰的坑也一并修：改用小线性背景 + Reinhard 呈现 ~RGB 25。

---

## 2026-09-06 — LO 6.pbr 复刻：6.1.1 lighting + 6.1.2 lighting_textured（引擎 PBR 直射）

- **用户**：刚才压测那个先不要了；继续复刻 LO 的 PBR 部分场景。
- **ex_6_1_1_pbr_lighting** ← LO 6.pbr/1.1.lighting：7×7 红球（albedo 0.5 红），`metallic=row/7`、`roughness=clamp(col/7,.05,1)`；4 盏 300 白光 (±10,±10,10)，LO `1/d²` 衰减（lo_attenuation c0/l0/q1）；引擎 PBR、IBL 关（skybox off、ibl 0）、clear 0.1、bloom/god-rays/TAA/SSAO 关；引擎后处理 ACES+gamma（LO 1.1.fs 用 Reinhard → 近似）；相机 dist 21 拉远取整幅矩阵。
- **ex_6_1_2_pbr_lighting_textured** ← LO 1.2：7×7 **rusted_iron** 球 + 单白光 (0,0,10) 强度150、1/d²；albedo/normal/ao 三张贴图 raw 载入（与 LO 一致不解码）；引擎 PBR 的 MR 是**合并贴图**（G=rough、B=metal），而 LO 是分开的 metallic/roughness 两张灰度 → 用标量 metal 0.9 / rough 0.6 近似（文件头注释了该引擎限制）。
- **素材**：`assets/textures/pbr/{rusted_iron,gold,grass,plastic,wall}`（此前 LO 镜像）。
- **验证**：capture 观感与 LO 图一致：6.1.1 上行金属镜面高光→下行哑光、左滑右糙；6.1.2 锈铁斑驳金属球。2.x IBL 系列为引擎内部能力，未单列（PORTING 已注明）。
- 撤销了上一轮 `ex_5_8_light_stress`（未提交，已删文件 + 去掉 CMake 注册）。

---

## 2026-09-06 — 窗口标题实时 FPS（引擎 Application + 每个 example 标题带 demo 名）

- **用户**：每个 example 的窗口 title 上能不能加上 FPS。
- **引擎**：`Application` 增加 `window_title_base_`（默认 "MEngine"）+ `SetWindowTitleBase()`；`GetDeltaTime` 每秒刷新 `fps_` 时顺带 `UpdateWindowTitle()` → `glfwSetWindowTitle(base + "  |  N FPS (x.x ms)")`（0 FPS 时只显示 base，窗口建好后先推一次 base）。
- **examples**：`ExampleApp` ctor 里 `SetWindowTitleBase(setup_.name)`，于是每个 demo 标题 = 自己的 demo 名 + 实时 FPS/帧时。
- **验证**：hidden 运行标题实测 `LO 5.9 ssao (engine SSAO, Space toggles)  |  895 FPS (1.1 ms)`（首秒启动为 1 FPS 属正常 ramp）；全 12 个 examples + editor/sandbox/voxel 编译链接通过。editor/sandbox/voxel 未设 base，仍显示 MEngine + FPS。

---

## 2026-09-06 — ex_5_9_ssao（引擎真实 SSAO 演示）+ ex_5_8_deferred_shading（前向等效 32 盏光）

- **用户**：advanced_lighting 里延迟着色与 SSAO 两个示例还没做，能不能加一下。
- **引擎（blinn_lo 点光上限 8→32）**：LO 8.1 的卖点是 **32 盏点光**，前向要复刻就得支持 32。`blinn_lo_frag.glsl` 的 `MAX_POINT_LIGHTS 8→32`（该着色器无 shadow-sampler 数组，弱驱动安全；classic blinn/pbr 仍 8，往数组越界上传是静默 no-op），`renderer.cpp` `kMaxPointLights 8→32`。既有 demo 点光数 ≤8 完全不变。
- **ex_5_8_deferred_shading**（◐ 前向等效，非延迟）：引擎为前向+实例化，无 G-buffer 延迟管线；只复刻 LO 8.1 的**场景与数据** → 3×3 背包网格（`assets/models/backpack/backpack.obj`，LoadObj 需带 `assets/` 前缀；scale .5，LO 位姿）+ **全部 32 盏** srand(13) 随机彩点光（LO 位置/颜色 + 衰减 1/(1+0.7d+1.8d²)）+ 32 发光小方块；**无太阳、用方向光做 LO 硬编码环境光 Diffuse×0.1**（blinn_lo 方向光 ambient 未衰减 ×albedo = LO 的 `Diffuse*0.1`；此前 NoSun 全黑看不到背包——LO deferred 截图能看清正是靠这个 0.1 ambient）；albedo 用 raw（LO G-buffer 存 sRGB 原字节不解码）；LoScene 黑底线性直出。相机 (0,0,5) FOV45 4:3。
- **ex_5_9_ssao**（✅ 引擎真实 SSAO）：LO 9.ssao 是独立 deferred-style SSAO 链，引擎不能照搬内部；改以 **classic blinn + 引擎 SSAO**（blinn 整段光照乘 AO，效果比 pbr 只压 IBL 明显）：木地板 + container2 木箱簇 + LO 背包（直立）；一盏高亮顶光 + 暖填充；经典后处理但关 bloom/god-rays/TAA；**SSAO 默认开，空格 on/off 实时对照**。相机 (0,0,5) FOV45。
- **验证**：debug 构建通过；SSAO on/off capture 像素差 YAVG 0.166（YMAX 157），AO 集中在物体/地板接触处；既有 5 个 demo（ex_2_1/2_6/5_3/5_4/5_6）hidden 冒烟 exit=0 无回归；deferred capture 可见背包网格被彩色点光照明 + 0.1 ambient。
- **教训**：`ModelLoader::LoadObj` 打开的是裸相对路径（不像 AssetManager 自带 assets 根前缀）→ exe 目录里必须写 `assets/models/...`。

---

## 2026-09-06 — ex_5_6 泛光 sRGB 处理与 LO 对齐（最终：shader 侧解码，防驱动黑屏）

- **用户反馈**：泛光比 LO 亮、白色块下过渡不自然；随后“压得啥都看不到”→ 要求处理/参数与 LO 一致且要看得见。
- **根因**：LO 7.bloom 把 wood/container2 按 sRGB 加载（采样解码到线性、最后 gamma），引擎当线性字节直读 → 线性域 albedo 偏高 → 偏亮。
- **引擎（最终方案）**：**不依赖 GL_SRGB 内格式**（Intel 驱动对它采样返回 0 → 整片黑），改为 **shader 侧解码**：`Material::SetAlbedoSRGB(true)` → Renderer 上传 `u_albedo_srgb` → `blinn_lo` 里 `albedo=pow(albedo, vec3(2.2))`（数学与 GL sRGB 解码等价、各驱动稳）。`GetTexture(path,srgb)`/`@srgb` 缓存键、Texture 接口保留（GL 上传一律 raw）。`BlinnLoDiffuse(path, shininess, srgb)` 在 srgb 时设旗标。
- **ex_5_6**：木地板/container2 按 sRGB 解码；曝光 **1.5**（LO 演示的曝光拨盘值，sRGB 下光池不爆白）；bloom 全量叠加 + 阈值 1.0 + god rays 关；LO `1-exp` tone + gamma。默认正视即可见 pale 地板/木箱/白绿光晕。
- **验证**：debug 全量（串行）零警告；capture：地板/木箱/发光体自然可见、不过曝。
- **已知残留差异**：引擎 bloom 模糊为半分辨率、LO 全分辨率 → 光晕略宽；如仍需更“贴”的光晕可加全分辨率 bloom 开关。

---

## 2026-09-06 — ex_5_3 shadow_mapping（LO 5.advanced_lighting/3.1.3）LO-exact 化（5 章完成）

- **引擎（blinn_lo）**：新增 **方向光阴影** 可选开关 `Scene/Renderer::SetLoDirShadow(bool)` → `u_lo_dir_shadow`：blinn_lo 声明引擎 shadow map（slot4/light_view_proj/PCF）并加 `DirShadowLit`（5×5 PCF 亮部比例，同 classic blinn），LO-exact 方向光的 diffuse+spec 乘该因子（=LO 的 (1-shadow)）。默认关，其它 LO 无阴影端口不变。
- **ex_5_3_shadow_mapping** ← LO 3.1.3：50×50 木地板（UV 0..25 平铺）+ 3 个木箱（LO 位姿/绕 (1,0,1) 旋转；引擎 cube scale=2×LO）；单一暗方向光 ambient .09/diffuse .3/spec .3（LO lightColor 0.3、ambient 0.3×0.3），Blinn halfway shininess 64（SetLoBlinnSpec）；太阳 travel=normalize(2,-4,1)（LO lightPos(-2,4,-1)→原点）；LoScene 0.1 线性直出 + SetLoDirShadow(true)；相机 (0,0,3) FOV45 4:3。
- **验证**：debug 构建过；capture 800×600：木地板 + 木箱 + 地面方向光阴影 + 暗背景，与 LO 3.1.3（本来就偏暗）相符。
- **至此 5.advanced_lighting 章节示例全部 LO-exact**：normal_mapping / shadow_mapping / hdr_bloom。

---

## 2026-09-06 — ex_5_6 hdr/bloom（LO 5.advanced_lighting/7.bloom）LO-exact 化

- **引擎（composite）**：新增 **`u_lo_hdr_tone`**（PostProcessing/Renderer/Scene::SetLoHdrTone）：LO 6.hdr/7.bloom 的色调 `1 - exp(-x*exposure)` 后接 gamma（区别于默认 ACES+gamma 与线性直出两种模式）。引擎 bloom（亮度阈值 luminance>threshold + 高斯模糊 + composite 叠加）本就与 LO 同构。
- **ex_5_6_hdr_bloom** ← LO 7.bloom：黑底；木地板（wood.png 拷贝自 LO 资源）+ 6 个 container2 方块（LO 位置/绕 (1,0,1) 旋转/缩放，引擎 cube 单±0.5 故 scale=2×LO）；4 个 HDR 点光（白 5 / 红 10 / 蓝 15 / 绿 5，**无 ambient/spec、衰减 1/d²** = lo_attenuation c0/l0/q1）；4 个亮色自发光光源小方块；`SetLoHdrTone(true)` + bloom on（threshold 1.0、strength 1.0 全量叠加）+ exposure 1.0 + NoSun；相机 (0,0,5) FOV45 4:3。
- **验证**：debug 构建过；capture 800×600：木地板 + 方块 + 彩色光池/自发光光晕，符合 LO bloom 版式。
- **下一步**：同章最后 shadow_mapping（ex_5_3）。

---

## 2026-09-06 — ex_5_4 normal_mapping（LO 5.advanced_lighting/4）LO-exact 化

- **用户**：做 5.advanced_lighting 章节（逐场景进行中）。
- **引擎（blinn_lo 增强）**：
  - blinn_lo_frag 加 **normal_map** 采样（世界空间 TBN 用 dFdx/dFdy 导数推导，与 pbr 同款；`has_normal_map`）；
  - `Scene/Renderer::SetLoBlinnSpec(bool)` + uniform `u_lo_blinn_spec`：LO 4.normal_mapping 用的是 **Blinn(halfway)** 高光，而 LO 光照章节其他 .fs 用 Phong(reflect)；`SpecTerm` 按开关二选一。
- **ex_5_4_normal_mapping** ← LO 4：2×2 砖墙（brickwall.jpg + brickwall_normal.jpg，uv 单次映射，双面不剔除），点光 (0.5,1,0.3) **无衰减**（LO 4 无 attenuation）：ambient .1 / diffuse 1 / specular 1、材质 spec 灰 0.2（= LO 的 vec3(0.2)）；墙按 LO 慢慢翻转（单轴 X euler 累积避免轴向翻转）；白灯小方块 0.1；LoScene 0.1 + NoSun + SetLoBlinnSpec(true)。
- **验证**：debug 构建过；capture 800×600：砖墙+浮雕+朝灯处亮、上方白色小光点，与 LO 一致。
- **下一步**：同章节 hdr/bloom（ex_5_6）→ shadow_mapping（ex_5_3）。

---

## 2026-09-06 — ex_2_5 手电聚光（LO 5.3/5.4）+ 修复无太阳 LO 场景的默认方向光污染

- **用户需求**：做手电聚光（LO 5.3 light_casters spot）。
- **引擎**：
  - `SpotLight::lo_flashlight`（bool）：LO 5.3/5.4 手电语义 —— ambient **全局有效（锥外也有，且不衰减**，= LO else 分支），diffuse/spec 仅在锥内按 intensity*衰减；关时维持 LO 6 的 CalcSpotLight（全部锥化+衰减）。Renderer 上传 `spot_light_lo_flashlight[i]`；`blinn_lo_frag` 的 spot 循环按该开关分支。
  - **默认方向光污染（真因）**：Scene 恒有一个默认 `DirectionalLight`（ambient .05/diffuse 1/spec 1），`blinn_lo` 的 `LoDirLight` 总会加它 → 之前 ex_2_2/2.3/2.4（LO 里只有一盏点光、没有太阳）等于被偷偷多打了一层“太阳”，比 LO 原版亮。这就是用户感觉“2.2 bling 偏亮”的根因。→ 新增 `examples::NoSun(scene)`（清零方向灯三分量），ex_2_2/2.3/2.4/2.5 全部调用；ex_2_2 恢复 LO 精确值（ambient .1/diffuse 1/spec .5，撤掉上一轮 0.08/0.9/0.4 的临时压暗）。
- **宿主（example_app）**：`Setup::update` 钩子签名改为 `(Scene&, eye, front, dt)` —— 每帧渲染前把当帧相机眼点/朝向传给场景，供“手电跟随相机”使用。
- **ex_2_5_light_casters** ← LO 5.3/5.4：10 木箱（container2 + spec 贴图，LO 位姿/旋转轴），**只有一盏相机手电**（每帧 ClearSpotLights+AddSpotLight，position=eye、direction=front、cutOff 12.5/outer 17.5 软边、ambient .1/diffuse .8/spec 1、c/l/q 1/0.09/0.032），LoScene 0.1、NoSun、相机 (0,0,3) FOV45 4:3。
- **验证**：debug 构建通过；smoke 通过；capture 800×600：中央木箱被手电照亮、四周木箱只剩 0.1×container 的暗环境（与 LO 一致）；ex_2_2/2.3/2.4 NoSun 后重拍：crate 恢复木色不再过曝、coral 立方恢复 LO 明暗。
- **下一步**：5.advanced_lighting（shadow/normal/hdr）与 3.model_loading 逐场景 LO-exact 化（PBR 演示版仍保留）。

---

## 2026-09-06 — ex_2_2（LO 2.2）高光微调 + 投光物绕立方旋转

- **用户反馈**：2.2 blinn 版高光“bling”还是稍微亮一点；希望投光物能围绕正方体旋转。
- **宿主（example_app）**：`ExampleApp::Setup` 末尾加可选 **`update(Scene&, float dt)`** 每帧钩子
  （放在 fov 之后，保证既有 `Setup{build,name,...}` 位置初始化不错位），OnUpdate 在渲染前调用。
- **ex_2_2_blinn_lighting**：高光从 LO 的 0.5 微调为 **0.4**（ambient 0.08 / diffuse 0.9，整体略暗一点）；
  点光 + 白色灯源方块以 LO 初始位 (1.2,1,2) 为起点、半径 ~2.33、y=1 绕立方 **轨道旋转**
  （每帧 `ClearPointLights`+`AddPointLight` 刷新光位 + 移动 lamp entity 的 Transform）。验证：debug+release
  全量构建通过；capture frame20 vs frame600 灯已从右上转到左侧（亮面/高光随灯移动），高光明显变柔。
- 说明：此微调相对 LO 1:1 数值有意下调一档（用户观感），如要严格对照可回到 0.5/1.0/0.1。

---

## 2026-09-06 — 引擎 LO-exact 光照（blinn_lo）+ 固定 800×600 + 2.x 光照章节 1:1 复刻

- **用户选择 A**：把复刻路径做成 LO-exact；固定 800:600；批量对齐其它场景参数。
- **引擎（本次 dev 提交）**：
  - `light.hpp`：`DirectionalLight/PointLight/SpotLight` 各加 `ambient/diffuse/specular`（vec3，LO 的每灯三分量；旧 color/intensity 路径不变）。
  - `material.hpp`：加 **specular map** 槽（`SetSpecularMap`，对应 LO `material.specular` 贴图）+ 可选 **specular color**（`SetSpecularColor`，LO 无贴图材质的 `material.specular` 颜色）。
  - `renderer.cpp/.hpp` + `scene`：specular_map 绑到 12 号单元并上传 `specular_map/has_specular_map`、`u_material_specular_color(+has)`；每灯上传 amb/diff/spec 数组；`Renderer/Scene::SetLoLighting`（LO 模式开关）。
  - `application`：`SetStartupWindowSize(800,600)` 静态启动尺寸（默认仍 1600×900）；每个 LO example 的 `CreateApplication` 先设 800×600 → PostProcessing 自动按 800×600 建内部缓冲。
  - 新着色器 **`assets/shaders/blinn_lo_frag.glsl`**（复用 `blinn_vert`）：逐项复刻 LO .fs —— 每灯 ambient/diffuse/specular、Phong(reflect) 高光**不乘 NdotL**、specular 贴图采样、LO c/l/q 衰减仅在 `lo_attenuation` 时启用（2.2/3.1/4.2 无衰减）、无阴影/无 IBL 环境光。`manifest.json` 注册 `blinn_lo`。
- **示例（2.lighting 章节 1:1 LO-exact 复刻，800×600，`LoScene()` 模板）**：
  - `ex_2_2_blinn_lighting` ← LO 2.2.basic_lighting_specular（coral 立方 + 白灯 1.2,1,2；ambient 0.1/diffuse 1/spec 0.5）
  - `ex_2_3_materials` ← LO 3.1.materials（coral 材质 + specular(0.5 灰)，光 0.1/0.5/1.0）
  - `ex_2_4_lighting_maps` ← LO 4.2（container2 + container2_specular，shininess 64，光 0.2/0.5/1.0）
  - `ex_2_6_multiple_lights` ← LO 6（10 木箱 spec map 旋转 + dir 0.05/0.4/0.5 + 4×点光 0.05/0.8/1.0 + c/l/q；Lamp 白灯）
  - `example_helpers.hpp`：`BlinnLo/BlinnLoTextured/LoScene/Lamp`；`examples::LoScene` = LO 模式 + 线性直出 + 关 TAA/bloom/SSAO/skybox + 背景 0.1。
- **验证**：debug 全量构建通过；10 个 example 逐一 `--frames` smoke 全过（exit 0，800×600 帧缓冲）。4 个 LO-exact 场景各自 `--capture-frame` 出 800×600 PPM→PNG：木箱高光/材质渐变/灯位与 LO 版式一致（PBR 引擎演示场景保留原样，仅加 800×600）。
- **问题与解决**：把 LO 数学塞进大而全的 `blinn_frag.glsl` 后，Intel UHD 驱动在 glCompileShader 阶段**段错误**（0xC0000005，无声崩溃）。逐项二分：只加 uniform 声明不崩、加上 LO 函数体就崩 → 拆出**独立小型 `blinn_lo_frag.glsl`**（只含 LO 路径）即正常。`blinn_frag.glsl` 恢复为 HEAD 原样（经典 Blinn 路径保留）。
- **下一步**：LO 5.3（相机手电聚光，需宿主支持相机跟随 SpotLight + “锥外仅环境光”语义）；5.x advanced / 3.model_loading 逐一 LO-exact 复刻。

---

## 2026-09-06 — 后处理“线性直出” + LO 数值审计（修复 MEngine 方块整体偏亮）

- **用户反馈**：整体亮度仍偏高；要求逐项比对渲染参数是否与 LO 对齐。
- **根因（审计）**：引擎默认 composite 做 **ACES + gamma**，会把 LO 那种“未 tone map 的原始值”整体抬亮（且 sRGB 贴图字节被当线性再 gamma ≈ 2.2× 提亮）。LO 教程是直接把 shader 结果写回 framebuffer。
- **改动（8214d5e，dev）**：
  - 引擎：`Scene/Renderer/PostProcessing::SetLinearOutput(bool)`；composite `u_linear_output=1` 时 **clamp(hdr)，不做 ACES/gamma**（默认关闭，其余场景不变）。LO 端口调用 `SetLinearOutput(true)` + 曝光 1.0。
  - `ex_2_6_multiple_lights` 数值对齐：方向光 0.4(=LO diffuse 0.4)、IBL 0.15≈dir ambient 0.05、材质 spec 0.4（去掉“发白”）、背景 0.008 近黑、点光 0.8 + LO c/l/q。capture：黑背景、受光面亮、白灯，已接近 LO。
- **仍存的差异（列清单，待选）**：(1) LO 是 Phong(reflect)，我们 Blinn(H)；(2) LO spec 不乘 NdotL；(3) LO 每灯 ambient/diffuse/spec 分离且 ambient 乘衰减，我们只有 color+intensity；(4) LO 用 container2_specular 逐像素高光贴图，我们只有统一标量；(5) gamma 已可用 SetLinearOutput 对齐。
  → 下一步二选一：A) 给引擎把 blinn 路径做成 LO-exact（每灯三分量 + specular 贴图 + Phong 分支）；B) 停在“观感接近”。

---

## 2026-09-06 — Material 自发光 + ex_2_6_multiple_lights 调成 LO 观感

- **用户对比 LO 截图反馈**：我们的背景/环境太亮、灯位小立方不是“纯白发光”，不像 LO。
- **原因**：① 引擎后处理有 ACES tone map + gamma，线性 0.1 的背景会被抬成中灰；② Blinn 环境光开太大；③ 灯位小立方用的是“被照亮的彩色材质”，而 LO 的灯是**纯色自发光白块**。
- **改动（95e2d9e，dev）**：
  - 引擎：`Material::SetUnlit/IsUnlit`（pbr/blinn 都支持 `u_material_unlit` 时直接输出 albedo，做自发光/灯源），纳入合批比较；helper 加 `examples::Unlit(color)`。
  - `ex_2_6_multiple_lights` 调参：背景线性 ~0.008（gamma 后近黑）；IBL/环境光 = 0（暗面近黑，同 LO）；灯位立方 = 纯白自发光；曝光 0.85。capture 确认：黑背景 + 木箱 + 白灯，接近 LO 原图。
- **经验（供后续端口复用）**：引擎是 gamma-correct 管线，若要复刻 LO 的“不 gamma”观感，背景/环境值要按“最终 sRGB ≈ 目标”反算（LO clear 0.1 → 我们线性 ~0.01 级），环境光尽量 0，灯源用 unlit。

---

## 2026-09-06 — 引擎 axis-angle 旋转 + 首个 1:1 LO 端口（multiple_lights）

- **需求（选 A）**：给引擎加任意轴旋转，以便 LO 那种 `rotate(axis, angle)` 能 1:1 复刻。
- **改动（4b3b253，dev）**：
  - `Transform::SetRotationAxisAngle(axis, degrees)`：内部存成欧拉(度)，净旋转 == axis-angle；编辑器/动画/Lua/物理回写零改动。`ExampleApp::Setup` 增加 `fov`（LO 用 45°）。
  - helpers 增加 `examples::PutAxis(...)`（位置+轴角+缩放）与 `examples::BlinnTextured(...)`。
  - **`ex_2_6_multiple_lights` 重写为首个 1:1 LO 端口**：Blinn 管线 + container2 贴图 + LO 的 10 个立方体位置/绕(1,.3,.5)·20°·i 轴角旋转 + 4 个 LO 点光（lo_attenuation c1/l.09/q.032、白 ~0.8）+ 暗淡方向光 + 灰 .1 背景 + LO 相机(0,0,3) FOV45 望 -Z。capture 验证倾斜木箱/灯位标记。
  - 与 LO 的已知差异：窗口 16:9 vs LO 800×600(4:3)（横向取景略不同）；相机手电聚光省略（宿主为轨道相机）。
- **模板确立**：后续 2.lighting/5.advanced_lighting 逐场景按此套路 1:1 端口。

---

## 2026-09-06 — LO 光衰减 + examples 命名规则（为 1:1 复刻铺路）

- **需求**：1:1 复刻 LO 前，先把 LO 的点/聚光衰减（constant/linear/quadratic）做进引擎；给示例 target 起名时能直接对上 LO 章节/源码。
- **改动**：
  - **LO 光衰减（307256a）**：`PointLight`/`SpotLight` 增加 `lo_attenuation`(bool,默认 false) + `constant/linear/quadratic`（默认 1/0.09/0.032）。Renderer 上传每组灯光的 c/l/q 与开关数组；`pbr_frag` 与 `blinn_frag` 都支持：开 → `1/(c+l·d+q·d²)`；关 → 沿用原 radius 衰减（旧场景不受影响）。已验证编译+运行。
  - **命名规则（15362f6）**：示例 target/源文件统一为 **`ex_<LO顶层章>_<LO小节>_<名>`**（如 `ex_2_6_multiple_lights` = src/2.lighting/6.multiple_lights）。现有 10 个全部改名，CMake 注释里写死 LO 源码路径，`examples/PORTING.md` 更新为完整对照表。
- **发现/待决**：LO 常绕任意 axis-angle 旋转立方体；MEngine `Transform.rotation` 只有欧拉角(XYZ,度)。做像素级 1:1 前需决定：给 Transform 加 axis-angle，还是每场景按欧拉近似。下一步从 2.lighting 逐场景 1:1 端口（Blinn 管线 + LO 数值 + lo_attenuation）时定。

---

## 2026-09-06 — 引擎新增第二套 Blinn-Phong 光照管线（可与 PBR 二选一）

- **需求**：为让 LO 示例能 **1:1** 复刻（LO 本身是 Blinn-Phong），希望引擎提供“两套可选”的光照管线。
- **做法**（通用能力，默认不影响 PBR）：
  - 新增 `assets/shaders/blinn_vert.glsl`（= pbr_vert 同构）+ `blinn_frag.glsl`：**经典 Blinn-Phong**（漫反射=albedo*光；镜面=light_color*specular_intensity*pow(Blinn H, shininess)；环境光=albedo*ibl_intensity*0.35），且与 pbr 共用同套 uniform —— 方向/点/聚光数组、PCF+点光立方阴影、SSAO、法线贴图、`u_render_mode`、输出 alpha。
  - `Material::SetShader(GetShader("blinn"))` 即选用；新增 `Material::SetShininess`（默认 32），Renderer 上传统一 `material_shininess`（pbr 无此 uniform，自动忽略）。pbr 着色器零改动。
  - `example_helpers.hpp` 加 `examples::Blinn(color, shininess, spec)`；新演示 `example_blinn_lighting`（同 basic_lighting 构图、Blinn 高光）验证通过。
- **验证**：`example_blinn_lighting` capture 白立方+清晰高光+阴影、深背景；debug 全量零警告；提交 `82da1ba`（dev）。后续 1:1 复刻 LO 场景可用 Blinn 管线做高光更贴近原文。

---

## 2026-09-06 — examples 复刻 LearnOpenGL：贴图/法线贴图场景 + 全量对照表

- **用户动作**：把整个 LearnOpenGL 仓库（代码+resources）放进根 `LearnOpenGL/`（已加 .gitignore 不提交）；要求参考它把 src 示例用 **MEngine 公共 API** 复刻、代码整理到 `examples/`。
- **结论/结构**：全部 ~90 个底层示例不可能（也不该）一比一用公共 API 直译。新增 **`examples/PORTING.md` 全量对照表**：把 8 章 src 逐目录标注 → ✅ 已复刻 / ◐ 引擎等价 / ⛔ 裸 GL 底层特性（MEngine 内部或需引擎扩展）/ ⬜ 待做，方便查阅与后续推进。
- **本轮新增（都 headless capture 验证）**：
  - `example_lighting_maps`（LO 2.lighting/4）：container2 贴图木箱 + 方向光/近距点光高光；从本地 LearnOpenGL 拷贝 `container2.png/_specular.png`、`brickwall.jpg/_normal.jpg` 到 `assets/textures/`。
  - `example_normal_mapping`（LO 5.advanced_lighting/4）：UV 平铺砖墙平面（新 helpers `TiledPlane/TiledWall` 生成可平铺 UV 网格）+ albedo/normal 贴图（走引擎 pbr 法线槽，dFdx TBN）→ 侧光下砖缝凹凸清晰。
  - CMake demo 列表按章节分组（2.lighting、5.advanced_lighting）便于对照。
- **引擎能力核查**：`AssetManager::GetTexture(path)` 直接按 asset root 加载任意 png/jpg（stb_image、翻转 V、REPEAT+LINEAR）；`ModelLoader::LoadObj` 只有 mesh（无 mtl），glTF 才有完整材质；法线贴图可用。
- **提交**：`2aa09bc`（dev）。后续按 PORTING.md 推进：model_loading、gamma/SSAO 演示、PBR 贴图组等；反射/文本/2D 游戏等需引擎能力扩展再议。

---

## 2026-09-06 — examples：按 LearnOpenGL 复刻的“光照/高级光照”演示（每个场景一个独立可执行）

- **需求**：参考 JoeyDeVries/LearnOpenGL，用 **MEngine 自己复刻**光照/高级光照实现与测试场景；每个场景做成**不同可执行文件**，代码统一放 `examples/`；示例贴图资源版权用户确认无碍（可直接从 GitHub 下，后续再加纹理场景时用）。
- **做法**（引擎零改动之外的纯示例层，只走公共 API）：
  - `examples/` 新工程：共享宿主 `example_app.{hpp,cpp}`（轨道相机：右键拖拽环视）+ `example_helpers.hpp`（PBR 材质/放置/太阳/纯色背景小工具）。
  - **每场景一个 exe**：`example_colors / example_basic_lighting / example_materials / example_multiple_lights / example_light_casters / example_shadow_mapping / example_hdr_bloom`；各自输出到独立子目录（自带 assets 拷贝，避免 POST_BUILD 并发冲突）。
  - 引擎为此加了一个通用能力：**天空盒开关 + 纯色场景背景**（见下条）→ 例子都关掉 IBL 天空、用深色纯背景 + 调低环境光，观感对齐 LearnOpenGL 原示例（浅天空/云背景不再是干扰）。
  - 高级光照大多复用引擎已有能力：PBR 方向/点/聚光、方向光+立方体点光阴影(PCF)、HDR/Bloom、TAA。
- **验证**：headless 逐例 capture（`--hidden --frames 30 --capture-frame 25`）确认：colors=红褐立方深底；multiple_lights=彩色 3×3 立方+灰底；shadow_mapping=墙/盒/球+软阴影；hdr_bloom=近黑背景+亮灯球辉光。debug/release 全量零警告。
- **待续**：纹理类章节（lighting_maps/normal/parallax/ssao 等）可按同一模式继续加 exe（先下载容器/砖墙贴图到 assets/textures）。

## 2026-09-06 — 引擎：天空盒开关 + 纯色场景背景（通用能力）

- 之前 `RenderMeshes` 恒画 IBL 天空盒，示例与 LearnOpenGL 的深色纯背景不一致。
- 引擎新增：`Renderer/Scene` 的 `SetSkyboxEnabled(bool)/IsSkyboxEnabled()`、`SetBackgroundColor(vec3)/GetBackgroundColor()`；`PostProcessing::BeginScene` 接收清屏色。天空盒关掉时场景清成纯色背景；**IBL 环境光不受影响**。默认 `skybox_enabled=true`、背景黑 → editor/voxel/sandbox 行为不变（沙盒仍画天空）。

---

## 2026-09-06 — engine 新增 sound 模块 P1（miniaudio：2D 音效播放）

- **用户澄清**：要给 engine 加的是 **sound（音频播放）**，不是 voice（语音识别/合成）；范围 2D/3D 都要，先做 2D。
- **现状**：引擎此前**无任何音频子系统**（deps 无音频库、engine 无 audio 代码）。
- **改动**：
  - **vendor miniaudio 0.11.21**（单头文件 C，公共领域/MIT）到 `deps/miniaudio/`；实现只在 `deps/miniaudio/ma.c` 编译一次并自建 `miniaudio` 静态库（`mengine_quiet_third_party`），Windows 链 `ole32/winmm`，不污染引擎告警。
  - **`engine/src/audio/audio.hpp/.cpp`**：公共 API（**pimpl**，miniaudio 类型绝不外泄到引擎头文件）：
    - `AudioSystem`：**懒初始化**输出设备（`Initialize`/`IsAvailable`）、主音量、`LoadSound`/`Play`/`StopAll`，以及**无设备也可用的静态 `ProbeFile`**（解码探测时长）。miniaudio 0.11 引擎自己跑线程，**无需每帧 update**。
    - `Sound`：`Play/Stop/Pause/Resume`、`SetVolume/SetPitch/SetLooping/SetPan(2D 立体声平衡)`、`IsPlaying`、`GetDuration`；持 `Ref<AudioSystem>` 保证设备比声音活得久。
  - **`Application` 持有 `Ref<AudioSystem>`**（`GetAudio()`），构造创建/析构回收；**设备首次播放才打开**，无声卡环境自动降级为无害 no-op。
  - `mengine.hpp` 纳入 audio；新增测试资源 `assets/audio/beep.wav`（440Hz、0.7s）；`sandbox` 加 **`MENGINE_AUDIO_SELFTEST`** 环境门控自检（解码探测 + 有设备则试播 + 限帧退出）。
- **验证**：自检输出 `probe ok dur=0.699977s` → `Audio system ready (48000 Hz)` → `playing duration=0.699958s`；debug/release **全量零警告**（engine/sandbox/editor/voxel）。
- **下一步（P2/P3）**：OGG(stb_vorbis)/MP3/流式长音频 → **3D 定位**（ma_engine 底层已支持 listener+position；加 `AudioSourceComponent` + 场景序列化 + 编辑器 Audio 面板 + Lua `MEngine.sound.*` 桥）。

---

## 2026-09-06 — voxel 打磨③：水半透明 + 水下可视（引擎新增通用半透明通道）

- **用户反馈**：水应该是半透明的、可以看到底下；但“到水底下只看到穿模的感觉，看不到水底的方块”。
- **根因（两个真 bug）**：① 网格器把 `Water` 当成不透明块做剔面（旧 `IsOpaque` 含 water）→ 邻水的沙/石**侧面被剔除**，水下岸壁是一圈空洞（这就是“穿模感”）；② 水只画了一张**不透明、背面剔除的顶面**，人在水下面抬头直接看到天空、周围没有水感。
- **引擎（通用能力，不是为 voxel 加私货）**：
  - `Material` 加 `translucent` 标记，透明度走 `base_color_factor.a`；`pbr_frag` 现在输出该 alpha（默认 1，对既有渲染零影响）。
  - `IRHI` 加 `SetDepthWrite`；半透明物体**只读深度、不写深度**。
  - `Scene::RenderMeshes` 主通道拆成两遍：先批量画不透明并写深度，再把半透明物体**按到相机距离远→近**逐个混合绘制；半透明物体不进阴影/点阴影/SSAO 这类深度 pass（透明不投实心影）。
- **voxel**：
  - 网格器改用新 `IsOccluder`（水永不遮挡邻居面）→ 水下岸壁/海底侧面照常渲染，不再有“洞”。
  - `BuildChunkMesh` 拆出 opaque + water 两份几何；每个区块有水时多建一个**半透明（双面、alpha 0.72）ChunkWater 实体**，重建/remesh 时同步增删，水顶面从水下也能看到。
  - 相机处于水格内时叠加整屏“水下蓝雾”（gl_VertexID 全屏三角形 + alpha 混合，透明度随水深增大）——抬头不再裸看天空，远景带蓝色雾感，水底沙子/岸壁清晰可见。调试相机可用 `MENGINE_VOXEL_DEBUG_CAM="x,y,z,yaw,pitch"` 复现任意视角（含水下）。
- **验证**：截图确认 水面半透明看得到底下/沙滩、水下蓝雾+可见岸壁（不再空洞）；debug/release 零警告；debug 300 帧长跑稳定；整仓（editor/sandbox/voxel）debug 构建干净。交互手感（游泳/潜水视角）仍需真机试玩。

---

## 2026-09-06 — voxel 打磨②：水 / 更密的树 / 地下不再单一 / 去掉方块预览改十字准星

- **用户反馈**：① “树没了”；② “没有水”；③ “地下土和石头层是一条平面，和真的 MC 不太一样”；④ UI 上不想看方块预览，中间画十字准星即可。
- **改动**：
  - **树回来了且更密**：`WantsTree` 命中率回调到 ~3.2%（`%10000<320`），且**只在区块内部 2..13 格种**（树干+树冠绝不越块），树仅种在高于海平面的草地上（`h>kSeaLevel && 顶部是 Grass`）。出生草地清树改为**以出生点（spawn_x_/spawn_z_）为中心**、只删 Wood/Leaves（绝不挖水/地形）。
  - **水（海平面）**：新增 `Block::Water` + `TileWaterTop`（程序化“蓝波纹”瓦片）。列高 `h<kSeaLevel(=16)` 时从 `h..15` 填水；网格器对水**只生成 +Y 顶面**（无侧墙、整片水面），碰撞/地面判定把它当**可穿过**（`IsCollidable` 排除 Water）。`Respawn()` 改为向外一圈圈扫描**最近海洋区块**，出生在紧邻水的干燥沙/草岸上并朝海平视（yaw 指向海），保证第一眼就看到海。
  - **地下不再是单平面**：泥土层厚度改为**随列变化 3..5**（不再一刀切），石头里加**三维 value-noise 洞穴**（`y∈[3,h-5)`、阈值 0.32 挖空）+ **煤矿/砾石点**（Hash3 `r<0.012`→CoalOre、`r>0.985`→Gravel），挖下去能看到空腔与矿。
  - **高度场重调（关键修复）**：旧 2-octave 噪声在本 seed 近原点整片偏高（实测 17..30、均值~22），海平面 16 永远到不了 → “没水”。改为 **3-octave**（宏观山脊 + 丘陵 + 局部起伏）并把高度**以海平面上 2 格为中心**、振幅 40：现在任意区域都有海/岸/丘陵（实测 441 采样列里 146 列低于海面）。
  - **去掉 ghost 预览，改十字准星**：删除 ghost 实体/材质；`RenderMeshes` 后直接 `glBindFramebuffer(0)` + `glScissor/glClearColor` 画两条暗色细条（竖 2×14 + 横 14×2）做中心准星。
- **验证**：capture 截图确认——画面中央有海/沙岸/草岸/两边成片树 + 居中十字准星 + 蓝天；debug/release 零警告；debug 300 帧长跑稳定。**交互（趟水可过、双击飞行、放/拆、挖到洞穴与矿）需真机试玩确认手感**。

---

## 2026-09-06 — voxel 打磨：方块侧面 UV 立正 + 无限地形（区块流式加载）+ 双击飞行

- **用户反馈**：① 方块侧面纹理横竖错（草坪侧面出现“竖的绿条”、树皮条纹方向怪）；② “好像没做地形生成、只有固定地图大小”；③ 想要双击（空格双击）飞行。
- **改动**：
  - **侧面 UV 修正（真 bug）**：旧版沿用引擎 cube 每面固定的 corner→UV 表，但引擎 ±X/±Z 面的 v 轴并不沿世界 Y → 纹理在侧面上被转了 90°（水平绿条变竖条）。新版网格器改为**按顶点局部坐标派生 UV**：侧面 v 恒沿世界 Y（uv 永远“立正”），顶/底面 u=x,v=z。截图确认山体现在“草顶在下泥土侧面之上”，绿条不再竖着。
  - **无限地形 + 区块流式**：`voxel_world` 重写为**按需确定性生成的块图**（`unordered_map` 装 16×16×40 chunk，value-noise 支持负/任意坐标；树在区块内部 2px 边距内种，避免跨块写）。`voxel_app` 改为**围绕玩家流式加载**（半径 5 区块 = 121 个 chunk 实体，跨界重建一次）；修复玩家中心、破坏/放置/重网格化（改块只 remesh 本区块+邻块）。出生点清出一块草地。seed 可由 `MENGINE_VOXEL_SEED` 覆盖（默认 1337）。
  - **双击空格 = 飞行**（MC 习惯），F 仍可切换；飞行中空格/Shift 升降。README/日志更新。
- **验证**：capture 截图确认流式 121 chunk 渲染、侧面纹理立正、出生草甸；debug/release 零警告；debug 300 帧长跑稳定。**交互（走路跨区块/双击飞行/放拆）需真机试玩确认**。

---

## 2026-09-06 — 新增独立 target `voxel`：Minecraft-like 体素演示（A 地形 / B 移动碰撞 / C 放置破坏）

- **需求**：独立新 target 做 Minecraft 复制，A/B/C 全做；**引擎保持通用，只消费公共 API，不为 voxel 加私货**（未改 engine）。
- **做法**：CMake 根加 `MENGINE_BUILD_VOXEL` 开关 + `add_subdirectory(voxel)`；新目录 `voxel/`（自己 namespace `vox`）。
  - `voxel_atlas.{hpp,cpp}`：Block 注册表 + **CPU 程序化纹理图集**（16px 瓦片 + 1px 复制边框防线性采样串色），经 `Texture::SetData` 上传为 pbr albedo。
  - `voxel_world.{hpp,cpp}`：稠密体素存储 + 确定性 value-noise 地形（草/泥土/石头/沙/橡树），`BuildChunkMesh` 逐块剔面建 16×16 区块 Mesh（沿用引擎 cube 绕序/UV，世界坐标顶点）。
  - `voxel_app.{hpp,cpp}`：`Application` 子类——每区块一个 `Scene` 实体 + 共享材质；太阳/阴影/天空走现有 `Scene::RenderMeshes`；玩家 AABB-体素碰撞（逐轴推进+回退），第一人称鼠标，中屏体素 DDA 拾取，LMB 破坏/RMB 放置 + 黄色 ghost 预览，区块+邻居重网格化。
- **验证**：capture 截图确认草/石/沙地形 + 橡树冠 + 阳光阴影渲染正确（多次调树密度/树冠后）；debug/release 全链零警告；voxel 长跑 200 帧无崩溃。**交互（行走/碰撞/放/拆）需真机试玩确认手感**。
- **通用性**：引擎零改动；若后续把体素相关能力通用化（顶点色、任意纹理过滤、线框高亮等）再单独立项。

---

## 2026-09-06 — Timeline 对齐引擎习惯：动画时长(Length) + 标尺/可拖关键帧（修 playhead 仍锁 0）

- **用户反馈**：playhead/timebar“一直都是 0 改不动”，希望像 Godot/UE/Unity：能**设置动画时长**再调整。
- **根因（真正修好）**：上一版把 playhead 钳到 `max(duration,…)`，但 `SetAnimationTime` 仍按“关键帧最晚时间”钳制；只有一个 t=0 键时 duration=0 → 一拖 playhead 就被 `SetAnimationTime` 拉回 0。所以 timebar 看起来永远 0。
- **引擎改动**：Scene 增加**独立动画时长** `anim_length_`（默认 1s）：`SetAnimationLength/GetAnimationLength`；`SetAnimationTime` 钳到 [0, anim_length_]（与关键帧无关，随时可移）；`AdvanceAnimation` 按 anim_length_ 回绕/停表；时长与 loop 一起**持久化** `root["animation"]={"loop","length"}`（旧文件无 length → 载入回退 max(duration,1)）。`GetAnimationDuration` 保留为“内容最后键”仅作信息。
- **编辑器 UI 重写（Timeline）**：顶部 Play/Pause/Stop/Loop/Auto-Key + **Length(时长) 输入** + 可输入 playhead；下方画 **Godot/Unity 式关键帧图**：时间标尺（秒刻度）+ **T/R/S 三条轨道**——标尺/轨道上点击拖动 = 移 playhead，轨道上的**菱形关键帧可左右拖动改时间**（自动夹在相邻键之间保持有序），点击菱形选中后在 inspector 改 time/xyz、Delete；顶部 Key T/R/S 在当前 playhead 记录当前位姿；Add-key 用统一 `put_key`，不再误建空组件。Selection 用 (channel,time) 每帧解析索引，避免编辑期指针悬垂。
- **验证**：debug/release 全链零警告；`tools/run_smoke.py` ALL PASS（图表面板需在编辑器手动确认交互：拖菱形改时间、标尺拖 playhead、改 Length 后播放时长变化）。

---

## 2026-09-06 — Editor Timeline：可移动 playhead + Auto-Key（修复“第二个关键帧又落在 t=0 覆盖第一个”）

- **用户反馈**：建了第一个关键帧后移动物体，再按 Key，第二个关键帧仍落在 t=0 把第一个覆盖了（并疑惑“牵扯到碰撞箱，真实引擎怎么处理”）。
- **根因（工作流/UX，非引擎 bug）**：关键帧永远记录在“当前 playhead”；只有一个 t=0 关键帧时 `duration=0` → playhead 被禁用锁在 0 → 用户无法把时间走到下个时刻，于是每次 Key 都在 0。这与真实引擎一致：关键帧属于“当前时间游标”，要在不同时间打不同关键帧，必须先把游标移到那个时间（Unity/Blender/UE 都是这样；再加“auto-key 记录”把位移动作录制到当前帧）。
- **改动**（editor.cpp 仅 UI）：
  - 时间轴长度 = `max(duration, 1.0)`：**playhead 从一开就可拖/可输入**（即使只有一个 t=0 键），能走到任意时间再加第二个键。
  - 新增 **Auto-Key** 开关（默认开）：选中实体**已有 AnimationComponent** 时，Edit 模式下用 gizmo 移动/旋转/缩放会**在当前 playhead 自动记录**改动的通道（同一时刻=覆盖/更新；不同于时刻则新增）——还原 Blender auto-keyframe / Unity record 的体验；不会给普通未动画物体乱打键。
  - Play 条件改用 `HasAnyAnimation()`；面板顶部提示更新为“第 1 步放 t0 打第一键 → 第 2 步把 playhead 拖到更晚时间再用 gizmo 移动”。
- **验证**：debug/release 全链零警告；`tools/run_smoke.py` ALL PASS（交互流程需在编辑器确认）。
- **关于“动画 + 碰撞箱”**：见回复说明——真实引擎做法是让带碰撞体的动画物体用 **kinematic（运动学）刚体**，动画每帧写 Transform，引擎据此移动碰撞体去推动动态物体；静态/动态刚体由物理模拟移动，不应直接叠动画。engine 目前刚体只有 Static/Dynamic，kinematic-follow 作为后续项（需 PhysicsWorld 支持 Kinematic + 每步 MoveKinematic）。

---

## 2026-09-06 — Editor Timeline：关键帧时间戳可直接编辑（修复“改不了时间戳”）

- **用户反馈**：Timeline 里“改不了动画的时间戳”——初版只能把关键帧加到“当前 playhead”，没有地方改/输入某个关键帧的时间；且无关键帧时 duration=0、scrub 被禁用。
- **改动**（editor.cpp 仅 UI）：
  - 每个通道头部新增**“时间输入框 + Key”**：可先输入目标时间（默认跟随 playhead）再 Key，第一帧也能落在任意 t。
  - 每个关键帧改成可编辑行：**时间可拖/可输入**（DragFloat，自动夹在相邻关键帧之间，保持有序、防重叠）、**值 x/y/z 可拖/可输入**、`x` 删除；改动后立即 `SetAnimationTime` 刷新视口位姿。
  - Playhead 由 SliderFloat 改为 **DragFloat（可输入精确时间）**，范围夹到 [0, duration]。
- **验证**：debug/release 全链零警告，`tools/run_smoke.py` ALL PASS（该面板行为需在编辑器里手动确认：选中实体后可拖动/输入每个关键帧的时间）。

---

## 2026-09-06 — Editor：关键帧时间轴动画（AnimationComponent + Timeline 面板 + Play 自动播放）

- **需求**：在父子层级地基之上实现“关键帧时间轴动画”（DEV-PLAN 推荐路线 A）。
- **引擎改动**（component.hpp / scene.hpp/.cpp / scene_serializer.cpp）：
  - 新增 `Keyframe{time, value}` + `AnimationComponent`（translation/rotation/scale 三条时间排序通道，`Empty()/Duration()`）；旋转存 XYZ 度数与 `Transform` 一致。
  - `Scene` 共享时间轴时钟：`SetAnimationTime`（采样所有动画实体写入本地 Transform，即 scrub/预览）、`AdvanceAnimation`、`ResetAnimation`、`GetAnimationDuration/HasAnyAnimation`；`SetAnimationPlaying/Loop`。采样按通道线性插值、端外钳制、空通道不写。层级渲染自动让动画作用在**本地 Transform** 上（父实体动、子实体跟随——上一阶段的成果直接复用）。
  - `StartSimulation` 先回 t=0 再 CapturePlaySnapshot → 每次 Play 从起点确定性播放且 Stop 恢复 t0 位姿；`StepSimulation` 每帧推进时钟；`StopSimulation`/文件操作重置时钟。
  - **Loop 是场景级设置并持久化**：`root["animation"]={"loop":bool}`（含动画才写），独立沙盒与编辑器播放行为一致；实体动画 JSON 只存三通道（无 per-entity loop）。Editor 文件自检新增 “round-trip preserves animation” 检查。
- **编辑器改动**：新增可停靠 `Timeline` 面板（底部 Dock，View 菜单可开关）：Play/Pause/Stop/Loop + 时间线 scrub；选中实体三通道 **Key** 按钮（当前时间记录当前位姿；同刻覆盖/按时间排序）、关键帧点击跳转、`x` 删除（删空自动移除组件）、Remove Animation。组件只在实际加关键帧时创建（不会因打开面板产生空组件）。Duplicate 深拷贝动画。
- **验证**：
  - 像素探针：非循环 clip（t0 x=0 → t2 x=2）：跑大量帧后**末帧 = 静态终点立方体逐像素一致（mean diff 0.000）**（自动播放、越界后钳制停表、采样到末关键帧）；**早帧严格落在 x0 与 x2 之间**（从 t0 起步、随时间线性推进）。
  - Editor 文件自检 8/8 PASS（新增 animation round-trip）；`tools/run_smoke.py` ALL PASS；debug/release 全链零警告。
- **已知边界**：采样为线性、无贝塞尔/切线；时间轴 UI 初版（无多选关键帧/无缩放条）；与物理同实体的动画由“每帧后写”覆盖（动画优先，文档化）；Camera/灯光组件仍世界系。
- **下一步**：动画时间轴打磨（切线/曲线、自动打关键帧“auto-key”跟随 gizmo）或 P4 Script 编辑器。

---

## 2026-09-05 — Editor/Engine：父子层级 + 场景树（父实体移动子实体跟随）

- **需求**：用户确认“先实现父子层级场景树”，为后续 Editor 关键帧动画 / glTF 骨骼动画铺路（此前 `Transform` 无 parent/child，渲染把本地矩阵当世界矩阵用）。
- **引擎改动**（scene/component.hpp、scene.hpp/.cpp、scene_serializer.cpp）：
  - 新增 `RelationshipComponent{ entt::entity parent }`（entt 引入 component.hpp）；`Scene::SetParent(child,parent)`（拒绝自环/子树回环，parent=null 即解除）、`GetParent/HasChildren/GetChildren/IsDescendantOf/GetWorldTransform/GetWorldPosition/SetLocalTransformFromWorld`（glm::decompose 求逆父矩阵）。
  - `DestroyEntity` 改为**级联删除整棵子树**（后序遍历收集，子先于父），Lua `destroy_entity` 与内容清理复用同一路径。
  - `RenderMeshes` 模型矩阵改为 `GetWorldTransform`（阴影/主 pass 共用，父缩放/旋转/平移自动传给子）。
  - 物理：有父实体的刚体在**世界位姿**建体；`WriteBackTransforms` 把模拟出的世界位姿写回父系本地 TRS（保留子自身 scale）；根实体路径保持与旧版逐位一致（无回归）。
  - 序列化：`entities` 数组按创建序写入，可选 `parent`（数组内父索引，父为 editor-only 时省略）；加载两遍（先建全部再回链），**旧场景无 `parent` 字段 → 全部为根，完全向后兼容**。
- **编辑器改动**（editor.cpp/.hpp）：Scene 面板改为**可折叠树**（根节点 + 递归子树）；右键节点 Create Child/Delete/Duplicate/Unparent；**拖拽重父化**（节点 = 设为子，列表下方空区 = 解除）；Duplicate 深拷贝整棵子树并保持父链；Gizmo 作用于**世界矩阵**再写回本地 TRS；`F` 聚焦、Collider 线框改用世界变换。
- **验证**：
  - 像素探针：父在 (0,0,0)/(5,0,0) 两种摆放下，子实体（本地 (2,3,0) / (-3,3,0)）与世界 (2,3,0) 的根立方体**逐像素一致（mean diff 0.000）**→ 层级组合正确。
  - Editor 文件自检 8/8 PASS，新增 **“Save/reopen 保留 parent-child links”** 检查；`tools/run_smoke.py` ALL PASS；debug/release 全链零警告零错误。
- **已知边界（本阶段接受，已在记忆记录）**：Camera/灯光组件自身仍是世界系（父化相机暂不生效，属后续动画阶段）；重父化默认保留本地 TRS（世界位置会跳到新父局部）；非均匀父缩放下 decompose 求本地旋转是近似。
- **下一步**：Editor 关键帧时间轴动画（AnimationComponent + 时间轴 UI + Play/沙盒回放 + 序列化），或先做 P4 Script 编辑器完善。

---

## 2026-09-05 — 重写 SSAO：修复“开 SSAO 球比方块暗”的伪影（commit 9bf0436 之后）

- **用户反馈**：`test_02.scene` 只有正方体+球、无脚本，开 SSAO 后球仍比方块暗；要求“正经修一下 / 重写”。
- **根因**：旧 SSAO 链（`ssao` + `ssao_blur` 两张纹理，blur 后绑定）产出的 AO 是常数（实测 0.625），且 blur 通道让 AO 丢失空间差异；叠加在环境光上把凸曲面（球）整体压暗，平面（方块顶）反而少受影响。像素级验证：球心 AO=0.625、屏幕亮度 ON<OFF。
- **改动**：重写为**单张半分辨率 AO 纹理**：新增 `assets/shaders/ssao2_{vert,frag}.glsl`（LearnOpenGL 式半球核采样；无几何处跳过、`sample_depth >= sample_pos.z + bias` 才算遮挡、背景直接 1.0、清除时 clear=1.0）；`ssao.hpp/.cpp` 去掉 blur FBO/纹理/着色器，`Generate()` 只做几何+AO 两趟，`BindTexture(7)` 直接绑 AO 纹理；`assets/manifest.json` 增加 `ssao2` 条目；参数 `radius=0.3, bias=0.025`。
- **验证**：`_verify_fix.py` 像素测量——**球心 ssao ON=OFF=192.2**（不再被压暗）；`tools/run_smoke.py` 8/8 PASS；debug/release 全链构建通过。
- **已知残余（可接受）**：平面上仍有轻微 ~9% 环境光衰减（AO≈0.914，与 radius/bias 无关，属视空间 SSAO 相机俯仰下的切向采样常见小瑕疵）；球/凸面伪影已消除。接触阴影在俯视角度较弱，后续可视需要再调。
- **经验**：着色器探针改 `FragColor` 后**必须加 `return`**，否则原输出行会覆盖探针颜色，导致测到的是天空而非物体；下结论前先用校准曲线把屏幕值映射回线性值。

- **用户反馈（截图）**：同一帧里正方体都亮、球(及其投下的圆影)明显发暗，“就是有问题”。要求直接跑 `assets/scenes/test_01.scene`（未跟踪文件，用户另存）。
- **复现与量化**：跑真实 `test_01`（静态俯视 + Play 时 `main.lua` 发射 Ball）均复现“球比同材质方块暗”。同构图像素测量（IBL=0.4）：**球心 185 vs 方块顶 205**；把 IBL 提到 1.0：**球心 221 vs 方块顶 215**（球反超）。
- **结论**：球法线/绕序/光照方向均正确（多次像素验证），暗球原因是**默认环境光(IBL)太低**：球大量表面不直对太阳，几乎全靠环境光；方块平顶吃满直射光。`Renderer` 默认 IBL 本就是 1.0，是编辑器/已存场景把它写成 0.4。
- **改动**：editor 默认与场景加载回退 `ibl_intensity 0.4 → 0.8`（已存场景各自保留存值；Rendering 面板可逐场景覆盖）；用户 `test_01.scene`（未跟踪）已改 IBL=1.0 作演示。
- **验证**：`tools/run_smoke.py` 8/8 PASS；debug/release 全链零警告。
- **经验**：判断“光照是否反/是否 bug”别只看单物体绝对亮度——先做**同帧同材质对照**（球 vs 方块）+ 改环境光看是否消除，再下结论。用户强调后要直接跑他给的文件，别只做侧面/正面视角的孤立测试。

---

## 2026-09-05 — 渲染诊断：stress 场景剔除核查 + 默认背面剔除

- **分支**：`refractor`；commit：`2c4e523`(editor --scene + grid 双面)、`53453db`(render 默认背面剔除 + 平面绕序 + tools/ppm_to_png.py)
- **背景/用户反馈**：①“打开 stress 场景好像没有真的 cull”；②“紧密排列的方块被前面的挡住，应该算深度测试吧”；③“相机移进方块内部后看到一些叠在一起的面”。
- **核查结论（均已实测/截图）**：
  1. **视锥剔除确实生效**：`stress_cull`(1600) 在编辑器 Edit 视角 culled=1149/visible=451；`stress_10000` culled=9357/visible=643；沙盒同理。run_smoke 原来只断言 culled+visible==总数（culled=0 也能过），所以才显得“像没剔除”。
  2. **深度缓冲/深度测试本来就工作**（场景 FBO 带 DEPTH24_STENCIL8，主 pass 逐像素正确遮挡；从外部/内部截图均只显示应显示的面）。
  3. **根因（用户看到的“叠影/内壁”）**：材质默认 `CullMode::None`（不背面剔除）→ 进入闭合几何内部时会画内壁面；相邻立方体共享**共面**面被画两次（谁先画谁赢/穿越边界时交替），即“叠在一起”的来源。
- **改动**：
  1. `Material` 默认 cull 改为 `Back`（闭合不透明网格不再光栅化内部/背面）；需双面的对象显式 `CullMode::None`（编辑器网格底纹已显式）。
  2. `Mesh::CreatePlane` 原绕序几何法线为 -Y（从上看是背面）→ 改为 +Y，保证默认背面剔除下地面/平面从上方可见。
  3. Editor 支持 `--scene <path>` 启动即打开场景（Edit 模式，镜像 File→Open），便于无人值守复现/回归。
  4. 新增 `tools/ppm_to_png.py`（纯标准库 P6→PNG），便于查看 `--capture-frame` 截图。
- **验证（无头+像素）**：cube/sphere/plane 外部视角渲染正常；内部视角帧像素与改动前一致且更干净（共面输家面被剔除）；editor 默认场景截图正常、网格可见；`python tools/run_smoke.py --preset windows-clang-debug` → **8/8 PASS**；debug/release 全链零警告。
- **补充修复（同批，commit `463999b`）**：用户反馈“正方体顶/底面的方向不对”。复核 `Mesh::CreateCube` 六面绕序：+X/-X/+Z/-Z 正确，但 **+Y(顶) 几何法线为 -Y、-Y(底) 为 +Y**（内法线）——以前 `CullMode::None` 无所谓；默认背面剔除后从外部看顶/底被剔成“开口/方向不对”。已反转这两面的角点顺序，使外法线正确为 +Y/-Y；从正上方/正下方/侧面截图均为实心面，`tools/run_smoke.py` 8/8 PASS。
- **说明/下一步**：以上处理的是“面/像素级遮挡”。若还要**跳过整棵被完全遮挡物体的 draw（对象级遮挡剔除/早期-Z）**，那是独立的大特性（HZB 或 occlusion query），见 DEV-PLAN P3 延伸；需要时再单独做。

## 2026-09-05 — 球体光照方向核查（结论：正确；排查 UI 语义误导）

- **用户反馈**：“球体渲染亮暗似乎是反的”。
- **核查（像素级，临时场景 + 定向光）**：`Mesh::CreateSphere` 顶点位置法线/绕序向外（背面剔除下从外部可见）；受光方向验证：光源在 **+X** → 球右侧(迎光)亮、左侧黑；光源在**正上方** `(0,-1,0)` → 上半球亮、下半黑。均符合物理，**并非渲染错误**。
- **根因（误导点）**：`DirectionalLight.direction` 语义是“光线行进方向 = 背离太阳”（`light.hpp` 注释；`pbr_frag` 用 `L = normalize(-light_dir)`）。Lighting 面板把该值标成 “Direction”，若用户按“太阳所在方向”填（如想太阳在上填 `(0,1,0)`）→ 光源跑到底下，球就顶暗底亮，看起来“反了”。
- **改动**：Lighting 面板方向控件标签改为 **“Direction (travel)”** 并加 `(?)` 悬停提示（示例 `(0,-1,0)=太阳正上方`；纯显示，不改数据/格式）。commit 待记（editor:）。
- **另提醒**：`Material` 默认 `metallic=1.0`，新建的裸材质球在无 IBL 下会几乎全黑只剩高光/环境反射（易被误认为“反向”）；编辑器建球走 `CreateDefaultMaterial()`（metallic=0）不受影响。如需默认材质更“塑料感”，可单独把 metallic 默认改为 0（行为变更，另行评估）。

---

## 2026-09-05 — Editor File 菜单：新建/打开/关闭/保存场景

- **分支**：`refractor`；commit：`editor:` File menu scene new/open/close/save（本记录后提交）
- **做了什么**：
  1. `Scene` 新增内容管理（scene.hpp/.cpp/scene_serializer.cpp）：
     `ClearContent()`（停模拟→清脚本/主脚本→清渲染光源→移除内容实体，保留编辑器网格）、
     `OpenSceneFile(path)`（读取并套用 方向光/点光/聚光/渲染设置 + 实体 + main_script，
     不启动脚本，返回是否成功）、`StopSimulationIfRunning()` 与
     `RemoveContentEntities()`（抽出后供 `RestorePlaySnapshot` 复用）。
  2. Editor 顶部 **File 菜单**（editor.hpp/.cpp）：New Scene(Ctrl+N) / Open Scene…(Ctrl+O) /
     Save(Ctrl+S) / Save Scene As… / Close Scene / Exit。Windows 用原生对话框
     （`commdlg` `GetOpenFileNameA/GetSaveFileNameA`，`.scene` 过滤器）；非 Windows 编译走
     no-op 桩（返回“取消”）。打开/保存前统一 `ExitGameModeForFileOp()` 先回 Edit 静止态
     （Clear 脚本→StopSimulation→显示网格），避免把 Play 中间态写入文件。
  3. 隐藏自检：设 `MENGINE_EDITOR_SELFTEST_SCENE=<path>` 后，首帧自动跑
     New→Open→Save→重开 往返校验（内容实体数、网格保留、路径记录），结束恢复默认演示场景，
     供无人值守回归。
- **验证**：`windows-clang-debug`/`windows-clang-release` 全链编译零警告；
  editor 无头 `--frames 400 --hidden` 正常退出；方法级自检 **7/7 PASS**
  （new 清空 / 网格保留 / open 记录路径 / open 载入实体 / 写盘 / 往返实体数一致）。
- **遇到的问题**：
  1. windows.h 的 `ERROR`/`min`/`max` 宏 → `NOMINMAX` + `#undef ERROR`（编辑器 TU 内收敛）。
  2. `std::getenv` 在 Windows clang-cl 被标记 deprecated → 加 `_dupenv_s` 便携封装。
  3. `Scene` 无 `HasEntity` → 改用 `GetRegistry().valid()`。
  4. 原生对话框无法在无头模式下点击 → 用环境变量驱动“方法级自检”替代 UI 点击。
- **下一步**：P4 其余 —— 场景“另存为”未保存变更提示、Content Browser 双击 `.scene` 打开、
  Editor 面板化拆分 + Script Editor 以 Viewport 同窗 Tab 呈现（等宽+CJK+高亮）。

---

## 2026-09-05 — 全项目审查（另一模型合并 P3/P5 后）

- **做了什么**：对 `refractor` 上另一模型新增的渲染/P5 提交做全量检查。
  1. 阅读其 docs（PERFORMANCE.md / WORKLOG / DEV-PLAN）与 `tools/run_smoke.py`，确认记录与
     实际提交一致；PERFORMANCE.md/WORKLOG 均为有效 UTF-8（终端乱码只是控制台代码页显示问题）。
  2. 构建：`windows-clang-debug` / `windows-clang-release` 全链通过。
  3. 修复 1 个编译警告：`scene.cpp` 中 `std::getenv` 在 Windows CRT 被标记 deprecated →
     改为 `_dupenv_s`（跨平台保留 `std::getenv`）。
  4. 冒烟：`python tools/run_smoke.py` → **8/8 PASS**（physics 20s、stress_cull 统计断言
     dc=5 inst=5 culled+visible=1600、editor 冒烟）。
  5. 清理：删除约 30 个未跟踪产物（根目录 *.ppm/*.bmp/capture_*/*.tmp 与实验场景
     cullx_*/g*/twocubes、`.workbuddy/`），并扩展 `.gitignore`（*.ppm/*.tmp/capture_*/.workbuddy/）。
     确认冒烟/PERFORMANCE 引用的场景均为已跟踪文件，无需未跟踪文件参与复现。
- **结论/发现**：
  - P3 渲染优化（剔除+实例化+材质内容批处理+uniform 缓存+CullMode+像素捕获对照）与 P5
    （stress 场景/PERFORMANCE/run_smoke）实现完整且经过无人值守验证。
  - 遗留路线图（未实现）：P3.5 Vulkan 完善、P3.6 多线程渲染、P4 Editor 面板化拆分与
    Script 编辑器（Tab+CJK+高亮）——另一模型在 WORKLOG 中已注明“超大体量，建议单独会话”。
  - 性能数据在 Intel UHD + 隐藏窗口无 vsync 下采集，属参考值。
- **下一步**：如要继续完成路线图，从 P3.5 Vulkan 或 P4 Editor 拆分/脚本编辑器（CJK+高亮+Tab）
  开始；每阶段一个会话 + WORKLOG 记录。

---

## 2026-09-05 — P5 压测与冒烟交付（PERFORMANCE.md + stress 场景 + run_smoke）

- **分支**：`refractor`；commit：P5 交付批次（eff3eba/151dd1e 后续）
- **做了什么**：
  1. 正式压测场景：`assets/scenes/stress_{1600→stress_cull,4096,10000}.scene`
     （四色循环材质、单位立方体 XZ 平铺；python 脚本生成，格式与编辑器一致）。
  2. `docs/PERFORMANCE.md`：环境、复现命令、release/debug 数据表、与
     P3 前基线对比、结论与局限。
  3. `tools/run_smoke.py`：无人值守回归冒烟（物理场景真时 20s 驱动 +
     渲染统计断言 + editor exe 目录冒烟），退出码聚合。
  4. DEV-PLAN 顶部进度标注 + docs/README.md 索引登记 PERFORMANCE.md。
- **验证（无人值守）**：`python tools/run_smoke.py --preset windows-clang-debug`
  → **8/8 PASS**（sensor enter / impact / 无错误 / dc=5 inst=5 culled+visible=1600 /
  editor 初始化+RenderStats）。数据结论（release，隐藏窗口无 vsync）：
  1600 实体 789 fps、4096 → 480 fps、10000 → 230 fps，每帧恒定 5 个
  drawcall（1 阴影批 + 4 材质批）；10000 实体主 pass ≈1.1 ms、剔除 7840/2160。
- **遇到的问题**：
  1. 无 vsync 窗口帧率上千，固定“帧数预算”给物理的时间≈0（300 帧只有
     0.15 s 模拟）→ 冒烟脚本改为**真实时长驱动**（20 s 墙钟）后 sensor/
     impact 事件如期出现。
  2. editor 从项目根启动会卡死在 ImGui 字体加载（`res/fonts` 相对 exe
     目录；从项目根无此路径时 AddFontFromFileTTF 异常挂起）→ 冒烟与日常
     使用均以 **exe 所在目录**为工作目录；已在冒烟脚本中固化该约定。
- **下一步**：P3.5 Vulkan 补全、P3.6 多线程渲染、P4 Editor 面板化拆分
  均为超大体量工程，已评估暂缓，保留 DEV-PLAN 路线图与 WORKLOG 断点；
  建议按“每阶段一个独立会话”推进。

---

## 2026-09-05 — P3 渲染优化主体完成（无人值守 + 像素级验证）

- **分支**：`refractor`；commit：`76e8471`(stats+运行参数)、`384bdf2`(视锥剔除)、
  `23c620a`(uniform location 缓存)、`f0c416a`(实例化+mesh 共享+材质内容批处理)、
  `b8965a2`(像素捕获工具+batch/材质比较修复)、+CullMode（本次收尾）
- **做了什么**：
  1. **运行设施**：`--scene/--frames/--api/--hidden/--capture-frame/--capture-out`；
     `Application::Run` 帧预算与捕获改用**总帧计数**（修复：原先误用每秒重置的
     FPS 滚动计数，低帧率下永不触发）。
  2. **渲染统计**：Renderer 帧级 drawcalls/triangles/instanced/culled 计数 +
     Scene 各 pass 计时（shadow/point/ssao/main/skybox/post），每 120 帧输出
     一条 `[RenderStats]`（无人值守友好）。
  3. **CPU 视锥剔除**：Mesh 缓存本地 AABB；每帧一次性预计算渲染项（model+世界
     AABB）；主 pass/SSAO 按相机视锥剔除（Gribb-Hartmann），阴影 pass 保持全量；
     方向光阴影体改由世界 AABB 并集拟合（替代逐顶点扫描）。
  4. **GL uniform location 缓存**：SetUniform 不再每次 glGetUniformLocation
     （此前 PBR 每 draw ~40 次查询，是大场景主要 CPU 瓶颈）。
  5. **GPU 实例化**：`IVertexArrayBackend::SetInstanceData`（location 3..6、
     divisor 1）+ `IRHI::DrawIndexedInstanced`；pbr/shadow/point-shadow/ssao
     顶点着色器全部改为 per-instance model；四个绘制方法提供 Instanced 变体；
     Scene 各 pass 按 mesh（阴影/SSAO）或 (mesh+材质内容)（主 pass）批处理。
  6. **共享网格与材质内容批处理**：`AssetManager::GetMesh(source)` 让同源
     网格共享一个 GPU 对象；主 pass 分组用**材质内容等价**（全字段 memcmp 风格
     比较，排序用无指针内容全序）——发现并修复 glm vec4 关系运算在该工具链
     debug 构建下误判（蓝绿材质被判相等），改用标量逐分量比较。
  7. **CullFace 状态**：`CullMode {None,Back,Front}` 到 Material/序列化/渲染
     全链路（`"cull": "back|front"`，缺省 None 向后兼容）；绘制后还原 None
     以免影响 ImGui/2D。验证发现内置 cube/plane/sphere 绕序本就是外部 CCW
     （可直接用 Back 剔除）。
  8. **像素捕获验证管线**：`IRHI::ReadBackBuffer` 输出 PPM；`MENGINE_NO_BATCH=1`
     让主 pass 逐实体绘制作为对照。
- **验证（全部无人值守，读 mengine.log + 像素 diff）**：
  - stress_cull.scene（1600 cube，默认正交相机可见 428）：
    drawcalls **2028 → 5**（阴影 1 + 主 4 个材质批）、triangles 不变 24336、
    culled=1172（1172+428=1600 自洽）；debug shadow 1.7→0.16ms、main 8.3→1.3ms；
    release main≈0.2ms。
  - **像素级一致**：batch 与 MENGINE_NO_BATCH 逐实体对照，1440000 像素 0 差异
    （1600 实体场景、第 250 帧 TAA/后处理就绪后）。
  - CullMode 三态场景（相机置于立方体内部）：Back 剔除后中心=天空、Front 保留；
    外部视角 Back 画面与 None 一致（外表面 front 正常显示）——剔除方向正确。
  - 回归：physics_test（物理日志正常、300 帧干净退出）、默认 demo、release 双配置
    编译通过。
- **遇到的问题**：
  1. glm `vec4` 的 `==/!=/<` 在 clang debug 构建对某些值误判（蓝/绿材质被判
     相等导致错误合批、画面偏色）→ 改为 `memcmp`+标量比较后精确分 4 组
     （114/102/122/90 = 可见四色数）。
  2. 帧预算此前用每秒重置的 FPS 计数 → 低帧率场景永不退出（表现为前台运行
     超时），改用总帧计数修复。
  3. **git 事故**：一次 `git stash` 被中断（SIGTERM）损坏了 `.git/refs` 与
     全部子模块 gitdir（HEAD/部分 objects 丢失）。已从 reflog 重建
     `refs/heads/refractor`；子模块 worktree 源码完好且构建不受影响；把损坏的
     gitdir 备份至 `.git/_broken_modules_backup/`，worktree 内 `.git` 指针改名为
     `.git.bak-modules`（git 视子模块为未初始化，status/commit 恢复正常）。
     ⚠️ 需要用户后续执行 `git submodule update --init`（配网）或手工重建
     子模块 gitdir 才能恢复子模块版本管理。
- **下一步**：P3 残余（Editor 渲染统计展示、Pass 链抽象）、P3.5 Vulkan、
  P3.6 多线程、P4 Editor 面板化、P5 压测（stress 场景+PERFORMANCE.md 已在筹备）。

---

## 2026-09-05 — P2 进阶：CCD/Sensor/Raycast + physics_test 场景（已无人值守验证）

- **分支**：`refractor`；commit：`60f7d5f`(physics CCD+sensor flags)、`755da48`(physics raycast)、
  physics_test 场景+脚本 commit。
- **做了什么**：
  1. CCD/Sensor：`RigidBodyComponent` 新增 `continuous_collision`(Jolt LinearCast 防穿透) 与
     `is_sensor`(触发器，无物理响应但仍产生接触事件)。Body creators 追加两个开关并写入
     `BodyCreationSettings.mMotionQuality/mIsSensor`；Scene/序列化/Editor(勾选框)/Lua
     (add_component('rigid_body',type,fric,rest,ccd,sensor)) 全部打通。
  2. Raycast：`PhysicsWorld::Raycast`(NarrowPhase 最近命中) → `Scene::Raycast` 映射回实体 →
     Lua `MEngine.raycast(ox,oy,oz,dx,dy,dz[,max])` 返回 (entity, distance)。
  3. 新增 `assets/scenes/physics_test.scene` + 脚本（physics_test.lua 驱动 + impact.lua +
     sensor.lua），涵盖胶囊/圆柱/复合体(两盒一球)/Sensor 穿越/射线每帧检测。
- **验证（无人值守，sandbox --scene + 读 mengine.log）**：日志确认——复合体穿传感器 enter/exit 计数、
  三种形状落地 impact 速度、每 1s raycast 命中最上层可移动体且距离正确；无穿透/断言。
- **遇到的问题**：Jolt RayCastResult 需 include CastResult.h；StaticCompoundShapeSettings 是
  具体类(基类抽象)。
- **下一步**：P2 剩余可选项(Overlap/ShapeCast/睡眠重力缩放配置可延后)→ P3 渲染优化
  （批处理/实例化/视锥剔除/CullFace/Pass 链与 Stats）→ P3.5 Vulkan → P3.6 多线程 →
  P4 Editor 拆分与 Script(Tab+中文+高亮) → P5 压测。

---

## 2026-09-05 — P1 收尾 + P2 起步（多碰撞盒/复合体）

- **分支**：`refractor`；commit：`6bc53b5`(refactor core: 移出 ImGui context)、`2e5b058`(physics: capsule/cylinder)、`6258bc8`(physics: compound collider group)
- **做了什么**：
  1. P1：`Application` 不再创建/持有 ImGui context（Editor 自己建），引擎核心彻底不再直接依赖 imgui；
     连同 `base.hpp`/common.hpp 去 imgui，完成 core 层 UI 解耦的主体（GLFW/glm 显式化仍在 TODO）。
  2. P2-A 常用形状：ColliderComponent 增加 Capsule/Cylinder（PhysicsWorld 新增
     CreateCapsuleBody/CreateCylinderBody；Scene 建体分发；序列化 round-trip；
     Editor 面板四种形状 + gizmo 外接盒近似；Lua add_component('collider','capsule'/'cylinder',r,half_h)）。
  3. P2-B **多碰撞盒/复合体**：新增 `ColliderGroupComponent`（可存多个 ColliderShapeData）。
     - `PhysicsWorld::CreateBody` 用 `StaticCompoundShapeSettings` 把多个形状合成一个体
       （每个形状带 local offset；body 中心=实体 transform）。
     - Scene：建体/移除判定纳入 collider group；复合体的写回不再减 primary offset；
       body 同步覆盖 group 实体。
     - 序列化保存/读取 `collider_group`（形状数组）；Editor 增加 “Collider Group”
       组件入口 + 检视面板（添加/删除/编辑每个形状）；Lua 增加
       has_component / add_component('collider_group', shape, …) / remove_component。
     - 顺带修复：加载场景时 ColliderComponent 未重新 AddComponent 的回归。
- **验证**：`windows-clang-debug` 与 `windows-clang-release` 全链编译通过。
- **遇到的问题**：Jolt `CompoundShapeSettings` 是抽象基类，需改用具体 `StaticCompoundShapeSettings`；
  局部 constexpr 数组不能隐式捕获于无捕获 lambda。
- **下一步（P2-C…）**：CCD/阻尼/重力缩放/sleep 配置；Raycast/ShapeCast/Overlap 查询 + Sensor
  (OnTrigger*)；`physics_test` 场景与脚本；随后 P3 渲染 / P3.5 Vulkan / P3.6 多线程 / P4 Editor /
  P5 压测。细节与进度会持续记录于此。

---

## 2026-09-05 — P1 续：core 去 imgui 依赖 + Ref 抽 base.hpp

- **分支**：`refractor`；commit：`134dee4`、`0523051`（另有前面 `docs…`/`7145267`）
- **做了什么**：
  1. 新建自包含 `engine/src/core/base.hpp` 承载 `Ref/CreateRef`，`common.hpp` 改包含之。
  2. 从 `core/common.hpp` 移除 `<imgui.h>/<imgui_internal.h>`：确认 imgui 实际只被
     `application.cpp`、RHI 的 ImGui 后端实现、editor 使用。
     - `application.cpp` 显式 `#include <imgui.h>`；
     - `rhi.hpp` 仅用 `ImDrawData*` 指针 → 前置声明，保持 render/core 不依赖 imgui；
     - `editor.cpp` 显式引入 `imgui.h + imgui_internal.h`。
  3. GLFW/glm 暂留在 common.hpp（平台/窗口依赖），后续再做 platform 边界收敛。
- **验证**：`windows-clang-debug` 与 `windows-clang-release` 全链编译通过。
- **遇到的问题**：`Application` 构造里直接 `ImGui::CreateContext()`（随后被 Editor 覆盖丢弃）是
  分层遗留；已记入待办（主循环/应用层职责清理）。本次仅做 include 边界，不改行为。
- **下一步**：P1 剩余 —— (a) application.cpp 中 ImGui context 移交给 Editor（engine 去 UI）；
  (b) platform/GLFW 显式化；(c) 接口/注释/LOG 审计；(d) 主循环/运行时抽象。

---

## 2026-09-05 — P1 起始：core 依赖收敛（base.hpp）等

- **分支**：`refractor`；commit：`docs…`、`7145267`(fix utils profiler)、`134dee4`(refactor core base)
- **做了什么**：
  1. 新建 `engine/src/core/base.hpp`：**自包含、零依赖**地承载 `Ref/CreateRef`；
     `common.hpp` 改为包含 base.hpp（避免重复定义）。
  2. 说明：`common.hpp` 里 imgui/GLFW/glm 的聚合仍保留以保证既有 TU 编译；下一步把
     imgui（仅 `application.cpp` 真正使用）从 core 头剔除，需要为 editor/application
     显式引入 imgui 头后再删，作为“依赖收敛”的后续 commit。
- **验证**：`windows-clang-debug` 编译通过（engine/sandbox/editor 全链）。
- **遇到的问题**：`application.cpp` 在引擎层直接 `ImGui::CreateContext()`（且其 context 后被
  editor 覆盖丢弃）是历史遗留的分层问题，已记录，纳入后续“主循环/应用层职责”清理范围。
- **下一步**：把 imgui/GLFW 从 core/common.hpp 剔除并显式加入使用方；随后接口/注释/LOG 审计。

---

## 2026-09-05 — P0：文档骨架 + profiler 头文件修复

- **分支**：`refractor`
- **做了什么**：
  1. 建立 `docs/DEV-PLAN.md`（总计划 + 分支/提交地图）与 `docs/WORKLOG.md`（本日志）；
     并在 `docs/README.md` 文档索引登记这两个文件。
  2. 审查 `engine/src/utils/profiler.h` / `profiler.cpp`，确认用户提示的头文件问题：
     - 头文件**非自包含**：使用 `std::this_thread`/`std::thread::id`/`std::string`/`std::hash`
       却未包含 `<thread>`/`<string>`/`<functional>`（依赖 TU 里其它头侥幸编译）。
     - `Profiler(const std::string&)` 构造用 `name_.c_str()` 存 const char* 指针 →
       若传入临时 string 会**悬垂指针**（析构时才读 name_，use-after-free 风险）。
     - `Dump()` 里 pid 与 tid 都取线程 id 的 hash（pid 实为线程 id，且 JSON 又硬编码 `"pid":0`）。
     - `profiler.cpp` 在 `PROFILER_ENABLED=0` 时仍会在 CWD 创建/截断 `profile_results.json` 并逐条 flush。
  3. 重写 `profiler.h`/`profiler.cpp`：头文件自包含、name 改为持有 `std::string`、
     pid/tid 语义正确、仅在 `PROFILER_ENABLED` 开启时落盘。
- **验证**：`cmake --build --preset windows-clang-debug` 与 `windows-clang-release` 均通过。
- **遇到的问题**：无（均为静态审查发现）。
- **下一步**：P1 —— 拆分 `core/common.hpp` 中 imgui/GLFW 等依赖，推进框架分层。
