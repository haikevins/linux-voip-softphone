#include "application/CallManager.hpp"

#include "core/Event.hpp"

#include <algorithm>
#include <cassert>
#include <utility>

namespace voip::application {
namespace {

[[nodiscard]] bool is_dialable_destination(const std::string_view destination)
{
    return !destination.empty() && destination.size() <= 64U &&
           std::all_of(destination.begin(), destination.end(), [](const char digit) {
               return (digit >= '0' && digit <= '9') || digit == '*' || digit == '#' ||
                      digit == '+';
           });
}

} // namespace

CallManager::CallManager(CallBackend *const backend)
    : owner_thread_(std::this_thread::get_id()), backend_(backend), machine_(sink_)
{}

void CallManager::check_thread() const
{
    assert(std::this_thread::get_id() == owner_thread_ && "CallManager accessed off owner thread");
}

CallSnapshot CallManager::snapshot() const
{
    check_thread();
    const auto &machine_error = machine_.last_error();
    return {.state = machine_.state(),
            .peer = machine_.peer().value_or(std::string{}),
            .error = !feedback_.empty() ? feedback_ : machine_error.value_or(std::string{}),
            .backend_available = backend_ != nullptr,
            .call_actions_enabled = backend_ != nullptr && backend_->supports_calls() &&
                                    machine_.state() == CallState::Ready};
}

void CallManager::set_observer(Observer observer)
{
    check_thread();
    observer_ = std::move(observer);
}

void CallManager::publish() const
{
    if (observer_) {
        observer_(snapshot());
    }
}

bool CallManager::reject(std::string message)
{
    feedback_ = std::move(message);
    publish();
    return false;
}

bool CallManager::request_registration()
{
    check_thread();
    if (backend_ == nullptr) {
        return reject("SIP backend is not connected");
    }
    if (machine_.state() != CallState::Idle) {
        return reject("Registration is not available in the current state");
    }
    feedback_.clear();
    if (!machine_.handle(RegisterRequested{})) {
        return reject("Registration state transition was rejected");
    }
    publish();

    // No backend is allowed to synchronously call back into this facade.
    bool accepted = false;
    try {
        accepted = backend_->begin_registration();
    } catch (...) {
        // Do not expose exception text: a SIP library may include credentials.
        accepted = false;
    }
    if (!accepted) {
        static_cast<void>(on_registration_failed("SIP backend rejected the registration request"));
        return false;
    }
    return true;
}

bool CallManager::request_outgoing_call(const std::string &destination)
{
    check_thread();
    if (backend_ == nullptr) {
        return reject("SIP backend is not connected");
    }
    if (machine_.state() != CallState::Ready) {
        return reject("Calls require a confirmed registration");
    }
    if (!backend_->supports_calls()) {
        return reject("Calling is not available in this backend");
    }
    if (!is_dialable_destination(destination)) {
        return reject("Destination must contain 1-64 dialable characters");
    }
    feedback_.clear();
    if (!machine_.handle(OutgoingCallRequested{destination})) {
        return reject("Call state transition was rejected");
    }
    publish();

    bool accepted = false;
    try {
        accepted = backend_->begin_outgoing_call(destination);
    } catch (...) {
        // Prevent backend exception details from reaching GUI-facing state.
        accepted = false;
    }
    if (!accepted) {
        static_cast<void>(on_call_failed("SIP backend rejected the outgoing call request"));
        return false;
    }
    return true;
}

bool CallManager::request_hangup()
{
    check_thread();
    if (backend_ == nullptr) {
        return reject("SIP backend is not connected");
    }
    if (machine_.state() != CallState::Calling && machine_.state() != CallState::Ringing &&
        machine_.state() != CallState::Connected) {
        return reject("There is no active call to hang up");
    }
    feedback_.clear();
    if (!machine_.handle(HangupRequested{})) {
        return reject("Hangup state transition was rejected");
    }
    publish();

    bool accepted = false;
    try {
        accepted = backend_->begin_hangup();
    } catch (...) {
        // Prevent backend exception details from reaching GUI-facing state.
        accepted = false;
    }
    if (!accepted) {
        static_cast<void>(on_call_failed("SIP backend rejected the hangup request"));
        return false;
    }
    return true;
}

bool CallManager::reset_after_failure()
{
    check_thread();
    if (machine_.state() != CallState::Failed || !machine_.handle(ResetRequested{})) {
        return false;
    }
    feedback_.clear();
    publish();
    return true;
}

bool CallManager::on_registration_succeeded()
{
    check_thread();
    if (machine_.state() != CallState::Registering || !machine_.handle(RegistrationSucceeded{})) {
        return false;
    }
    feedback_.clear();
    publish();
    return true;
}

bool CallManager::on_registration_failed(std::string reason)
{
    check_thread();
    if (machine_.state() != CallState::Registering ||
        !machine_.handle(RegistrationFailed{std::move(reason)})) {
        return false;
    }
    feedback_.clear();
    publish();
    return true;
}

bool CallManager::on_incoming_call(std::string peer)
{
    check_thread();
    if (machine_.state() != CallState::Ready || peer.empty() ||
        !machine_.handle(IncomingCallReceived{std::move(peer)})) {
        return false;
    }
    feedback_.clear();
    publish();
    return true;
}

bool CallManager::on_remote_ringing()
{
    check_thread();
    if (machine_.state() != CallState::Calling || !machine_.handle(RemoteRinging{})) {
        return false;
    }
    publish();
    return true;
}

bool CallManager::on_call_connected()
{
    check_thread();
    if ((machine_.state() != CallState::Calling && machine_.state() != CallState::Ringing) ||
        !machine_.handle(CallConnected{})) {
        return false;
    }
    publish();
    return true;
}

bool CallManager::on_call_ended()
{
    check_thread();
    const auto state = machine_.state();
    if (state == CallState::Calling || state == CallState::Ringing ||
        state == CallState::Connected) {
        if (!machine_.handle(HangupRequested{})) {
            return false;
        }
    } else if (state != CallState::Terminating) {
        return false;
    }
    if (!machine_.handle(CallEnded{})) {
        return false;
    }
    feedback_.clear();
    publish(); // Peer is now cleared; observers never see an intermediate state.
    return true;
}

bool CallManager::on_call_failed(std::string reason)
{
    check_thread();
    if (machine_.state() != CallState::Calling && machine_.state() != CallState::Ringing &&
        machine_.state() != CallState::Connected && machine_.state() != CallState::Terminating) {
        return false;
    }
    if (!machine_.handle(CallFailed{std::move(reason)})) {
        return false;
    }
    feedback_.clear();
    publish();
    return true;
}

} // namespace voip::application
