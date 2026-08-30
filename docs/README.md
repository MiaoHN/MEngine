# MEngine 文档索引

> 本目录是 MEngine 的架构与开发文档，供后续实现（特别是 3D 图形渲染）时查阅。

## 文档地图

| 文档 | 内容 |
| --- | --- |
| [architecture.md](./architecture.md) | 整体架构、模块划分、数据流、设计模式 |
| [build-and-toolchain.md](./build-and-toolchain.md) | 构建系统、CMake Presets、跨平台工具链 |
| [core.md](./core.md) | 核心层：Application 生命周期、Logger、ScriptEngine、UUID、Input 等 |
| [rendering.md](./rendering.md) | 渲染层：Renderer/Pipeline/Pass、资源抽象、RHI 后端、现状与局限 |
| [asset-management.md](./asset-management.md) | 资源管理设计：统一 AssetManager、共享 assets/ 目录、迁移计划 |
| [scene.md](./scene.md) | 场景层：ECS(EnTT)、Entity、组件、Scene、相机 |
| [editor.md](./editor.md) | 编辑器应用：面板结构、渲染到纹理、交互 |
| [roadmap.md](./roadmap.md) | 3D 图形引擎路线图：模型导入、3D 渲染、光照与后处理 |

## 项目速览

- **定位**：一个"Just for fun"的简单游戏引擎，已从 2D 升级为 3D 渲染引擎（PBR 材质 + 阴影 + HDR/Bloom + 天空盒/IBL + SSAO + 体积光 + TAA），配 ImGui 编辑器。
- **语言/标准**：C++17。
- **构建**：CMake（≥3.21）+ Ninja，统一由 `CMakePresets.json` 管理，支持 Windows / Linux / macOS。
- **渲染后端**：OpenGL 4.6（可用）、Vulkan（部分实现，实验性）。
- **核心第三方库**：EnTT（ECS）、GLFW（窗口）、GLAD（GL 加载）、GLM（数学）、ImGui + ImGuizmo（GUI）、Lua（脚本）、stb_image（图像）。

## 代码目录

```
MEngine/
├── engine/          # 引擎库（core / render / scene / utils）
├── editor/          # ImGui 编辑器（可执行程序）
├── sandbox/         # 最小示例（可执行程序）
├── assets/          # 共享资源（shaders / textures / models / icons / scripts，含 manifest.json）
├── deps/            # 第三方库（git submodule）
├── cmake/           # 公共 CMake 模块（编译警告配置）
├── docs/            # 本文档
└── CMakePresets.json
```
