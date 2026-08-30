# 渲染层（render）

路径：`engine/src/render`，RHI 抽象在 `engine/src/render/rhi`。

## 层次划分

```mermaid
graph TB
    subgraph 高层["高层封装 (面向游戏逻辑)"]
        RENDERER[Renderer 2D 渲染器]
        SCENE[Scene::Render]
    end

    subgraph 中间层["渲染资源与组织"]
        PIPELINE[RenderPipeline]
        PASS[RenderPass]
        SHADER[Shader / ShaderLibrary]
        TEX[Texture / TextureLibrary]
        FB[FrameBuffer]
    end

    subgraph RHI层["RHI 抽象 + 后端"]
        IRHI[IRHI]
        FACTORY[Backend 工厂 resource_backend.cpp]
        GL[OpenGL 实现]
        VK[Vulkan 实现]
    end

    RENDERER --> PIPELINE
    RENDERER --> PASS
    PIPELINE --> SHADER
    PIPELINE --> VAO[IVertexArrayBackend]
    SCENE --> RENDERER
    PASS --> FB
    SHADER --> IShaderBackend
    TEX --> ITextureBackend
    FB --> IFrameBufferBackend
    FACTORY --> GL
    FACTORY --> VK
    PIPELINE --> IRHI
```

## 高层封装

### Renderer（2D 渲染器）
- 构造时创建：一个四边形 VAO（顶点含 `aPos`(vec3) + `aTexCoord`(vec2)，索引缓冲 6 个），加载默认 shader（`res/shaders/default_{vert,frag}.glsl`），并组装一个 `RenderPipeline`。
- 提供 `RenderSprite(Sprite2D&)` 与 `RenderSprite(AnimatedSprite2D&)`：
  1. 绑定 shader 与 texture（无纹理时生成 1×1 纯色纹理）。
  2. 设置 `model`、`proj_view`、`texture1` uniform。
  3. `pipeline_->Execute()` 绘制。
- ⚠️ 当前是 **2D 专用**：VAO 硬编码为四边形、shader 硬编码为 2D 默认 shader，无 Mesh/模型概念。3D 化需要重构（见 roadmap）。

### RenderPipeline
- 一个绘制单元 = `IVertexArrayBackend`（几何）+ `Shader`（着色器）。
- `Execute()`：绑定 shader → 绑定 VAO → `rhi->DrawIndexedTriangles(count)` → 解绑。

### RenderPass
- 持有离屏帧缓冲（`fb_`）与若干 `RenderPipeline`。
- `Begin()` 绑定 FBO 并清屏；`End()` 解绑回默认帧缓冲；`Execute()` 依次执行所有 pipeline。
- 帧缓冲 id 由 `IRHI::CreateFramebuffer()` 创建。

### RenderContext（疑似重复抽象）
- 与 `RenderPass` 几乎一致（持 FBO + pipelines），但**未被使用**。3D 化时可合并或删除。

## 资源类（Backend 模式）

每个资源类都是「高层包装 + 后端指针」结构，后端通过工厂创建：

| 高层类 | 后端接口 | OpenGL 实现 | Vulkan 实现 |
| --- | --- | --- | --- |
| `Shader` | `IShaderBackend` | `OpenGLShaderBackend`（编译 GLSL，`program_`） | `VulkanShaderBackend`（**空壳**，只缓存矩阵） |
| `Texture` | `ITextureBackend` | `OpenGLTextureBackend`（`glGenTextures`） | `VulkanTextureBackend`（**CPU 端缓存像素**） |
| `FrameBuffer` | `IFrameBufferBackend` | `OpenGLFrameBufferBackend`（FBO+纹理+渲染缓冲） | `VulkanFrameBufferBackend`（**空壳**） |
| （VAO） | `IVertexArrayBackend` | `OpenGLVertexArrayBackend`（VAO/VBO/IBO） | `VulkanVertexArrayBackend`（**CPU 端缓存顶点/索引**） |

工厂函数在 `resource_backend.cpp`：

