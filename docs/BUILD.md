# 构建、硬件前置与固定依赖

> 2026-11-12：ADR-014阶段2自有构建已通过 VM301 ARM64/QEMU 完整链接、离线与有限模拟验收；旧 Sunshine CMake/就地overlay退役，真实媒体和设备验收未执行。详见 [验收表](ACCEPTANCE.md)。

> 2026-09-23 本地更新：完整 Sunshine 与真实 MPP worker 已在 Linux amd64 编译链接通过；25 原生、69 Python/联合、25 sanitizer 测试通过。目标 ARM64/P1/P2/P3 仍未测试。最新结果见 [构建记录](../results/20260923-build/STATUS.md)。下文原交接记录保留供追溯。

## 0. 环境

独立组件要求 Linux、CMake≥3.20、C++17、Python≥3.10、pthread、Linux UAPI。专用服务端要求 **CMake≥3.24、Ninja、C++23**。自有构建仅保留固定 Sunshine 网络/输入源码，依赖 OpenSSL、Boost≥1.87（含 Endian、Format、Property Tree）、Opus、ALSA；阶段2暂保留完整 Linux misc/虚拟HID层，因此还需要 GIO、libevdev、libcap。无需 FFmpeg、curl、miniupnpc、PulseAudio、EGL/GL、npm。不要把系统旧编译器一定可用作为假设。[S1/S2]

本次云端：x86_64、GNU g++14.2.0、CMake3.31.6；没有MPP SDK/板卡。SDK源码和Sunshine完整构建未通过。以下是可执行构建流程，不是已在目标板成功的记录。

先只读记录本地 `uname -a`、`g++ --version`、`cmake --version`、`python3 --version`、`ldd --version` 和实际安装包版本。不得自动换发行版/内核/软件源。GCC/Clang/libstdc++缺C++23能力时列出可选的私有构建工具链，不擅自apt升级整机。

## 1. 本包离线构建

```bash
./tools/build.sh offline
# 额外内存检查；没有硬件依赖：
cmake -S . -B build/asan -DRKMOON_SANITIZE=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build build/asan -j2
ctest --test-dir build/asan --output-on-failure
```

默认 `RKMOON_MPP=OFF` **不会生成假的rkmoon-worker**。它只构建核心库、V4L2只读探测程序和测试。不能拿这个结果称“编码器已构建”。

## 2. 下载固定源码

```bash
python3 tools/fetch_sources.py
```

只新建vendor/mpp、vendor/sunshine；遇已有目录拒绝，不reset、不清工作树。Git子模块跟随固定Sunshine gitlink。不再运行 Sunshine CMake 或下载其 CPM/FFmpeg/WebUI 依赖；子模块跟随固定 gitlink。Boost 可使用系统开发包或显式指定已有源码目录；OS/编译器包版本仍需记录。

## 3. MPP 私有构建与原生worker

```bash
./tools/build.sh mpp
# 二进制默认在 build/native/rkmoon-worker
# 首先审阅实际加载的是私有版本，不替换旧KVM的系统libmpp：
ldd build/native/rkmoon-worker
```

私有prefix为`local/mpp`，不运行ldconfig、不覆盖/usr。`mpp` CMake选项/输出路径和SDK运行内核兼容性要在第一次本地构建核实。发现`find_path`/动态库目录不一致时明确传入`-DMPP_INCLUDE_DIR=... -DMPP_LIBRARY=...`，不要混用不同SDK头和系统库。

MPP的动态库路径若未由build rpath解决，只给本实例设置私有LD_LIBRARY_PATH；不要全局写`/etc/ld.so.conf`。特别检查旧KVM仍加载原来的库。

## 4. 自有服务端构建（ADR-014 阶段2）

```bash
# 只读核对固定 HEAD、干净状态、子模块和唯一补丁锚点；不写 vendor：
python3 tools/prepare_sunshine.py vendor/sunshine
./tools/build.sh sunshine
# 等价入口（不会调用 Sunshine 自身 CMake）：
cmake -S . -B build/server -G Ninja -DRKMOON_SERVER=ON -DBUILD_TESTING=OFF
cmake --build build/server --target rkmoon-kvm -j2
```

