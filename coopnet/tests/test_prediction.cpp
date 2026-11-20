#include "coopnet/prediction.h"

#include <cstddef>
#include <vector>

#include "test_framework.h"

using coopnet::InputHistory;
using coopnet::InputState;

namespace {

InputState inputAt(std::uint32_t tick) {
    InputState input;
    input.tick = tick;
    input.moveY = 1.0f;
    return input;
}

}  // namespace

COOP_TEST(prediction_history_keeps_unacknowledged_inputs) {
    InputHistory history;
    CHECK(history.record(inputAt(1)));
    CHECK(history.record(inputAt(2)));
    CHECK(history.record(inputAt(3)));
    CHECK_EQ(history.size(), std::size_t{3});

    CHECK_EQ(history.acknowledge(2), std::size_t{2});
    CHECK_EQ(history.size(), std::size_t{1});
    CHECK_EQ(history.pending().front().tick, 3u);

    CHECK_EQ(history.acknowledge(3), std::size_t{1});
    CHECK(history.pending().empty());
}

COOP_TEST(prediction_history_ignores_stale_ticks) {
    InputHistory history;
    CHECK(history.record(inputAt(5)));
    CHECK(!history.record(inputAt(5)));
    CHECK(!history.record(inputAt(4)));
    CHECK_EQ(history.size(), std::size_t{1});

    // A tick that was never recorded acknowledges nothing: the caller must
    // snap to the server state instead of replaying.
    CHECK_EQ(history.acknowledge(1), std::size_t{0});
    CHECK_EQ(history.size(), std::size_t{1});
}

COOP_TEST(prediction_history_replays_pending_inputs_in_order) {
    InputHistory history;
    history.record(inputAt(10));
    history.record(inputAt(11));
    history.record(inputAt(12));
    history.acknowledge(10);

    std::vector<std::uint32_t> replayed;
    std::vector<float> steps;
    history.replay(1.0f / 60.0f, [&](const InputState& input, float dt) {
        replayed.push_back(input.tick);
        steps.push_back(dt);
    });

    CHECK_EQ(replayed.size(), std::size_t{2});
    CHECK_EQ(replayed[0], 11u);
    CHECK_EQ(replayed[1], 12u);
    CHECK_EQ(steps.size(), std::size_t{2});
    CHECK(steps[0] > 0.016f && steps[0] < 0.017f);
}

COOP_TEST(prediction_history_is_bounded) {
    InputHistory history;
    for (std::size_t i = 0; i < InputHistory::kMaxPending + 20; ++i) {
        CHECK(history.record(inputAt(static_cast<std::uint32_t>(i + 1))));
    }
    CHECK_EQ(history.size(), InputHistory::kMaxPending);
    CHECK_EQ(history.pending().back().tick,
             static_cast<std::uint32_t>(InputHistory::kMaxPending + 20));

    history.clear();
    CHECK(history.pending().empty());
    // After a reset the tick sequence may start over.
    CHECK(history.record(inputAt(1)));
}