```cpp
std::unique_ptr<ITextureBackend> CreateTextureBackend();
std::unique_ptr<IShaderBackend>  CreateShaderBackend(vert, frag);
std::unique_ptr<IFrameBufferBackend> CreateFrameBufferBackend(w, h);
std::unique_ptr<IVertexArrayBackend> CreateVertexArrayBackend();
```

它们根据 `GetActiveRHI()->GetAPI()` 选择实现。

### Shader
- 加载 GLSL 顶点/片元源码，`SetUniform{Int,Float,Vec2,Vec3,Vec4,Mat4}` 封装 uniform 设置。
- `ShaderLibrary`：`Add/Load/Get/Exists`，按 name 管理（用于编辑器资产管理）。

### Texture
- 通过 `stb_image` 加载（`STB_IMAGE_IMPLEMENTATION` 在 `texture.cpp`）。
- 支持静态纹理与 SpriteSheet 子纹理（`SetSubTexture` / `h_frames` / `v_frames`）。
- `TextureLibrary`：按 name 管理纹理。

### FrameBuffer
- 离屏渲染目标（默认 1600×900，TODO：可配置）。
- `AttachTexture / AttachRenderBuffer / CheckStatus / Clear / Resize / GetTextureId`。

## Mesh 与 3D 网格（M1 新增）

- `Vertex`（`render/vertex.hpp`）：交错顶点 = position(vec3) + normal(vec3) + texcoord(vec2)，`GetLayout()` 返回与内存布局一致的属性描述。
- `Mesh`（`render/mesh.hpp/.cpp`）：
  - 复用 `IVertexArrayBackend`（`CreateVertexArrayBackend()`）作为几何后端，**未新增后端接口**，Vulkan 复用已有空壳。
  - 保留 CPU 端顶点/索引副本，供重上传/导出/拾取使用。
  - `Mesh::CreateCube(size)`：24 顶点 + 36 索引的单位立方体（每面独立法线与 UV）。
- `Renderer::DrawMesh(mesh, shader, texture, model, proj_view, view_pos)`：绑定 shader/texture → 设置 uniform → `DrawIndexedTriangles`；无纹理时使用 1×1 白色兜底纹理。
- `Scene::RenderMeshes(proj_view, camera_pos)`：遍历带 `MeshComponent` 的实体，结合 `Transform` 计算 model 矩阵后绘制。

## 模型导入

- `ModelLoader`（`render/model_loader.hpp/.cpp` + `render/gltf_loader.cpp`）：加载模型文件为 `Mesh`。
  - **Wavefront OBJ**：`v` / `vt` / `vn` / `f`（含 `v/vt`、`v//vn`、`v/vt/vn` 三种形式）、多边形扇形三角化、负索引；文件缺 `vn` 时自动生成平面面法线。
  - **glTF 2.0**（`.gltf` / `.glb`，基于 tinygltf）：取第一个 mesh 的第一个 primitive，使用 POSITION / NORMAL（缺时生成平面法线）/ TEXCOORD_0 属性；`LoadGltfBaseColorTexture` 可提取第一份材质的 baseColor 贴图。
  - 返回 `nullptr` 表示加载失败。
- 依赖：`deps/tinygltf/tiny_gltf.h` + `deps/nlohmann/json.hpp`（vendored 单头文件，MIT）。
- `MeshLibrary`（`mesh.hpp`）：按名字缓存 `Mesh`，与 `ShaderLibrary`/`TextureLibrary` 对齐。
- 示例资源：`sandbox/res/models/sphere.obj`（由 `tools/gen_sphere.py` 生成）、`sandbox/res/models/duck.glb`（Khronos glTF 样例）。
- 后续扩展点：Assimp 多格式、多网格/多材质 `Model`。

## 材质与光照（M3a 新增）

