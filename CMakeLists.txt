cmake_minimum_required(VERSION 3.25)
project(clear_sky_coop LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    add_compile_options(/W4 /permissive- /EHsc /utf-8)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# The engine lives outside this tree: it is a patched OpenXRay checkout, see
# engine/README.md. Only our own code is built here.
enable_testing()
find_package(Threads REQUIRED)
add_subdirectory(coopnet)

add_executable(coopnet_selftest tools/coopnet_selftest/main.cpp)
target_link_libraries(coopnet_selftest PRIVATE coopnet Threads::Threads)
add_test(NAME coopnet_selftest COMMAND coopnet_selftest)

add_executable(coopnet_echo tools/coopnet_echo/main.cpp)
target_link_libraries(coopnet_echo PRIVATE coopnet Threads::Threads)
add_test(NAME coopnet_echo COMMAND coopnet_echo local)
set_tests_properties(coopnet_echo PROPERTIES TIMEOUT 40)

add_executable(coopnet_snap tools/coopnet_snap/main.cpp)
target_link_libraries(coopnet_snap PRIVATE coopnet Threads::Threads)
add_test(NAME coopnet_snap COMMAND coopnet_snap local 3000)
set_tests_properties(coopnet_snap PROPERTIES TIMEOUT 60)

add_test(NAME coopnet_snap_two COMMAND coopnet_snap two 3000)
set_tests_properties(coopnet_snap_two PROPERTIES TIMEOUT 60)

add_executable(coopnet_move tools/coopnet_move/main.cpp)
target_link_libraries(coopnet_move PRIVATE coopnet Threads::Threads)
add_test(NAME coopnet_move COMMAND coopnet_move local 600)
set_tests_properties(coopnet_move PROPERTIES TIMEOUT 60)

add_test(NAME coopnet_move_two COMMAND coopnet_move two 3000)
set_tests_properties(coopnet_move_two PROPERTIES TIMEOUT 60)

add_executable(net_sim tools/net_sim/main.cpp)
target_link_libraries(net_sim PRIVATE coopnet Threads::Threads)

if(WIN32)
    add_test(NAME coopnet_snap_relay
        COMMAND powershell -NoProfile -ExecutionPolicy Bypass
            -File ${CMAKE_CURRENT_SOURCE_DIR}/scripts/test_relay.ps1
            -BuildDir ${CMAKE_BINARY_DIR})
    set_tests_properties(coopnet_snap_relay PROPERTIES TIMEOUT 60)
endif()

message(STATUS "clear_sky_coop: coopnet configured (engine build is separate)")