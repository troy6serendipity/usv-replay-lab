cmake_minimum_required(VERSION 3.20)
project(usv_comm_consumer LANGUAGES CXX)

find_package(usv_comm CONFIG REQUIRED)
add_executable(usv_comm_consumer src/main.cpp)
target_compile_features(usv_comm_consumer PRIVATE cxx_std_17)
target_link_libraries(usv_comm_consumer PRIVATE usv_comm::usv_comm)

if(WIN32)
    add_custom_command(
        TARGET usv_comm_consumer POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "$<TARGET_FILE:usv_comm::usv_comm>" "$<TARGET_FILE_DIR:usv_comm_consumer>"
        VERBATIM
    )
endif()

enable_testing()
add_test(NAME installed_shared_library COMMAND usv_comm_consumer)
