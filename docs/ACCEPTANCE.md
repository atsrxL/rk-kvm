# 验收矩阵

## 八模式 EDID 输入兼容性

2026-09-24：最终8模式（首选1080p120）全部通过Znas RTX5060Ti→T6的实际输入锁定；不是新增模式的采集/编码/客户端验收。EDID已临时写入、未开机持久化；实际输出回到1080p60。详见 [逐项结果](EDID-EIGHT-MODES.md)。

通用源模式服务端（ADR-009）已部署T6。真实采集+MPP硬编+独立解码：1440p120（HEVC/H.264）、1600p120、1280p90、4K60、4K30、1080p60、1080p30 通过；1080p120 未通过（CPU拷贝路径约108fps）。Moonlight客户端在这些新模式下的播放尚未验收。见 [扫测](../results/20260924-general-modes/HARDWARE-SWEEP.md)。

## 当前自动画面模式任务

输入延迟与断线缓冲（ADR-010）：HID 改为持久 kvmd WebSocket、worker 采集5秒内自愈、bridge 丢帧等IDR。已构建（离线117项通过）并部署T6（rkmoon-kvm 8c7cbf8a…，worker 647c109e…）。客户端实测鼠标延迟和断线恢复：未测试。见 [ADR-010](ADR-010-input-stream-recovery.md)。

2026-09-24 用户新增要求：Ctrl+Alt+Shift+C 显示本地指针；客户端自动跟随 HDMI 尺寸/帧率，设置仅有码率和编码。实现与本轮构建/部署证据单列于 [自动跟随记录](../results/20260924-1440p90/DISPLAY-FOLLOW.md)，不能把下方旧手动预设验收当作新功能验收。

## 1440p90当前阶段

| 项目 | 状态 | 证据/限定 |
|---|---|---|
| 实验server/worker完整构建与离线回归 | 通过 | Linux ARM64容器完整链接；30原生通过，93 Python/联合中91通过2跳过，配置gate15项通过；见[构建记录](../results/20260924-1440p90/SERVER-BUILD.md)，非实机编码 |
| NVIDIA输出与T6锁定 | 通过 | 两次2560×1440、89.9935Hz，临时EDID/NVAPI，见NVIDIA专项记录 |
| 完整V4L2缓冲采集30s | 通过 | 2695帧、timestamp89.9981fps、每帧11059200字节、无缺帧/错误；无编码，见[采集记录](../results/20260924-1440p90/CAPTURE.md) |
| 真实HEVC/H264 90fps编码与独立解码 | 通过 | 各60s/5394帧，89.998/89.982fps；DMA-BUF，HEVC0skip/H2641skip；仅当前较静态源，见[编码记录](../results/20260924-1440p90/ENCODE.md) |
| 客户端1440p90预设/请求参数 | 通过 | Mac arm64 Qt12/12、Python3/3；实验模式，默认仍1080p60 |
| 新Windows90Hz手动预设包 | 通过 | bac1b3e776b1，Qt Windows12/12，Target读回SHA256 06381939e037dc79b13060f706959ee243b5e639e43d809ec26f3977fe9ef07e；不含本轮自动跟随功能 |
| 真实90fps客户端视频/音频/USB | 未测试 | 不从采集结果推导端到端性能 |

以下时间顺序记录保留历史状态；以上矩阵为本阶段最新验收。

> 1440p90后续突破：NVIDIA NVAPI自定义输出成功，两次T6时序探测均为2560×1440、89.9935Hz，接收锁定通过。下条BADMODE为历史失败。持续采集、MPP编码及客户端90fps仍未测试；见 [NVAPI实测](../results/20260924-1440p90/NVIDIA.md)。

> 1440p90专项：EDID试验未能使Znas直通RTX5060Ti接受90Hz，Windows模式测试BADMODE(-2)。原EDID已恢复、实际1080p59.9952输入正常；90Hz接收/采集/编码/客户端均未测试通过，代码仍保留60Hz限制。见 [证据](../results/20260924-1440p90/STATUS.md)。

> 最新要求：HTTP＋密码，取消HTTPS/证书验证；默认密码 `kvm`，Windows为主要客户端。此条取代下文旧TLS/TOFU要求，见 [ADR-005](ADR-005-http-password.md)。

> 2026-09-24：按用户要求改为 IP＋密码（初版 `kvm`），取消 PIN/pair 和客户端证书登记授权。TLS/输入加密保留；旧配对说明及旧实机通过记录不代表新认证验收。见 [认证决策](ADR-004-password-auth.md)。新版本尚未部署到设备。

