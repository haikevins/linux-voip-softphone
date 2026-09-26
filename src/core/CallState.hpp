#pragma once

#include <cstdint>
#include <string_view>

namespace voip {

enum class CallState : std::uint8_t
{
    Idle,
    Registering,
    Ready,
    Calling,
    Ringing,
    Connected,
    Terminating,
    Failed,
};

struct StateTransition
{
    CallState from;
    CallState to;
};

[[nodiscard]] std::string_view to_string(CallState state) noexcept;
[[nodiscard]] bool can_transition(StateTransition transition) noexcept;

} // namespace voip