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

### M3b — 方向光阴影映射 ✅

**日期**：2026-08-30
**commit**：`feat(render): add directional shadow mapping`

**新增能力：**
- `DirectionalLight` + `ShadowMap`（2048×2048 深度贴图）。
- 阴影 pass（depth-only shader）+ 主 pass 采样阴影（bias 硬阴影）。
- `Renderer` 新增 `BeginShadowPass/DrawMeshShadow/EndShadowPass`；`Scene::RenderMeshes` 两遍渲染。

**验证：**
- Clang / MSVC 均构建通过（0 错误）。
- 视觉待用户确认（头盔投射阴影）。

**已知限制：**
- 仅单个方向光，无点光/聚光（多光源待 M3c）。
- 硬阴影（无 PCF 软阴影）。
- 阴影体固定半径 2.0（适配归一化模型）。
- `ShadowMap` 为 OpenGL 专属，未抽象到 RHI。

### M3c — 多光源（点光源）✅

**日期**：2026-08-30
**commit**：`feat(render): add point lights for multi-light pbr`

**新增能力：**
- `PointLight`（position/color/intensity/radius 距离衰减）。
- Renderer/Scene 支持点光源列表（上限 8）。
- PBR shader 对每个点光源累加 Cook-Torrance（距离平方衰减 + 软截止）。
- sandbox 加了暖色/冷色两个点光源演示。

**验证：**
- Clang / MSVC 均构建通过（0 错误）。
- 视觉待用户确认（头盔受暖/冷双色点光源影响）。

**已知限制：**
- 点光源无阴影（需要 cube shadow map）。
- 无聚光（spot light）。
- 光照数据每帧按索引 uniform 名上传（未用 UBO）。

### M3d — 软阴影（PCF）+ 点光源阴影 + 聚光 ✅

**日期**：2026-08-30
**commit**：`feat(render): add pcf shadows point cube shadows and spot lights`

**新增能力：**
- PCF 软阴影：方向光阴影采样改为 3×3 百分比渐近滤波（`shadow_map_size` uniform），阴影边缘变柔和。
- `CubeShadowMap`（`render/cube_shadow_map.hpp/.cpp`）：立方体深度贴图 + FBO（1024×1024，GL_DEPTH_COMPONENT），用于全方向阴影。
- `PointLight` 新增 `casts_shadow` + `GetShadowMatrices()`（6 面 90° 透视视图投影）；点光源阴影 pass 逐面渲染场景到 cube depth map（`point_shadow_depth_{vert,frag}.glsl`，写入归一化距离 `gl_FragDepth`）。
- PBR shader 用 `texture(point_light_shadow_maps[i], fragToLight)` 采样点光阴影，带距离 bias；最多 4 个点光投影阴影（`Renderer::kMaxPointShadows`）。
- `SpotLight`（position/direction/range/cutoff/outer_cutoff）+ PBR shader 聚光贡献（内外锥平滑衰减）。
- `Renderer`/`Scene` 新增 `AddSpotLight/ClearSpotLights`、`GetPointLights/GetPointShadowIndex` 与点光阴影 pass 接口。
- sandbox：两个点光开启 `casts_shadow`，并加一盏冷白聚光灯瞄准头盔。

**验证：**
- Clang / MSVC 均构建通过（0 错误）。
- 视觉待用户确认（软阴影 + 地面点光阴影 + 头盔聚光锥）。

**已知限制：**
- 点光阴影单次采样（无 PCF），边缘较硬。
- 聚光无阴影（未做 spot shadow map）。
- 点光阴影逐面重绘全场景（每盏灯 6 次绘制，最多 4 盏）。
- `CubeShadowMap` 为 OpenGL 专属。

### M4a — HDR 帧缓冲 + Bloom 后处理 ✅

**日期**：2026-08-30
**commit**：`feat(render): add hdr framebuffer and bloom post-processing`

**新增能力：**
- `PostProcessing`：RGBA16F HDR 帧缓冲 + bloom（brightness 提取 + 高斯 blur ping-pong）+ ACES tone mapping + gamma。
- 主 pass 渲染到 HDR 帧缓冲，PBR shader 输出 HDR 线性。
- 全屏三角形后处理（`gl_VertexID`，无需顶点缓冲）。