> 实机更新：ARM64 worker/Sunshine 已构建；T6 真实 1080p60 HEVC/H264 各 60 秒及独立解码通过；原版 Moonlight HEVC 硬解超过 11 分钟、H264 重连通过。USB 尚未完成。见 [T6 验收记录](../results/20260923-t6/STATUS.md)。

> 2026-09-23 本地更新：完整 Sunshine 与真实 MPP worker 已在 Linux amd64 编译链接通过；25 原生、69 Python/联合、25 sanitizer 测试通过。目标 ARM64/P1/P2/P3 仍未测试。最新结果见 [构建记录](../results/20260923-build/STATUS.md)。下文原交接记录保留供追溯。

状态必须为：通过 / 失败 / 未测试 / 部分完成。每次结果绑定具体硬件、commit、二进制/动态库、客户端和证据目录。原记录不是新验收。

## 当前HTTP基础密码版本

服务端完整构建、28原生、91/93工具测试（2跳过）、生产binary HTTP认证、无HTTPS监听/证书生成及重启身份回归通过。Windows客户端构建由子代理执行，最终交付另记；未部署T6。见 [HTTP版本证据](../results/20260924-http-password/STATUS.md)。

## 2026-09-24 密码认证版本

| 检查 | 状态 | 证据 / 限定 |
|---|---|---|
| P0 ARM64 Linux完整构建 | 通过 | [记录与hash](../results/20260924-password/STATUS.md)，隔离容器，非 T6 原生 |
| 密码认证及无pair网络回归 | 通过 | 生产binary + 合成probe worker，真实HTTP/HTTPS；正确/错误/缺失密码、pair关闭、重启身份稳定，无媒体流 |
| 原生/联合/脱敏回归 | 通过 | 28原生；93工具/联合中91通过2内核fixture跳过；14 overlay、5脱敏；生产密码guard C++测试 |
| 新客户端本地构建/认证 | 通过 | macOS ARM64链接、3源码组、10/10 Qt含loopback TLS；详见 [客户端验收](../client/docs/ACCEPTANCE.md) |
| 新客户端＋真实 HDMI/音频/USB | 未测试 | 本次未访问设备，旧实机通过记录不迁移 |

## 最小服务端独立分支（`feat/minimal-kvm-server`，不覆盖已实测 main）

| 检查 | 状态 | 证据 / 限定 |
|---|---|---|
| 原版 T6 HDMI/MPP、Moonlight 视频及 P3 USB 基线 | 通过 | 仅 `main` 的 results/20260923-t6/STATUS.md；不作为本分支的新测试 |
| 独立 `rkmoon-kvm` P0 构建与无Web启动 | 通过 | 冻结912613c ARM64 QEMU链接exit0；隔离模拟probe启动TCP仅47984/47989/48010，无47990；见 results/20260923-minimal/STATUS.md。不是实机HDMI |
| 私有 Unix PIN 操作与同 UID 权限 | 部分完成 | 分包/取消/边界fixture和真实Unix LIST、216ms退出通过；未完成新binary真实客户端PIN握手 |
| HDMI ALSA 48k 双声道→Opus/RTP 音频 | 部分完成 | 真实生产object合成ALSA null+注入stale/xrun/初次无源5/10/20ms、Opus解码/恢复通过；真实HDMI PCM/声音/同步/重连未测，父会话发现T6 HDMI PCM未暴露，仍是阻碍 |
| 无音频源不阻止视频/输入 | 未测试 | 代码路径保留视频入口；尚无本分支真实会话 |
| 定制客户端真实视频+声音+USB | 未测试 | 等待客户端子会话与协调测试窗口 |

本分支P0已通过（912613c，25原生+87 Python/联合/工具），新P1/P2/P3仍未测试，由父会话协调实机窗口。项目构建 VM301 为 Debian x86_64客体+arm64 QEMU容器；不是T6原生执行。最小服务端的“通过”只适用于明确列出的P0及模拟启动，不能声称新HDMI/客户端闭环已实测。

## 本次云端

| 检查 | 状态 | 证据 / 限定 |
|---|---|---|
| 架构入口、固定版本、许可清单 | 部分完成 | ADR/SOURCES，源码可读；未全量安全审计或全链路验证 |
| 自有核心+V4L2 probe构建 | 通过 | results/cloud/configure.log、build.log；x86_64 Linux |
| worker CLI独立编译 | 通过 | results/cloud/worker-cli-compile.log；未链接encoder_mpp.cpp |
| 原生25项 | 通过 | results/cloud/ctest.log |
| Python+原生输入联合+工具68项 | 通过 | results/cloud/python-tests.log；fake USB backend |
| ASan/UBSan原生25项 | 通过 | results/cloud/asan-tests.log |
| 完整Sunshine套补丁/编译链接 | 未测试 | 容器无法解析GitHub，仅连接器可读；不是已成功的patch |
| 真实SDK的encoder_mpp.cpp编译链接 | 未测试 | 容器无MPP SDK/板卡 |
| T6只读基线 | 未测试 | 用户尚未提供当前设备结果 |
| 60秒真实HEVC/Main8/H264编码与独立解码 | 未测试 | 不采用旧H264结果填表 |
| 1080p60原版Moonlight配对/硬解/10分钟 | 未测试 | 客户端资料与设备均未取得 |
| 实际USB相对输入与断连释放 | 未测试 | 旧记录绝对模式是P3阻碍，不属于新已修复结果 |
| 物理端到端/高分辨率/长稳 | 未测试 | 没有测量数据 |

