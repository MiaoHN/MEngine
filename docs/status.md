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

## 待办（后续里程碑）

- [ ] M2：模型导入（glTF/obj/fbx，tinygltf 或 Assimp）
- [ ] M2：Mesh/Material 资源缓存与场景序列化
- [ ] M3：多光源 + 阴影映射 + PBR
- [ ] M4：HDR/Bloom/ToneMapping、SSAO、体积光、TAA/降噪
- [ ] M5：编辑器 3D 视口 + 轨道相机 + Gizmo（ImGuizmo）+ 资产导入 UI
- [ ] 补全 Vulkan 资源后端

## 已知问题 / 技术债

- 当前无背面剔除（`glEnable(GL_CULL_FACE)` 未开启），立方体所有面都绘制；后续开启时需确认面绕序。
- 光照参数（方向光方向/颜色）目前在 shader 内默认，未暴露为引擎级 Light 抽象。
- `Renderer::DrawMesh` 每帧重复设置全部 uniform，后续可引入 material/UBO 批量上传。
- `RenderContext` 与 `RenderPass` 重复抽象仍未清理。
- `Camera2D` / `OrthographicCamera` 重叠，相机体系待统一。
