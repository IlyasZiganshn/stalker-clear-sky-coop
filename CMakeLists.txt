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
add_subdirectory(coopnet)

if(WIN32)
    add_executable(coopnet_selftest tools/coopnet_selftest/main.cpp)
    target_link_libraries(coopnet_selftest PRIVATE coopnet ws2_32)
    add_test(NAME coopnet_selftest COMMAND coopnet_selftest)
endif()

message(STATUS "clear_sky_coop: coopnet configured (engine build is separate)")