**验证：**
- Clang / MSVC 均构建通过（0 错误）。
- 视觉待用户确认（高光泛光）。

**已知限制：**
- 帧缓冲尺寸固定为构造时的窗口尺寸（无 resize 处理）。
- `PostProcessing` 为 OpenGL 专属。
- 无 Bloom 强度/曝光运行时调节（硬编码默认值）。

### M4b — 天空盒 + IBL（环境反射）✅

**日期**：2026-08-30
**commit**：`feat(render): add skybox and ibl environment lighting`

**新增能力：**
- `Skybox`（`render/skybox.hpp/.cpp`）：从 6 张 face 图像构建 GL_TEXTURE_CUBE_MAP（sRGB），生成 mipmap，背景渲染（`glDepthFunc(GL_LEQUAL)` + 去平移 view + `pos.xyww`）。
- 辐射照度预计算：半球卷积把环境立方体贴图烘焙为 32×32 irradiance cubemap（RGBA16F），供漫反射 IBL 采样。
- PBR shader 用 `texture(irradiance_map, N)` 做漫反射 IBL、`textureLod(environment_map, R, roughness * max_mip_level)` 做镜面 IBL（粗糙度选 mip），并新增 `FresnelSchlickRoughness`。
- `Renderer::DrawMesh` 绑定环境/辐射照度贴图（slot 5/6）并上传 `environment_map/irradiance_map/max_mip_level`。
- `Scene::RenderMeshes` 签名改为 `(view, proj, camera_pos)`，主 pass 后渲染天空盒背景。
- 新 shader：`skybox_{vert,frag}.glsl`、`irradiance_frag.glsl`。
- 资源：`sandbox/res/textures/skybox/` 与 `editor/res/textures/skybox/`（learnopengl.com 6 面天空盒）。

**验证：**
- Clang / MSVC 均构建通过（0 错误）。
- 视觉待用户确认（背景天空盒 + 金属模型反射环境）。

**已知限制：**
- 环境贴图为 8-bit LDR（JPEG），无 HDR（`.hdr`）加载。
- 无预过滤镜面卷积（specular 直接采样原环境 mip，无 GGX 预过滤）。
- irradiance 卷积为半球均匀采样，无 cos 加权重要性采样。
- `Skybox` 为 OpenGL 专属。

### M4c — HDR 环境贴图 + 预过滤镜面 IBL ✅

**日期**：2026-08-30
**commit**：`feat(render): add hdr environment and prefiltered specular ibl`

**新增能力：**
- `Skybox` 改为从等距柱状（equirectangular）HDR 环境图加载（`stbi_loadf` → `GL_RGBA16F` 2D 纹理），用 `equirect_to_cube_frag.glsl` 转成 512×512 环境立方体贴图。
- 预过滤镜面卷积（`prefilter_frag.glsl`）：用 Hammersley 低差异序列 + GGX 重要性采样，把环境立方体贴图烘焙为 128×128、5 级 mip 的 prefiltered cubemap（每级对应一个粗糙度）。
- PBR shader 镜面 IBL 改为 `textureLod(prefiltered_map, R, roughness * max_prefilter_mip)`，粗糙度 → mip 精确对应预过滤结果。
- `Renderer::DrawMesh` 绑定 `irradiance_map`（slot 5）+ `prefiltered_map`（slot 6），上传 `max_prefilter_mip`。
- 资源：`sandbox/res/textures/hdr/kloppenheim_06_puresky_1k.hdr`（Poly Haven CC0，同样放于 editor）。
- 新增 shader：`equirect_to_cube_frag.glsl`、`prefilter_frag.glsl`。

**验证：**
- Clang / MSVC 均构建通过（0 错误）。
- 视觉待用户确认（HDR 背景 + 金属模型镜面反射更真实）。

**已知限制：**
- 预过滤为 GGX 重要性采样（无预积分 BRDF LUT），镜面边缘仍略有能量偏差。
- `Skybox` 为 OpenGL 专属。
- 未实现 SSAO / 体积光 / TAA（后续里程碑）。

### M4d — SSAO（屏幕空间环境光遮蔽）✅

**日期**：2026-08-30
**commit**：`feat(render): add screen-space ambient occlusion`

