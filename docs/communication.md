# UDP / TCP 动态通信库

`usv_comm` 是项目的 C++17 动态通信模块。Windows 生成 `usv_comm.dll` 和导入库 `usv_comm.lib`，Linux 构建目标生成 `libusv_comm.so`。UDP 和 TCP 客户端、服务端示例都通过公开头文件调用共享库。

~~~mermaid
flowchart LR
    A["UDP 或 TCP 客户端"] --> B["usv_comm 动态库"]
    B --> C["操作系统 IPv4 套接字"]
    C --> D["服务端进程"]
    D --> E["usv_comm 动态库"]
~~~

## 能力和约定

| 接口 | 行为 |
| --- | --- |
| `UDPSocket` | 绑定地址和端口、发送完整数据报、接收数据及来源地址/端口。支持二进制和零字节数据报，上限 65,507 字节。 |
| `TCPServer` | 绑定监听、查询系统分配的端口、按时限接受连接。每次 `accept` 返回独立拥有的 `TCPStream`。 |
| `TCPStream` | 连接、按帧发送、按帧接收、关闭。内部处理部分发送与接收。 |
| `CommunicationError` | 通过 `getCode()` 区分超时、关闭、非法参数、系统错误和帧过大。`what()` 提供诊断。 |

地址使用 IPv4 数字形式，如 `127.0.0.1`。绑定端口 `0` 由系统分配，再用 `localPort()` 获取。操作超时参数为正整数毫秒。一个操作的截止时间覆盖全部部分 I/O。每个对象独占套接字，允许移动，析构时释放资源。同一对象的调用由使用方串行化，不同对象可在不同线程中使用。

TCP 线格式为 **4 字节网络字节序的无符号长度 + 对应数量的载荷字节**，允许零长度，单帧上限 1 MiB。对端采用相同格式即可互通。多次发送与接收的分段由操作系统决定，程序按长度字段还原消息。

TCP 帧操作发生超时或 I/O 失败时关闭连接，避免后续调用把残缺帧当成新帧。帧过大等发送前参数检查失败保留连接。连接恢复由上层明确发起，失败的数据是否重发也由上层决定。

## 构建和运行

在包含 CMake 的 Visual Studio 开发者终端，从项目根目录执行：

~~~powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
~~~

UDP 在两个终端中依次运行：

~~~powershell
.\build\Release\udp_echo_server.exe 19001
.\build\Release\udp_echo_client.exe 19001
~~~

TCP 同样依次运行：

~~~powershell
.\build\Release\tcp_echo_server.exe 19002
.\build\Release\tcp_echo_client.exe 19002
~~~

示例只绑定本机 `127.0.0.1`。服务端回显一次消息后退出，等候时间有上限。自动测试使用端口 `0` 并从服务端输出读取实际端口。C++ 的 `network_loopback` 测试复用同一套公开接口检查更多收发情形。

Linux 使用同一个工程：

~~~sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
./build/udp_echo_server 19001
~~~

## 安装后供其他工程使用

~~~powershell
cmake --install build --config Release --prefix build/stage
cmake -S tests/consumer -B build/my_consumer -G "Visual Studio 17 2022" -A x64 -DCMAKE_PREFIX_PATH="$PWD/build/stage"
cmake --build build/my_consumer --config Release
ctest --test-dir build/my_consumer -C Release --output-on-failure
~~~

最小消费工程使用以下 CMake 接口：

~~~cmake
find_package(usv_comm 0.2 CONFIG REQUIRED)
add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE usv_comm::usv_comm)
~~~

在 C++ 中包含 `<usv_comm/communication.h>`。Windows 运行时将对应 DLL 放在可执行程序旁，独立消费示例已安排复制。C++ 接口采用标准库类型，调用方与库须使用兼容的编译器、运行库、架构和构建配置。该版本采用同工具链的 C++ ABI。

## 验证与演进

`network_loopback` 检查库接口，`examples_and_wire` 检查独立进程互通及 TCP 线格式，`installed_consumer` 将库安装后再构建和运行消费工程。原有 `heartbeat_replay` 保留离线时间模型的回归。

下一项工程工作是将实际接收到的设备消息转换为回放事件，形成“真实通信故障 → 可分享事件 → 离线回归”的接入案例。当前库适合设备通信和培训中的小型同步流程，DNS、IPv6、TLS、异步事件循环和实艇无线链路表现可按实际需求单独设计和验证。