## 本地待填环境

- 板型/内存/镜像/内核/散热：未确认。
- HDMI节点/源尺寸cadence/plane/stride/sizeimage/color range/transfer：未确认。
- MPP头版本/运行动态库路径与SHA256：未确认。
- 新Sunshine binary SHA256/子模块版本/补丁hash：未确认。
- kvmd版本/socket鉴权/mouse.absolute/既有gadget所有权：未确认。
- Moonlight系统/version/GPU/decoder/屏幕刷新率/网络：未确认。
- 采集与输入独占授权范围/窗口/旧服务恢复方法：未确认。

## 本地阶段记录模板

| 时间/阶段 | 环境ID | 检查 | 状态 | 证据路径/hash | 未完成/回滚 |
|---|---|---|---|---|---|
| 待执行 | 待填 | P0-build | 未测试 | — | — |
| 待执行 | 待填 | H0/P1-HEVC/P1-H264 | 未测试 | — | — |
| 待执行 | 待填 | P2-video-only | 未测试 | — | — |
| 待执行 | 待填 | P3-USB | 未测试 | — | — |

一次fail不能改成“部分通过”掩盖致命问题。软件解码是独立验证工具，不证明客户端硬解；MPP黑帧成功不证明HDMI；client连接成功不证明尺寸、编码或输入正确。

## 2026-09-24：服务端正式安装

| 检查 | 状态 | 说明 |
|---|---|---|
| 发布包生成（package_server.py，T6 本机） | 通过 | 20260924-r9，MANIFEST 逐文件 SHA256 |
| install_server.py 安装、enable、启动 | 通过（实机） | 停旧服务后 HID 恢复标记不存在再切换 |
| EDID 开机服务 | 通过（实机，未重启） | EDID 已一致，未重写 |
| 47989 密码认证、服务器身份保持 | 通过（实机） | 正确密码返回原 uniqueid，错误密码 401 |
| 客户端出画面/声音/键鼠 | 未测试 | 需用户用 Windows 客户端连接 |
| T6 整机重启后自动恢复 | 未测试 | 按用户要求暂不重启 |

## 2026-09-25：无信号会话（ADR-011）与客户端信息面板

| 检查 | 状态 | 说明 |
|---|---|---|
| worker/Sunshine ARM64 构建（VM301） | 通过 | worker 990f5377…，rkmoon-kvm 4fec78c4… |
| 发布 20260925-r10 安装并启动 | 通过（实机） | tar.gz 0fdafa31…，四个服务 active，serverinfo 正常报告 no_signal |
| worker 占位模式（T6 实机，真实 MPP，IPC 协议驱动） | 通过（实机） | 5 秒 51 帧 ≈10fps，首帧 IDR 841B，P 帧 ≤42B，stop 正常退出 0；未打开 HDMI 设备 |
| 客户端 Connect 只显示信息、Start/无信号可启动 | 通过（Mac 离线） | Qt 26/26 |
| 无信号经客户端进入黑屏并用键鼠唤醒、信号出现后切到真实模式 | 未测试 | 需用户用新 Windows 客户端实测 |

## RP2350 HID 桥固件（ADR-012，2026-09-28）

| 项目 | 状态 | 证据/限定 |
|---|---|---|
| 协议解析/CRC/看门狗/拆分合并 主机单元测试 | 通过（离线） | Mac clang 与 Docker gcc 13.3，ASan/UBSan，200712 项检查 0 失败，含共享向量 docs/rp2350-protocol-vectors.json |
| 固件交叉编译（pico-sdk 2.3.1，板型 waveshare_rp2350_usb_c，自带头文件） | 通过（仅编译） | arm-none-eabi-gcc 13.2.1；uf2 351562e6…，elf 08eb4e42…；text 28424 B |
| Target 交付 RKMoon-RP2350-HID | 通过 | uf2 读回 cmp 一致，SHA256 351562e62cef259720ef57986ffc82f80e8240e176c7db1c388fb7354785da31 |
| 上板：枚举、键鼠输入、boot 协议、remote wakeup、灯色、UART 1 Mbaud、500 ms 释放 | 未测试 | 未上板 |
| PIO-USB CDC 控制通道 B | 部分完成 | v0.2 独立 CDC 已实现，默认 ON，编译/主机测试通过，未上板；见下方最新矩阵 |

