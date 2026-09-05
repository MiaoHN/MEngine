# 资源管理设计（Asset Management）

> 状态：**已实现（Phase 1 + 2 + manifest 映射文件）**
> 目标：把散落在引擎各处的硬编码路径与重复资源，收敛为一个统一的 `AssetManager` 服务，参考 Unity `Resources` / UE `AssetManager` / Blender DataBlock 的成熟思路，同时贴合 MEngine 现有代码结构。

## 0. 实现记录

- **AssetManager**（`engine/src/render/asset_manager.hpp/.cpp`）：单例，`SetAssetRoot` / `Resolve` /
  `GetShader(name)` / `GetTexture(path)` / `GetDefaultShader` / `GetDefaultTexture`，内部复用
  `ShaderLibrary` / `TextureLibrary` 缓存。
- **manifest.json**（`assets/manifest.json`）：逻辑名 → 文件路径的映射表（shader：名字 → vert/frag
  两个文件；texture：名字 → 相对路径）。`GetShader(name)` 优先查 manifest，失败回退到
  `shaders/{name}_vert/frag.glsl` 命名约定。
- **资源根目录**：仓库根共享 `assets/`（单一来源），`Application` 构造时
  `SetAssetRoot("assets")`；`sandbox/editor` 的 CMake POST_BUILD 把 `assets/` 拷到各 app build 目录。
- **去重**：`sandbox/res/shaders`、`editor/res/shaders` 等重复目录已合并进 `assets/`，各 app 的
  `res/` 仅保留 `imgui.ini` 等独有内容。
- **迁移范围**：引擎核心（`Renderer` / `Skybox` / `PostProcessing` / `SSAO`）的 shader、
  default texture、HDR 路径，以及 sandbox 的模型/pbr shader、editor 的图标/脚本，均已改走
  `AssetManager`。

## 1. 现状与痛点

### 1.1 现有资源类

| 类 | 位置 | 职责 | 现状 |
| --- | --- | --- | --- |
| `Shader` | `engine/src/render/shader.hpp/.cpp` | 编译 GLSL，持有 `IShaderBackend` | 按路径构造 `Shader(vert_path, frag_path)` |
| `ShaderLibrary` | 同上 | 名字 → `Ref<Shader>` 缓存 | 已存在，但引擎核心**未使用** |
| `Texture` | `engine/src/render/texture.hpp/.cpp` | stb_image 加载 2D 纹理 / 内存数据 | 按路径构造、`SetData` 手动上传 |
| `TextureLibrary` | 同上 | 名字 → `Ref<Texture>` 缓存 | 已存在，但引擎核心**未使用** |
| `Mesh` | `engine/src/render/mesh.hpp/.cpp` | 顶点/索引缓冲 + `CreateCube/CreatePlane` | 程序化或由加载器创建 |
| `MeshLibrary` | 同上 | 名字 → `Ref<Mesh>` 缓存 | 已存在，但引擎核心**未使用** |

### 1.2 痛点

1. **路径硬编码散落**：`Renderer` / `Skybox` / `PostProcessing` / `SSAO` 内部大量
   `CreateRef<Shader>("res/shaders/xxx_vert.glsl", "res/shaders/xxx_frag.glsl")`。
2. **资源重复**：`sandbox/res/shaders/` 与 `editor/res/shaders/` 各有一份相同 shader
   （每次改 shader 都要同步两份）。
3. **资源根目录无统一配置**：各 app 依赖 CWD 相对路径 `res/`，由各自 CMake
   `POST_BUILD copy_directory` 拷贝到 build 目录。
4. **缓存类形同虚设**：`ShaderLibrary` 等存在但引擎核心绕过它们直接 `CreateRef`，
   同一 shader 可能被多次加载。

## 2. 设计目标

1. **单一入口**：所有资源通过 `AssetManager` 获取，调用方只关心「名字/相对路径」。
2. **懒加载 + 缓存**：首次访问时加载，之后返回同一实例（`Ref` 共享）。
3. **可配置资源根目录**：一个共享的 `assets/` 目录作为单一来源，app 启动时设置。
4. **默认回退**：加载失败回退到内置默认资源（default shader / 1×1 white texture）。
5. **可演进**：为后续资源句柄/ID、热重载、Material/Model 统一缓存、异步加载留口。

