# MEngine 整理重构 —— 执行日志（WORKLOG）

> 逐条记录：日期 / 分支 / commit / 做了什么 / 如何验证 / 遇到的问题与解决 / 下一步。
> 配套总计划见 [DEV-PLAN.md](./DEV-PLAN.md)。每条以“执行记录”为单位，新记录加在最上方。

---

## 2026-09-05 — P1 续：core 去 imgui 依赖 + Ref 抽 base.hpp

- **分支**：`refractor`；commit：`134dee4`、`0523051`（另有前面 `docs…`/`7145267`）
- **做了什么**：
  1. 新建自包含 `engine/src/core/base.hpp` 承载 `Ref/CreateRef`，`common.hpp` 改包含之。
  2. 从 `core/common.hpp` 移除 `<imgui.h>/<imgui_internal.h>`：确认 imgui 实际只被
     `application.cpp`、RHI 的 ImGui 后端实现、editor 使用。
     - `application.cpp` 显式 `#include <imgui.h>`；
     - `rhi.hpp` 仅用 `ImDrawData*` 指针 → 前置声明，保持 render/core 不依赖 imgui；
     - `editor.cpp` 显式引入 `imgui.h + imgui_internal.h`。
  3. GLFW/glm 暂留在 common.hpp（平台/窗口依赖），后续再做 platform 边界收敛。
- **验证**：`windows-clang-debug` 与 `windows-clang-release` 全链编译通过。
- **遇到的问题**：`Application` 构造里直接 `ImGui::CreateContext()`（随后被 Editor 覆盖丢弃）是
  分层遗留；已记入待办（主循环/应用层职责清理）。本次仅做 include 边界，不改行为。
- **下一步**：P1 剩余 —— (a) application.cpp 中 ImGui context 移交给 Editor（engine 去 UI）；
  (b) platform/GLFW 显式化；(c) 接口/注释/LOG 审计；(d) 主循环/运行时抽象。

---

## 2026-09-05 — P1 起始：core 依赖收敛（base.hpp）等

- **分支**：`refractor`；commit：`docs…`、`7145267`(fix utils profiler)、`134dee4`(refactor core base)
- **做了什么**：
  1. 新建 `engine/src/core/base.hpp`：**自包含、零依赖**地承载 `Ref/CreateRef`；
     `common.hpp` 改为包含 base.hpp（避免重复定义）。
  2. 说明：`common.hpp` 里 imgui/GLFW/glm 的聚合仍保留以保证既有 TU 编译；下一步把
     imgui（仅 `application.cpp` 真正使用）从 core 头剔除，需要为 editor/application
     显式引入 imgui 头后再删，作为“依赖收敛”的后续 commit。
- **验证**：`windows-clang-debug` 编译通过（engine/sandbox/editor 全链）。
- **遇到的问题**：`application.cpp` 在引擎层直接 `ImGui::CreateContext()`（且其 context 后被
  editor 覆盖丢弃）是历史遗留的分层问题，已记录，纳入后续“主循环/应用层职责”清理范围。
- **下一步**：把 imgui/GLFW 从 core/common.hpp 剔除并显式加入使用方；随后接口/注释/LOG 审计。

---

## 2026-09-05 — P0：文档骨架 + profiler 头文件修复

- **分支**：`refractor`
- **做了什么**：
  1. 建立 `docs/DEV-PLAN.md`（总计划 + 分支/提交地图）与 `docs/WORKLOG.md`（本日志）；
     并在 `docs/README.md` 文档索引登记这两个文件。
  2. 审查 `engine/src/utils/profiler.h` / `profiler.cpp`，确认用户提示的头文件问题：
     - 头文件**非自包含**：使用 `std::this_thread`/`std::thread::id`/`std::string`/`std::hash`
       却未包含 `<thread>`/`<string>`/`<functional>`（依赖 TU 里其它头侥幸编译）。
     - `Profiler(const std::string&)` 构造用 `name_.c_str()` 存 const char* 指针 →
       若传入临时 string 会**悬垂指针**（析构时才读 name_，use-after-free 风险）。
     - `Dump()` 里 pid 与 tid 都取线程 id 的 hash（pid 实为线程 id，且 JSON 又硬编码 `"pid":0`）。
     - `profiler.cpp` 在 `PROFILER_ENABLED=0` 时仍会在 CWD 创建/截断 `profile_results.json` 并逐条 flush。
  3. 重写 `profiler.h`/`profiler.cpp`：头文件自包含、name 改为持有 `std::string`、
     pid/tid 语义正确、仅在 `PROFILER_ENABLED` 开启时落盘。
- **验证**：`cmake --build --preset windows-clang-debug` 与 `windows-clang-release` 均通过。
- **遇到的问题**：无（均为静态审查发现）。
- **下一步**：P1 —— 拆分 `core/common.hpp` 中 imgui/GLFW 等依赖，推进框架分层。
