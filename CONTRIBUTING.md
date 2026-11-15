# C++ 贡献指南

项目的 C++ 代码、示例和测试统一执行团队《无人艇软件开发组 C++ 程序编码规范》V0.01 第 3、4 章。项目自身用 C++17、CMake 和显式的场景契约组织实现。规范第 5 章的接口规则尚待补充，接口决定记录在设计文档中。

| 检查项 | 项目规则 |
| --- | --- |
| 工程和文件 | 工程名和代码目录、文件用小写下划线。`include/` 放头文件，`src/` 放源文件。普通头文件 `.h`，源文件 `.cpp`。引入模板实现文件时用 `include/impl/*.hpp`。提供 `CMakeLists.txt` 和记载架构、版本、构建及依赖的 `Readme.txt`。 |
| 头文件引用 | 同目录双引号，跨目录尖括号。保护宏由 `include/` 下相对路径形成，例如 `USV_REPLAY_LAB_REPLAY_H_`。宏全大写，末尾有下划线，`#endif` 行注释对应宏。 |
| 命名空间和函数 | 命名空间用小写下划线，头文件顶格声明。实现文件的项目函数和成员定义带命名空间限定。禁用 `using namespace`。标准要求全局定义的 `main` 保持全局。函数用小驼峰。 |
| 类型和变量 | 类、结构体大驼峰，缩写大写。普通变量用下划线分隔。形参、函数局部量以下划线开头，成员以下划线结尾，容器保留类型后缀，例如 `_heartbeat_ms_vector`、`event_vector_`。命名常量全大写。局部前缀后用小写字母，避开 C++ 保留标识符。 |
| 花括号和缩进 | Allman 风格，语句块花括号另起一行，单语句分支也使用花括号。缩进 4 个空格。访问修饰符与类声明同级。 |

提交前运行项目构建和 CTest。使用项目的 `.clang-format` 检查所有自有 C++ 文件。当前配置经 clang-format 18.1.8 验证。

从项目根目录扫描库、示例、测试和独立消费工程，避免只检查早期回放文件。

PowerShell：

~~~powershell
$usvCppFiles = @(rg --files -g '*.cpp' -g '*.h' include src tests/consumer)
clang-format --style=file --dry-run --Werror @usvCppFiles
~~~

Linux：

~~~sh
rg --files -g '*.cpp' -g '*.h' include src tests/consumer | xargs clang-format --style=file --dry-run --Werror
~~~

本项目文件名不含空格；新增文件同样遵守小写下划线规则。独立消费工程的源码位于它自己的 `src/`，并提供 `CMakeLists.txt` 和 `Readme.txt`。示例辅助头文件放在 `include/usv_comm_examples/`，不随通信 SDK 安装。

| 复核方式 | 覆盖内容 |
| --- | --- |
| clang-format 自动检查 | 缩进、访问修饰符位置、花括号、换行、禁止短控制块与单行 lambda。 |
| 构建与 CTest 自动检查 | 公开头文件可编译；离线输入和报告契约；UDP/TCP 回环；客户端/服务端进程；安装后独立查找、链接并运行。 |
| 人工代码复核 | 文件/变量/函数命名、容器类型后缀、头文件保护宏、同目录与跨目录包含规则、实现函数的命名空间限定、禁用 `using namespace`、避开保留标识符。 |
| 人工设计复核 | 超时和错误契约、线程与资源所有权、共享库 ABI 约束、测试平台和真实采用证据是否与报告相符。 |

格式检查通过不能替代命名与接口复核。扩充用例时提交输入、固定预期和复现命令，并用真实运行结果更新对应报告。当前完整测试名称为 `heartbeat_replay`、`network_loopback`、`examples_and_wire`、`installed_consumer`；套接字测试仅使用本机回环。公开 SDK 变更同时检查独立消费工程。当前 MIT 许可证覆盖项目原创代码，不能据此转授第三方附件或实际日志的权利。
