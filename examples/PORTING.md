# LearnOpenGL → MEngine 复刻对照表 (PORTING)

> 目标：用 **MEngine 公共 API** 复刻 [LearnOpenGL](https://github.com/JoeyDeVries/LearnOpenGL)
> `src/` 下的示例（本地源码/资源在仓库根 `LearnOpenGL/`）。**不复刻裸 GL 代码**——而是
> 把每个示例的“场景与效果”用 MEngine（Scene/实体/PBR 材质/灯光/后处理/skybox 等）重建。
>
> 每个复刻成品 = `examples/src/<name>.cpp` 一个**独立可执行**（`example_<name>`），
> 共享宿主在 `examples/src/example_app.*`，小工具在 `example_helpers.hpp`。
>
> 状态：✅ = 已复刻可运行；◐ = 部分/可等价（引擎能力演示）; ⛔ = 底层裸 GL 特性，
> 公共 API 无法直译（或在 MEngine 内部已实现，非示例层）；⬜ = 待做。

## 1. getting_started（入门：都是裸管线/窗口/VBO/着色器底层）
| 目录 | 内容 | 状态 |
|---|---|---|
| 1.1 hello_window … 2.x hello_triangle / shaders / 4.x textures / 5.x transformations / 6.x coordinate_systems / 7.x camera | 窗口、三角形、uniform、UV/纹理、变换、坐标系、相机 | ⛔ 这些是“从零搭 OpenGL 管线”的底层教学，MEngine 已封装（窗口/相机/变换/纹理/管线都是引擎内部），无对应“用户层场景”。概念等价可看现有任一 example（orbit 相机 + 变换 + 纹理）。 |

## 2. lighting（光照）—— 引擎为 PBR，逐场景等价重建
| 目录 | 内容 | MEngine 成品 |
|---|---|---|
| 1.colors | 颜色相乘 | ✅ `example_colors` |
| 2.x basic_lighting (+specular/exercise) | 漫反射+高光 | ✅ `example_basic_lighting`（diffuse+specular via PBR roughness）|
| 3.x materials | 材质参数 | ✅ `example_materials`（metallic/roughness 扫描）|
| 4.x lighting_maps (diffuse/specular) | 贴图（diffuse/specular map）| ✅ `example_lighting_maps`（container2 贴图 + 低粗糙度高光；specular map ≈ roughness/金属度）|
| 5.x light_casters (dir/point/spot/soft) | 方向/点/聚光 | ✅ `example_light_casters`（方向+两聚光+点光；软边 via outer_cutoff）|
| 6.multiple_lights | 多光源 | ✅ `example_multiple_lights` |
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
| 1.advanced_lighting | Blinn-Phong | ◐ 引擎用 GGX PBR；等价高光 via roughness 已演示（basic_lighting）|
| 2.gamma_correction | Gamma | ◐ 引擎输出已含 gamma（post）；无单独场景 |
| 3.x shadow_mapping (+point/soft/csm) | 阴影映射/点阴影 | ✅ `example_shadow_mapping`（方向光+立方体点光阴影）；CSM ⬜（引擎单级）|
| 4.normal_mapping | 法线贴图 | ✅ `example_normal_mapping`（砖墙 albedo+normal，引擎 pbr 法线槽）|
| 5.x parallax (incl steep/pom) | 视差映射 | ⬜（引擎 pbr 无视差；需引擎扩展或 ⛔）|
| 6.hdr | HDR | ◐ 引擎 HDR 内部；`example_hdr_bloom` 演示高动态亮度 |
| 7.bloom | 泛光 | ✅ `example_hdr_bloom`（bloom）|
| 8.x deferred (+volumes) | 延迟着色 | ⛔ 引擎为前向+实例化 |
| 9.ssao | 屏幕空间环境光遮蔽 | ◐ 引擎已有 SSAO（内部开关）；可做一个 SSAO 开关演示 exe ⬜ |

## 6. pbr
| 目录 | 内容 | 状态 |
|---|---|---|
| 1.x lighting / textured | PBR 直射 | ✅ 引擎即 PBR；`example_materials` 覆盖（加贴图见 lighting_maps/normal）|
| 2.x ibl (irradiance/specular conversion) | IBL 预计算 | ⛔ 引擎 skybox 在内部已完成 IBL（prefilter/irradiance）；非用户层 |
| PBR 资源 (rusted_iron/gold 等) | — | 可下载到 `assets/textures/pbr/` 后做 PBR 贴图材质场景 ⬜ |

## 7. in_practice / 8. guest
| 目录 | 内容 | 状态 |
|---|---|---|
| 1.debugging | 调试技巧 | ⛔ 文档性质 |
| 2.text_rendering (FreeType) | 文字渲染 | ⬜（引擎无文本/字体模块；较大，另议）|
| 3.2d_game | 2D Breakout 游戏 | ⬜ 大工程（引擎 2D Sprite 能力有限）|
| 8.guest (2020/2021/2022) | 嘉宾教程 | ⬜ 逐个评估 |

## 现有例子命名对照（examples/src）
```
colors              -> 2.lighting/1.colors
basic_lighting      -> 2.lighting/2.x
materials           -> 2.lighting/3.x
lighting_maps       -> 2.lighting/4.x   (新增)
multiple_lights     -> 2.lighting/6
light_casters       -> 2.lighting/5.x
shadow_mapping      -> 5.advanced_lighting/3.x
hdr_bloom           -> 5.advanced_lighting/6-7
normal_mapping      -> 5.advanced_lighting/4   (新增)
```