## 采集缓冲池 ADR-013（2026-09-28，r12）

| 项 | 结果 | 证据 |
|---|---|---|
| VM301 ARM64 构建 worker+kvm | 通过 | worker 5979dbfe…，kvm 9d1735c6… |
| 离线测试 | 通过 | 117 项，2 跳过 |
| 开机池分配（T6 实机） | 通过 | 4×24,883,200 B，socket at:0600 |
| 碎片化后运行时分配 | 失败（预期） | 需重启，已记录 |
| worker 池化 1080p60 HEVC 实机采集+编码+独立解码 | 通过 | 175 帧，ffmpeg 无错，画面正常 |
| Windows 客户端连接 | 未测试 | 待用户实测 |

## RP2350 v0.2 控制通道 B（2026-09-28 最新；编译/主机测试，未上板）

| 检查 | 状态 | 证据与边界 |
|---|---|---|
| Docker 固件 ON / OFF | 通过 | text/bss 47904/19436 与 28424/6044 B；固定 SDK/PIO commit |
| Mac + Docker ASan/UBSan | 通过 | 核心 ON/OFF 各 200718 checks；CDC 描述符/请求、10万随机 setup、100万字节 SPSC 跨线程、通道仲裁/失效 |
| CDC EP0/bulk/ZLP 状态机 | 通过（模拟） | 生产 CDC 栈 + 控制器桩，非 USB 硬件 |
| 0.1 客户端协议兼容 | 通过（Mac 离线） | 原 Qt 12/12，未改客户端、未重新 Windows 构建 |
| Target UF2 交付与 SHA256 读回 | 通过 | results/20260928-rp2350-cdc/delivery.json |
| PIO CDC Windows/Linux 枚举、USB 时序、长稳/背压/Hub | 未测试 | 无真板 |
| 原生 HID、UART A/B 切换、拔线/DTR/挂起释放、WS2812 | 未测试 | 见固件 README 上板清单，先确认控制线断 VBUS |

完整产物/hash 与失败修复记录见 [本轮记录](../results/20260928-rp2350-cdc/STATUS.md)。没有 T6 操作，没有服务端或 client/ 改动，没有实机通过声明。

## ADR-014 阶段 1：HTTP 与认证替换（2026-11-12）

阶段 1 完成。仅离线、编译及模拟 HTTP 启动验收；未访问 T6、未打开真实 HDMI/HID，未做 Windows/客户端交付或实机验收。阶段 2/3/4 未实施。

| 检查 | 状态 | 证据与边界 |
|---|---|---|
| 独立 Sunshine 克隆与子模块 | 通过 | VM301 从上游递归克隆，HEAD `63d35f702ee9e362e43263742981836ec0710384`；应用前工作树干净，递归子模块全部与固定提交一致 |
| 新 overlay dry-run / --apply | 通过 | 13 个原文件、406 行补丁，摘要 `67eda06100cdd4641961efbfd16d8a8a1f8ef58b8142bb318f63b1163ad6d9ce`；原 HTTP 两个 cpp 不再编译，新增 rkmoon_http.cpp |
| dedicated rkmoon-kvm 完整链接 | 通过（仅编译） | VM301 amd64 上 ARM64/QEMU 容器；`RKMOON_BUILD_ROOT=/work/build/1440p90 JOBS=8 tools/build.sh sunshine -DFETCHCONTENT_UPDATES_DISCONNECTED=ON`；新 binary SHA256 `0175e353f520b423828715518cc962e40da011a12e90a581f3a04e9e08c15ed7` |
| tools/build.sh offline | 通过（离线） | 同一 ARM64/QEMU 容器，`RKMOON_BUILD_ROOT=/work/build/adr014-offline JOBS=4 tools/build.sh offline`；CTest 31/31；Python/联合 114 项、2 项因缺精确内核源码 fixture 跳过，其余通过 |
| test_password_auth.py | 通过（离线） | VM301 amd64 单独运行 1 项，亦包含在 ARM64 offline；生产 verifier 的失败预算、长度限制、密码文件热更旧拒新准、权限/符号链接拒绝 |
| test_log_redaction.py / test_overlay.py | 通过（离线） | ARM64/QEMU 容器分别 4/4、14/14；保留 RTSP/input/control 脱敏与固定锚点回归 |
| test_minimal_startup.py | 通过（模拟） | 非 root、独立无网络 Docker；合成 probe worker，无媒体流。六路由逐请求认证、重复头拒绝、密码热更、仅 HTTP47989/RTSP48010、无证书/PIN/Web、身份重启稳定、查询哨兵不入日志；root 拒绝另测通过 |
| 旧版完整链接及 serverinfo 对比 | 通过（模拟） | 同一独立固定克隆以 `228dae5` 的 apply_sunshine.py 生成旧版并链接；相同模拟 probe、身份和配置，鼠标能力开/关两种情形，根属性及全部 21 字段实际值严格比较通过后才脱敏。含 RKMoonAuth、Display*、MouseModes、HttpsPort、PairStatus、编解码字段 |
| 首次环境准备失败 | 已解决（保留历史） | 2026-11-11 复制的旧 vendor 是引用 `/work/...` 的 worktree，宿主 Git 返回128；当时停止，获继续授权后新建独立克隆，未修复或复用旧 worktree，未动本机 vendor |
| 真实 HDMI、MPP 编码、USB、Windows 客户端 | 未测试 | 不属于阶段 1 验收，既有历史通过不代表本次新实现通过 |

