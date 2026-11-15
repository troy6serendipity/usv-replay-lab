usv_comm_consumer
版本：0.2.0 配套消费示例

目的与架构
  本目录是独立 CMake 工程。src/main.cpp 仅包含已安装的公开头文件，
  通过 find_package(usv_comm CONFIG REQUIRED) 和 usv_comm::usv_comm 链接共享库。
  不引用通信库实现源码，不依赖主工程内的 add_subdirectory。
  Windows 构建后把已安装的 DLL 复制到消费程序旁以供运行时加载。

依赖
  CMake 3.20+、C++17 编译器、已经安装的 usv_comm 0.2.0。
  消费程序和库须使用兼容编译器、C++ 运行库、架构及构建配置。

从主项目根目录构建（Windows 开发者终端）
  cmake --install build --config Release --prefix build/stage
  cmake -S tests/consumer -B build/my_consumer -G "Visual Studio 17 2022" -A x64 -DCMAKE_PREFIX_PATH="%CD%/build/stage"
  cmake --build build/my_consumer --config Release
  ctest --test-dir build/my_consumer -C Release --output-on-failure
  上述路径表达式适用于 cmd；PowerShell 使用 "$PWD/build/stage"。

从主项目根目录构建（Linux）
  cmake --install build_linux --prefix "$PWD/build_linux/stage"
  cmake -S tests/consumer -B build_linux/my_consumer -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="$PWD/build_linux/stage"
  cmake --build build_linux/my_consumer
  ctest --test-dir build_linux/my_consumer --output-on-failure

验收
  installed_shared_library 实际运行成功，退出码为 0。
  主项目 CTest 中的 installed_consumer 会自动安装、配置、构建并运行本工程。
  本例只验证安装包可消费；更完整的 UDP/TCP 行为由主项目回归覆盖。
  0.2.0 首次提供此安装消费流程，实测记录见 ../../docs/communication_validation.md。
