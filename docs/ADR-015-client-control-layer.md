# ADR-015：客户端控制层自有化，媒体层保留

状态：规划（2026-11-12），未实施。基于对 `client/` 的静态阅读；未编译、未运行。行数统计见 `private/results/local/client-review/REPORT.md`（本地，不提交）。

接手者先读：`AGENTS.md`、本文件、`client/docs/MOUSE-CONTRACT.md`、`docs/ADR-005-http-password.md`、`docs/ADR-011-no-signal-session.md`、`docs/ADR-014-sunshine-extraction.md`。

## 1. 现状

客户端已经是“自写 Qt Widgets 小窗口 + 选择性编译 moonlight-qt”，不是完整 Moonlight：

- 自写：`client/app/src` 20 个文件 1707 行。
- 补丁：`client/tools/apply_moonlight.py` 改 10 个上游文件和 `moonlight-common-c/src/Connection.c`，新增 90 行、删除 159 行；每个锚点恰好一次，无历史叠加。
- 仍编译的上游应用代码：26 个文件约 1.33 万行（`client/app/rkmoon-app.pro`）。
- 已不编译：配对、身份密钥、多主机管理、mDNS、QML、真实手柄、Discord、更新检查。

所以问题不在“编进了太多无关模块”，而在自写控制层质量和它与上游通用模型的耦合。

## 2. 审查发现

### 2.1 缺陷

| # | 级别 | 位置 | 问题 |
|---|---|---|---|
| D1 | 高 | `kvmwindow.cpp` `closeEvent`、Forget、Connect、Settings、鼠标模式下拉 | 都以 `m_pollBusy` 为闸门并静默忽略。轮询每 1 秒一次、单次超时 5 秒；主机不通时 `m_pollBusy` 几乎一直为真，窗口关不掉、按钮无反应。鼠标模式下拉每秒被禁用再启用，恰好在请求期间的选择被悄悄还原 |
| D2 | 高 | `kvmhost.cpp:199` | `refreshAsync(apps=true)` 直接解引用 `m_computer`；`forget()` 后旧应答返回即空指针。旧应答还会经 `acceptInfo` 把上一个主机的 uuid/name/appId 写回配置。无请求代号、无取消 |
| D3 | 中 | `kvmwindow.cpp` `POLL_FAILURE_LIMIT` | “约 10 秒容错”按失败次数计；超时类失败每次 5 秒且忙时跳过轮询，实际可达 50 秒以上 |
| D4 | 中 | `kvmhost.h` `m_network` | 未设 `QNetworkProxy::NoProxy`，上游 `NvHTTP` 设了。系统配置代理时，认证请求（带 Basic 头）与会话请求走不同路径。未实测 |
| D5 | 中 | `kvmwindow.cpp` `sessionEnded` | 用界面文字判断状态：`m_status->text() == tr("HDMI stream connected")` |
| D6 | 低 | `kvmwindow.cpp` `settings()` | 借 `m_connecting` 当“忙”锁（`QScopedValueRollback`），对话框打开期间轮询全部跳过 |
| D7 | 低 | `kvmwindow.cpp` `runStream` | 无信号占位按 1920×1080@60 协商，服务端实际约 10 帧/秒（ADR-011）；帧率统计与帧节奏按 60 计算 |
| D8 | 低 | `tools/package_windows.ps1` | 产出目录名 `RKMoon` 并打 ZIP，与 `AGENTS.md` 要求的 `RKMoon-Windows-x64` 直接可运行目录不符；仍复制 `dav1d.dll`、`libssl-3-x64.dll`、`tls\qschannelbackend.dll`（无 AV1、无 TLS） |
| D9 | 低 | `vendor/moonlight-qt` | 是指向 `/tmp/rkmoon-work/...` 的失效软链，本地默认构建入口不可用 |

### 2.2 冗余

