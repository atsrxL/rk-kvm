# 项目名称：rk-kvm

2026-11-11：按用户要求，项目统一命名为 `rk-kvm`，覆盖 GitHub 仓库、本地源码目录、Paseo 项目显示名、顶层 CMake 项目名称、源码清单和客户端显示名。

- GitHub：<https://github.com/atsrxL/rk-kvm>；旧仓库名为 `rkkvm-sunmoon`。
- 本地源码目录：`/Users/at/Documents/GitRepository/rk-kvm`。旧目录保留兼容符号链接，供已有 Paseo 会话和工作区引用使用。
- 旧的源码清单项目名 `rk3588-moonlight-kvm` 统一为 `rk-kvm`；Sunshine、MPP 及其他依赖的固定版本不变。

## 兼容标识

`RKMoon` / `rkmoon` 作为现有实现的兼容标识继续保留：协议字段（如 `RKMoonAuth`）、环境变量、C++/Python 名称、二进制名、systemd 服务、运行目录和 USB 描述符均维持原值。Qt 的应用标识和组织标识维持原值，以保留现有设置和日志目录；用户可见名称通过 display name 和窗口标题显示为 `rk-kvm`。

Windows 稳定交付目录仍遵守现有要求：`RKMoon-Windows-x64`，RP2350 工具和固件也沿用已有交付目录与产物名。仓库更名不会迁移或重启已部署服务，不会替换 SMB 上的二进制。历史记录中的名称、路径、commit 和 SHA256 保留原样，仍对应当时产物。

## 本次验证边界

本次仅更名及静态一致性核对，不构建、不访问设备、不更新交付产物。客户端新的显示名称需下次 Windows 构建交付后才会出现在已安装客户端中。项目继续暂停，所有原有未提交修改保留，不随更名提交。