## 3. 总体架构

```
Application 启动
   └─ AssetManager::SetAssetRoot("<repo>/assets")   // 单一来源根目录

调用方（Renderer / Skybox / PostProcessing / SSAO / 加载器 / app）
   └─ AssetManager::GetShader("pbr")      → Ref<Shader>
   └─ AssetManager::GetTexture("textures/x") → Ref<Texture>
   └─ AssetManager::GetMesh("models/x")   → Ref<Mesh>

AssetManager（单例服务）
   ├─ asset_root_ : 路径
   ├─ Resolve(rel) : 相对路径 → 绝对路径（拼接 + 规范化）
   ├─ ShaderLibrary   ── 内部持有，负责 shader 缓存
   ├─ TextureLibrary  ── 内部持有，负责纹理缓存
   ├─ MeshLibrary     ── 内部持有，负责网格缓存
   └─ default_shader_ / default_texture_  // 回退资源
```

## 4. 目录结构（仓库根共享 `assets/`）

```
assets/
├── shaders/            # 引擎与各 app 共享的 GLSL（单一来源）
│   ├── default_vert.glsl / default_frag.glsl
│   ├── pbr_vert.glsl / pbr_frag.glsl
│   ├── skybox_vert.glsl / skybox_frag.glsl
│   ├── irradiance_frag.glsl
│   ├── prefilter_frag.glsl
│   ├── equirect_to_cube_frag.glsl
│   ├── post_vert.glsl / brightness_frag.glsl / blur_frag.glsl
│   ├── composite_frag.glsl / god_rays_frag.glsl / taa_frag.glsl
│   ├── ssao_*.glsl
│   └── shadow_depth_*.glsl / point_shadow_depth_*.glsl
├── textures/
│   ├── hdr/kloppenheim_06_puresky_1k.hdr
│   └── ...
├── models/
│   └── ...
├── icons/              # editor 专用图标（若必要）
└── scripts/            # Lua 脚本
```

CMake 层面：在**根 CMakeLists** 里给所有 target 统一加一条 POST_BUILD，把 `assets/`
拷贝到各 app 的 build 目录（例如 `$<TARGET_FILE_DIR:sandbox>/assets`），app 启动时
`SetAssetRoot("assets")`。`sandbox/res` 与 `editor/res` 中与 `assets/` 重复的部分删除，
只保留各 app 独有内容（如 `imgui.ini`）。

## 5. API 设计（草案）

```cpp
namespace MEngine {

class AssetManager {
 public:
  static AssetManager &Instance();          // 单例

  void SetAssetRoot(const std::string &root);   // 启动时调用一次

  // 资源根目录内的相对路径 → 绝对路径（找不到返回空串）
  std::string Resolve(const std::string &relative) const;

  // —— Shader：用「名字」，内部约定展开为 shaders/{name}_vert/frag.glsl ——
  Ref<Shader> GetShader(const std::string &name);                  // 如 "pbr"
  Ref<Shader> GetShader(const std::string &name,
                        const std::string &vert_rel,
                        const std::string &frag_rel);              // 显式指定两个文件

  // —— Texture / Mesh ——
  Ref<Texture> GetTexture(const std::string &relative);            // 如 "textures/x.png"
  Ref<Mesh>    GetMesh(const std::string &relative);               // 或名字 → 由 MeshLibrary 提供

  // —— 默认资源 ——
  Ref<Shader>  GetDefaultShader();
  Ref<Texture> GetDefaultTexture();

 private:
  AssetManager();
  std::string   asset_root_;
  ShaderLibrary shader_library_;
  TextureLibrary texture_library_;
  MeshLibrary   mesh_library_;
  Ref<Shader>   default_shader_;
  Ref<Texture>  default_texture_;
};

}  // namespace MEngine
```

### 5.1 Shader 命名约定

