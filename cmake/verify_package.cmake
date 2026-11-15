if(NOT TEST_CONFIG)
    set(TEST_CONFIG Release)
endif()
set(stage "${PROJECT_BUILD_DIR}/consumer_test/${TEST_CONFIG}/stage")
set(consumer_build "${PROJECT_BUILD_DIR}/consumer_test/${TEST_CONFIG}/build")

execute_process(COMMAND "${CMAKE_COMMAND}" --install "${PROJECT_BUILD_DIR}"
    --config "${TEST_CONFIG}" --prefix "${stage}"
    RESULT_VARIABLE result)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "Installing the shared library failed: ${result}")
endif()

set(configure_args -S "${CONSUMER_SOURCE_DIR}" -B "${consumer_build}"
    -G "${TEST_GENERATOR}" "-DCMAKE_PREFIX_PATH=${stage}"
    "-DCMAKE_BUILD_TYPE=${TEST_CONFIG}")
if(TEST_PLATFORM)
    list(APPEND configure_args -A "${TEST_PLATFORM}")
endif()
if(TEST_INSTANCE)
    list(APPEND configure_args "-DCMAKE_GENERATOR_INSTANCE=${TEST_INSTANCE}")
endif()
execute_process(COMMAND "${CMAKE_COMMAND}" ${configure_args} RESULT_VARIABLE result)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "Configuring the independent consumer failed: ${result}")
endif()
execute_process(COMMAND "${CMAKE_COMMAND}" --build "${consumer_build}"
    --config "${TEST_CONFIG}" RESULT_VARIABLE result)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "Building the independent consumer failed: ${result}")
endif()
execute_process(COMMAND "${CMAKE_CTEST_COMMAND}" --test-dir "${consumer_build}"
    -C "${TEST_CONFIG}" --output-on-failure RESULT_VARIABLE result)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "Running the installed-library consumer failed: ${result}")
endif()
