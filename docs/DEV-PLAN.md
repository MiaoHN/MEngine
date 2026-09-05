# MEngine 整理重构 —— 开发计划（DEV-PLAN）

> 本文档是 MEngine “整理与重构”专项的**总目标 + 任务拆解 + 分支/提交地图**，与
> [WORKLOG.md](./WORKLOG.md)（执行日志）配套。改动以“每个可验证单元一个 commit”为原则，
> 每完成一项就同步更新 WORKLOG，保证可回溯、可检查、可断点续跑。

- 创建日期：2026-09-05
- 工作分支：`refractor`（推进中）
- 相关执行细节、遇到的问题与验证结果请见 [WORKLOG.md](./WORKLOG.md)。

---

## 1. 目标

把 MEngine 整理成：**框架分层清晰、注释/日志完备、物理通用、渲染高效（可扩展到 Vulkan 与多线程）、
Editor 结构不臃肿且操作贴近主流引擎**；全程文档化；提供带大量实例的测试场景并实测帧率。

## 2. 范围与任务拆解

### P0 基线 & 文档骨架（docs/）
- 全量构建 + 启动 Editor 确认基线可用。
- 建立 `DEV-PLAN.md` 与 `WORKLOG.md`，登记到 docs 索引。
- 修复发现的基础头文件问题（例如 `utils/profiler.h` 非自包含、字符串悬垂指针等）。

### P1 框架梳理与优化（`refactor/framework` 分支）
- **依赖边界收敛**：`core/common.hpp` 目前混入 imgui/GLFW/glm 等，需要拆分出
  `core/base.hpp`（Ref/CreateRef/基础类型）与平台相关头，保证 core 不反向依赖 UI/窗口后端；
  logger、UUID、资源管理等归属清晰。
- **接口/注释/日志审计**：public API 补注释；关键路径补 `LOG_TRACE/DEBUG`；
  收敛重复代码（默认材质/网格、场景序列化、RHI 后端创建等）。
- **主循环抽象**：抽取 Editor/Sandbox 重复的
  “StepSimulation → Update → Render”为引擎层运行时对象，入口只保留差异。

### P2 物理完善（`physics/advanced` 分支）
- **多碰撞盒**：`ColliderComponent` 支持一个实体挂多个子碰撞盒（Jolt `CompoundShape`），
  编辑器面板可增删/排序子形状；碰撞事件可区分到子形状。对 Lua/序列化保持兼容。
- **常用形状**：Capsule / Cylinder / StaticPlane / ConvexHull / Trimesh。
- **更真实的模拟与查询**：CCD(LinearCast)、阻尼/重力缩放/sleep 配置；
  Raycast / ShapeCast / Overlap 查询（含 layer 过滤）暴露给 Lua 与编辑器；
  Sensor/Trigger（OnTriggerEnter/Stay/Exit）。
- **测试场景**：`physics_test.scene` + 脚本，输出与预期一致（看 mengine.log）。

### P3 渲染优化（`render/perf` → `render/vulkan` → `render/multithread`）
- **Drawcall 优化**：静态批处理 + 实例化（`glDrawElementsInstanced`，per-instance model）。
- **Culling**：CPU 视锥剔除（AABB）；CullFace 状态按材质/网格配置（front/back/none）。
- **Pass 链 / RenderGraph 雏形**：把 `Renderer` 大流程拆成可开关、可计时的 Pass 链；
  统计窗口显示 drawcall / 三角形 / 剔除数量。
- **Vulkan 完善**：A) swapchain/command/sync 真正跑通并正确显示 lit 场景；
  B) 资源/descriptor 抽象补齐（纹理/UBO/采样器）；C) shadow / post / skybox 对齐，`-api vulkan` 可跑 Editor。
- **多线程渲染**：主线程逻辑（固定步物理+脚本）与渲染线程并行（命令双缓冲、管线化 1–2 帧延迟），
  大量实例场景验证 FPS 与稳定性。

### P4 Editor 重构（`editor/refactor` 分支）
- 拆分臃肿 `Editor` 类为：`Panel` 基类 + SceneHierarchy / Properties / Viewport / ContentBrowser /
  Log / Stats / Script 面板；EditorCamera / Gizmo / Selection / DragDrop / PlaySession。
- **Script 编辑器 = Viewport 同窗口的不同 Tab**（如 “Game / Script”），工具栏保留 Play/Stop/Launch。
- 等宽字体 + **CJK 中文/UTF-8 支持**；**Lua 语法高亮 + 行号 + 括号匹配**；错误行号可跳转。
- 交互对齐主流引擎：Ctrl+Z/Y、F 聚焦、右键菜单、拖拽赋值、文件重命名/删除等。

### P5 综合验证与压测（`test/performance` 分支）
- `stress_test.scene`：数百到数万实例（静态/动态），演示批处理/实例化/剔除收益。
- Stats 显示 FPS / 帧时间 / drawcall / 三角形 / 可见数；提供自动 Play 冒烟便于无人值守验证。
- 回归 collision demo / physics test / stress test；交付 `docs/PERFORMANCE.md`（命令、环境、数据、结论）。

## 3. 分支 / 提交纪律

- 分支：以 `refractor` 为集成分支；按阶段建立 `refactor/framework`、`physics/advanced`、
  `render/perf`、`render/vulkan`、`render/multithread`、`editor/refactor`、`test/performance`，
  完成后合并回 `refractor`。
- commit 前缀：`refactor:` / `physics:` / `render:` / `editor:` / `scene:` / `test:` / `docs:` / `fix:`。
- 每次改动先验证 `windows-clang-debug` 与 `windows-clang-release` 编译通过再 commit；
  行为性改动用 Editor Play/Standalone + `mengine.log` + 帧率验证。
- 兼容性红线：场景 JSON 文件格式、Lua API、已有资源与脚本保持不变。

## 4. 文档-提交绑定规则

- 代码 commit 里带上对应的文档变更；若来不及，则在 WORKLOG 追加与该 commit message 关联的记录。
- 每次阶段性完成，把“下一步 + 待办”写回仓库记忆（/memories/repo/build.md 或 session 记录），保证断点续跑。

## 5. 待办速查（当前进度）
最新执行进度请见 [WORKLOG.md](./WORKLOG.md) 顶部；总任务状态以 DEV-PLAN 各阶段清单为准。
