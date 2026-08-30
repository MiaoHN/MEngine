# 构建与工具链

## 概览

MEngine 使用 **CMake（≥3.21）+ Ninja** 作为统一构建体系，所有平台/编译器/构建类型的组合都由根目录的 `CMakePresets.json` 声明。

支持的组合：

| 平台 | 工具链 | Preset 前缀 |
| --- | --- | --- |
| Windows | MSVC | `windows-msvc-*` |
| Windows | Clang | `windows-clang-*` |
| Linux | GCC | `linux-gcc-*` |
| Linux | Clang | `linux-clang-*` |
| macOS | AppleClang | `macos-*` |

每个前缀都有 `-debug` / `-release` 两个变体。

## 常用命令

```bash
cmake --list-presets            # 查看当前平台可用的 preset

cmake --preset windows-msvc-debug      # 配置
cmake --build --preset windows-msvc-debug   # 构建
ctest --preset windows-msvc-debug      # 测试（当前暂无测试用例）
```

> Windows 的 `windows-msvc-*` 使用 Ninja 生成器，需在 **Developer PowerShell/CMD** 中运行（`cl.exe` 需在 PATH）。
> `windows-clang-*` 需要 MSVC 兼容的 Clang 环境在 PATH 上。

## 构建目录

- 每个 preset 的输出目录为 `build/<preset-name>/`，彼此隔离，互不干扰。
- `build/` 已被 `.gitignore` 忽略。
- 编译数据库（`compile_commands.json`）默认开启，供 IDE / clangd 使用。

## CMake 选项

| 变量 | 默认 | 说明 |
| --- | --- | --- |
| `MENGINE_ENABLE_VULKAN` | `ON` | 是否尝试查找 Vulkan SDK；找不到时自动回退（仅编译 OpenGL 后端） |
| `MENGINE_BUILD_SANDBOX` | `ON` | 是否构建 sandbox 示例 |
| `MENGINE_BUILD_EDITOR` | `ON` | 是否构建 editor |

## 编译器警告配置

统一在 `cmake/CompilerWarnings.cmake` 中：

- `mengine_enable_warnings(target)`：对 MEngine 自有目标开启警告
  - MSVC：`/W4 /permissive- /Zc:__cplusplus`
  - GCC/Clang：`-Wall -Wextra -Wpedantic`
- `mengine_quiet_third_party(target)`：静音第三方库在较新 Clang 上默认开启的 `-Wnontrivial-memcall`。

第三方库的头文件以 `SYSTEM` include 方式引入（GCC/Clang 的 `-isystem`，MSVC 的 `/external:I`），从而抑制第三方头文件内部的警告，只保留 MEngine 自身代码的警告。

## 目标结构

```
MEngine/
├── engine/   → 静态库 engine（依赖 glfw、lua、imgui、ImGuizmo、Threads::Threads）
├── editor/   → 可执行程序 editor（链接 engine）
├── sandbox/  → 可执行程序 sandbox（链接 engine）
└── deps/     → glfw(子目录) / lua / imgui / ImGuizmo 以 add_subdirectory 或静态库方式接入
```

依赖接入细节（根 `CMakeLists.txt`）：

- `glfw`：`add_subdirectory(deps/glfw)`，使用其导出的 `glfw` target。
- `lua`：手动将 `deps/lua/*.c` 编成静态库 `lua`（剔除 `onelua.c`）。
- `imgui`：手动 GLOB `deps/imgui/*.cpp` + `backends/imgui_impl_{glfw,opengl3,vulkan}.cpp` 编成静态库 `imgui`。
- `ImGuizmo`：GLOB `deps/ImGuizmo/*.cpp` 编成静态库 `ImGuizmo`。
- `glad`：`glad.c` 直接编入 `engine`。
- `entt/glm/stb`：纯头文件，以 include 目录方式使用。

## CI

`.github/workflows/build-test.yml` 使用 GitHub Actions 矩阵验证四组工具链：

- Windows (MSVC)：`ilammy/msvc-dev-cmd` 提供 MSVC 环境。
- Linux (GCC / Clang)：安装 X11 / Wayland / OpenGL 开发包。
- macOS (AppleClang)。

## 跨平台源码约定

为保证三平台可编译，注意：

- 不要使用 MSVC 专属函数（如 `localtime_s`、`strcpy_s`）。已有约定：
  - `logger.cpp` 时间格式化：`_WIN32` 用 `localtime_s`，其余用 `localtime_r`。
  - `editor.cpp` 字符串拷贝：用 `std::snprintf`。
- `_SILENCE_STDEXT_ARR_ITERS_DEPRECATION_WARNING` 仅在 `if(MSVC)` 下定义。
- `ERROR` 宏冲突（GLFW → `windows.h` 定义 `ERROR`）已在 `core/common.hpp` 中通过 `#undef ERROR` 处理；涉及 GLFW 的 `.cpp` 应先包含其 `.hpp`（间接含 `common.hpp`），再包含 `imgui_impl_glfw.h` / `logger.hpp`。
- `engine` 链接 `Threads::Threads`（profiler 使用 `std::mutex` / `std::this_thread`）。

## 第三方依赖（git submodule）

- entt、glad、glfw、glm、imgui、lua、spdlog、stb（`deps/` 下）。
- `spdlog` 目前**未接入构建**（README 提及但 CMake 未使用）。
- 克隆时使用 `git clone --recursive`。