- `Material`（`render/material.hpp`）：glTF metallic-roughness PBR 材质，持有 albedo / normal / metallic-roughness / AO 四张贴图及 baseColor/metallic/roughness 因子，由 `Renderer::DrawMesh` 负责绑定与 uniform 上传。
- `MeshComponent` 现在绑定 `Mesh + Material`（取代了之前的 `shader + texture`）。
- PBR shader：`res/shaders/pbr_{vert,frag}.glsl`——Cook-Torrance GGX 微面元 BRDF + 方向光 + 环境光，支持法线贴图（导数法 TBN，无需切线属性）、金属/粗糙度、AO，以及 Reinhard tone mapping + gamma 校正。
- glTF 加载器新增 `ModelLoader::LoadGltfMaterial`，提取 PBR 贴图与因子。
- 样例：`sandbox/res/models/damaged_helmet.glb`（Khronos PBR 测试模型）。

## 阴影映射（M3b 新增）

- `DirectionalLight`（`render/light.hpp`）：方向光（direction/color），`GetLightSpaceMatrix` 生成正交光照空间矩阵。
- `ShadowMap`（`render/shadow_map.hpp/.cpp`）：深度贴图 + FBO（2048×2048，GL_DEPTH_COMPONENT），OpenGL 专属（待 Vulkan 后端抽象）。
- 渲染流程（`Scene::RenderMeshes`）：
  1. 阴影 pass：用 depth-only shader（`shadow_depth_{vert,frag}.glsl`）从光视角渲染所有网格到阴影贴图。
  2. 主 pass：PBR shader 采样阴影贴图（`ShadowCalculation`，带 bias），对直接光乘以阴影因子。
- `Renderer` 持有 `ShadowMap` + depth shader + `DirectionalLight`，提供 `BeginShadowPass/DrawMeshShadow/EndShadowPass`。

## 多光源（M3c 新增）

- `PointLight`（`render/light.hpp`）：点光源（position/color/intensity/radius，距离衰减）。
- `Renderer` 维护点光源列表（`AddPointLight/ClearPointLights`），`Scene` 透传。
- PBR shader 用 uniform 数组（`point_light_positions/colors/intensities/radii`，上限 8），对每个点光源累加 Cook-Torrance 贡献（距离平方衰减 + radius 软截止）。
- 方向光保留阴影；点光源暂不投影阴影。

## 软阴影 + 点光阴影 + 聚光（M3d 新增）

- **PCF 软阴影**：方向光阴影采样改为 3×3 百分比渐近滤波（`shadow_map_size` uniform），边缘柔化。
- `CubeShadowMap`（`render/cube_shadow_map.hpp/.cpp`）：立方体深度贴图 + FBO（1024×1024），逐面附着 + 清除。
- `PointLight` 新增 `casts_shadow` + `GetShadowMatrices()`（6 面 90° 透视视图投影）；点光阴影 pass 用 `point_shadow_depth_{vert,frag}.glsl` 写入归一化距离（`gl_FragDepth`）。
- PBR shader 用 `texture(point_light_shadow_maps[i], fragToLight)` 采样点光阴影（带距离 bias）；`Renderer::kMaxPointShadows = 4`。
- `SpotLight`（position/direction/range/cutoff/outer_cutoff）；PBR shader 聚光贡献（内外锥 `clamp((theta - outer) / (cutoff - outer))` 平滑衰减）。
- 流程（`Scene::RenderMeshes`）：方向光阴影 pass → 点光 cube shadow passes（逐面） → 主 pass → 天空盒 → 后处理。
- 已知限制：点光阴影无 PCF、聚光无阴影；`CubeShadowMap` 为 OpenGL 专属。

## HDR 与后处理（M4a 新增）

- `PostProcessing`（`render/post_processing.hpp/.cpp`）：HDR 渲染目标（RGBA16F）+ bloom。
- 流程（`Scene::RenderMeshes`）：阴影 pass → 主 pass 渲染到 HDR 帧缓冲（`BeginScene/EndScene`）→ `PostProcess()` 做 brightness 提取 + 高斯模糊（ping-pong）+ 合成（ACES tone mapping + gamma）。
- PBR shader 改为输出 **HDR 线性**（tone mapping/gamma 移到后处理）。
- 相关 shader：`post_vert.glsl`（全屏三角形）、`brightness_frag.glsl`、`blur_frag.glsl`（双 pass 高斯）、`composite_frag.glsl`（ACES）。
- OpenGL 专属（同 ShadowMap），待 Vulkan 后端抽象。

