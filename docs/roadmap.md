# 实现路线图与验收

每一步提交输入、可运行实现、自动断言和实测结果。工期为一位开发者的净工时估计，实际节奏取决于熟悉 C++、操作系统和外部团队的试用安排。未来阶段的命令是约定的验收入口，在对应测试加入后运行；没有选中测试不能视为通过。`0.2.0` 已完成通信基础库，下一项是把使用方监视器与设备消息接入确定性回放。

## 0. 离线心跳闭环

**状态：已完成，Windows 与本机 WSL Ubuntu 26.04.1 回归通过。** 交付 C++17/CMake、合成 CSV、两个场景、故意漏报对照、CTest、格式配置与实测报告。

验收：

~~~text
python tools/generate_heartbeat.py --check
ctest --test-dir build -C Release --output-on-failure
python tests/verify_replay.py --binary build/Release/usv_replay_lab.exe --report-dir evidence/step_01
~~~

Windows Release 下，三次演示调用分别返回 `0 / 0 / 1`。断链在 800 毫秒告警、1100 毫秒恢复。重复运行报告一致。详见[第一步复现记录](step_01.md)。

## 已完成的 0.2.0 通信基础库

**状态：本地两平台构建与四组 CTest 通过。** 实现 `usv_comm` 动态库、UDP/TCP 客户端和服务端、安装包配置、独立消费工程。TCP 的 4 字节长度分帧已处理部分 I/O、空帧、二进制载荷、超时、截断和过大输入。UDP 保留数据报边界及来源地址。

已交付 `include/usv_comm/`、`src/comm/`、`src/examples/comm/`、`src/tests/comm/`、`tests/consumer/`、`cmake/`、[接口与安装说明](communication.md)。Windows 验证 DLL 和导入库，本机 WSL Ubuntu 26.04.1 验证 `.so`。实际日志和验证范围见[通信验证记录](communication_validation.md)。

~~~text
ctest --test-dir build -C Release --output-on-failure
ctest --test-dir build_linux --output-on-failure
~~~

分别应执行 `heartbeat_replay`、`network_loopback`、`examples_and_wire`、`installed_consumer`，四组均通过。最后一组安装库后在独立工程中查找、链接并运行。`.github/workflows/ci.yml` 已配置 Ubuntu 24.04 与 Windows 2022；远程执行结果以仓库 Actions 的实际运行记录为准。现有回环通信尚未把心跳故障场景接入岸站状态机。

## 1. 把监视器接入能力做成可复用入口

**待实现，预计 8–14 小时。依赖：** 现有离线及通信回归通过。优先做这一步，以便用一条实际故障验证项目价值。

1. 分离回放调度与监视器。定义最小接口，能交付心跳、推进逻辑时间并收集状态变化。保留截止时刻先交付心跳的现有语义。
2. 将参考监视器和漏报对照接入同一接口，再做一个独立示例适配。让外部模块能复用现有场景，无须修改核心时间逻辑。
3. 写出一页接入说明。让使用者准备轨迹、期望和接口映射，记录版本、输入哈希和结果。保留 `heartbeat_v1` 兼容性。
4. 将新增契约测试纳入已有 CMake/CTest 与 CI 配置。用同一份 `heartbeat_v1` 验证参考实现、漏报对照和外部适配示例，保存实际报告。GitHub 上的实际运行状态以发布后可核验的记录为准。

**预计文件：** `include/usv_replay_lab/link_monitor.h`、`src/link_monitor.cpp`、`src/examples/monitor_adapter.cpp`、`src/tests/adapter_contract.cpp`、`docs/monitor_adapter.md`。
**验收：** `ctest --test-dir build -C Release -R "heartbeat_replay|adapter_contract" --output-on-failure`。两套测试都应实际执行。参考监视器与现有输出相同，故意漏报仍返回 `1`，示例适配能收到规定的时间顺序。必须覆盖 800 毫秒截止前一刻、恰好截止时到达和无后续输入直至场景终点三种边界。截止时刻先交付心跳的现有契约保持不变。

## 2. UDP 本机虚拟设备

**待实现，预计 12–20 小时。依赖：** 第 1 步监视器接口稳定、现有 `UDPSocket` 回归通过，至少有一个目标使用流程。

