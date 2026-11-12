# ADR-016：服务端 ADR-014 之后的收尾

状态：待实施（2026-11-12）。来源：对提交 `6cbb3c8`..`0566904` 及此前服务端代码的静态审查；未编译、未运行。标注 [推测] 的条目未经验证。

接手者先读：`AGENTS.md`、`docs/ADR-014-sunshine-extraction.md`、`docs/ACCEPTANCE.md` 的 ADR-014 各小节、本文件。T6 当前运行发布 `20261112-adr014-c99f217`，回滚目标 `20260928-r12`。

## 现状评价

ADR-014 的目标已达到：补丁从 16 个上游文件降到 6 个，构建不再使用 Sunshine 的 CMake，依赖 NEEDED 14→7，密码可配置，新旧 `serverinfo` 21 字段一致。不足：新增代码大半是上游原样搬入而非按 KVM 需要重写；若干缺陷和第一次审查的问题尚未处理；resume、加密 RTSP、真实 HDMI/键鼠/Windows 客户端未实测。

## 任务 1：认证与运行时加固

| # | 位置 | 问题 | 做法 |
|---|---|---|---|
| 1.1 | `sunshine/rkmoon_password.hpp` `read_password_file` | 文件不存在时静默回落到 `kvm`；文件被误删或状态目录变更后密码悄悄变回默认 | 服务端运行时缺文件即拒绝（401 并记一次日志，不含路径以外的信息）；默认文件只由 `tools/run.py` 创建。`http::init` 同步改为缺文件启动失败 |
| 1.2 | 同上 `authorize_locked` | 全局令牌桶：`tokens_ < 1` 时在校验前返回 false，局域网任意主机每秒 1 次以上错误请求即可让正确密码持续被拒；客户端每秒轮询，表现为反复掉线 | 改为定长（例如 64 项）按来源地址分桶表，表满时淘汰最久未用项；正确密码不受其他地址的失败影响。保留“无攻击者可控的无界分配” |
| 1.3 | `sunshine/rkmoon_http.cpp` 鉴权包装的 `catch (...)` | [推测] `launch`/`resume` 内的 `fail_guard` 在栈展开时已写一次响应，外层再写 400，可能产生两段响应体 | 先确认哪些参数解析会抛异常；改为处理函数内部统一生成错误响应，外层只兜底未写过响应的情况 |
| 1.4 | `sunshine/rkmoon_platform.cpp` `adjust_thread_priority` | 去掉 RTKit 后直接 `setpriority`，无特权时负 nice 必然失败：每个会话打警告，发送线程以普通优先级运行 | `systemd/rkmoon.service` 增加 `LimitNICE=-15`（不需要 root 或 capability）；失败日志降为每进程一次。实机确认线程 nice 值 |
| 1.5 | `sunshine/rkmoon_http.cpp` `http::init` | 状态文件 JSON 损坏时启动失败且无具体原因 | 日志说明是状态文件解析失败；不自动重建身份（避免客户端绑定失效而无人察觉） |
| 1.6 | `sunshine/server.cmake` | [推测] 系统 Boost 分支把纯头文件库 `endian`/`format`/`property_tree` 写成 `COMPONENTS`，可能找不到；链接了用不到的 `OpenSSL::SSL` | 在远端用系统 Boost 配置一次验证；纯头文件库改用 `Boost::headers`；确认无 TLS 符号引用后去掉 `OpenSSL::SSL` |

完成标准：新增离线测试——缺密码文件时启动被拒；地址 A 连续失败不影响地址 B 的正确密码；`launch` 带非法参数只返回一段格式正确的 XML。现有回归全部通过。

## 任务 2：删除已失效的链路

| # | 内容 |
|---|---|
| 2.1 | `apps.json` 整条链路：`tools/run.py` 生成、`rkmoon_config.cpp` 拷贝、`rkmoon_main.cpp` 要求 `assets/apps.json` 存在、`config/hdmi-apps.json`、`prepare_sunshine.py` 拷贝。应用列表在代码里固定为 “HDMI” |
| 2.2 | `rkmoon_config.cpp`：只保留实际使用的键（`sunshine_name`、`min_log_level`、`port`、`bind_address`、`address_family`、`external_ip`、`ping_timeout`、`fec_percentage`、`packetsize`、两个加密模式、`stream_audio`）。手柄、DS4、按键映射、虚拟手柄 MAC、`upnp`、`file_apps`、`file_state`、`log_path` 等删除。已部署的 `sunshine.conf` 含 `upnp` 和 `file_apps`：`run.py` 启动时重写该文件，或解析器对这两个键显式忽略并记一次日志，二选一并写明 |
| 2.3 | `rkmoon_sunshine::enabled()` 及补丁中的 `if (rkmoon_sunshine::enabled())`：专用构建恒为真，改为无条件 |
| 2.4 | `tools/apply_sunshine.py` 已不能独立运行：并入 `prepare_sunshine.py` 或改名为模块（如 `sunshine_patches.py`），同步测试与文档引用 |
| 2.5 | `allow_1440p90_experiment`：配置、环境变量、`translate()`、worker 命令行、`Config`、`describe_timing(…, ninety)` 整条删除（无任何效果） |
| 2.6 | `Encoder::bitrate()` 与 `Kind::bitrate`：bridge 从不发送；且比较对象是当前值而非初始值。删除，或改对并接入，二选一 |
| 2.7 | `rkmoon_platform.cpp` 中的 FreeBSD 条件分支 |