## 天空盒与 IBL（M4b 新增）

- `Skybox`（`render/skybox.hpp/.cpp`）：从 6 张 face 图像（right/left/top/bottom/front/back）构建 `GL_TEXTURE_CUBE_MAP`（`GL_SRGB8_ALPHA8` + mipmap），计算 `max_mip_level = log2(face_size)`。
- 背景渲染：`Render(view, proj)` 用 `glDepthFunc(GL_LEQUAL)` + `glDepthMask(GL_FALSE)`，vertex shader 中去平移（`glm::mat3(view)`）并输出 `pos.xyww`（`gl_Position = pos.xyww`）使天空盒深度落在最远端。
- 辐射照度预计算：`GenerateIrradiance()` 用 capture FBO 依次渲染 6 个面，把环境贴图经半球卷积烘焙为 32×32 irradiance cubemap（`RGBA16F`）。
- `Renderer::DrawMesh` 绑定环境贴图（slot 5）+ 辐射照度贴图（slot 6），上传 `environment_map/irradiance_map/max_mip_level`。
- PBR shader IBL：漫反射 `texture(irradiance_map, N)`；镜面 `textureLod(environment_map, R, roughness * max_mip_level)`；新增 `FresnelSchlickRoughness`。
- `Scene::RenderMeshes(view, proj, camera_pos)`：主 pass 之后调用 `RenderSkybox(view, proj)`。
- 相关 shader：`skybox_{vert,frag}.glsl`、`irradiance_frag.glsl`。
- 资源：`sandbox/res/textures/skybox/`（learnopengl.com 6 面天空盒，LDR JPEG）。
- 已知限制：无 HDR（`.hdr`）加载、无 GGX 预过滤镜面卷积、irradiance 为均匀半球采样；`Skybox` 为 OpenGL 专属。

## HDR 环境 + 预过滤镜面 IBL（M4c 新增）

- `Skybox` 改为从等距柱状 HDR 加载：`stbi_loadf` 读入浮点 HDR → `GL_RGBA16F` 2D 纹理 → `equirect_to_cube_frag.glsl` 转成 512×512 环境立方体贴图（`GenerateEnvironment`）。
- 预过滤镜面卷积（`GeneratePrefilter` + `prefilter_frag.glsl`）：Hammersley 低差异序列 + GGX 重要性采样，把环境立方体贴图烘焙为 128×128、5 级 mip 的 prefiltered cubemap，每级 mip 对应一个粗糙度（0 / 0.25 / 0.5 / 0.75 / 1.0）。
- PBR shader 镜面 IBL 改为 `textureLod(prefiltered_map, R, roughness * max_prefilter_mip)`。
- `Renderer::DrawMesh` 绑定 `irradiance_map`（slot 5）+ `prefiltered_map`（slot 6）。
- 新增 shader：`equirect_to_cube_frag.glsl`、`prefilter_frag.glsl`。
- 资源：`res/textures/hdr/kloppenheim_06_puresky_1k.hdr`（Poly Haven CC0）。
- 已知限制：无预积分 BRDF LUT（镜面能量略偏）；`Skybox` 为 OpenGL 专属。

## SSAO（M4d 新增）

- `SSAO`（`render/ssao.hpp/.cpp`）：几何 pass 用 MRT 写 G-buffer（视图空间 position + normal，RGBA16F），全屏 pass 用 64 个切空间半球样本 + 4×4 随机旋转噪声估计遮蔽，最后 4×4 box blur 去噪。
- 流程（`Scene::RenderMeshes`）：方向光阴影 → 点光 cube 阴影 → **SSAO 几何 pass + AO 生成** → 主 pass → 天空盒 → 后处理。
- PBR shader：`ssao = texture(ssao_map, gl_FragCoord.xy / textureSize(...))`，只乘进 ambient 项（`ambient *= ssao`），不影响直接光。
- 新增 shader：`ssao_geometry_{vert,frag}.glsl`、`ssao_{vert,frag}.glsl`、`ssao_blur_frag.glsl`。
- 已知限制：无 resize 处理、全分辨率 64 样本无优化；`SSAO` 为 OpenGL 专属。