- **两套主机逻辑**：`KvmHost::refresh()` + `hdmiApp()`（同步，71 行）只被 `client/tests/client_ui.cpp:740/758/776` 调用；生产只走 `refreshAsync`。应用列表有两个解析器（`NvHTTP::getAppList` 与手写 `QXmlStreamReader`）。
- **`acceptInfo` 为解析 XML 构造 `NvHTTP` 和 `NvComputer`**；`NvComputer`（576 行）带 QSettings 序列化、WOL、多地址、应用缓存，本客户端只用到 uuid、name、appVersion、pairState、currentGameId、serverCodecModeSupport 和地址可达性判断。
- **两套配置**：`KvmConfig` 每次启动会话把自己逐字段抄进上游 `StreamingPreferences`（367 行，启动时读一整套游戏偏好）。`KvmConfig` 里 `width/height/fps` 其实是每次会话的临时参数。
- **无界面入口的持久化字段**：`fullScreen`、`vsync`、`framePacing`、`captureSystemKeys` 没有设置界面（设置对话框只有码率和编码）；`quitAppOnDisconnect` 被读写但 `applyTo` 固定 `quitAppAfter=true`。
- **`KvmWindow` 用 10 个布尔加 3 个计数器表达状态**：`m_wantStream`、`m_restart`、`m_cleanup`、`m_reconnectPending`、`m_sessionConnected`、`m_pollBusy`、`m_streaming`、`m_connecting`、`m_connected`、`m_unreachable`；`KvmHost::streaming` 又复制一份。
- **补丁里的残留**：`abstouch.cpp` 的 `if (false)` 原生触摸分支；`input.cpp` 注释 “Relative HID only” 已过期；`nvhttp.cpp` 把 “HTTPS 基址” 指回 HTTP 端口的三段兼容补丁；`nogamepad.cpp` 97 行空桩。
- **`Session`（2339 行）** 混有旧 GFE、HDR、AV1、4:4:4、手柄、游戏提示等分支。
- **文档**：`client/README.md` 前后矛盾（relative-only、分辨率预设、旧 ZIP）；`sources.lock.json` 的 usage 仍写 “pairing reused”；状态文字中英混用（两条中文用 UTF-16 转义硬编码）。

### 2.3 做得对、必须保留的

- `RkmoonAuth`：密码只在内存、只发给绑定的 `http://host:port`、强制手动重定向、不进 URL 和日志。
- `rkmoon_http_log.h`：只记录白名单操作名。
- `RkmoonPointer`：与渲染器同一套等比缩放取整，坐标夹到最后一个像素。
- `Connection.c` 补丁：去掉连接时的相对鼠标微移（否则撤销绝对鼠标租约）。
- 会话清理顺序：`Session` 与主机对象必须活过 `readyForDeletion`。
- 每次启动前重新校验密码、主机身份和源模式；模式变化需连续两次轮询确认。

## 3. 目标设计

### 3.1 分层

```
KvmWindow            只做界面：输入框、按钮、状态文字。不持有协议状态。
   │ 信号/槽
StreamController     显式状态机；轮询、重连、模式跟随；墙钟预算。
   │
HostClient           唯一的 HTTP 出口：serverinfo / applist / launch / resume / cancel。
   │                 异步、可取消、带请求代号；返回不可变 HostInfo。
RkmoonAuth           保持不变。
   │
StreamSession        会话编排：LiStartConnection、回调、清理顺序。
   │                 阶段 A/B 仍是上游 Session；阶段 C 换成自有薄实现。
媒体层（上游原样）    moonlight-common-c、ffmpeg + D3D11VA/DXVA2、pacer、SDL 输入/窗口、Opus、音频渲染器。
```

### 3.2 `HostClient`

- 一个 `QNetworkAccessManager`，显式 `NoProxy`，所有请求经 `RkmoonAuth::prepareRequest`。
- `bind(address, port)` / `forget()` 各自使代号加一并 `abort()` 在途请求；应答回来先比对代号，不符即丢弃。
- 一个 XML 解析函数产出：

```cpp
struct HostInfo {            // 不可变快照
    QString uuid, name, appVersion, gfeVersion;
    int currentGameId; int codecSupport; bool authorized;
    RkmoonDisplay display; bool mouseAbsolute, mouseRelative;
    int hdmiAppId;           // 由 applist 解析，0 表示未解析
};
```

- 错误分三类并显式返回：`Transport`（可重试）、`Auth`（401/403，含 XML status_code）、`Protocol`（身份变化、方案不符、字段非法）。取代现在的 `transientFailure` 布尔。
- 同步 `refresh()`、`hdmiApp()` 删除；测试改测这一条生产路径。

### 3.3 `StreamController`

状态：`Idle → Authenticating → Ready → Starting → Streaming → Stopping → RetryWaiting → Ready/Idle`，另有 `Ready` 下的 `unreachable` 标记。

