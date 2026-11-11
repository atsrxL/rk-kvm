# 隔离构建与离线验证

2026-09-23 已在 Debian 13 amd64 容器完整编译固定 MPP worker 与专用 Sunshine。结果见 [构建记录](../results/20260923-build/STATUS.md)。目标 ARM64 编译和硬件验收仍需独立执行。

以下命令在获授权的远端构建机执行（本次为 MS-A2/VM301），不在本机编译。既有镜像可复用；它包含兼容历史构建的额外开发包，不能据此推断新二进制链接了这些库。

```bash
docker build -f tools/Dockerfile.build -t rkmoon-build .
# 若系统 Boost 不满足版本，显式挂载已有 Boost 源码；不运行 Sunshine CMake 下载依赖。
docker run --rm -it -v "$PWD:/work" -v /absolute/boost-source:/deps/boost:ro -w /work rkmoon-build
tools/build.sh offline
python3 tools/fetch_sources.py
python3 tools/prepare_sunshine.py vendor/sunshine
JOBS=8 tools/build.sh mpp
JOBS=8 tools/build.sh sunshine -DRKMOON_BOOST_SOURCE=/deps/boost
readelf -d build/server/rkmoon-kvm
ldd build/server/rkmoon-kvm
```

新服务端的唯一入口是本仓库 `RKMOON_SERVER` 目标，Sunshine 克隆须保持干净；输出在 `build/server`，源码子集在其 `protocol` 子目录。上游 Git/子模块只读使用，不对旧 vendor 覆盖或清理。生成目录若被手工编辑会拒绝刷新。不要提交 vendor、私有配置或产物。

镜像包版本随时间变化，不宣称逐字节可复现。构建容器不映射视频/HID 设备，也不用 privileged。MPP worker 保持真实硬件路线。运行产物需要旁边的 `assets/apps.json`、`assets/box.png` 和正确的目标架构运行库；main 通过二进制路径定位 assets，可在另一个 cwd 启动。打包遵循 SERVER-INSTALL，只选实际动态依赖，不能仅复制二进制或整个旧依赖目录。

实际新构建验收见 ACCEPTANCE，编译/合成测试与硬件验收分开。

## 下一阶段的具体操作范围

1. 在独立 ARM64 构建环境完成同样构建，保存二进制和动态库 SHA256。
2. 使用目标原生 probe 读取 /dev/video0 的 timings、format、stride，不启动 streaming。
3. 明确采集窗口和恢复方式；仅在授权窗口内停止占用视频节点的旧实例，测试完成后恢复并检查旧服务。
4. 根据目标私有 MPP 实际需要，为运行用户临时授予 /dev/mpp_service 和所需 DMA heap 的读写 ACL；保存原 ACL，测试后精确恢复。不得用 root 启动 Sunshine 或全局 chmod 666。
5. 先合成黑帧 MPP 探测，再真实 1080p60 HEVC/H264 各 60 秒、独立解码，再 video-only Moonlight。
6. USB 输入阶段单独确认 kvmd 相对鼠标、API 鉴权与独占授权。绝对 HID descriptor 变更须单独审核；未验证前保持 input.enabled=false。

权限/采集窗口尚未执行，当前仓库不是已通过实机验收的可部署发行版。