完成标准：`serverinfo`/`applist` 输出不变；启动测试用旧格式 `sunshine.conf` 仍能启动；回归通过。

## 任务 3：`rkmoon_http.cpp` 瘦身

当前 583 行基本是上游 `nvhttp.cpp` 去掉配对后的原样代码。可去掉：

- `get_codec_mode_flags` 中 AV1、4:4:4、HDR 分支（只报告 H.264 与按探测结果的 HEVC）。
- `display_device::configure_display` / `revert_configuration` 调用及 `revert_display_configuration` 逻辑（现为空函数），连同 `sunshine/display_device.h` 中不再需要的声明。
- `launch`/`resume` 中每次重新 `probe_encoders()`（多启动一次 worker 并探测两种编码）。启动时已探测；改为不重复探测，或仅在上次探测失败时重试。
- `appasset` 路由：定制客户端不调用。删除前确认 `client/` 无引用。
- 统一代码风格：与仓库其余自写文件一致，不保留上游缩进混排。

约束：`serverinfo` 21 字段逐值不变；`make_launch_session` 的字段语义不变；密码错误仍为 HTTP 200 + XML `status_code=401`。

完成标准：`tools/test_minimal_startup.py`、`tools/test_rtsp_handshake.py`、新旧 `serverinfo` 对比通过；记录文件行数前后对比。

## 任务 4：采集与 worker 缺陷（第一次审查遗留）

| # | 级别 | 位置 | 问题 | 做法 |
|---|---|---|---|---|
| 4.1 | 高 | `src/worker_main.cpp` `recover()` | `last` 取自 `encoder ? encoder->sequence() : 0`；第一次重开失败后 `encoder` 为空，下一轮 `last=0`，重开成功后序号从 1 开始，服务端 `SequenceGate` 判为不连续而断流。5 秒恢复窗口实际只在首次重开成功时有效 | `last` 提到 lambda 外，只在 `encoder` 非空时更新。加离线测试 |
| 4.2 | 中 | `tools/capture_pool.py` `serve()` | 目录 chown 给串流用户后，root 再按路径 `bind`/`chown`/`chmod` 套接字；该用户可在其间把 `pool.sock` 换成符号链接，使 root chown 任意文件 | 目录保持 root 属主，`root:<用户组> 0750`，套接字 0660；或至少 `follow_symlinks=False`。注意不要重启正在运行的池服务（ADR-013） |
| 4.3 | 中 | `src/capture.cpp` `Capture::latest()` | 平面边界检查在 `V4L2_BUF_FLAG_ERROR` 判断之前；错误缓冲若 `bytesused=0` 会触发整条管线重建 [推测，未在该驱动确认] | 先判 ERROR 并 `release`，再做边界检查 |
| 4.4 | 中 | `src/worker_main.cpp`、`sunshine/rkmoon_bridge.cpp` `args()` | `idr_at` 默认 30 在 IPC 模式也生效：每个会话第 30 秒被强制插入 IDR。`--gop` 等于帧率，即每秒一个 IDR，CBR 下每秒一次码率脉动 | IPC 模式传 `--idr-at 0`；GOP 放宽（已有按需 IDR）。改动后实机看码率与恢复时间 |
| 4.5 | 中 | `tools/run.py` `spawn()` | `sunshine.log`、`hid.log` 追加写入无轮转；会话期间每秒两行统计 | 交给 journald，或启动时按大小轮转；每秒统计降到 debug |
| 4.6 | 低 | worker/capture 的 stderr JSON | 直接拼接 `e.what()`，未转义 | 转义引号、反斜杠和控制字符 |
| 4.7 | 低 | `worker_main.cpp` `emit()`、`pixels.cpp` `to_nv12()`、`rkmoon_audio.cpp` | 无 stats 文件仍逐帧拼 CSV；每帧整幅 memset 后又整幅覆盖；音频每包多拷贝一次同类型缓冲 | 去掉多余工作 |

完成标准：4.1、4.3 有离线测试；4.2 有权限检查测试；4.4 实机记录前后码率曲线或统计；其余回归通过。

## 任务 5：实机验证缺口

T6 上运行的已是 ADR-014 版本，以下尚未实测，完成前不得在文档中标为通过：

- 断线续连（resume）、加密 RTSP（`corever>=1`）。
- 真实 HDMI 画面的 HEVC/H.264、音频与音画同步。
- USB 键盘、相对与绝对鼠标、会话结束后的租约释放。
- Windows 客户端端到端。

前提：一台带 GPU、可登录桌面、且不是被控机本身的 Windows 主机（192.168.123.127 是被控机 Tiny11_clone，不能用于键鼠验证）；T6 的 HDMI 信号与 USB 直通需先恢复（2026-11-12 时为 no_signal、USB offline）。可与 ADR-015 阶段 A 的真机冒烟合并进行。

## 顺序与约束

建议顺序：任务 1 → 4.1/4.2 → 任务 2 → 任务 3 → 任务 4 其余 → 任务 5。每个任务单独提交、单独更新 `docs/ACCEPTANCE.md`。

- Sunshine 固定提交 `63d35f7`、MPP 固定提交不变；不改线上协议。
- 所有编译与测试在远端构建机执行。
- 部署到 T6、打开 HDMI 采集、发送键鼠事件、修改 systemd 单元都需要用户明确授权；不重启 `rkmoon-capture-pool`。
- 任何失败保留证据并停止推进；T6 上保留可回滚的旧发布。
