#include "application/CallManager.hpp"

#include "core/Event.hpp"

#include <exception>
#include <string_view>
#include <utility>

namespace voip::application {
namespace {

[[nodiscard]] std::string operation_failure(const std::string_view operation,
                                            const std::exception &error)
{
    return std::string{operation} + ": " + error.what();
}

[[nodiscard]] std::string unknown_operation_failure(const std::string_view operation)
{
    return std::string{operation} + ": unknown failure";
}

} // namespace

CallManager::CallManager(SipEngine &engine, StateChangeSink &state_change_sink)
    : engine_(engine), state_machine_(state_change_sink)
{
    engine_.set_observer(this);
}

CallManager::~CallManager()
{
    engine_.set_observer(nullptr);
}

CallState CallManager::state() const
{
    const std::lock_guard lock{state_mutex_};
    return state_machine_.state();
}

std::optional<std::string> CallManager::peer() const
{
    const std::lock_guard lock{state_mutex_};
    return state_machine_.peer();
}

std::optional<std::string> CallManager::last_error() const
{
    const std::lock_guard lock{state_mutex_};
    return state_machine_.last_error();
}

bool CallManager::register_account()
{
    {
        const std::lock_guard lock{state_mutex_};
        if (!state_machine_.handle(RegisterRequested{})) {
            return false;
        }
    }

    try {
        engine_.register_account();
        return true;
    } catch (const std::exception &error) {
        return fail_registration(operation_failure("register account", error));
    } catch (...) {
        return fail_registration(unknown_operation_failure("register account"));
    }
}

bool CallManager::make_call(const std::string &destination)
{
    if (destination.empty()) {
        return false;
    }

    {
        const std::lock_guard lock{state_mutex_};
        if (!state_machine_.handle(OutgoingCallRequested{destination})) {
            return false;
        }
    }

    try {
        engine_.make_call(destination);
        return true;
    } catch (const std::exception &error) {
        return fail_call(operation_failure("make call", error));
    } catch (...) {
        return fail_call(unknown_operation_failure("make call"));
    }
}

bool CallManager::answer_call()
{
    {
        const std::lock_guard lock{state_mutex_};
        if (state_machine_.state() != CallState::Ringing) {
            return false;
        }
    }

    try {
        engine_.answer_call();
        return true;
    } catch (const std::exception &error) {
        return fail_call(operation_failure("answer call", error));
    } catch (...) {
        return fail_call(unknown_operation_failure("answer call"));
    }
}

bool CallManager::hangup_call()
{
    {
        const std::lock_guard lock{state_mutex_};
        if (!state_machine_.handle(HangupRequested{})) {
            return false;
        }
    }

    try {
        engine_.hangup_call();
        return true;
    } catch (const std::exception &error) {
        return fail_call(operation_failure("hang up call", error));
    } catch (...) {
        return fail_call(unknown_operation_failure("hang up call"));
    }
}

bool CallManager::reset()
{
    const std::lock_guard lock{state_mutex_};
    return state_machine_.handle(ResetRequested{});
}

void CallManager::on_registration_succeeded()
{
    const std::lock_guard lock{state_mutex_};
    static_cast<void>(state_machine_.handle(RegistrationSucceeded{}));
}

void CallManager::on_registration_failed(std::string reason)
{
    const std::lock_guard lock{state_mutex_};
    static_cast<void>(state_machine_.handle(RegistrationFailed{std::move(reason)}));
}

void CallManager::on_incoming_call(std::string peer)
{
    const std::lock_guard lock{state_mutex_};
    static_cast<void>(state_machine_.handle(IncomingCallReceived{std::move(peer)}));
}

void CallManager::on_remote_ringing()
{
    const std::lock_guard lock{state_mutex_};
    static_cast<void>(state_machine_.handle(RemoteRinging{}));
}

void CallManager::on_call_connected()
{
    const std::lock_guard lock{state_mutex_};
    static_cast<void>(state_machine_.handle(CallConnected{}));
}

void CallManager::on_call_ended()
{
    const std::lock_guard lock{state_mutex_};
    const auto state = state_machine_.state();
    if (state == CallState::Calling || state == CallState::Ringing ||
        state == CallState::Connected) {
        static_cast<void>(state_machine_.handle(HangupRequested{}));
    }

    static_cast<void>(state_machine_.handle(CallEnded{}));
}

void CallManager::on_call_failed(std::string reason)
{
    const std::lock_guard lock{state_mutex_};
    static_cast<void>(state_machine_.handle(CallFailed{std::move(reason)}));
}

bool CallManager::fail_registration(std::string reason)
{
    const std::lock_guard lock{state_mutex_};
    static_cast<void>(state_machine_.handle(RegistrationFailed{std::move(reason)}));
    return false;
}

bool CallManager::fail_call(std::string reason)
{
    const std::lock_guard lock{state_mutex_};
    static_cast<void>(state_machine_.handle(CallFailed{std::move(reason)}));
    return false;
}

} // namespace voip::application
