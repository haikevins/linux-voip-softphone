#include "core/CallStateMachine.hpp"

#include <type_traits>
#include <utility>

namespace voip {
namespace {

template <typename... Visitors> struct Overloaded : Visitors...
{
    using Visitors::operator()...;
};

template <typename... Visitors> Overloaded(Visitors...) -> Overloaded<Visitors...>;

} // namespace

CallStateMachine::CallStateMachine(StateChangeSink &sink) : sink_(sink) {}

CallState CallStateMachine::state() const noexcept
{
    return state_;
}

const std::optional<std::string> &CallStateMachine::peer() const noexcept
{
    return peer_;
}

const std::optional<std::string> &CallStateMachine::last_error() const noexcept
{
    return last_error_;
}

const std::vector<CallState> &CallStateMachine::history() const noexcept
{
    return history_;
}

bool CallStateMachine::handle(const Event &event)
{
    return std::visit(
        Overloaded{
            [this](const RegisterRequested &) {
                last_error_.reset();
                return transition_to(CallState::Registering);
            },
            [this](const RegistrationSucceeded &) { return transition_to(CallState::Ready); },
            [this](const RegistrationFailed &failed) {
                last_error_ = failed.reason;
                return transition_to(CallState::Failed);
            },
            [this](const OutgoingCallRequested &requested) {
                const auto previous_peer = peer_;
                peer_ = requested.destination;
                if (transition_to(CallState::Calling)) {
                    return true;
                }
                peer_ = previous_peer;
                return false;
            },
            [this](const IncomingCallReceived &incoming) {
                const auto previous_peer = peer_;
                peer_ = incoming.peer;
                if (transition_to(CallState::Ringing)) {
                    return true;
                }
                peer_ = previous_peer;
                return false;
            },
            [this](const RemoteRinging &) { return transition_to(CallState::Ringing); },
            [this](const CallConnected &) { return transition_to(CallState::Connected); },
            [this](const HangupRequested &) { return transition_to(CallState::Terminating); },
            [this](const CallEnded &) {
                if (!transition_to(CallState::Ready)) {
                    return false;
                }
                peer_.reset();
                return true;
            },
            [this](const CallFailed &failed) {
                last_error_ = failed.reason;
                return transition_to(CallState::Failed);
            },
            [this](const ResetRequested &) {
                if (!transition_to(CallState::Idle)) {
                    return false;
                }
                peer_.reset();
                last_error_.reset();
                return true;
            }},
        event);
}

bool CallStateMachine::transition_to(const CallState next)
{
    const StateTransition transition{
        .from = state_,
        .to = next,
    };

    if (!can_transition(transition)) {
        return false;
    }

    state_ = next;
    history_.push_back(state_);
    sink_.on_state_changed(transition);

    return true;
}

} // namespace voip