密码路径及格式见 README/SECURITY 的“服务端密码文件”；状态目录 `http-password`，本人所有、普通文件、0600。HTTP 每请求重新读取，保留原失败预算与输入加密。会话构造从旧 overlay 提取，保留 key/IV、音频参数、corever 与鼠标模式、独占/cancel 路径；没有重写 RTSP/RTP/FEC。

脱敏比较与摘要：`private/results/local/adr014/serverinfo-comparison.json`、`STATUS.md`。完整构建/测试日志及新旧二进制保存在 VM301 `/root/artifacts/rk-kvm-adr014-20261111/resume-20261112/`，不将真实请求、密码或查询写入日志。

## ADR-014 阶段2：自有构建与模块替身（2026-11-12）

本阶段完成；阶段3/4未实施。不访问 T6/设备、不改 client/firmware/worker/python、不交付 Windows。构建和测试均在 MS-A2 VM301 amd64 客体的 ARM64/QEMU 容器中进行；不能称为 ARM64 实机或媒体流验收。

| 检查 | 状态 | 证据与边界 |
|---|---|---|
| 自有构建与源码隔离 | 通过（编译） | RKMOON_SERVER 默认OFF，tools/build.sh sunshine 唯一转至本仓库 build/server；不运行 Sunshine CMake 或就地 --apply。干净固定 HEAD63d35f7、子模块及唯一锚点检查，生成协议源码/替身头/逐文件SHA256清单，输入克隆始终干净 |
| ARM64 完整链接 | 通过（仅编译） | gcc14.2、Ninja；新 binary SHA256 `3b3f36c698e91abfe3cbdeb6a6e3201ed25f9267d2e36ad86fcbc8e93373094d`。保留完整 misc.cpp 与虚拟HID层；不编译 graphics/audio/publish/video/config/process 等旧实现 |
| 保留补丁 | 通过（机械比较） | stream/rtsp/input/rtsp.h/thread_safe.h/misc/virtualhid 共7文件的变换结果与阶段1提交6cbb3c8逐字节一致；新增 logging.cpp 只去FFmpeg/libdisplaydevice日志挂钩 |
| 配置默认值与键名 | 通过（静态/机械） | config-defaults.md 逐字段核对；脚本提取固定 config.cpp reader 与原始默认块，34个支持键名均存在上游，额外键0。修正 audio_stream→stream_audio、超时/重复率 sentinel 与日志级别名称；移除模块的配置明确拒绝 |
| readelf NEEDED + ldd | 通过（依赖检查） | 直接NEEDED14→11；移除libva、libX11、libXtst、libpulse、libpulse-simple等直接项；新增直接libstdc++/libgcc_s/libgobject（完整前后清单留证）。新版NEEDED和全部ldd均无libav*/libsw*、curl、miniupnpc、pulse、EGL/GL，亦无缺库。FFmpeg原为静态部分，另核对新链接命令无这些库 |
| build.sh offline | 通过（离线） | CTest31/31；Python/联合115项，2项缺精确内核fixture跳过，其余通过；包含阶段1密码1项、日志4项、overlay12项及新增依赖打包检查 |
| test_minimal_startup.py | 通过（模拟） | uid1000、无网络/设备容器；密码热更、重复头、六路由逐请求认证、端口集合、无证书/PIN/Web、身份重启稳定、日志哨兵与资源清理；root拒绝另测通过 |
| test_audio_loop.py | 通过（合成） | 复用生产rkmoon_audio对象，5/10/20ms×ALSA null注入/初始无源共6场景，Opus完整解码、静音/stale/xrun恢复、有界退出；无真实HDMI录音 |
| 新旧serverinfo运行对比 | 通过（模拟） | 基线为阶段1 binary0175e353…，两种鼠标能力配置；根属性和21字段实际值全等，比较后才脱敏身份/地址 |
| launch→RTSP额外检查 | 通过（有限模拟） | 合成probe-only worker，appid1 launch/独占launch拒绝、OPTIONS200、DESCRIBE200（344B SDP）、cancel及非法鼠标400。未授权采集、未调用媒体worker，不走ANNOUNCE/SETUP/PLAY |
| 未覆盖 | 未测试 | resume、加密RTSP、完整媒体握手、真实HDMI/MPP编码/声音/USB与客户端；阶段4前不能宣称通过 |

