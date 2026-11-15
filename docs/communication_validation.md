# 动态通信库 0.2.0 验证记录

验证日期：2026-11-12，北京时间。输入全部由项目或测试程序构造；网络用例使用本机 IPv4 回环套接字及系统分配的临时端口。没有使用实艇或内部日志。

## 实际结果

| 环境 | 构建 | CTest | 动态链接与安装消费 |
| --- | --- | --- | --- |
| Windows x64、MSVC 19.42、CMake 3.29.5-msvc4、Python 3.12.7 | 退出码 0 | 4/4 通过，退出码 0，约 3.41 秒 | `dumpbin /DEPENDENTS` 列出 `usv_comm.dll`；独立消费工程安装、查找、链接、运行通过 |
| WSL Ubuntu 26.04.1、GCC 15.2.0、CMake 4.2.3、Python 3.14.4 | 退出码 0 | 4/4 通过，退出码 0，约 10.80 秒 | `ldd` 列出 `libusv_comm.so.0`；独立消费工程安装、查找、链接、运行通过 |

两平台是 2026-11-12 的本机实测。Ubuntu 24.04 已写入 CI 矩阵，但不在上述本机测试范围内；GitHub CI 的执行情况以仓库 Actions 的实际记录为准。构建不依赖 Qt、ROS 或其他网络库，Windows 使用 Winsock，Linux 使用 POSIX 套接字。

## 四组验收覆盖

| 测试 | 实际断言 |
| --- | --- |
| `heartbeat_replay` | 健康链路和预期断链返回 0；故意漏报告警返回 1；报告重复运行一致，输入边界与同时间戳规则通过。 |
| `network_loopback` | UDP 二进制、空包、来源端点、自动绑定、超时、大小限制、移动和关闭；TCP 256 KiB 二进制、多帧、空帧、移动、大小限制、接受连接超时、对端关闭；接收超时后流失效；非法 IPv4 被拒绝。 |
| `examples_and_wire` | 四个独立可执行程序分别通过 UDP/TCP 回显；按字节多次写入的二进制 TCP 帧正确恢复；截断帧和超长帧使服务端返回 1。 |
| `installed_consumer` | 先安装到构建目录内的临时前缀，再用独立 CMake 工程 `find_package(usv_comm CONFIG REQUIRED)`；仅通过导出目标链接并实际收发。 |

`examples_and_wire` 的多次小写入可能被操作系统合并，因此本次不能证明底层每一种短读、短写路径均被确定性覆盖。半帧停顿超时、发送背压、连接拒绝以及负载测试可作为后续专项验证。代码审查确认一帧的头部和载荷共用同一个单调时钟截止时间。

精确的 800 ms 告警属于离线虚拟时钟断言。回环网络测试使用真实操作系统时钟，仅验证行为和有界等待，不代表实艇无线丢包率、端到端时延或控制安全性能。

## 重跑命令

Windows，Visual Studio 开发者终端：

~~~powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release -- /m:1 /nr:false
ctest --test-dir build -C Release --output-on-failure --output-junit results.xml
python tools/generate_heartbeat.py --check
~~~

本次自动执行环境的网络隔离会阻止回环收发，因此网络测试使用获准的隔离外执行环境；测试地址仍是 `127.0.0.1`。没有修改防火墙配置。

Linux，在工程根目录：

~~~sh
cmake -S . -B build_linux -DCMAKE_BUILD_TYPE=Release
cmake --build build_linux -j 2
ctest --test-dir build_linux --output-on-failure --output-junit results.xml
python3 tools/generate_heartbeat.py --check
ldd build_linux/tcp_echo_client
~~~

复跑时四个 CTest 项必须实际出现，不能只依据空筛选结果的退出码。预期全部通过；测试输入和示例均无需下载数据集。库接口与线格式见 [通信库说明](communication.md)。

## 保存的证据

[汇总 JSON](../evidence/communication/summary.json) 保存平台、真实退出码、各项结果、源码与产物 SHA-256。原始 JUnit 和 CTest 日志保存在运行机器上，未随公开仓库发布。重新运行 CTest 可生成本机 `Testing/Temporary/LastTest.log` 和 JUnit 输出。

clang-format 18.1.8 对全部项目自有 C++ 头文件、实现、示例和独立消费程序检查通过，退出码 0。另按原规范第 3、4 章人工复核命名、保护宏、include 路径、命名空间限定及保留标识符；独立只读复核未发现阻塞交付的问题，并已修正公共头文件对参数错误的关闭语义注释。该复核不等于形式化验证。

下一步把岸站链路监视器接入确定性回放，再接入 UDP 虚拟设备。最终用一个独立使用方的真实问题形成“修复前失败、修复后通过、持续接入 CI”的证据。当前只有本地工程结果，没有外部采用或维护历史可据此声称。
