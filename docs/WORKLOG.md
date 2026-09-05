# MEngine 整理重构 —— 执行日志（WORKLOG）

> 逐条记录：日期 / 分支 / commit / 做了什么 / 如何验证 / 遇到的问题与解决 / 下一步。
> 配套总计划见 [DEV-PLAN.md](./DEV-PLAN.md)。每条以“执行记录”为单位，新记录加在最上方。

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
