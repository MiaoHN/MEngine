# MEngine 文档

> 一个「Just for fun」的 C++17 游戏引擎：ECS + Lua + OpenGL 4.6，**2D 与 3D 都是一等公民**，
> 配 ImGui/ImGuizmo 编辑器。当前主线是三套材质管线（`pbr` / `blinn` / `blinn_lo`）+ 阴影/IBL/SSAO/
> 体积光/TAA，以及独立的 2D 精灵渲染路径。

**先看这里**：[构建与工具链](./build-and-toolchain.md) → [架构总览](./architecture.md) →
[2D 工作流](./2d.md) / [渲染层](./rendering.md) / [场景层](./scene.md) → [编辑器](./editor.md)。

## 文档地图

### 上手

| 文档 | 内容 |
| --- | --- |
| [build-and-toolchain.md](./build-and-toolchain.md) | 构建系统、CMake Presets、跨平台工具链、如何在 CI 里跑 |
| [architecture.md](./architecture.md) | 顶层结构、模块划分、数据流、设计模式 |
| [core.md](./core.md) | 核心层：Application 生命周期、Logger、ScriptEngine、Input、Audio 等 |

### 渲染

| 文档 | 内容 |
| --- | --- |
| [rendering.md](./rendering.md) | 渲染层全貌：`Renderer` 阶段式接口、RHI/后端、材质与光照、**2D 渲染路径**、平铺、现状与局限 |
| [PERFORMANCE.md](./PERFORMANCE.md) | 压测环境/命令/数据（视锥剔除 + 实例化合批的收益）与无头冒烟方法 |

### 2D 与场景

| 文档 | 内容 |
| --- | --- |
| [2d.md](./2d.md) | **2D 工作流**：场景维度、`SpriteComponent` / 精灵动画 / 平铺、Lua 控制角色、Launch 与 sandbox2d |
| [scene.md](./scene.md) | 场景层：ECS(EnTT)、Entity、组件清单、`Scene` API、相机、两条渲染路径 |
| [asset-management.md](./asset-management.md) | 资源管理：`AssetManager`、共享 `assets/`、manifest、路径解析规则 |

### 编辑器与流程

| 文档 | 内容 |
| --- | --- |
| [editor.md](./editor.md) | 编辑器应用：面板结构、2D/3D 视口、Gizmo、资产导入、场景文件操作 |
| [roadmap.md](./roadmap.md) | 路线图：已完成里程碑与后续方向 |
| [status.md](./status.md) | 实现状态记录：逐里程碑的新增能力与验证（M1…M7） |
| [DEV-PLAN.md](./DEV-PLAN.md) | 重构专项的总计划、任务拆解、分支/提交纪律 |
| [WORKLOG.md](./WORKLOG.md) | 逐条执行日志（做了什么 / 如何验证 / 遇到什么问题与如何解决） |

## 项目速览

- **定位**：简单游戏引擎 + 编辑器，2D 与 3D 共用一套 ECS / 资产 / 脚本体系。
- **语言/标准**：C++17。
- **构建**：CMake（≥3.21）+ Ninja，统一由 `CMakePresets.json` 管理，Windows / Linux / macOS 一致。
- **渲染后端**：OpenGL 4.6（可用）、Vulkan（部分实现，实验性）。
- **核心第三方库**：EnTT（ECS）、GLFW（窗口）、GLAD（GL 加载）、GLM（数学）、ImGui + ImGuizmo（GUI）、
  Jolt（物理）、Lua（脚本）、miniaudio（音频）、stb_image（图像）。
- **可执行程序**：`editor`（编辑器）、`sandbox3d` / `sandbox2d`（最小 3D / 2D 示例）、
  `voxel`（体素演示）、`examples/*`（LearnOpenGL 复刻集）。

## 代码目录

```
MEngine/
├── engine/          # 引擎库（core / render / scene / physics / audio / utils）
├── editor/          # ImGui 编辑器（可执行程序）
├── sandbox/         # sandbox3d + sandbox2d（可执行程序）
├── voxel/           # 体素演示（异步区块流式 + 常驻区块）
├── examples/        # LearnOpenGL 复刻集（每个示例一个 exe）
├── assets/          # 共享资源（shaders / textures / models / icons / scripts，含 manifest.json）
├── deps/            # 第三方库（git submodule）
├── cmake/           # 公共 CMake 模块（编译警告配置）
├── tools/           # 小工具（截图 diff、PPM→PNG、贴图生成等）
├── docs/            # 本文档（也是 GitHub Pages 站点源）
└── CMakePresets.json
```

## 文档站点（GitHub Pages）

本目录同时是 MkDocs 站点的内容源，配置在仓库根的 `mkdocs.yml`：

```bash
python -m pip install -r requirements-docs.txt   # 只需一次
python -m mkdocs serve                            # 本地预览 http://127.0.0.1:8000
python -m mkdocs build --strict                   # 与 CI 相同的检查（警告即失败）
```

`.github/workflows/docs.yml` 会在 `main` / `dev` 上文档有改动时自动构建并发布到
`https://miaohn.github.io/MEngine/`（首次需要在仓库 **Settings → Pages → Source** 选择
“GitHub Actions”）。写文档时请遵守：

- 新增页面要挂到 `mkdocs.yml` 的 `nav`，否则 `--strict` 会报「文件未出现在导航中」；
- 页面之间用**相对链接**（`./rendering.md`），链接到不存在的文件会直接让 CI 失败；
- 图表用 ` ```mermaid ` 代码块（Material 自带 mermaid.js），不要贴图片代替结构图。
