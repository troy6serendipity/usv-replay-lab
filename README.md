# USV Replay Lab

USV Replay Lab helps developers test communications software for uncrewed surface vehicles without a vessel. It combines deterministic heartbeat replay with an installable C++17 UDP/TCP shared library. The replay CLI checks expected timeout and recovery events and returns JSON and a process exit code suitable for automated regression tests. The project uses synthetic fixtures and local IPv4 sockets.

**Status:** Version 0.2.0 is an early development version. Four test suites passed locally on Windows/MSVC and WSL Ubuntu 26.04.1/GCC, including a separate application built against the installed shared library. The network library and replay engine are currently tested separately. Hardware, real wireless links, and third-party integrations remain to be validated. See [validation results](docs/communication_validation.md) and the [roadmap](docs/roadmap.md).

### Quick start

Requirements: CMake 3.20 or later, a C++17 compiler, and Python 3.8 or later for the tests and fixture generator. On Linux, run from the repository root:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
./build/usv_replay_lab datasets/heartbeat_v1/heartbeat.csv datasets/heartbeat_v1/dropout.case
```

The dropout scenario reports a timeout at 800 ms and recovery at 1100 ms, and exits with status 0. Appending `ignore_timeout` selects an intentionally faulty monitor, which should exit with status 1. For Windows commands, see below.

To install the communication library into a local prefix, run `cmake --install build --prefix "$PWD/stage"` after building. Consumer projects use `find_package(usv_comm 0.2 CONFIG REQUIRED)` and link `usv_comm::usv_comm`. See the [communication guide](docs/communication.md) and [independent consumer](tests/consumer/Readme.txt) for Windows commands and runtime requirements.

The library supports numeric IPv4 addresses, UDP datagrams, and TCP frames with a four-byte big-endian length prefix. TCP I/O failures close the stream. Callers must use a compatible C++ compiler and runtime with the shared library. The original project code is distributed under the [MIT License](LICENSE).

无人艇通信回放与故障回归工具箱。把“岸站多久没收到心跳、该在什么时候告警”写成可共享的测试，让开发者在电脑上复现问题，并在修改通信模块后自动检查结果。

本地开发版 `0.2.0` 提供 C++17 离线回放和可独立安装的 UDP/TCP 动态通信库。Windows 生成 DLL 与导入库，Linux 生成 `.so`；四个客户端/服务端示例调用同一份公开接口。离线核心从合成 CSV 读取心跳，在虚拟时钟上模拟固定窗口的丢失，比较预期与观测事件，输出 JSON 和退出码。

Windows/MSVC 与本机 WSL Ubuntu 26.04.1/GCC 已通过四组 CTest，包括真实回环套接字、独立进程互通及安装后消费测试。详细平台、命令和证据见[通信验证记录](docs/communication_validation.md)。Ubuntu 24.04/Windows 的 GitHub Actions 配置已写入，执行情况以仓库 Actions 实际记录为准。当前网络库和离线回放分别验证；将设备消息接入回放是[下一步](docs/roadmap.md)。

## 运行第一步

从项目根目录执行。构建需要 C++17 编译器和 CMake 3.20 或更新版本。测试与数据生成使用 Python 3.8 或更新版本的标准库。

Windows（PowerShell，Visual Studio 2022 C++ 工具链）：

~~~powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
.\build\Release\usv_replay_lab.exe datasets/heartbeat_v1/heartbeat.csv datasets/heartbeat_v1/dropout.case
~~~

Ubuntu（用独立目录构建，避免复用 Windows 构建缓存）：

~~~sh
cmake -S . -B build_linux -DCMAKE_BUILD_TYPE=Release
cmake --build build_linux --parallel
ctest --test-dir build_linux --output-on-failure
./build_linux/usv_replay_lab datasets/heartbeat_v1/heartbeat.csv datasets/heartbeat_v1/dropout.case
~~~

断链示例应看到 `timeout@800` 和 `recovered@1100`，退出码为 `0`。在同一条命令末尾加 `ignore_timeout`，可运行故意漏报告警的教学对照，退出码应为 `1`。数据生成和结果保存方式见[第一步复现实验](docs/step_01.md)。

## 使用通信动态库

先运行服务端，再在另一个终端运行客户端。Windows 示例：

~~~powershell
.\build\Release\udp_echo_server.exe 19001
.\build\Release\udp_echo_client.exe 19001
~~~

TCP 使用 `tcp_echo_server.exe` 和 `tcp_echo_client.exe`。Linux 使用 `./build_linux/udp_echo_server` 等无扩展名程序。示例绑定 `127.0.0.1`，完成一次回显后退出。TCP 采用 4 字节网络字节序长度与载荷的分帧格式，支持二进制、空帧和部分 I/O，单帧上限 1 MiB。

其他工程可通过 `cmake --install` 安装后使用 `find_package(usv_comm 0.2 CONFIG REQUIRED)`，链接 `usv_comm::usv_comm`。完整命令、API、超时和连接关闭约定见[动态库使用指南](docs/communication.md)，独立工程见[消费示例](tests/consumer/Readme.txt)。当前 C++ 接口要求库与调用方使用兼容的编译器、运行库及架构。

所有演示数据由项目生成，无须下载外部数据。离线回放检查时间逻辑；本机套接字检查收发与分帧；它们不代表实艇无线链路、硬件或航行系统验收。项目原创代码采用 [MIT 许可证](LICENSE)。

## 项目资料

| 文件 | 用途 |
| --- | --- |
| [项目设计](docs/project_design.md) | 使用者、问题、架构、同类工具与预期交付 |
| [分阶段路线图](docs/roadmap.md) | 工期估计、依赖、任务拆分和验收 |
| [合成心跳数据集](datasets/heartbeat_v1/README.md) | 字段、生成方法、故障窗口 |
| [输入与报告契约](docs/formats.md) | 版本、同时间戳顺序、错误和退出码 |
| [Windows 实测](docs/step_01.md) | 三个场景的结果、运行命令和证据 |
| [动态库使用指南](docs/communication.md) | UDP/TCP API、客户端/服务端、安装与链接 |
| [通信验证记录](docs/communication_validation.md) | 两平台构建、回环测试与安装消费证据 |
| [C++ 贡献指南](CONTRIBUTING.md) | 编码规范与复核方法 |
| [版本记录](CHANGELOG.md) | 功能与验证范围变更 |
