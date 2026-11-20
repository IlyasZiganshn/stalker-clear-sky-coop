add_library(coopnet STATIC
    src/bitstream.cpp
    src/quantize.cpp
    src/ack.cpp
    src/wire.cpp
    src/reliable.cpp
    src/fragment.cpp
    src/session.cpp
    src/handshake.cpp
)
target_include_directories(coopnet PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

add_executable(coopnet_tests
    tests/main.cpp
    tests/test_bitstream.cpp
    tests/test_quantize.cpp
    tests/test_wire.cpp
    tests/test_reliable.cpp
    tests/test_fragment.cpp
    tests/test_session.cpp
    tests/test_handshake.cpp
)
target_link_libraries(coopnet_tests PRIVATE coopnet)
target_include_directories(coopnet_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/tests)

add_test(NAME coopnet_tests COMMAND coopnet_tests)