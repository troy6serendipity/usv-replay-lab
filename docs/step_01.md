# 第一步实测：离线心跳超时

首个 C++17 程序已通过本机 Windows Release 构建和 CTest。它验证岸站通信模块能否按时发现心跳中断，并在收到下一条心跳时记录恢复。实验用虚拟时钟和合成输入，结果可独立重跑。

## 数据和结果

[heartbeat.csv](../datasets/heartbeat_v1/heartbeat.csv) 由生成脚本按每 100 毫秒一条生成，共 16 条。正常和断链场景共用该 CSV。断链用例丢弃 600、700、800、900、1000 毫秒的五条心跳。最后一次断链前接收在 500 毫秒，设定 300 毫秒超时，因此预期在 800 毫秒告警。

| 实际调用 | 交付 / 丢失 | 观察结果 | 实际退出码 |
| --- | ---: | --- | ---: |
| 健康，参考实现 | 16 / 0 | 在线，无超时和恢复事件 | 0 |
| 断链，参考实现 | 11 / 5 | 800 毫秒超时，1100 毫秒恢复 | 0 |
| 断链，故意漏报的对照 | 11 / 5 | 观测为空，和预期事件不匹配，测试正确识别漏报 | 1 |

回放覆盖 0 到 1500 毫秒。重复运行断链场景，程序退出码和 JSON 内容一致。自动检查还覆盖截止时间相同及重复时间戳、空轨迹启动超时、乱序、非法时间、超过终点和未知场景版本。

实测证据：[报告摘要](../evidence/step_01/summary.json)、[健康](../evidence/step_01/healthy.json)、[断链](../evidence/step_01/dropout.json)、[漏报](../evidence/step_01/missed_alert.json)。三个报告保存了实际退出码与程序输出。摘要记录输入的 SHA-256 和边界用例。

## 复现命令

在项目根目录用 Windows PowerShell 运行。CMake 和 CTest 可通过 Visual Studio 自带工具路径调用。

~~~powershell
python tools/generate_heartbeat.py --check
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
python tests/verify_replay.py --binary build/Release/usv_replay_lab.exe --report-dir evidence/step_01
~~~

若想逐条观察退出码：

~~~powershell
$usvTrace = 'datasets/heartbeat_v1/heartbeat.csv'
$usvHealthy = 'datasets/heartbeat_v1/healthy.case'
$usvDropout = 'datasets/heartbeat_v1/dropout.case'
.\build\Release\usv_replay_lab.exe $usvTrace $usvHealthy
$LASTEXITCODE
.\build\Release\usv_replay_lab.exe $usvTrace $usvDropout
$LASTEXITCODE
.\build\Release\usv_replay_lab.exe $usvTrace $usvDropout ignore_timeout
$LASTEXITCODE
~~~

最后一条的退出码 `1` 是预期结果。CTest 的测试脚本识别这一结果，因此整组测试通过。Ubuntu 的构建方式见[项目入口](../README.md)。

## 已验证范围

- 2026-11-12 在 Windows x64 实编译并运行。使用 Visual Studio 2022 / MSVC 19.42.34435、Windows SDK 10.0.22621.0、CMake 3.29.5-msvc4、Python 3.12.7。CTest 组 `heartbeat_replay` 通过。
- 5 个 C++ 文件通过 clang-format 18.1.8 的 `--dry-run --Werror` 检查。按团队规范复核了目录、文件和标识符命名、头文件保护宏、包含方式和命名空间限定。
- 结论适用于合成事件上的通信状态逻辑。Ubuntu、套接字及串口集成、实艇时间与实际链路表现属于后续阶段的验收范围。