执行命令（均 VM301 ARM64/QEMU）：

```bash
RKMOON_BUILD_ROOT=/work/build JOBS=8 tools/build.sh sunshine \
  -DRKMOON_SUNSHINE_SOURCE=/upstream -DRKMOON_BOOST_SOURCE=/deps/boost-src
RKMOON_BUILD_ROOT=/work/build/stage2-offline JOBS=4 tools/build.sh offline
python3 tools/test_minimal_startup.py --binary /evidence/new/rkmoon-kvm --output /evidence/startup
python3 tools/test_audio_loop.py --build /work/build/server --output /evidence/audio
python3 tools/test_rtsp_handshake.py --binary /evidence/new/rkmoon-kvm --output /evidence/handshake
```

首轮编译缺 Boost Endian/Format；迭代补齐 PropertyTree 与禁用桌面 EGL 声明后链接通过。回归曾发现“清单相同就跳过生成文件完整性校验”，原因定位并修复为先校验再复用，重跑115项通过。音频夹具的跨UID源码读取/链接依赖文件权限，以及额外RTSP夹具的Content-Length假设均已修正；没有改协议来迎合测试。失败日志保留，不把先前失败计为通过。按监督员澄清，正常开发编译缺项可直接迭代；未知回归原因、超ADR协议补丁或禁止范围变更才暂停。

证据：`private/results/local/adr014/stage2/`；完整日志、二进制、清单和测试源码归档于 VM301 `/root/artifacts/rk-kvm-adr014-20261111/stage2-complete/`。阶段1与本阶段都不替代实机/客户端验收。


## ADR-014 阶段3：裁剪平台层（2026-11-12）

本阶段完成；阶段4未实施。所有编译/测试在MS-A2 VM301（amd64客体，ARM64/QEMU容器，GCC14.2/CMake3.31.6）执行。未访问T6或任何设备，不改client/firmware/worker/python，不交付Windows或完整设备发布包。

| 检查 | 状态 | 证据与边界 |
|---|---|---|
| 实际平台调用/抽取 | 通过（机械审计） | 脚本扫描保留源码及自有实现，35个调用形式符号有文件/行号；地址转换、UDP/GSO/批量发送、QoS/主机名、计时主体与固定上游逐字节一致。无调用者的MAC/桌面/DRM/VAAPI等不保留 |
| 生成/保留补丁 | 通过（离线） | 干净固定独立克隆，check-only及CMake生成均成功；生成文件241→180。仅stream.cpp、rtsp.cpp、input.cpp、thread_safe.h、logging.cpp及既有rtsp.h鼠标字段；六份变换结果与阶段2完全相同。rtsp.h为唯一偏差，维持每会话鼠标模式传递，无新增协议改动 |
| ARM64完整链接 | 通过（仅编译） | SHA256 `b2bdc550c7ebb8411b6de448c0aac4fa799383e1cbbc9386dcde976eb69c98fb`；misc/两个virtualhid实现与graphics/VAAPI垫片退役，链接命令无libvirtualhid、libevdev、GIO、cap、lizardbyte-common、Boost.Locale或旧媒体/图形库 |
| 依赖/私有lib目录 | 通过（目标镜像检查） | 实测NEEDED11→7；移除cap/evdev/gio/glib直接依赖，完整ldd无GIO/GLib/GObject和阶段2禁用项、无缺库。按package_server.runtime_closure生成服务端组件lib目录17→5，18,404,856→11,817,832字节；glibc/加载器/C++运行库仍为系统依赖，worker/MPP不变且排除计数。不是完整设备包数量声明 |
| build.sh offline | 通过（离线） | CTest31/31；Python113项，2项缺精确内核fixture跳过，其余通过。包含密码1项、日志4项、overlay10项与包闭包检查；较阶段2删除两个只针对退役平台补丁文本的断言 |
| 启动/密码回归 | 通过（隔离模拟） | root拒绝、普通用户私有状态、两个端口、六路由逐请求认证、密码文件热更旧拒新通过、重启uniqueid稳定、日志脱敏及清理均通过 |
| 音频循环 | 通过（合成） | 生产audio对象，5/10/20ms×注入ALSA null/无源6场景、Opus解码/恢复/有界停止；无真实声卡 |
| 输入转发 | 通过（合成） | 生产platf入口→rkmoon_bridge→HidClient→同UID假UDS，相对/绝对各7事件按序逐值相等；空Unicode/touch/pen不产生事件、手柄返回不支持；verbose日志无事件内容，观察到租约EOF。不创建uinput，不等于USB消费/物理释放验收 |
| serverinfo运行对比 | 通过（模拟） | 基线为阶段2 binary3b3f36c6…；两种鼠标能力配置，根属性及21字段实际值全等，身份/地址比较后脱敏 |
| launch→RTSP | 通过（有限模拟） | 合成probe-only worker；launch、独占拒绝、OPTIONS200、DESCRIBE200（344B SDP）、cancel、非法鼠标400；未走媒体worker |
| 实机/未覆盖 | 未测试 | resume、加密RTSP、ANNOUNCE/SETUP/PLAY/完整媒体握手、真实HDMI/MPP/音频/USB、客户端与性能；负nice权限不足时仍使用普通调度，未做延迟实测 |

