# MEngine 整理重构 —— 执行日志（WORKLOG）

> 逐条记录：日期 / 分支 / commit / 做了什么 / 如何验证 / 遇到的问题与解决 / 下一步。
> 配套总计划见 [DEV-PLAN.md](./DEV-PLAN.md)。每条以“执行记录”为单位，新记录加在最上方。

---

## 2026-09-05 — P0：文档骨架 + profiler 头文件修复

- **分支**：`refractor` → `refactor/framework`（docs 骨架先落在 `refactor/framework`）
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
