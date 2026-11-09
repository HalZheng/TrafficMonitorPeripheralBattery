cmake_minimum_required(VERSION 3.20)

project(TrafficMonitorPeripheralBattery VERSION 1.2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    add_compile_options(/utf-8 /W3)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(PB_ARCH "x64")
else()
    set(PB_ARCH "x86")
endif()

set(PB_OUTPUT_DIR "${CMAKE_SOURCE_DIR}/bin/${PB_ARCH}")
set(PB_ARCHIVE_DIR "${CMAKE_SOURCE_DIR}/build/lib/${PB_ARCH}")

add_library(peripheral_core STATIC
    src/core/DeviceRegistry.cpp
    src/core/HidTransport.cpp
    src/core/DeviceManager.cpp
    src/protocols/ProtocolRegistry.cpp
    src/protocols/JuxingProtocol.cpp
    src/protocols/AulaBycomboProtocol.cpp
    src/protocols/FlydigiPipeProtocol.cpp
    src/util/PathUtil.cpp
)

target_include_directories(peripheral_core PUBLIC
    "${CMAKE_SOURCE_DIR}/include"
    "${CMAKE_SOURCE_DIR}/src"
)

target_compile_definitions(peripheral_core PUBLIC
    UNICODE
    _UNICODE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
)

target_link_libraries(peripheral_core PUBLIC setupapi hid)

add_library(PeripheralBatteryPlugin SHARED
    src/dllmain.cpp
    src/plugin/Plugin.cpp
    src/plugin/BatteryItem.cpp
    src/plugin/OptionsDialog.cpp
    src/plugin/Settings.cpp
)

target_link_libraries(PeripheralBatteryPlugin PRIVATE
    peripheral_core
    comdlg32
    shell32
)

set_target_properties(PeripheralBatteryPlugin PROPERTIES
    PREFIX ""
    OUTPUT_NAME "PeripheralBatteryPlugin"
)

add_executable(hid-probe
    tools/hid-probe/main.cpp
)

target_link_libraries(hid-probe PRIVATE peripheral_core)

add_executable(plugin-host-test
    tools/plugin-host-test/main.cpp
)

target_include_directories(plugin-host-test PRIVATE
    "${CMAKE_SOURCE_DIR}/include"
)

add_executable(protocol_tests
    tests/protocol_tests.cpp
)

target_link_libraries(protocol_tests PRIVATE peripheral_core)

enable_testing()
add_test(NAME protocol_tests COMMAND protocol_tests)

foreach(target PeripheralBatteryPlugin hid-probe plugin-host-test protocol_tests)
    set_target_properties(${target} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${PB_OUTPUT_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_RELEASE "${PB_OUTPUT_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_DEBUG "${PB_OUTPUT_DIR}"
        LIBRARY_OUTPUT_DIRECTORY "${PB_OUTPUT_DIR}"
        LIBRARY_OUTPUT_DIRECTORY_RELEASE "${PB_OUTPUT_DIR}"
        LIBRARY_OUTPUT_DIRECTORY_DEBUG "${PB_OUTPUT_DIR}"
        ARCHIVE_OUTPUT_DIRECTORY "${PB_ARCHIVE_DIR}"
    )
endforeach()
