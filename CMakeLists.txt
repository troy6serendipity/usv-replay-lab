cmake_minimum_required(VERSION 3.20)
project(usv_replay_lab VERSION 0.2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
find_package(Threads REQUIRED)

add_library(usv_comm SHARED src/comm/communication.cpp)
add_library(usv_comm::usv_comm ALIAS usv_comm)
target_compile_definitions(usv_comm PRIVATE USV_COMM_BUILD)
target_compile_features(usv_comm PUBLIC cxx_std_17)
target_include_directories(usv_comm PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
set_target_properties(usv_comm PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION 0
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN YES
)
if(WIN32)
    target_link_libraries(usv_comm PRIVATE ws2_32)
endif()

option(USV_BUILD_EXAMPLES "Build UDP and TCP client/server examples" ON)
if(USV_BUILD_EXAMPLES)
    foreach(example IN ITEMS udp_echo_server udp_echo_client tcp_echo_server tcp_echo_client)
        add_executable(${example} src/examples/comm/${example}.cpp)
        target_link_libraries(${example} PRIVATE usv_comm::usv_comm)
    endforeach()
endif()

install(TARGETS usv_comm EXPORT usv_comm_targets
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
install(DIRECTORY include/usv_comm DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(FILES LICENSE DESTINATION ${CMAKE_INSTALL_DATADIR}/licenses/usv_comm)
install(EXPORT usv_comm_targets
    FILE usv_comm_targets.cmake
    NAMESPACE usv_comm::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/usv_comm
)
configure_package_config_file(cmake/usv_comm_config.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/usv_commConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/usv_comm
)
write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/usv_commConfigVersion.cmake
    VERSION ${PROJECT_VERSION} COMPATIBILITY SameMinorVersion
)
install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/usv_commConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/usv_commConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/usv_comm
)

add_library(usv_replay_core STATIC src/replay.cpp)
target_include_directories(usv_replay_core PUBLIC include)

add_executable(usv_replay_lab src/main.cpp src/case_io.cpp)
target_link_libraries(usv_replay_lab PRIVATE usv_replay_core)

foreach(target IN ITEMS usv_comm usv_replay_core usv_replay_lab)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive-)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endforeach()

include(CTest)
if(BUILD_TESTING)
    find_package(Python3 3.8 REQUIRED COMPONENTS Interpreter)
    add_test(
        NAME heartbeat_replay
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/verify_replay.py"
            --binary "$<TARGET_FILE:usv_replay_lab>"
    )
    add_executable(network_loopback src/tests/comm/network_loopback.cpp)
    target_link_libraries(network_loopback PRIVATE usv_comm::usv_comm Threads::Threads)
    if(WIN32)
        target_link_libraries(network_loopback PRIVATE ws2_32)
    endif()
    add_test(NAME network_loopback COMMAND network_loopback)
    set_tests_properties(network_loopback PROPERTIES TIMEOUT 40)
    if(USV_BUILD_EXAMPLES)
        add_test(NAME examples_and_wire
            COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/verify_examples.py"
                --bin-dir "$<TARGET_FILE_DIR:tcp_echo_server>"
        )
        set_tests_properties(examples_and_wire PROPERTIES TIMEOUT 60)
    endif()
    add_test(NAME installed_consumer
        COMMAND ${CMAKE_COMMAND}
            "-DPROJECT_BUILD_DIR=${CMAKE_CURRENT_BINARY_DIR}"
            "-DCONSUMER_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}/tests/consumer"
            "-DTEST_CONFIG=$<CONFIG>"
            "-DTEST_GENERATOR=${CMAKE_GENERATOR}"
            "-DTEST_PLATFORM=${CMAKE_GENERATOR_PLATFORM}"
            "-DTEST_INSTANCE=${CMAKE_GENERATOR_INSTANCE}"
            -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/verify_package.cmake
    )
    set_tests_properties(installed_consumer PROPERTIES TIMEOUT 150)
endif()