- 用户意图（想串流 / 不想）与实际状态分开存，只有这两项，不再有布尔组合。
- 界面可用性从状态推导；轮询请求在途不改变任何控件可用性（修 D1）。
- 关闭窗口：`Idle/Ready/RetryWaiting` 立即取消在途请求并关闭；`Starting/Streaming/Stopping` 先发停止再关闭。
- 容错用单调时钟：串流中元数据不可达预算 10 秒；重连退避 1/2/4/8/8 秒不变（修 D3）。
- 状态文字由状态和错误类别生成，不反向读界面（修 D5）；统一一种语言。

### 3.4 配置

- `KvmConfig` 只保留真正持久化且有界面的项：地址、端口、主机 uuid/name、应用 id、码率、编码、鼠标模式、音量、静音。其余字段改为代码常量或加上界面，二选一，不留暗项。
- 新增 `SessionParams { width, height, fpsX100, bitrateKbps, codec, absoluteMouse, ... }`，每次启动由 `HostInfo.display` 生成；无信号时帧率按服务端占位实际节奏协商（先与服务端 `--fps-x100` 行为核对，修 D7）。
- 阶段 B 起不再编译上游 `streamingpreferences.cpp`：在 `client/app/src/settings/streamingpreferences.h` 提供同名替身类，只含保留的上游文件实际读取的字段（做法同 ADR-014 的替身头文件；字段清单用脚本从保留文件提取）。

### 3.5 线上契约（不得改变）

- 六个路由、单一 HTTP 端口、`Authorization: Basic base64("kvm:"+密码)`。
- 密码错误：HTTP 200，XML 根 `status_code=401`。
- `serverinfo` 必需字段：`RKMoonAuth=password-http-v1`、`RKMoonDisplayVersion=1`、`RKMoonDisplayStatus/Width/Height/FpsX100`、`RKMoonMouseModes`、`uniqueid`、`appversion`、`PairStatus=1`、`HttpsPort=0`、`currentgame`、编解码能力字段。
- `launch`/`resume` 查询参数与上游一致，另加 `rkmoonMouseMode=absolute|relative`。
- RTSP、控制流、RTP、输入加密由 moonlight-common-c 负责，不动。

## 4. 阶段

每个阶段单独提交、单独验收，更新 `client/docs/ACCEPTANCE.md`，离线测试 / Windows 构建 / 真机实测分开标记。

### 阶段 A：修缺陷与清理（不换上游模块）

1. 引入 `HostClient` 的代号、取消、`NoProxy` 和三类错误；`KvmHost` 的同步路径和第二个应用解析器删除（D2、D4）。
2. `KvmWindow` 的布尔组收敛为 `StreamController` 状态机；轮询不再闸住界面；关闭与 Forget 可随时生效（D1、D5、D6）。
3. 容错改墙钟（D3）。
4. 清配置暗项；删 `abstouch` 死分支和过期注释；状态文字统一语言。
5. `package_windows.ps1` 改为产出 `RKMoon-Windows-x64` 目录、不打 ZIP、去掉 `dav1d.dll`；`libssl`、`qschannelbackend.dll` 先用依赖查看工具确认无引用再去（D8）。修 `vendor/moonlight-qt` 入口说明（D9）。
6. 重写 `client/README.md`、`sources.lock.json` 的 usage。

完成标准：新增离线测试覆盖——请求在途时 forget/rebind 不崩溃且不写回旧身份；主机不通时窗口可关闭、按钮有反馈；10 秒预算在“快速失败”和“超时”两种情形下都成立。原有离线测试通过。Windows 构建通过。真机冒烟：连接、Start、退出、重连各一次。

### 阶段 B：自有 Host / HTTP / Config

1. `HostClient` 承担 `launch`/`resume`/`cancel`，不再编译 `nvhttp.cpp`、`nvcomputer.cpp`、`nvaddress.cpp`、`nvapp.cpp`、`streamingpreferences.cpp`；`apply_moonlight.py` 中对应补丁段删除。
2. 上游 `Session` 仍在：先用脚本列出 `session.cpp`、`input/*.cpp`、`audio.cpp`、`ffmpeg.cpp` 对 `NvComputer`、`NvHTTP`、`NvApp`、`StreamingPreferences` 的全部成员引用，据此提供同名替身头文件；`Session` 内的 `launch`/`resume`/`quit` 调用改走 `HostClient`（补丁或替身实现，取改动小者）。
3. `Session::getActiveAddressReachability` 等用于 MTU/VPN 判断的逻辑要么保留等价实现，要么固定为局域网并写明依据。