**新增能力：**
- `SSAO`（`render/ssao.hpp/.cpp`）：几何 pass 把视图空间 position + normal 写入 G-buffer（RGBA16F MRT），随后全屏 pass 用 64 个切空间半球样本 + 4×4 随机旋转噪声估计遮蔽，再用 4×4 box blur 去噪。
- `Renderer` 新增 `BeginSSAOPass/DrawMeshSSAO/EndSSAOPass/GenerateSSAO/BindSSAO` + `SetSSAOEnabled`；`Scene::RenderMeshes` 在主 pass 前插入 SSAO 几何 pass 与 AO 生成。
- PBR shader 新增 `ssao_map`/`ssao_enabled`，把 SSAO 乘进环境光项（只压暗 ambient，不影响直接光）。
- 新增 shader：`ssao_geometry_{vert,frag}.glsl`、`ssao_{vert,frag}.glsl`、`ssao_blur_frag.glsl`。
- sandbox 开启 SSAO 演示。

**验证：**
- Clang / MSVC 均构建通过（0 错误）。
- 视觉待用户确认（头盔/地面接触处出现软 AO 暗角）。

**已知限制：**
- SSAO 纹理尺寸固定为构造时窗口尺寸（无 resize 处理）。
- 全分辨率 64 样本，无半分辨率/时域优化。
- `SSAO` 为 OpenGL 专属。

### M4e — 体积光（God Rays）✅

**日期**：2026-08-30
**commit**：`feat(render): add volumetric light god rays`

**新增能力：**
- 后处理新增 god rays pass（`god_rays_frag.glsl`）：从方向光太阳的屏幕位置做径向模糊，累加场景亮部形成光柱。
- `composite_frag.glsl` 新增 `god_rays`/`god_rays_strength`，与 bloom 一起叠加在 tone mapping 之前。
- `Renderer::PostProcess(view, proj)` 把方向光反方向（太阳）投影到屏幕空间作为光源，传给后处理。
- `Renderer`/`Scene` 新增 `SetGodRaysStrength`；sandbox 调整方向光使太阳可见并开启体积光。

**验证：**
- Clang / MSVC 均构建通过（0 错误）。
- 视觉待用户确认（太阳方向出现光柱）。

**已知限制：**
- 为屏幕空间径向模糊（非真正体积雾/光线步进），无深度遮挡。
- god rays 纹理尺寸固定为半分辨率、构造时窗口尺寸。

## 待办（后续里程碑）

- [x] M2a：OBJ 模型导入（`ModelLoader::LoadObj`）
- [x] M2b：glTF 2.0 导入（tinygltf）
- [ ] M2c：多网格/多材质 `Model`、MeshLibrary 接入资产系统
- [x] M3a：PBR 材质（metallic-roughness）+ 法线贴图
- [x] M3b：方向光阴影映射
- [x] M3c：多光源（点光源）
- [x] M3d：软阴影（PCF）、点光源阴影（cube shadow map）、聚光
- [x] M4a：HDR 帧缓冲 + Bloom + ACES tone mapping
- [x] M4b：天空盒 + IBL（环境反射）
- [x] M4c：HDR 环境贴图（`.hdr`）+ 预过滤镜面 IBL
- [x] M4d：SSAO（屏幕空间环境光遮蔽）
- [x] M4e：体积光（God Rays）
- [ ] M4f：TAA/降噪
- [ ] M5：编辑器 3D 视口 + 轨道相机 + Gizmo（ImGuizmo）+ 资产导入 UI
- [ ] 补全 Vulkan 资源后端

## 已知问题 / 技术债

- 当前无背面剔除（`glEnable(GL_CULL_FACE)` 未开启），立方体所有面都绘制；后续开启时需确认面绕序。
- 光照参数（方向光方向/颜色）目前在 shader 内默认，未暴露为引擎级 Light 抽象。
- `Renderer::DrawMesh` 每帧重复设置全部 uniform，后续可引入 material/UBO 批量上传。
- `RenderContext` 与 `RenderPass` 重复抽象仍未清理。
- `Camera2D` / `OrthographicCamera` 重叠，相机体系待统一。
- 点光阴影逐面全量重绘、无 PCF，后续可做分层渲染/软阴影优化。
