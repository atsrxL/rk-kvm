# ADR-014：只抽取 Sunshine 的串流与输入模块

状态：规划（2026-11-11），未实施。行数与依赖来自对固定提交 `63d35f7` 的静态统计，未经编译验证。

## 问题

项目目标是“只用 Sunshine 的图像串流模块和键鼠输入模块”做 KVM 服务器。现状是“整个 Sunshine 加补丁关掉功能”：

- `tools/apply_sunshine.py` 只从构建里移除 `main.cpp`、`confighttp.cpp`、`upnp.cpp`，其余约 2.5 万行照常编译，靠入口处的 `rkmoon_sunshine::enabled()` 短路。
- 补丁改 16 个文件，其中 `nvhttp.cpp` 一个占大半：逐块删除 PIN 配对、客户端证书、TLS，再改写成密码校验。
- 二进制仍链接 ffmpeg、libcurl、miniupnpc、PulseAudio、EGL、libvirtualhid、libdisplaydevice 等从不使用的依赖。
- 密码写死在补丁里（`password_auth password;` 无参构造），无法配置。

真正需要原样保留的只有约 7400 行。

## 决定

把 Sunshine 当作源码库使用：只编译下面“保留”的文件，其余由本仓库提供薄实现。固定提交不变，线上协议不变，客户端不需要改。

### 保留（原样，约 7400 行）

| 文件 | 行数 | 仍需的修改 |
|---|---|---|
| `stream.cpp` | 2385 | 日志脱敏 4 处 |
| `rtsp.cpp` | 1438 | 日志脱敏 2 处、独占会话准入 |
| `input.cpp` | 2408 | 3 处（取消光标唤醒、禁止日志、禁用快捷键） |
| `crypto.cpp` | 574 | 无 |
| `network.cpp` | 284 | 无 |
| `logging.cpp`、`globals.cpp` | 306 | `logging.cpp` 去掉 ffmpeg 与 libdisplaydevice 日志挂钩 |
| `thread_safe.h`、`utility.h`、`task_pool.h`、`thread_pool.h`、`sync.h`、`uuid.h`、`move_by_copy.h`、`round_robin.h` | — | `thread_safe.h` 的三个队列方法 |
| `third-party/moonlight-common-c`（头文件、`RtspParser.c`、nanors、enet） | — | 无 |

不重写 `stream.cpp`、`rtsp.cpp`、`input.cpp`。`input.cpp` 中的手柄、触摸、笔代码保留，用空桩满足链接。

### 由本仓库替换

| 原文件 | 行数 | 替身需要提供的内容 |
|---|---|---|
| `nvhttp.cpp`、`httpcommon.cpp` | 2081 | `serverinfo`、`applist`、`appasset`、`launch`、`resume`、`cancel` 六个路由；每个请求做密码校验；`unique_id` 持久化；构造 `launch_session_t`（`rikey`、`rikeyid`、`mode`、`appid`、`surroundAudioInfo`、`corever`、鼠标模式）并调用 `launch_session_raise` |
| `video.cpp`、`video_colorspace.cpp`、`cbs.cpp` | 4314 | `config_t`、`packet_t`、`hdr_info_t`、`capture()`、`active_hevc_mode`、`active_av1_mode`、参考帧失效标志；实现即现有 `rkmoon_bridge.cpp` |
| `audio.cpp` | 359 | `config_t`、`packet_t`、`buffer_t`、`stream_configs`、`capture()`；实现即现有 `rkmoon_audio.cpp` |
| `config.cpp` | 2103 | 固定结构体，约 20 个字段：`stream.{ping_timeout,fec_percentage,packetsize,lan_encryption_mode,wan_encryption_mode}`、`sunshine.{address_family,bind_address,port}`、`input.*` 9 个、`video.dd`，以及包大小和加密模式常量 |
| `process.cpp` | 808 | `running()` 与 `get_last_run_app_name()`，固定返回单一 “HDMI” 应用 |
| `platform/linux/misc.cpp` | 1569 | 抽出约 12 个函数：`send`、`send_batch`、`enable_socket_qos`、`set_thread_name`、`adjust_thread_priority`、`create_high_precision_timer`、`from_sockaddr`、`from_sockaddr_ex`、`get_mac_address`、`streaming_will_start`、`streaming_will_stop`、`get_capabilities` |
| `platform/virtualhid_input.cpp`、`linux/input/virtualhid.cpp` | 1203 | 约 25 个 `platf::` 输入函数：键盘、相对/绝对鼠标、按键、滚轮转给 `rkmoon_bridge`，其余为空 |
| `main.cpp` | 537 | 现有 `rkmoon_main.cpp` |

### 删除

`display_device.cpp`（留 1 个空函数 `revert_configuration`）、`system_tray.cpp`（留 2 个空函数）、`platform/linux/graphics.cpp`、`platform/linux/audio.cpp`、`platform/linux/publish.cpp`、`entry_handler.cpp`、`file_handler.cpp`、`stat_trackers.cpp`、`upnp.cpp`、`confighttp.cpp`，以及全部 Web UI 资源。