`RKMOON_SERVER` 默认 OFF，offline/mpp 目标保持独立。源码子集与替身头位于 `build/server/protocol/src`，相对包含总是命中该目录中的薄实现；`rkmoon-source-manifest.json` 记录固定提交和每个文件的 SHA256。拷贝脚本不修改 upstream，拒绝脏树、不同提交/子模块及缺失/重复补丁锚点。重新配置可刷新本工具生成且校验值未被手工修改的目录；有未知文件或手工编辑则拒绝覆盖。

既有 vendor 若是旧 overlay 脏树，不要 reset 或清理用户工作。改用远端独立、干净的固定克隆，例如：

```bash
./tools/build.sh sunshine -DRKMOON_SUNSHINE_SOURCE=/absolute/clean/Sunshine
# 已有模块化 Boost 源码可复用；不隐式下载或改系统：
./tools/build.sh sunshine -DRKMOON_SUNSHINE_SOURCE=/absolute/clean/Sunshine \
  -DRKMOON_BOOST_SOURCE=/absolute/boost-source
```

产物为 `build/server/rkmoon-kvm`，旁边保留 `assets/apps.json` 与固定上游 `assets/box.png`，许可文件在 `build/server/protocol/licenses`。`tools/build.sh sunshine` 只是现有命令名，内部唯一构建路径是本仓库目标；`apply_sunshine.py` 只作为只读变换模块使用，原就地 --apply/CMake overlay 路径已退役。

main 继续要求普通用户、绝对配置路径、本人0700 HOME/XDG_CONFIG_HOME，并核验二进制旁两个资产。只支持固定 HDMI 应用，不执行 apps/prep 命令、不启用桌面抓屏或软件编码、不生成证书或开放 WebUI。HTTP 状态/密码约定见 README/SECURITY。配置沿用上游同名键与默认值；只接受专用保留子集，移除的桌面编码、证书/WebUI 等选项会被拒绝且不记录值。默认值/键名审计命令：

```bash
python3 tools/audit_server_config.py /absolute/clean/Sunshine --output config-key-audit.json
readelf -d build/server/rkmoon-kvm
ldd build/server/rkmoon-kvm
python3 tools/test_minimal_startup.py --binary build/server/rkmoon-kvm --output PRIVATE_OUT
python3 tools/test_audio_loop.py --build build/server --output PRIVATE_AUDIO_OUT
```

启动验证在隔离容器中以非root运行，音频工具使用 ALSA null/注入状态，均不能作为真实 HDMI、MPP 或客户端验收。链接清单应不含 libav*/libsw*、curl、miniupnpc、pulse、EGL/GL；原动态依赖与新依赖的对比单独留证。不得运行 Sunshine 的安装流程或覆盖旧服务。当前实际测试状态见 ACCEPTANCE。

## 5. 设备权限

专用Sunshine不以root或filecap运行。根据实际设备，仅授予运行用户需要的video/render/MPP/dma_heap访问权限和kvmd私有socket访问；组名不是通用事实。`/dev/hidg*`不由本包直接打开；独立 rkmoon-input 后端管理自己的 gadget，见 INDEPENDENT-INSTALL.md。权限变更单列授权/回滚，不运行“chmod 666全部设备”。

## 6. P1与P2先决条件

拿到独占采集窗口后，先确认HDMI真实1080p60、单planeNV12/BGR24、source color明晰。首次只读probe不启动stream；`--probe`真实MPP能力测试会调用编码硬件但不读HDMI，也需要硬件访问授权。

```bash
build/native/rkmoon-worker --probe
# 授权后真实HDMI，明确允许像素拷贝（不是软件编码）：
python3 tools/p1_capture.py --worker build/native/rkmoon-worker \
  --device /dev/videoN --codec hevc --seconds 60 --result results/local/p1-hevc \
  --allow-copy --ack-capture-ownership
# 核对源range/transfer后填写；下面以BT709 limited为示例，而不是对实机的断言。
python3 tools/validate_capture.py results/local/p1-hevc/capture.hevc \
  results/local/p1-hevc/frames.csv --codec hevc \
  --expected-range tv --expected-transfer bt709 \
  --output results/local/p1-hevc/validation.json
```

随后以新结果目录测试H264。独立解码可在PC进行；解码使用CPU不等于生产视频编码走CPU。不能把 `worker --probe` 的合成黑帧统计当成真实 HDMI验收。
