# MEngine 整理重构 —— 执行日志（WORKLOG）

> 逐条记录：日期 / 分支 / commit / 做了什么 / 如何验证 / 遇到的问题与解决 / 下一步。
> 配套总计划见 [DEV-PLAN.md](./DEV-PLAN.md)。每条以“执行记录”为单位，新记录加在最上方。

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
