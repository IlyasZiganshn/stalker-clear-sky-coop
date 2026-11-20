add_library(coopnet STATIC
    src/bitstream.cpp
    src/quantize.cpp
    src/ack.cpp
    src/wire.cpp
    src/reliable.cpp
    src/fragment.cpp
    src/session.cpp
    src/handshake.cpp
    src/net.cpp
    src/peer.cpp
    src/server.cpp
    src/snapshot.cpp
    src/snapshot_store.cpp
    src/input.cpp
    src/interp.cpp
    src/prediction.cpp
    src/delta.cpp
    src/baseline.cpp
    src/snapshot_sender.cpp
    src/ids.cpp
    src/interest.cpp
    src/replicator.cpp
    src/movement.cpp
    src/client_predictor.cpp
    src/actor.cpp
    src/app_message.cpp
    src/player_state.cpp
    src/world_info.cpp
)
target_include_directories(coopnet PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
if(WIN32)
    target_link_libraries(coopnet PUBLIC ws2_32)
endif()

add_executable(coopnet_tests
    tests/main.cpp
    tests/test_bitstream.cpp
    tests/test_quantize.cpp
    tests/test_wire.cpp
    tests/test_reliable.cpp
    tests/test_fragment.cpp
    tests/test_session.cpp
    tests/test_handshake.cpp
    tests/test_net.cpp
    tests/test_peer.cpp
    tests/test_server.cpp
    tests/test_snapshot.cpp
    tests/test_snapshot_store.cpp
    tests/test_input.cpp
    tests/test_interp.cpp
    tests/test_prediction.cpp
    tests/test_delta.cpp
    tests/test_baseline.cpp
    tests/test_snapshot_sender.cpp
    tests/test_world.cpp
    tests/test_ids.cpp
    tests/test_interest.cpp
    tests/test_replicator.cpp
    tests/test_movement.cpp
    tests/test_actor.cpp
    tests/test_app_message.cpp
    tests/test_player_state.cpp
    tests/test_world_info.cpp
)
target_link_libraries(coopnet_tests PRIVATE coopnet Threads::Threads)
target_include_directories(coopnet_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/tests)

add_test(NAME coopnet_tests COMMAND coopnet_tests)