完成标准：新旧客户端对同一服务端发出的 `launch`/`resume`/`cancel` 请求（路径、查询参数名与取值格式、头）逐项一致，用本地回环假服务端录制对比；离线测试、Windows 构建通过；真机：HEVC 与 H.264 各 60 秒、音频、两种鼠标模式、断线重连。

### 阶段 C：自有会话编排

1. 新 `StreamSession` 取代上游 `session.cpp`：只保留 SDR 8 位 4:2:0、H.264/HEVC、立体声、键鼠；删 HDR/AV1/4:4:4/手柄/旧 GFE 分支和 `nogamepad.cpp`。
2. 必须原样保留并写成测试或检查清单的行为：解码器先于 `LiStopConnection` 销毁；停止时释放所有按键和鼠标键；`readyForDeletion` 前对象不析构；SDL 事件循环在主线程；窗口失焦/全屏切换/本地指针切换快捷键；音量与静音在 Opus 解码后生效。
3. 去掉 `session.cpp` 补丁里的 `goto` 与循环内 `processEvents`，改为明确的停止信号。

完成标准：`apply_moonlight.py` 只剩 `input.cpp`、`input.h`、`keyboard.cpp`、`mouse.cpp`、`abstouch.cpp`、`audio.cpp`、`Connection.c`（若仍有偏差写明原因）；真机全量验收（见第 5 节）。

### 阶段 D（可选）：依赖裁剪

逐项、每次只去一项并测量：libsoundio（保留 SDL 音频）、DXVA2（仅留 D3D11VA）、SDL_ttf 与覆盖层文字、swscale、AntiHooking。每项记录包内文件数和字节数前后对比。不做的理由成立就不做。

## 5. 验收矩阵（真机）

需要一台带 GPU、可登录桌面、且不是被控机本身的 Windows 主机（2026-11-12 已确认 192.168.123.127 是被控机 Tiny11_clone，不能用于键鼠验证）。

| 项 | 阶段 A | B | C |
|---|---|---|---|
| 连接、身份不变、错误密码提示 | ✓ | ✓ | ✓ |
| HEVC / H.264 各 60 秒，硬件解码 | 冒烟 | ✓ | ✓ |
| 音频、音量、静音 | — | ✓ | ✓ |
| 绝对 / 相对鼠标、键盘、按键释放 | — | ✓ | ✓ |
| 断线重连、强退后重连、resume | 冒烟 | ✓ | ✓ |
| 源模式切换跟随、无信号占位 | — | ✓ | ✓ |
| 主机不通时窗口可关、按钮有反馈 | ✓ | ✓ | ✓ |
| 全屏 / 窗口切换、失焦、多显示器 | — | — | ✓ |

## 6. 约束

- moonlight-qt 固定提交 `f786e94c`、moonlight-common-c `8599b604` 不变；锚点失败先读实际代码，不放宽匹配。
- 不改线上协议；服务端不需要配合修改。
- 不去掉硬件解码要求（`VDS_FORCE_HARDWARE` 语义），不加软件解码兜底。
- 密码不落盘、不进日志和 URL；输入内容不记录。
- 所有编译和测试在远端构建机执行；Windows 交付为 `RKMoon-Windows-x64` 目录，按 `AGENTS.md` 顶部的 SMB 交付规则替换。
- 任一阶段真机验收失败即停止推进，保留上一版交付目录可回退。

## 7. 风险

- 阶段 B 的替身头文件：字段遗漏是编译错误，可控；默认值差异不会报错，需逐字段对照上游 `StreamingPreferences::reload` 的默认值。
- 阶段 C 风险最高：SDL 主线程与 Qt 事件循环的重入、异步清理的对象寿命、D3D11 设备重建。没有稳定真机基线前不要开始。
- 阶段 A 把轮询与界面解耦后，原来靠 `m_pollBusy` 偶然避免的竞态（例如轮询应答与 Start 同时到达）需要由状态机显式处理，测试要覆盖。
- 首轮不会明显缩小安装包：大 DLL（ffmpeg、Qt、SDL、OpenSSL）都还在。

## 8. 待用户决定

1. 真机测试主机（见第 5 节）。
2. 是否保留 macOS 构建目标（`.pro` 里仍有 VideoToolbox 分支；保留则每阶段多一份构建验证）。
3. 阶段 D 是否要做。
