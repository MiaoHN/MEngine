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

### M4f — TAA（时间抗锯齿）✅

**日期**：2026-08-30
**commit**：`feat(render): add temporal anti-aliasing`

**新增能力：**
- 每帧用 Halton(2,3) 低差异序列给相机投影加亚像素抖动（`PostProcessing::GetJitter` + `Renderer::GetJitteredProjection`）。
- `taa_frag.glsl`：把抖动后的当前帧与历史帧混合（`mix(history, current, blend)`），并用邻域 AABB 截钳历史颜色抑制鬼影。
- `PostProcessing::ResolveTAA` 用双缓冲（ping-pong）维护历史，主 pass 后解析，随后 bloom/god rays/合成都采样解析后的纹理。
- `Renderer`/`Scene` 新增 `SetTAAEnabled`；sandbox 开启 TAA 演示。

**验证：**
- Clang / MSVC 均构建通过（0 错误）。
- 视觉待用户确认（边缘锯齿减少）。

**已知限制：**
- 无运动向量（运动模糊/快速运动可能有鬼影）。
- TAA 历史纹理尺寸固定为构造时窗口尺寸。

### M5 — 编辑器 3D 化 + 资产工作流 ✅

**日期**：2026-09-05

**新增能力：**
- **相机统一**：新的 `Camera`（透视 + 正交一体，`scene/camera.hpp`）取代 `Camera2D`/`OrthographicCamera`/`PerspectiveCamera`；`CameraComponent` 作为 ECS 组件挂载相机。
- **编辑器轨道相机**：`EditorCamera`（`editor/src/editor_camera.hpp`，target/yaw/pitch/distance 模型），右键环绕/中键平移/滚轮缩放。
- **3D 视口**：`Scene::RenderMeshes` 支持自定义目标 FBO（`target_fbo/target_w/target_h`），编辑器把场景合成到视口纹理；程序化网格着色器（X 红/Z 蓝轴线，随距离淡出）。
- **实体创建/层级**：Empty/Cube/Plane/Sphere（新增 `Mesh::CreateSphere`）、场景面板选中/删除/复制。
- **模型导入**：从内容浏览器拖 `.obj`/`.gltf`/`.glb` 到视口，自动取景并落地；OBJ 按文件名约定自动套 diffuse/normal/roughness/ao 贴图。
- **材质编辑**：Albedo/Normal/Roughness/AO 贴图槽（拖拽 + 右键清除）+ Base Color/Metallic/Roughness/Specular 因子；新增 `Material::specular_factor` 与 shader `specular_intensity`。
- **光照面板**：方向光 + 点光源列表增删改。
- **Gizmo**：ImGuizmo 移动/旋转/缩放（`W`/`E`/`R`），`F` 聚焦，`Ctrl+D` 复制。
- **布局**：默认停靠布局（DockBuilder）+ View 菜单面板显隐 + Reset Layout。

**修复：**
- 视口 FBO 未解绑导致 ImGui 画进离屏纹理（灰屏/闪烁）。
- 视口折叠为 1px 帧缓冲（最小尺寸钳制）。
- god rays 在太阳位于屏幕外时从中心假采样导致棱角条纹（太阳不可见时关闭 god rays）。
- `sample` 为 GLSL 保留字导致的 god_rays 着色器编译失败。

### M6 — LearnOpenGL 移植期（渲染/后期扩展 + examples 复刻）✅

**日期**：2026-09-06

**背景**：把 LearnOpenGL `src/` 的示例逐个用 MEngine 公共 API 复刻成独立可执行
（`examples/`，见 `examples/PORTING.md`），1:1 对齐 LO 场景/参数以便并排比对。复刻过程
倒逼引擎补了一批渲染/后期/材质能力。

**引擎新增能力：**
- **三套材质/光照管线**（`assets/shaders`，AssetManager manifest）：
  1. `pbr`——引擎 PBR（GGX Cook-Torrance，默认）；
  2. `blinn`——经典 Blinn-Phong（`shininess`/`specular`/`SetSpecularMap`）；
  3. `blinn_lo`——**LO-exact**：逐灯 ambient/diffuse/specular 三分量、Phong(reflect)/Blinn 高光、
     逐像素 specular 贴图、LO c/l/q 衰减（`PointLight.lo_attenuation`）、点光上限 32。
- **后期 tone 模式**（`composite_frag.glsl`，`PostProcessing`/`Scene` 开关）：
  `SetLinearOutput`（raw clamp）、`SetLoHdrTone`（`1-exp(-x)`+gamma，LO 6/7）、
  `SetReinhardTone`（`color/(color+1)`+gamma，LO PBR）；默认 ACES+gamma。
- **LO-exact 光照开关**：`SetLoLighting`/`SetLoBlinnSpec`/`SetLoDirShadow`；无太阳场景 `NoSun`。
- **材质扩展**（`Material`）：`SetAlbedoSRGB`（shader 内 `pow 2.2` 解码 albedo 贴图）、
  `SetSpecularColor`、合并 **MR 贴图**约定（R=1/G=roughness/B=metallic，`pbr_frag` 逐像素）；
  `tools/make_pbr_mr.ps1` 由 LO 两张灰度图生成 `mr.png`。