依赖取舍：GIO/RTKit改为上游setpriority回退，线程nice映射保持；libcap改读/proc/self/status相同能力位并在失败时拒绝；lizardbyte-common环境读取改getenv。nlohmann_json继续不存在；Simple-Web-Server为现有六路由HTTP保留，Boost/OpenSSL/ENet/Opus/ALSA承担实际协议/日志/编解码。没有为了裁剪改写协议。

实际命令（均VM301，运行夹具均在无网络、无设备、cap-drop ALL容器中，root拒绝外使用uid1000）：

```bash
python3 tools/prepare_sunshine.py /upstream
RKMOON_BUILD_ROOT=/work/build JOBS=8 tools/build.sh sunshine \
  -DRKMOON_SUNSHINE_SOURCE=/upstream -DRKMOON_BOOST_SOURCE=/deps/boost-src
RKMOON_BUILD_ROOT=/work/build/stage2-offline JOBS=4 tools/build.sh offline
python3 tools/audit_platform.py /work/build/server/protocol --upstream /upstream --output platform-references-after.json
python3 tools/audit_server_dependencies.py --before /evidence/old/rkmoon-kvm --after /evidence/new/rkmoon-kvm --output /evidence/dependencies
python3 tools/test_minimal_startup.py --binary /evidence/new/rkmoon-kvm --output /evidence/startup
python3 tools/test_audio_loop.py --build /work/build/server --output /evidence/audio
python3 tools/test_input_bridge.py --build /work/build/server --output /evidence/input
python3 /compare.py
python3 tools/test_rtsp_handshake.py --binary /evidence/new/rkmoon-kvm --output /evidence/handshake
```

两轮开发/最终ARM64构建及全部回归均通过，无失败验收被改标通过。证据：`private/results/local/adr014/stage3/`；完整日志、二进制、生成清单、依赖lib目录和夹具归档于VM301 `/root/artifacts/rk-kvm-adr014-20261111/stage3-complete/`。源码缓存按SSHH期限保留，不作为交付路径。


## ADR-014 阶段4：T6服务端实机与占位会话（2026-11-12）

**服务端已部署并保持运行；客户端验收待测试代理。** 用户授权安装/重启rkmoon和使用已有输入/采集；本轮未改EDID/CMA/内核/引导/网络/防火墙/USB/被控机或Znas，未重启采集池。

