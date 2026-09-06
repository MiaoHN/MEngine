# LearnOpenGL → MEngine 复刻对照表 (PORTING)

> 目标：用 **MEngine 公共 API** 复刻 [LearnOpenGL](https://github.com/JoeyDeVries/LearnOpenGL)
> `src/` 下的示例（本地源码/资源在仓库根 `LearnOpenGL/`）。**不复刻裸 GL 代码**——而是
> 把每个示例的“场景与效果”用 MEngine（Scene/实体/PBR 材质/灯光/后处理/skybox 等）重建。
>
> 每个复刻成品 = `examples/src/<name>.cpp` 一个**独立可执行**（target `example_<name>`），
> 共享宿主在 `examples/src/example_app.*`，小工具在 `example_helpers.hpp`。
>
> 状态：✅ = 已复刻可运行；◐ = 部分/可等价（引擎能力演示）; ⛔ = 底层裸 GL 特性，
> 公共 API 无法直译（或在 MEngine 内部已实现，非示例层）；⬜ = 待做。

> **三套材质/光照可用**：MEngine 现在提供
> 1. **PBR(GGX，默认)** —— `shader "pbr"`，`example_helpers.hpp` 的 `Pbr(...)` / `PbrTextured(...)`；
> 2. **经典 Blinn-Phong** —— `shader "blinn"`（`assets/shaders/blinn_*`），`Blinn(...)` / `BlinnTextured(...)`，
>    高光指数 `shininess`、强度 `specular`，环境光由 `ibl_intensity` 控制；
> 3. **LO-exact（复刻用）** —— `shader "blinn_lo"`（`assets/shaders/blinn_lo_frag.glsl`），`BlinnLo(...)` /
>    `BlinnLoTextured(...)`。它**逐项复刻 LO 的 .fs**：每灯 ambient/diffuse/specular 三分量、
>   Phong(reflect) 高光且**不乘 NdotL**、`container2_specular` 逐像素高光贴图（Material `SetSpecularMap`）、
>   LO c/l/q 衰减（`PointLight/SpotLight` 加 `lo_attenuation`）、无阴影/无 IBL 环境光。
>     - 端口统一走 `examples::LoScene(scene)`：`SetLoLighting(true)` + `SetLinearOutput(true)`
>      （composite 原样 clamp，不做 ACES/gamma，和 LO 直写一致）+ 关 skybox/TAA/bloom/SSAO + 背景 0.1；
>    - 每灯在灯体上设 `ambient/diffuse/specular`（替代旧 color/intensity）；    - **没有太阳的 LO 场景必须 `examples::NoSun(scene)`** 把方向灯的三分量清零——
      否则引擎默认方向灯（ambient .05/diffuse 1/spec 1）会平白给物体加一层“太阳光”，
      这正是 LO 4.2/3.1/2.2 曾比原版偏亮的主因；>    - 材质无贴图时可 `SetSpecularColor(...)` 给出 LO 的 `material.specular` 颜色；
>    - **窗口固定 800×600**（LO 的原生尺寸/4:3）：每个 example 的 `CreateApplication` 先
>      `Application::SetStartupWindowSize(800, 600)`；相机 `target(0,0,0), yaw0, pitch0, dist 3, fov45`
>      即 LO 相机 (0,0,3) 看 -Z。

## 1. getting_started（入门：都是裸管线/窗口/VBO/着色器底层）
| 目录 | 内容 | 状态 |
|---|---|---|
| 1.1 hello_window … 2.x hello_triangle / shaders / 4.x textures / 5.x transformations / 6.x coordinate_systems / 7.x camera | 窗口、三角形、uniform、UV/纹理、变换、坐标系、相机 | ⛔ 这些是“从零搭 OpenGL 管线”的底层教学，MEngine 已封装（窗口/相机/变换/纹理/管线都是引擎内部），无对应“用户层场景”。概念等价可看现有任一 example（orbit 相机 + 变换 + 纹理）。 |

## 2. lighting（光照）—— LO-exact 逐项复刻（blinn_lo）
> 状态说明：✅ **1:1 LO-exact 复刻**（LO 相机/数值/贴图/输出，可并排比对）；◐ PBR 引擎能力演示
> （不等同 LO 单场景截图）。

| 目录 | 内容 | MEngine 成品 |
|---|---|---|
| 1.colors | 颜色相乘 | ◐ `ex_2_1_colors`（PBR 引擎演示）|
| 2.2 basic_lighting_specular | Phong 漫反射+高光（单点光）| ✅ `ex_2_2_blinn_lighting`（blinn_lo，coral 立方+白灯 1.2,1,2）；◐ `ex_2_2_basic_lighting`（PBR 演示）|
| 3.1 materials | 材质参数（Phong）| ✅ `ex_2_3_materials`（blinn_lo：coral 材质 + 灰 specular 0.5，光 0.1/0.5/1.0）|
| 4.2 lighting_maps_specular | diffuse+specular 贴图 | ✅ `ex_2_4_lighting_maps`（blinn_lo：container2 + container2_specular，shininess 64，光 0.2/0.5/1.0）|
| 5.x light_casters (dir/point/spot/soft) | 方向/点/聚光 | ✅ `ex_2_5_light_casters`（blinn_lo：10 木箱 + **相机手电**软聚光，LO 5.3/5.4：锥内 diffuse/spec 带衰减，锥外仅环境光 0.1；手电每帧跟随轨道相机）|
| 6.multiple_lights | 多光源 | ✅ `ex_2_6_multiple_lights`（blinn_lo：10 木箱+spec map，dir 0.05/0.4/0.5，4×点光 0.05/0.8/1.0 + c/l/q；LO 相机手电省略）|
| exercises | 练习 | ◐ 概念已含在上面对应成品中 |

## 3. model_loading
| 目录 | 内容 | 状态 |
|---|---|---|
| 1.model_loading | 加载 backpack 等模型 | ⬜（引擎 `ModelLoader::LoadObj`/`LoadGltf` 可用；LO 用 .obj+mtl，计划用其 mesh + 手动贴图做成 `example_model_loading`）|

## 4. advanced_opengl（多数为底层 GL 特性）
| 目录 | 内容 | 状态 |
|---|---|---|
| 1.x depth_testing | 深度测试 | ⛔ 引擎已做（深度缓冲+遮挡）|
| 2.stencil_testing | 模板测试（描边）| ⛔ MEngine 无模板接口（内部管线未开）|
| 3.x blending (discard/sort) | Alpha/透明排序 | ◐ 引擎已加通用半透明 pass（voxel 水用过）；discard 需引擎 alpha test（未加）→ 待评估 |
| 4.face_culling | 面剔除 | ⛔ 引擎已做（Material cull mode）|
| 5.x framebuffers | 离屏 FBO | ⛔ 引擎内部（FrameBuffer/RenderMeshes target_fbo 已封）|
| 6.x cubemaps (skybox/environment) | 天空盒/环境映射反射 | ◐ 天空盒引擎已有（examples 默认关）；**反射(reflection)材质**引擎未暴露 → 需引擎加，⬜ |
| 8.advanced_glsl_ubo | UBO | ⛔ 引擎内部 |
| 9.x geometry_shader | 几何着色器 | ⛔ 引擎无几何着色器出口 |
| 10.x instancing | 实例化 | ◐ 引擎内部已实例化绘制；无用户层接口 → ⛔/⬜ |
| 11.x MSAA | 多重采样 | ⛔ 引擎内部未开 MSAA 选项 |

## 5. advanced_lighting
| 目录 | 内容 | MEngine 成品 |
|---|---|---|
| 1.advanced_lighting | Blinn-Phong | ✅ LO 2.2 高光即 **Phong/Blinn**；`ex_2_2_blinn_lighting` 是 LO-exact 端口（blinn_lo）|
| 2.gamma_correction | Gamma | ◐ 引擎输出已含 gamma（post）；无单独场景 |
| 3.x shadow_mapping (+point/soft/csm) | 阴影映射/点阴影 | ✅ `ex_5_3_shadow_mapping`（blinn_lo 方向光阴影 `SetLoDirShadow`，LO 3.1.3：木地板+木箱，Blinn^64，光 0.3）；点阴影/CSM ⬜ |
| 4.normal_mapping | 法线贴图 | ✅ `ex_5_4_normal_mapping`（blinn_lo + 法线贴图：2×2 砖墙，点光 0.5,1,0.3，ambient .1/diffuse 1/spec(材质 0.2 灰)，Blinn 高光 `SetLoBlinnSpec`）|
| 5.x parallax (incl steep/pom) | 视差映射 | ⬜（引擎 pbr 无视差；需引擎扩展或 ⛔）|
| 6.hdr | HDR | ✅ `ex_5_6_hdr_bloom`（blinn_lo，LO 曝光色调 `1-exp(-x)`+gamma，`SetLoHdrTone`）|
| 7.bloom | 泛光 | ✅ `ex_5_6_hdr_bloom`（引擎 bloom：亮度阈值>1 + 高斯模糊；木地板 + container 方块 + 4 HDR 点光 1/d²）|
| 8.x deferred (+volumes) | 延迟着色 | ◐ `ex_5_8_deferred_shading` = **前向等效版**（引擎为前向，非延迟；仅复刻 LO 8.1 场景：3×3 背包网格 + 全部 32 盏 srand(13) 随机彩点光 + LO 衰减，见下）|
| 9.ssao | 屏幕空间环境光遮蔽 | ✅ `ex_5_9_ssao` = **引擎真实 SSAO 演示**（LO 9.ssao 场景精神的木地板+木箱+背包；SSAO 默认开，**空格切换** on/off 对照）|

## 6. pbr
| 目录 | 内容 | 状态 |
|---|---|---|
| 1.1.lighting | PBR 直射 | ✅ `ex_6_1_1_pbr_lighting`（引擎 PBR：7×7 红球，metallic=行/7、roughness=列/7，4×300 白光 1/d²；IBL 关、clear 0.1；后处理 ACES vs LO Reinhard）|
| 1.2.lighting_textured | PBR 直射+贴图 | ✅ `ex_6_1_2_pbr_lighting_textured`（rusted_iron albedo/normal/ao 贴图 raw；引擎用合并 MR 贴图、LO 分开 metallic/roughness 两张灰度 → 以标量金属/粗糙近似，见文件头注）|
| 2.x ibl (irradiance/specular conversion) | IBL 预计算 | ⛔ 引擎 skybox 在内部已完成 IBL（prefilter/irradiance）；非用户层 |
| PBR 资源 (rusted_iron/gold 等) | — | ✅ 已镜像到 `assets/textures/pbr/{rusted_iron,gold,grass,plastic,wall}/`（6.1.2 已用 rusted_iron）|

## 7. in_practice / 8. guest
| 目录 | 内容 | 状态 |
|---|---|---|
| 1.debugging | 调试技巧 | ⛔ 文档性质 |
| 2.text_rendering (FreeType) | 文字渲染 | ⬜（引擎无文本/字体模块；较大，另议）|
| 3.2d_game | 2D Breakout 游戏 | ⬜ 大工程（引擎 2D Sprite 能力有限）|
| 8.guest (2020/2021/2022) | 嘉宾教程 | ⬜ 逐个评估 |

## 现有例子命名对照（target / 源文件 = ex_<LO章>_<LO小节>_<名>）
```
ex_2_1_colors              -> 2.lighting/1.colors (PBR 演示)
ex_2_2_basic_lighting      -> 2.lighting/2.2.basic_lighting_specular (PBR 演示)
ex_2_2_blinn_lighting      -> 2.lighting/2.2.basic_lighting_specular (✅ blinn_lo 1:1)
ex_2_3_materials           -> 2.lighting/3.1.materials (✅ blinn_lo 1:1)
ex_2_4_lighting_maps       -> 2.lighting/4.2.lighting_maps_specular_map (✅ blinn_lo 1:1)
ex_2_5_light_casters       -> 2.lighting/5.3.light_casters_spot / 5.4 soft (✅ blinn_lo 1:1，相机手电)
ex_2_6_multiple_lights     -> 2.lighting/6.multiple_lights (✅ blinn_lo 1:1)
ex_5_3_shadow_mapping      -> 5.advanced_lighting/3.1.3.shadow_mapping (✅ blinn_lo 1:1)
ex_5_4_normal_mapping      -> 5.advanced_lighting/4.normal_mapping (✅ blinn_lo 1:1)
ex_5_6_hdr_bloom           -> 5.advanced_lighting/6.hdr + 7.bloom (✅ blinn_lo 1:1)
ex_5_8_deferred_shading   -> 5.advanced_lighting/8.1.deferred_shading (◐ 前向等效：9 背包 + 32 盏彩点光；引擎为前向、点光上限 32 blinn_lo)
ex_5_9_ssao               -> 5.advanced_lighting/9.ssao (✅ 引擎真实 SSAO 演示，空格 on/off)
ex_6_1_1_pbr_lighting     -> 6.pbr/1.1.lighting (✅ 引擎 PBR 直射：7×7 金属/粗糙矩阵)
ex_6_1_2_pbr_lighting_textured -> 6.pbr/1.2.lighting_textured (◐ 引擎 PBR 贴图：rusted_iron；MR 分离贴图→标量近似)
```
> 每个 target 对应 LO 源码：`LearnOpenGL/src/<章>/<小节>/<源码名>.cpp`（CMake 注释里已写死）。
> 新建端口一律沿用该命名，如 `ex_3_1_model_loading`、`ex_6_1_1_pbr_lighting` 等。