1. 定义一个可公开的最小示例数据报，写明协议版本、设备标识、序号、时间字段与字节序。用已实现的 `UDPSocket` 在 `127.0.0.1` 与系统分配的端口间收发。
2. 用虚拟设备读取同一份合成心跳 CSV，按场景发送或跳过数据报。接收端记录单调时钟到达时刻和原始序号，转换成显式带版本的回放事件，避免把设备时间误当接收时间。
3. 分别检查正常消息、固定窗口缺失、重复、乱序、错误版本和恢复。先明确重复/乱序的业务规则，再生成固定预期。保留输入哈希、发送记录、接收记录和状态报告。
4. 把捕获的最小事件序列交给第 1 步接口，验证同一输入重复回放得到相同 JSON。精确 800 毫秒告警用虚拟时间断言；实际套接字验证消息内容、收发数量、释放资源和事先说明的时间容差，不能要求系统调度无误差。

**预计文件：** `include/usv_replay_lab/heartbeat_packet.h`、`src/heartbeat_packet.cpp`、`src/examples/udp_heartbeat_device.cpp`、`src/examples/udp_heartbeat_receiver.cpp`、`src/tests/udp_heartbeat.cpp`、场景、接入文档与报告。
**验收：** `ctest --test-dir build -C Release -R "adapter_contract|udp_heartbeat|heartbeat_replay" --output-on-failure`。必须实际选中并执行三组测试；健康场景无告警，缺失场景离线回放在 800 毫秒告警、1100 毫秒恢复，漏报实现判失败。接收记录能在不启动虚拟设备的情况下独立重放。

## 3. 通用字节流解析与 Linux 虚拟串口

**待实现，预计 18–30 小时。依赖：** 第 1 步适配契约，及真实使用者对传输方式的优先级反馈。当前 TCP 长度分帧与本机连接已经实现；这一阶段增加可离线喂入字节的协议解析层与串口适配。

1. 把协议分帧独立成可直接喂入字节的解析器。给同一合法数据流提供逐字节、分块和多帧同块用例，校验输出序列一致。
2. 给新的设备协议定义错误分类与恢复策略，再加入截断、错误长度及校验失败用例。现有 TCP 长度协议没有校验和，残缺帧后关闭连接；不能在没有同步标记的字节流中承诺自动恢复边界。
3. 将设备协议事件接入已有 TCP 连接，验证协议层与传输层的职责。TCP 的发送次数与接收分块没有固定对应关系，任意分块边界在纯解析器测试里控制。
4. 复用字节解析器接 Linux PTY。虚拟串口验证软件读写和分帧。RS232、RS422、RS485 的电气与总线行为留给相应硬件测试。

**预计文件：** `include/usv_replay_lab/frame_decoder.h`、`src/frame_decoder.cpp`、TCP/PTY 适配器、分帧数据和各自测试。
**验收：** 在 Linux 执行 `ctest --test-dir build_linux -R "stream_decoder|network_loopback|serial_pty" --output-on-failure`。分帧单测与目标平台支持的集成测试均应实际运行。报告列明运行平台和执行过的用例。

## 4. 发布和真实采用

**预计 8–16 小时准备首发，试用与维护持续进行。依赖：** 至少一条完整的接入和回归路径。第 1 步完成后即可试用，无须等每种传输方式齐备。

1. 本地已有 MIT 许可证、共享库包和独立消费示例。发布前核对原创代码、数据、文档归属与发布范围，保留内部培训附件和实艇日志在原处。补版本变更和已知限制。
2. 为每次发布保留可复现的 Git 提交。从干净检出运行两平台构建、安装与消费命令，保存对应提交的实际 CI、版本与发布包链接。自动化配置存在不等于实际 CI 通过。
3. 与独立使用方核验一个真实通信问题。记录原有复现过程、使用工具后的命令、修复 PR 和 CI 中的回归用例。以实际使用决定下一个阶段重点。
4. 记录维护者负责的 Issue、代码审查和发布。证据齐备后根据当时官方条款判断申请准备度。

**预计文件：** 使用指南、版本记录、许可证、发布清单、接入案例和 CI 日志链接。
**验收：** 用第 0 步的 CMake、CTest 和报告命令从干净检出运行，检查两平台实际通过。外部采用须能指向使用方仓库中的 Issue、合并记录、配置或测试结果。

## 下一次结对实现

从第 1 步的接口拆分开始。先在现有 `replayHeartbeats` 的调用路径上写出监视器应接收的事件与截止时刻，然后把参考和漏报两个实现挂到同一接口。完成一次小改动就运行原有测试，直到新增适配示例与旧场景同时通过。

再按第 2 步把 UDP 虚拟设备接上，交付一条完整的“合成输入 → 实际收发 → 事件记录 → 离线回放 → 修复前后回归”流程。它是后续邀请独立团队试用的具体入口。联系团队或提交申请待用户另行要求。