- **split-sum BRDF LUT**：`Skybox::GenerateBRDF`（RG16F 512）+ `BindBRDF`（单元 13），
  `pbr_frag` 镜面 IBL 改 LO split-sum 形式 `prefiltered*(F*brdf.x+brdf.y)`（取代旧简化项）。
- **镜面 IBL 开关**：`Scene::SetIblSpecular(bool)`（`pbr_frag` `u_ibl_specular`），可只保留漫反射
  IBL（复刻 LO 6.pbr/2.1.2）。
- **环境 HDR 逐应用覆盖**：`Application::SetEnvironmentHdrPath/GetEnvironmentHdrPath` 与
  `SetEnvironmentHdrFlip`（默认 kloppenheim；6.2.x 用 newport_loft，glTF 导出 HDR 需翻转）。
- **应用宿主工具**：窗口标题实时 FPS、无头/定时抓帧（`SetMaxFrames`/`SetCaptureFrame`/`SetWindowHidden`）；
  examples 共享宿主加**滚轮 FOV 缩放**与 **WASD/Space/Ctrl 飞行相机**。

**示例覆盖（`examples/`，每示例独立 exe + 自带 assets 副本）**：
- 2.lighting：`ex_2_1_colors`、`ex_2_2_basic_lighting`(PBR)、`ex_2_2_blinn_lighting`(LO 1:1)、
  `ex_2_3_materials`、`ex_2_4_lighting_maps`、`ex_2_5_light_casters`、`ex_2_6_multiple_lights`
- 3.model_loading：`ex_3_1_model_loading`
- 5.advanced_lighting：`ex_5_3_shadow_mapping`、`ex_5_4_normal_mapping`、`ex_5_6_hdr_bloom`、
  `ex_5_8_deferred_shading`(前向等效 32 灯)、`ex_5_9_ssao`
- 6.pbr：`ex_6_1_1_pbr_lighting`、`ex_6_1_2_pbr_lighting_textured`、`ex_6_2_1_ibl_irradiance`、
  `ex_6_2_2_ibl_specular`、`ex_6_2_2_ibl_specular_textured`
- 用户资产：`ex_6_2_cerberus`、`ex_model_viewer`（含 `PbrSidecarTextured`：GLB 只嵌 albedo 时从
  A/M/R/N/AO sidecar 文件组装完整 PBR 材质）。

## 待办（后续里程碑）

- [x] M2a：OBJ 模型导入（`ModelLoader::LoadObj`）
- [x] M2b：glTF 2.0 导入（tinygltf）
- [ ] M2c：多网格/多材质 `Model`、MeshLibrary 接入资产系统、OBJ `.mtl` 解析（贴图不再靠文件名约定）
- [x] M3a：PBR 材质（metallic-roughness）+ 法线贴图
- [x] M3b：方向光阴影映射
- [x] M3c：多光源（点光源）
- [x] M3d：软阴影（PCF）、点光源阴影（cube shadow map）、聚光
- [x] M4a：HDR 帧缓冲 + Bloom + ACES tone mapping
- [x] M4b：天空盒 + IBL（环境反射）
- [x] M4c：HDR 环境贴图（`.hdr`）+ 预过滤镜面 IBL
- [x] M4d：SSAO（屏幕空间环境光遮蔽）
- [x] M4e：体积光（God Rays）
- [x] M4f：TAA（时间抗锯齿）
- [x] M5：编辑器 3D 视口 + 轨道相机 + Gizmo（ImGuizmo）+ 资产导入 UI
- [x] M6：LO 移植期（三管线/后期 tone 模式/BRDF LUT/镜面 IBL 开关/环境覆盖/examples）
- [x] 场景序列化（`LoadScene/SaveScene`，JSON：实体+材质+灯光+渲染参数；editor File→Open/Save）
- [ ] 补全 Vulkan 资源后端
- [ ] 深度整理：Light 组件化、Model 多网格/多材质 + `.mtl`、Renderer uniform 批量/去重、Editor 渲染选项接入

## 已知问题 / 技术债

- 背面剔除已按材质启用（renderer 每 draw 设 `rhi->SetCullMode(material->GetCullMode())`，默认 Back；2D/UI 前恢复 None）——旧的"全局无剔除"已解决；后续仅需确认新网格绕序符合。
- 光照：点/聚/方向光已支持 ECS 组件（`Point/Spot/DirectionalLightComponent`；位置=实体 Transform，
  有组件时每帧驱动 renderer；旧列表 API 兼容保留；灯光组件已按实体级序列化）。遗留：renderer 仍只支持
  **1 个方向光**（多方向光需 shader 方向光数组，未做）；Lighting 面板与组件灯二选一（有组件时提示改用实体）。
- `Renderer::DrawMesh` 每帧重复设置全部 uniform，后续可引入 material/UBO 批量上传。
- OBJ 的 `.mtl` 未解析，贴图靠文件名约定自动套用。
- 点光阴影逐面全量重绘、无 PCF，后续可做分层渲染/软阴影优化。
- 环境 HDR 路径/翻转是 `Application` 全局静态，非 per-scene（编辑器多场景/运行时切换不灵活）。
- `pbr`/`blinn`/`blinn_lo` 三套 fragment shader 各自实现，BRDF/阴影/IBL/点光循环公共部分重复较多，
  可收敛为共享 GLSL 头。