| 项目 | 结果 | 实际证据与边界 |
|---|---|---|
| ARM64完整包 | 通过（VM301编译/打包） | 独立干净c99f217 checkout，ARM64/QEMU自有目标链接exit0，server SHA b2bdc550…与阶段3一致；package_server.py生成49文件、8个私有库的完整包，22,256,616字节。含worker/MPP/codec等现有组件，与当前r12逐SHA相同，不重编或加载codec |
| 安装与回滚准备 | 通过（T6） | 安装`20261112-adr014-c99f217`，旧`20260928-r12`保留；安装器不加--start，随后只restart rkmoon.service，避免启动EDID/audio/pool流程。四个原unit/配置与身份基线留root私有备份；未触发回滚 |
| 服务/日志/权限 | 通过（T6） | server实际UID1000；新实例启动和会话日志无Error/Fatal。旧实例退出时ASIO Operation canceled留在监督stdout，按新实例时间边界分开记录；setpriority权限不足、USB offline为预期警告，未提权。新发布保持active |
| 密码文件/身份 | 通过（T6） | 升级前无文件；run.py自动创建`/var/lib/rkmoon/http-password`，普通文件、UID1000、0600。状态uniqueid前后相同，运行配置原字节不变；身份只保存脱敏校验值 |
| LAN成功/拒绝响应 | 通过（VM301→T6） | serverinfo/applist根属性及全部嵌套字段逐值一致；本机20个serverinfo字段，未配置ExternalIP（此前21字段模拟包含该项）。错误密码新旧均HTTP200，XML root status_code=401和status_message等属性完全一致 |
| 池/其他服务 | 通过（T6状态） | capture-pool始终PID568，ExecMainStartTimestamp及ActiveEnterTimestamp与基线完全相同；没有restart或重新分配池。旧发布及其组件不删除 |
| HDMI独立3秒采集 | 条件未满足 | 先后均no_signal、尺寸/帧率0，无活动worker；按授权记录无信号，未打开V4L2或宣称池化HDMI采集/解码通过 |
| 无信号占位真实会话 | 通过（真实T6→VM301网络） | 固定moonlight-common-c最小接收器发起launch，完成RTSP/控制/媒体握手；T6日志确认ADR-011 MPP硬编黑帧占位、约10fps。5秒收到52帧H264/63,706字节，VM301 ffmpeg解码exit0、ffprobe确认1920×1080/52帧。不是合成worker/软件视频编码或真实HDMI图像 |
| 音频网络/解码 | 通过（无信号静音） | 同会话收到760个Opus包，成功解码182,400样本/声道、解码错误0；服务端检测HDMI音频空闲后发静音。未验证真实有声/主观听感/音画同步 |
| USB offline隔离 | 通过（拒绝行为） | 现有kvmd /hid报告enabled但键鼠offline；新桥明确拒绝租约、server未出现lease acquired，视频仍52帧到达。另以同UID hello实测返回ok=false；没有恢复USB或向目标发送测试按键 |
| 会话清理/交接 | 通过（T6/VM301） | receiver正常退出、HTTP cancel成功、currentgame0/SUNSHINE_SERVER_FREE；T6无worker，未成功持有HID租约，无release失败。新发布继续运行供客户端代理接测 |
| Windows/真实端到端 | 待测试代理 | HEVC/H264真实画面、声音/同步、相对/绝对鼠标、键盘、快捷键、断线重连/resume、加密RTSP和无信号后信号恢复；本轮不标通过，不交付Windows包 |

发布名：`20261112-adr014-c99f217`；完整归档SHA256：`290a7ec7b84b101957acb1be0d96d7b716a159e98ad208462405146c42291332`。完整包私有库数由当前r12的71降至8；此处包含C++/GCC运行库及MPP，和阶段3仅服务端闭包17→5的统计口径不同。

执行命令/位置：

```bash
# MS-A2 VM301，ARM64/QEMU；固定c99f217源码，复用阶段3构建缓存
RKMOON_BUILD_ROOT=/work/build JOBS=8 tools/build.sh sunshine \
  -DRKMOON_SUNSHINE_SOURCE=/upstream -DRKMOON_BOOST_SOURCE=/deps/boost-src
python3 tools/package_server.py --server /server/rkmoon-kvm --assets /server/assets \
  --worker /components/bin/rkmoon-worker --mpp-lib /components/lib/librockchip_mpp.so.1 \
  --runtime-libs /components/lib --codec-ko /components/kernel/rkmoon_hdmirx_codec.ko \
  --licenses /licenses --output /output --version 20261112-adr014-c99f217
# T6 root：已有安装器验证包、保留旧发布；专用服务继续以at运行
python3 /opt/rkmoon/releases/20260928-r12/tools/install_server.py --release PACKAGE.tar.gz --user at
systemctl restart rkmoon.service
# VM301：私有最小moonlight-common-c接收器，仅读取内存管道中的临时会话密钥
cmake -S /client -B /client/build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build /client/build -j8
python3 /session.py /evidence
ffmpeg -hide_banner -v error -i placeholder.h264 -f null -
```

首次停止原因是部署前工具错误地断言HTTP401；监督员确认应检查XML401后，修正工具并重新抓基线，保留首次失败证据。新版无需代码修改；本轮后续构建/打包/实机验证均通过。无信号与USB offline按最新授权作为环境状态记录。

证据：`private/results/local/adr014/stage4-resume/`；VM301完整包/日志/接收器与源码归档于`/root/artifacts/rk-kvm-adr014-20261112/stage4-complete/`。T6基线及回滚资料为`/root/agent.backup/rkmoon-adr014-stage4-20261112/`，仅root可读；原`20260928-r12`可按DEPLOY-ROLLBACK/SERVER-INSTALL的--activate流程恢复。
