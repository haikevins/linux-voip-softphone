#pragma once

#include "core/CallStateMachine.hpp"
#include "core/StateChangeSink.hpp"

#include <functional>
#include <string>
#include <string_view>
#include <thread>

namespace voip::application {

// Backend requests are non-blocking from the CallManager's point of view.
// Completion events must be dispatched back to the CallManager's owning thread;
// a true return value means only that the backend accepted the request.
class CallBackend
{
  public:
    virtual ~CallBackend() = default;

    [[nodiscard]] virtual bool begin_registration() = 0;
    // A registration-only adapter must never advertise a callable dialer.
    [[nodiscard]] virtual bool supports_calls() const noexcept
    {
        return true;
    }
    [[nodiscard]] virtual bool begin_outgoing_call(std::string_view destination) = 0;
    [[nodiscard]] virtual bool begin_answer()
    {
        return false;
    }
    [[nodiscard]] virtual bool begin_hangup() = 0;
};

struct CallSnapshot
{
    CallState state{CallState::Idle};
    std::string peer;
    std::string error;
    bool backend_available{false};
    bool call_actions_enabled{false};
    bool registration_confirmed{false};
    bool recovery_actions_enabled{false};
    bool incoming_call{false};
    bool answer_actions_enabled{false};
};

// Qt-free application facade, reusing the core CallStateMachine. The backend
// is borrowed and must outlive CallManager. All methods and notifications
// are confined to the constructing thread; no worker-thread calls are safe.
class CallManager final
{
  public:
    using Observer = std::function<void(const CallSnapshot &)>;

    explicit CallManager(CallBackend *backend = nullptr);

    CallManager(const CallManager &) = delete;
    CallManager &operator=(const CallManager &) = delete;
    CallManager(CallManager &&) = delete;
    CallManager &operator=(CallManager &&) = delete;

    [[nodiscard]] CallSnapshot snapshot() const;
    void set_observer(Observer observer);

    [[nodiscard]] bool request_registration();
    [[nodiscard]] bool request_outgoing_call(const std::string &destination);
    [[nodiscard]] bool request_answer();
    [[nodiscard]] bool request_hangup();
    [[nodiscard]] bool reset_after_failure();

    // SIP adapters post only sanitized lifecycle events on the owner's event
    // loop (Qt::QueuedConnection for callbacks originating off the UI thread).
    [[nodiscard]] bool on_registration_succeeded();
    [[nodiscard]] bool on_registration_failed(std::string reason);
    [[nodiscard]] bool on_incoming_call(std::string peer);
    [[nodiscard]] bool on_remote_ringing();
    [[nodiscard]] bool on_call_connected();
    [[nodiscard]] bool on_call_ended();
    [[nodiscard]] bool on_call_failed(std::string reason);

  private:
    void check_thread() const;
    [[nodiscard]] bool reject(std::string message);
    void publish() const;
    void clear_call_direction() noexcept;

    const std::thread::id owner_thread_;
    CallBackend *const backend_;
    NullStateChangeSink sink_;
    CallStateMachine machine_;
    std::string feedback_;
    bool registration_confirmed_{false};
    bool incoming_call_{false};
    bool answer_pending_{false};
    Observer observer_;
};

} // namespace voip::application
