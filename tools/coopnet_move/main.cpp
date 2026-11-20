#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <string>

#include "coopnet/client_predictor.h"
#include "coopnet/movement.h"

namespace {

constexpr float kDt = 1.0f / 60.0f;
constexpr std::uint32_t kLatencyTicks = 4;   // ~67 ms each way
constexpr std::uint32_t kSnapshotEvery = 3;  // 20 Hz against a 60 Hz tick
constexpr std::uint32_t kKnockbackTick = 90;
constexpr std::uint32_t kDefaultTicks = 600;

struct SnapshotMessage {
    coopnet::MoveState state;
    std::uint32_t ackInput = 0;
};

coopnet::InputState makeInput(std::uint32_t tick) {
    coopnet::InputState input;
    input.tick = tick;
    const std::uint32_t phase = tick % 240;
    if (phase < 120) {
        input.moveY = 1.0f;
        input.viewYaw = 0.0f;
    } else {
        input.moveY = 0.6f;
        input.moveX = 0.4f;
        input.viewYaw = 0.8f;
    }
    if (phase >= 60 && phase < 120) {
        input.buttons |= coopnet::kButtonSprint;
    }
    if (tick % 50 == 0) {
        input.buttons |= coopnet::kButtonJump;
    }
    return input;
}

bool parseTicks(const char* text, std::uint32_t& out) {
    char* end = nullptr;
    const unsigned long value = std::strtoul(text, &end, 10);
    if (end == text || *end != '\0' || value == 0 || value > 100000) {
        return false;
    }
    out = static_cast<std::uint32_t>(value);
    return true;
}

}  // namespace

int runLocal(std::uint32_t ticks) {
    const coopnet::MovementConfig config;
    coopnet::ClientPredictor client(config, kDt);

    coopnet::MoveState serverState;
    std::uint32_t lastAppliedInput = 0;
    std::map<std::uint32_t, coopnet::MoveState> serverAfterInput;
    std::map<std::uint32_t, coopnet::InputState> uplink;
    std::map<std::uint32_t, SnapshotMessage> downlink;

    std::uint32_t applied = 0;
    std::uint32_t dropped = 0;
    std::uint32_t corrections = 0;
    float maxCorrection = 0.0f;
    float maxPendingError = 0.0f;

    const std::uint32_t lastTick = ticks + kLatencyTicks + kSnapshotEvery + 2;
    for (std::uint32_t tick = 1; tick <= lastTick; ++tick) {
        if (auto it = downlink.find(tick); it != downlink.end()) {
            ++applied;
            const float error =
                client.reconcile(it->second.state, it->second.ackInput);
            if (error > 1e-4f) {
                ++corrections;
                maxCorrection = std::max(maxCorrection, error);
            }
            downlink.erase(it);
        }

        if (tick <= ticks) {
            const coopnet::InputState input = makeInput(tick);
            client.predict(input);
            uplink[tick + kLatencyTicks] = input;
        }

        if (auto it = uplink.find(tick); it != uplink.end()) {
            const coopnet::InputState& input = it->second;
            if (input.tick == kKnockbackTick) {
                // Something the client cannot predict: an explosion pushes
                // the actor. The next snapshot must correct it.
                serverState.velocity.x += 3.0f;
                serverState.velocity.y += 2.0f;
                serverState.onGround = false;
            }
            coopnet::stepMovement(serverState, input, config, kDt);
            lastAppliedInput = input.tick;
            serverAfterInput[input.tick] = serverState;
            uplink.erase(it);
        }

        const bool snapshotDue = tick % kSnapshotEvery == 0;
        if (snapshotDue && tick <= ticks) {
            const bool lose = tick % 21 == 0 && tick != ticks;
            if (lose) {
                ++dropped;
            } else {
                downlink[tick + kLatencyTicks] = SnapshotMessage{
                    serverState, lastAppliedInput};
            }
        }
    }

    const auto expected = serverAfterInput.find(ticks);
    if (expected == serverAfterInput.end()) {
        std::printf("move: server never applied input %u\n", ticks);
        return 1;
    }
    const coopnet::Vec3& finalClient = client.state().position;
    const coopnet::Vec3& finalServer = expected->second.position;
    const float dx = finalClient.x - finalServer.x;
    const float dy = finalClient.y - finalServer.y;
    const float dz = finalClient.z - finalServer.z;
    const float finalError = std::sqrt(dx * dx + dy * dy + dz * dz);
    maxPendingError = std::max(maxPendingError, finalError);

    std::printf(
        "move: %u ticks (%.1f s), latency %u ticks, snapshots %u applied, "
        "%u lost, inputs applied %u, corrections %u (max %.3f m), "
        "final error %.4f m\n",
        ticks, ticks * kDt, kLatencyTicks, applied, dropped, lastAppliedInput,
        corrections, maxCorrection, finalError);

    const bool ok = finalError < 0.02f && corrections >= 1 && applied > 0 &&
                    dropped >= 1 && lastAppliedInput == ticks;
    std::printf("coopnet_move: %s\n", ok ? "PASSED" : "FAILED");
    return ok ? 0 : 1;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::printf("usage:\n  coopnet_move local [ticks]\n");
        return 2;
    }
    const std::string mode = argv[1];
    if (mode != "local") {
        std::printf("coopnet_move: unknown mode '%s'\n", mode.c_str());
        return 2;
    }
    std::uint32_t ticks = kDefaultTicks;
    if (argc > 2 && !parseTicks(argv[2], ticks)) {
        std::printf("coopnet_move: bad tick count\n");
        return 2;
    }
    return runLocal(ticks);
}