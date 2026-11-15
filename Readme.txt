USV Replay Lab
版本：0.2.0（本地开发版）

代码架构
  include/usv_replay_lab/    回放核心、场景与报告声明
  include/usv_comm/          可安装的 UDP/TCP 公开接口与导出宏
  include/usv_comm_examples/ 示例参数解析头文件，不属于安装 SDK
  src/comm/                 动态通信库实现
  src/examples/comm/        UDP/TCP 客户端及服务端，共四个程序
  src/tests/comm/           动态库回环回归测试
  src/replay.cpp            虚拟时间上的心跳超时与恢复
  src/case_io.cpp           CSV / .case 输入校验与 JSON 输出
  src/main.cpp              命令行与退出码
  datasets/heartbeat_v1/    可再生的合成数据及场景
  tests/                    对编译后程序进行端到端检查
  tests/consumer/           独立 find_package 消费工程
  cmake/                    包配置与安装消费验证
  .github/workflows/        两平台 CI 配置，执行结果见仓库 Actions 实际记录
  build/                    CMake 生成的构建目录

版本更新
  0.2.0：增加 UDP/TCP 动态库、四个示例、CMake 安装包。
         支持 IPv4、限时 I/O、二进制/空消息、TCP 长度分帧与 RAII 资源管理。
         增加回环收发、独立进程及安装后链接运行测试。
  0.1.0：离线心跳回放、固定丢失区间、超时和恢复断言。
         提供教学对照、可重复输入、JSON 报告及 CTest。

依赖环境
  CMake 3.20 或更新版本，支持 C++17 的编译器。
  启用 CTest 或生成数据时需要 Python 3.8 或更新版本。
  通信库使用标准库与系统套接字，Windows 由 CMake 链接 Winsock。
  Qt、ROS、外部数据集均不是当前构建依赖。
  已实测：Windows x64 / MSVC 19.42 / CMake 3.29.5-msvc4，
          WSL Ubuntu 26.04.1 / GCC 15.2.0。
  两平台 CTest 均通过 heartbeat_replay、network_loopback、
  examples_and_wire、installed_consumer 四组测试，细节见通信验证记录。
  原培训目标 Ubuntu 24.04 已列入 CI 矩阵，但没有该版本的实测记录。

Windows 构建
  cmake -S . -B build -G "Visual Studio 17 2022" -A x64
  cmake --build build --config Release
  ctest --test-dir build -C Release --output-on-failure

Ubuntu 构建
  cmake -S . -B build_linux -DCMAKE_BUILD_TYPE=Release
  cmake --build build_linux --parallel
  ctest --test-dir build_linux --output-on-failure

动态库产物与使用
  Windows: build/Release/usv_comm.dll 及 usv_comm.lib。
  Linux: build_linux/libusv_comm.so（实际版本文件与链接由 CMake 生成）。
  cmake --install build --config Release --prefix build/stage
  其他 CMake 工程 find_package(usv_comm 0.2 CONFIG REQUIRED)，
  target_link_libraries(my_app PRIVATE usv_comm::usv_comm)。
  Windows 调用方须能找到 DLL；库和调用方使用兼容 C++ 工具链与运行库。
  运行示例、独立安装工程和接口契约见 docs/communication.md。

验证边界
  离线合成数据验证确定性时间逻辑；实际回环套接字验证软件通信。
  尚未接入实艇或验证无线、电气、传感器与控制系统。
  独立采用和长期维护记录待真实发生后补充。
  当前原创代码许可为 MIT，见 LICENSE；原始培训附件不在发布范围。

数据集与实测记录见 README.md、docs/step_01.md 和 docs/communication_validation.md。