## 体积光 / God Rays（M4e 新增）

- 后处理新增 god rays pass（`god_rays_frag.glsl`）：从方向光太阳的屏幕位置做径向模糊（累加场景亮部，decay/density/weight 控制衰减），形成光柱。
- `composite_frag.glsl` 在 ACES tone mapping 之前叠加 `god_rays * god_rays_strength`。
- `Renderer::PostProcess(view, proj)` 把 `-light.direction`（太阳方向）投影到屏幕空间作为光源。
- `Renderer`/`Scene` 新增 `SetGodRaysStrength`。
- 已知限制：屏幕空间径向模糊（无深度遮挡/真正体积雾）；god rays 为半分辨率、构造时尺寸。

## RHI 抽象（IRHI）

`engine/src/render/rhi/rhi.hpp`：

```cpp
class IRHI {
  virtual GraphicsAPI GetAPI() const = 0;
  virtual void SetupWindowHints() const = 0;
  virtual bool Initialize(GLFWwindow*) = 0;
  virtual void BeginFrame(const glm::vec4& clear_color) const = 0;
  virtual void EndFrame(GLFWwindow*) const = 0;
  virtual bool InitializeImGuiBackend(GLFWwindow*) = 0;
  virtual void ShutdownImGuiBackend() const = 0;
  virtual void BeginImGuiFrame() const = 0;
  virtual void RenderImGuiDrawData(ImDrawData*) const = 0;
  virtual void DrawIndexedTriangles(int index_count) const = 0;
  virtual unsigned int CreateFramebuffer() const = 0;
  virtual void DestroyFramebuffer(unsigned int) const = 0;
  virtual void BindFramebuffer(unsigned int) const = 0;
  virtual void ClearBoundFramebufferColor(const glm::vec4&) const = 0;
};
```

工厂：`CreateRHI(GraphicsAPI)`，另有 `SetActiveRHI` / `GetActiveRHI` 管理当前后端（单例式指针）。

### OpenGLRHI
- `SetupWindowHints`：请求 OpenGL 4.6 core。
- `Initialize`：`glfwMakeContextCurrent` + `gladLoadGLLoader`。
- ImGui 后端：`imgui_impl_opengl3`。

### VulkanRHI（部分实现）
- 有 instance / surface / device / swapchain 等初始化代码（`CreateInstance`、`CreateSwapchain` 等）。
- 编译开关：`MENGINE_HAS_VULKAN`（CMake 在找到 Vulkan SDK 时定义）。
- 未定义时，`Initialize` 打日志并返回 false，`CreateRHI` 回退到 OpenGL。
- ⚠️ 资源后端（Texture/Shader/FrameBuffer/VAO）均为空壳，因此 **Vulkan 路径目前无法真正渲染**，处于实验状态。

## 着色器资源

- `sandbox/res/shaders/default_vert.glsl`（`#version 460`）：输入 `aPos`/`aTexCoord`，uniform `model`/`proj_view`，输出 `TexCoord`。
- `sandbox/res/shaders/default_frag.glsl`：`texture(texture1, TexCoord)`。
- `sandbox/res/shaders/lit_vert.glsl`（M1 新增）：输入 `aPos`/`aNormal`/`aTexCoord`，输出世界空间 `FragPos`/`Normal`，计算法线矩阵。
- `sandbox/res/shaders/lit_frag.glsl`（M1 新增）：Blinn-Phong 方向光 + 可选纹理（`has_texture`）+ 镜面高光。

## 当前渲染局限

1. **无背面剔除**：`GL_CULL_FACE` 未开启，所有面都绘制。
2. **Vulkan 未完成**：后端空壳，无真实 GPU 资源（网格复用 `IVertexArrayBackend`，接口已就位）。
3. **光照未抽象**：方向光参数写死在 shader 默认值，尚无引擎级 Light/Material。
4. **RenderContext 重复**：与 RenderPass 冗余。
5. **资源加载路径写死**：默认 shader 路径硬编码在 Renderer 构造中。
6. **无渲染图（Render Graph）/ 无自动资源生命周期管理**。
