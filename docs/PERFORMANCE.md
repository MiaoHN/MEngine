# MEngine 渲染性能测试报告（PERFORMANCE）

> 配套整理重构计划见 [DEV-PLAN.md](./DEV-PLAN.md)。本文记录 P3 渲染优化
> （视锥剔除 + GPU 实例化 + 材质内容批处理 + uniform 缓存）的测量方法、
> 数据与结论。所有数据**无人值守**采集：`sandbox --scene --frames N --hidden`
> 运行后解析 `mengine.log` 中的 `[RenderStats]` 周期行（每 120 帧一条）。

## 1. 环境

| 项 | 值 |
| --- | --- |
| 主机 | Windows 11（win32，隐藏窗口 / 无垂直同步） |
| GPU / 驱动 | Intel(R) UHD Graphics（Intel driver 32.0.101.7085） |
| API | OpenGL 4.6 core（`--api opengl`） |
| 工具链 | Clang（`windows-clang-debug` / `windows-clang-release`，Ninja） |
| 相机 | 场景默认正交相机（原点看向 -Z，ortho ±5×±8.9） |

## 2. 复现命令

```bash
# 构建
cmake --build --preset windows-clang-release
cmake --build --preset windows-clang-debug

# 无人值守压测（运行 ~2000 帧，从 mengine.log 取 [RenderStats] 行）
./build/windows-clang-release/sandbox/sandbox.exe \
    --scene assets/scenes/stress_10000.scene --frames 2000 --hidden

# 批处理对照（MENGINE_NO_BATCH=1 时主 pass 逐实体绘制；像素应与批处理完全一致）
MENGINE_NO_BATCH=1 ./build/windows-clang-release/sandbox/sandbox.exe \
    --scene assets/scenes/stress_10000.scene --frames 600 \
    --capture-frame 500 --capture-out nobatch.ppm --hidden
```

测试场景（脚本生成，四色循环材质、单位立方体平铺 XZ 平面）：

| 场景 | 实体数 | 文件 |
| --- | --- | --- |
| `stress_cull.scene` | 1600 | `assets/scenes/stress_cull.scene` |
| `stress_4096.scene` | 4096 | `assets/scenes/stress_4096.scene` |
| `stress_10000.scene` | 10000 | `assets/scenes/stress_10000.scene` |

## 3. 数据（2026-09-05 采集）

`drawcalls` 为单帧全部 API 绘制（阴影 1 批 + 主 pass N 个材质批）；
`visible/culled` 为视锥剔除结果；`fps` 由 RenderStats 行时间戳中位数换算。

### release（windows-clang-release）

| 场景 | 实体 | fps | drawcalls | 实例批 | 三角形 | 可见 | 剔除 | shadow | main | post |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| stress_cull | 1600 | **789** | 5 | 5 | 24 336 | 428 | 1172 | 0.024 ms | 0.170 ms | 0.002 ms |
| stress_4096 | 4096 | **480** | 5 | 5 | 60 696 | 962 | 3134 | 0.068 ms | 0.405 ms | 0.004 ms |
| stress_10000 | 10000 | **230** | 5 | 5 | 145 920 | 2160 | 7840 | 0.611 ms | 1.147 ms | 0.004 ms |

### debug（windows-clang-debug，同场景）

| 场景 | 实体 | fps | drawcalls | main | shadow |
| --- | --- | --- | --- | --- | --- |
| stress_cull | 1600 | 51 | 5 | 1.78 ms | 0.20 ms |
| stress_4096 | 4096 | 20 | 5 | 3.33 ms | 0.41 ms |

### 与优化前基线对比（1600 实体，debug，同机同场景）

| 指标 | P3 前（逐实体绘制） | P3 后 | 变化 |
| --- | --- | --- | --- |
| drawcalls/帧 | 2028（1600 阴影 + 428 主） | **5**（1 阴影批 + 4 材质批） | ≈ **×400 减少** |
| 主 pass 耗时（debug） | ≈ 8.3 ms | ≈ 1.8 ms | ≈ ×4.6 |
| 方向光阴影 pass（debug） | ≈ 1.7 ms | ≈ 0.2 ms | ≈ ×8.5 |
| 三角形提交量 | 24 336 | 24 336 | 不变（正确性） |
| 像素输出（batch vs 逐实体） | — | **0 / 1 440 000 差异** | 像素级一致 |

基线（2028 drawcalls / 8.3 ms main）取自实例化落地前同一压测场景的
RenderStats 记录（见 WORKLOG 2026-09-05 P3 条目）。

## 4. 结论

1. **实例化 + 材质内容批处理是本机最大的单帧收益来源**：1 万实体场景
   每帧仅 5 个 API 绘制（1 次阴影实例批 + 4 个材质实例批），主 pass 在
   release 下约 1.1 ms、整体 230+ fps（无 vsync 隐藏窗口）。
2. **视锥剔除按相机有效削减主 pass 负载**：1 万实体仅 2160 可见
   （culled 7840，自洽：2160+7840=10000）。剔除集合逐帧稳定。
3. **正确性由像素级对照背书**：`MENGINE_NO_BATCH=1` 对照（逐实体绘制）
   与批处理输出 0 差异像素（1600 实体场景、第 250 帧、含后处理）。
4. 剩余热点（release）：1 万实体时阴影 pass 0.6 ms / 主 pass 1.1 ms，
   后续可沿 P3.6 多线程渲染与阴影图分层渲染继续优化。

## 5. 局限

- 数据为**单机（Intel UHD 集显）**采样，仅供趋势参考；独立显卡与
  vsync 开启时绝对数值会不同。
- 场景为静态四色实例布局；动态/独立材质较多的场景批数会增加。
- 帧率由隐藏窗口无 vsync 测得，不代表编辑器（1600×900 视口 FBO +
  ImGui）的实际帧率。
- `sandbox --scene` 不加载编辑器默认光照参数（SSAO/TAA/Bloom 由场景
  决定），本组数据均为后处理默认关闭或轻载状态。

## 6. 回归冒烟（P5 收尾验证）

```bash
# physics 冒烟（物理+脚本，期待 sensor/impact/raycast 日志与干净退出）
./build/windows-clang-debug/sandbox/sandbox.exe \
    --scene assets/scenes/physics_test.scene --frames 300 --hidden

# 渲染冒烟（期待 drawcalls<=实体数级、无 ERROR/FATAL）
./build/windows-clang-debug/sandbox/sandbox.exe \
    --scene assets/scenes/stress_cull.scene --frames 600 --hidden

# editor 冒烟（注意：需以 exe 所在目录为工作目录，字体为相对路径）
cd build/windows-clang-debug/editor && ./editor.exe --frames 200 --hidden
```
