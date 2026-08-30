# 实现状态记录（Implementation Status）

> 记录 3D 图形引擎改造的进度。每完成一个里程碑，更新此文件并对应一次 git commit。

## 里程碑

### M1 — 3D 渲染地基（阶段 0/1）✅

**日期**：2026-08-30
**commit**：`feat(render): add mesh & perspective camera 3d pipeline`

**新增能力：**
- `Vertex`（`render/vertex.hpp`）：位置 + 法线 + UV，附带 `GetLayout()` 属性布局。
- `Mesh`（`render/mesh.hpp/.cpp`）：复用已有 `IVertexArrayBackend` 抽象（OpenGL 走 VAO/VBO/IBO，Vulkan 端已留 CPU 桩），提供 `CreateCube()` 生成单位立方体。
- `PerspectiveCamera`（`scene/perspective_camera.hpp`）：透视相机（lookAt 模型）。
- `MeshComponent`（`scene/component.hpp`）：ECS 组件，绑定 Mesh + Shader + 可选 Texture。
- `Scene::RenderMeshes(proj_view, camera_pos)`：遍历带 `MeshComponent` 的实体并绘制。
- `Renderer::DrawMesh(...)`：绑定 shader/texture、设置 uniform、绘制网格，带 1×1 白色兜底纹理。
- 光照 shader：`res/shaders/lit_{vert,frag}.glsl`（Blinn-Phong 方向光 + 可选纹理）。
- `sandbox` 改为 3D 演示：旋转的立方体 + 透视相机。

**验证：**
- Clang 与 MSVC 均构建通过（0 错误）。
- 视觉效果待用户运行 `sandbox.exe` 确认。

**为 Vulkan 预留的空间：**
- 网格复用 `IVertexArrayBackend` 接口（已有 Vulkan 空壳实现，后续只需补全）。
- 相机/shader 接口与后端解耦。

### M2a — OBJ 模型导入 ✅

**日期**：2026-08-30
**commit**：`feat(render): add obj model loader and mesh library`

**新增能力：**
- `ModelLoader::LoadObj(path)`：解析 Wavefront OBJ（`v`/`vt`/`vn`/`f`，支持 `v/vt`、`v//vn`、`v/vt/vn`），多边形三角化、负索引、无 `vn` 时生成平面法线。
- `MeshLibrary`：按名字缓存 Mesh。
- `tools/gen_sphere.py`：UV 球体 OBJ 生成脚本。
- `sandbox/res/models/sphere.obj`：演示模型。
- sandbox 演示：左侧程序化立方体 + 右侧导入球体，各自旋转。

**验证：**
- Clang / MSVC 均构建通过（0 错误）。
- 视觉效果待用户运行 `sandbox.exe` 确认（左侧方块、右侧球体）。

### M2b — glTF 2.0 导入 ✅

**日期**：2026-08-30
**commit**：`feat(render): add gltf 2.0 loader via tinygltf`

**新增能力：**
- `ModelLoader::LoadGltf(path)`：基于 tinygltf 加载 `.gltf`/`.glb`，取第一个 mesh 的第一个 primitive，使用 POSITION/NORMAL/TEXCOORD_0（缺法线自动生成平面法线）。
- `ModelLoader::LoadGltfBaseColorTexture(path)`：提取第一份材质的 baseColor 贴图并转为 RGBA。
- vendored 依赖：`deps/tinygltf/tiny_gltf.h`（v2.9.7）+ `deps/nlohmann/json.hpp`（v3.11.3），MIT。
- 样例：`sandbox/res/models/duck.glb`（Khronos glTF 样例）。
- sandbox 改为渲染 duck.glb，并加了**自动取景**（按包围盒居中 + 自动相机距离）。

**验证：**
- Clang / MSVC 均构建通过（0 错误）。
- 视觉待用户运行确认（贴图小黄鸭）。

**已知限制：**
- 只取第一个 mesh/primitive，多网格、多材质待 M2c。
- 不支持 sparse accessor、Draco 压缩、骨骼/动画。
- 法线贴图（normal.png 等 PBR 贴图）尚未接入，待 M3/PBR。

### M3a — PBR 材质与光照 ✅

**日期**：2026-08-30
**commit**：`feat(render): add pbr material and metallic-roughness lighting`

**新增能力：**
- `Material`（metallic-roughness PBR）：albedo/normal/metallic-roughness/AO 贴图 + 因子。
- PBR shader：Cook-Torrance GGX + 方向光 + 环境光，法线贴图（导数法 TBN）、AO、Reinhard tone mapping + gamma。
- `MeshComponent` 改为绑定 `Mesh + Material`；`Renderer::DrawMesh` 绑定四贴图 + uniform。
- glTF 加载器 `LoadGltfMaterial` 提取 PBR 贴图/因子。
- sandbox 演示 DamagedHelmet（经典 PBR 测试模型）。

**验证：**
- Clang / MSVC 均构建通过（0 错误）。
- 视觉待用户确认（金属头盔）。

**已知限制：**
- 仅单个方向光（参数在 shader 默认值），无多光源/点光源。
- 无阴影映射（M3b）。
- tone mapping 作用于 LDR，无 HDR 帧缓冲（M4）。

## 待办（后续里程碑）

- [x] M2a：OBJ 模型导入（`ModelLoader::LoadObj`）
- [x] M2b：glTF 2.0 导入（tinygltf）
- [ ] M2c：多网格/多材质 `Model`、MeshLibrary 接入资产系统
- [x] M3a：PBR 材质（metallic-roughness）+ 法线贴图
- [ ] M3b：多光源 + 阴影映射
- [ ] M4：HDR/Bloom/ToneMapping、SSAO、体积光、TAA/降噪
- [ ] M5：编辑器 3D 视口 + 轨道相机 + Gizmo（ImGuizmo）+ 资产导入 UI
- [ ] 补全 Vulkan 资源后端

## 已知问题 / 技术债

- 当前无背面剔除（`glEnable(GL_CULL_FACE)` 未开启），立方体所有面都绘制；后续开启时需确认面绕序。
- 光照参数（方向光方向/颜色）目前在 shader 内默认，未暴露为引擎级 Light 抽象。
- `Renderer::DrawMesh` 每帧重复设置全部 uniform，后续可引入 material/UBO 批量上传。
- `RenderContext` 与 `RenderPass` 重复抽象仍未清理。
- `Camera2D` / `OrthographicCamera` 重叠，相机体系待统一。