- `GetShader("pbr")` 展开为 `shaders/pbr_vert.glsl` + `shaders/pbr_frag.glsl`。
- 只有极少数 shader 不符合 `{name}_vert/{name}_frag` 约定（如全屏 `post_vert` 搭配多个
  frag、`skybox_vert` 搭配多个 frag），这些仍可用显式双路径重载，但名字要能唯一标识。

### 5.2 资源类型约定

| 类型 | 参数 | 说明 |
| --- | --- | --- |
| Shader | `name`（逻辑名） | 默认到 `shaders/` 下展开两文件 |
| Texture | `relative`（相对 assets 根） | 保留扩展名，如 `textures/hdr/x.hdr` |
| Mesh | `relative` 或 `name` | 优先走加载器，程序化网格仍用 `Mesh::Create*` |

## 6. 生命周期与缓存

- 缓存持有 `Ref<T>`（`shared_ptr`），外部拿到后生命周期安全；`AssetManager` 析构时统一释放。
- `Get*` 若缓存命中直接返回；未命中则加载 + 入缓存 + 返回。
- 引擎核心（Renderer 等）在构造时通过 `AssetManager` 一次性取到所需 `Ref` 存为成员，
  与现在行为等价，只是路径来源变了。

## 7. 默认 / 回退资源（UE `DefaultEngine` 风格）

- `GetDefaultShader()`：首次访问时加载 `shaders/default_vert/frag`，失败则编译一个最小
  passthrough shader。
- `GetDefaultTexture()`：1×1 白色纹理（现 `Renderer::default_texture_` 的逻辑上收）。
- 任何 `Get*` 失败：记录 `LOG_WARN` 并返回对应默认资源，保证渲染不崩。

## 8. 分阶段迁移计划

### Phase 1 — 引入 AssetManager + 迁移引擎内部（视觉无变化）
- 新增 `engine/src/core/asset_manager.hpp/.cpp`（或 `engine/src/asset/`）。
- `Application::Initialize` 中 `SetAssetRoot`。
- 替换引擎核心硬编码路径：`Renderer`（default/shadow/point_shadow）、`Skybox`
  （skybox/irradiance/prefilter/equirect + HDR）、`PostProcessing`（post/brightness/blur/
  composite/god_rays/taa）、`SSAO`（ssao_*）。
- `Renderer::default_texture_` 改为 `AssetManager::GetDefaultTexture()`。
- 验收：三平台构建通过，渲染结果与改动前一致。

### Phase 2 — 目录去重（单一来源 assets/）
- 新建仓库根 `assets/`，把共享 shader/HDR 迁入。
- 根 CMakeLists 统一拷贝 `assets/` 到各 app build 目录。
- 删除 `sandbox/res/shaders`、`editor/res/shaders` 等重复目录；`res/` 仅保留 app 独有内容。
- `editor` 的图标、Lua 脚本等一并归入 `assets/` 或保留 app 私有目录（视情况）。

### Phase 3 — 进阶（可选，后续）
- 资源句柄 / 稳定 ID（避免字符串比较）。
- Shader/Texture 热重载（文件监听 + 失效重建）。
- `Material` / `Model` 统一进 `AssetManager`。
- 异步加载与引用计数卸载。

## 9. 与现有类的关系

- `ShaderLibrary` / `TextureLibrary` / `MeshLibrary` **保留**，作为 `AssetManager` 的内部缓存，
  不再要求调用方直接使用（其公开接口可保留兼容）。
- `Shader` / `Texture` / `Mesh` 的构造方式**不变**，`AssetManager` 只负责「路径解析 +
  缓存 + 回退」。
- 不改 RHI/Backend 抽象，Vulkan 后端接入时 `AssetManager` 无需改动。

## 10. 参考

- **Unity**：`Resources.Load(path)` / `Shader.Find(name)` —— 名字化、运行时加载。
- **UE**：`UAssetManager` / `UObjectLibrary` —— 集中式资产注册、异步加载、PrimaryAssetId。
- **Blender**：DataBlock（`bpy.data`）—— 按名字索引、引用计数、唯一实例。

---

*本文档为设计稿，API 与目录可在实现时微调；实现前请确认 Phase 1 的命名约定与回退策略。*
