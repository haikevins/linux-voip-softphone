#include "core/CallState.hpp"

namespace voip {

std::string_view to_string(const CallState state) noexcept
{
    switch (state) {
    case CallState::Idle:
        return "Idle";
    case CallState::Registering:
        return "Registering";
    case CallState::Ready:
        return "Ready";
    case CallState::Calling:
        return "Calling";
    case CallState::Ringing:
        return "Ringing";
    case CallState::Connected:
        return "Connected";
    case CallState::Terminating:
        return "Terminating";
    case CallState::Failed:
        return "Failed";
    }

    return "Unknown";
}

bool can_transition(const StateTransition transition) noexcept
{
    const auto from = transition.from;
    const auto to = transition.to;

    switch (from) {
    case CallState::Idle:
        return to == CallState::Registering;
    case CallState::Registering:
        return to == CallState::Ready || to == CallState::Failed;
    case CallState::Ready:
        return to == CallState::Calling || to == CallState::Ringing || to == CallState::Registering;
    case CallState::Calling:
        return to == CallState::Ringing || to == CallState::Connected ||
               to == CallState::Terminating || to == CallState::Failed;
    case CallState::Ringing:
        return to == CallState::Connected || to == CallState::Terminating ||
               to == CallState::Failed;
    case CallState::Connected:
        return to == CallState::Terminating || to == CallState::Failed;
    case CallState::Terminating:
        return to == CallState::Ready || to == CallState::Failed;
    case CallState::Failed:
        return to == CallState::Idle || to == CallState::Registering;
    }

    return false;
}

} // namespace voip