### 构建方式

保留文件用 `#include "config.h"` 这类相对引用，会优先命中同目录的原文件，替身头文件无法靠包含路径顶替。因此：

1. 新增脚本从固定提交的 `vendor/sunshine` 把“保留”清单拷到构建目录，并应用剩余的小补丁（沿用 `apply_sunshine.py` 的“锚点恰好出现一次，否则拒绝”规则）。
2. 替身头文件与实现放在本仓库 `sunshine/`，与拷出的文件同处一个包含目录。
3. 本仓库顶层 `CMakeLists.txt` 新增目标直接列出源文件，不再使用 Sunshine 自身的 CMake，也不再修补它。

### 预期依赖

- 去掉：ffmpeg、libcurl、miniupnpc、PulseAudio、glad/EGL、gio、libvirtualhid、libevdev、libdisplaydevice、nv-codec-headers。
- 保留：OpenSSL、Boost、ENet、moonlight-common-c、Opus、ALSA。
- 待定：Simple-Web-Server（阶段 1 继续用它承载六个路由）、nlohmann_json、libcap、lizardbyte-common。

## 阶段

每个阶段独立可回退，完成后更新 `docs/ACCEPTANCE.md` 与 `HANDOFF.md`，不合并推进。

### 阶段 1：替换 HTTP 与认证

- 新增 `sunshine/rkmoon_http.cpp` 取代 `nvhttp.cpp` 与 `httpcommon.cpp`，仍在现有 Sunshine 构建内编译。
- 密码从状态目录下属主、0600 的文件读取，缺省为 `kvm`；`tools/run.py` 负责创建与权限检查。
- `apply_sunshine.py` 删除全部 `nvhttp.cpp`、`nvhttp.h`、`httpcommon.cpp` 修改段。
- 完成标准：`serverinfo` 字段与现有输出逐项一致（含 `RKMoonAuth=password-http-v1`、`RKMoonDisplay*`、`RKMoonMouseModes`、`HttpsPort=0`、`PairStatus=1`）；`tools/test_minimal_startup.py`、`tests/test_password_auth.py`、`tests/test_log_redaction.py`、`tests/test_overlay.py` 通过；新增“修改密码后旧密码被拒”的检查。

### 阶段 2：自有构建与模块替身

- 新增拷贝脚本与本仓库 CMake 目标；提供 `video`、`audio`、`config`、`process`、`display_device`、`system_tray` 的替身头文件和实现。
- 平台层暂时整文件保留 `misc.cpp`，只删 `graphics.cpp`、`audio.cpp`、`publish.cpp`。
- 完成标准：链接清单不含 ffmpeg、curl、miniupnpc、PulseAudio、EGL；阶段 1 的全部测试通过；`tools/test_audio_loop.py` 通过；ARM64 构建通过。

### 阶段 3：裁剪平台层

- 从 `misc.cpp` 抽出所需函数到 `sunshine/rkmoon_platform.cpp`；输入函数改为直接实现，移除 `virtualhid_input.cpp` 与 libvirtualhid。
- 完成标准：`apply_sunshine.py` 只剩 `stream.cpp`、`rtsp.cpp`、`input.cpp`、`thread_safe.h`、`logging.cpp` 的修改；发布包 `lib/` 中的私有库数量下降并记录前后对比。

### 阶段 4：实机验收

- T6 上按 `docs/SERVER-INSTALL.md` 安装新发布，旧发布保留用于回滚。
- Windows 客户端验证：连接、1080p60 HEVC/H264、音频、相对与绝对鼠标、键盘、断线重连、无信号占位会话。
- 实机与客户端验收单独标记，不由离线测试替代。

## 约束

- Sunshine 固定提交 `63d35f7` 不变；锚点失败先读实际代码，不放宽为模糊匹配。
- 协议行为不变：不改 RTSP、控制流、RTP、FEC、输入加密。
- 不引入 CPU 视频编码或桌面抓屏路径。
- 日志脱敏不能因文件替换而丢失：自写 HTTP 端点不记录请求内容、头或密码。
- 所有编译与测试在远端构建机执行，不在本机编译。
- 阶段 4 之前不触碰 T6。

## 风险

- 替身头文件遗漏字段会表现为编译错误，可控；语义差异（例如 `config::stream` 默认值与原实现不同）不会报错，阶段 2 需逐字段对照 `config.cpp` 的默认值。
- `launch_session_t` 构造必须与原 `make_launch_session` 一致，尤其 `rikey`/`rikeyid` 到 `gcm_key`/`iv` 的转换和 `corever` 对加密 RTSP 的影响；阶段 1 用同一请求对比新旧实现的会话字段。
- `platform/common.h`（1361 行）被保留文件直接包含，可能间接引入已删除模块的头文件；阶段 2 开始时先确认，必要时提供裁剪版。
- 以后升级 Sunshine 提交时，替身接口需要手工跟进。

## 不在本 ADR 范围

代码审查中发现的其他问题（worker 恢复序号、采集池套接字权限、错误帧处理、默认第 30 秒 IDR、日志轮转等）另行处理。
