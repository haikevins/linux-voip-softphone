#pragma once

#include "application/SipEngine.hpp"
#include "core/CallStateMachine.hpp"

#include <optional>
#include <string>

namespace voip::application {

class CallManager final : public SipEngineObserver
{
  public:
    CallManager(SipEngine &engine, StateChangeSink &state_change_sink);
    ~CallManager() override;

    CallManager(const CallManager &) = delete;
    CallManager &operator=(const CallManager &) = delete;
    CallManager(CallManager &&) = delete;
    CallManager &operator=(CallManager &&) = delete;

    [[nodiscard]] CallState state() const noexcept;
    [[nodiscard]] const std::optional<std::string> &peer() const noexcept;
    [[nodiscard]] const std::optional<std::string> &last_error() const noexcept;

    [[nodiscard]] bool register_account();
    [[nodiscard]] bool make_call(const std::string &destination);
    [[nodiscard]] bool answer_call();
    [[nodiscard]] bool hangup_call();
    [[nodiscard]] bool reset();

    void on_registration_succeeded() override;
    void on_registration_failed(std::string reason) override;
    void on_incoming_call(std::string peer) override;
    void on_remote_ringing() override;
    void on_call_connected() override;
    void on_call_ended() override;
    void on_call_failed(std::string reason) override;

  private:
    [[nodiscard]] bool fail_registration(std::string reason);
    [[nodiscard]] bool fail_call(std::string reason);

    SipEngine &engine_;
    CallStateMachine state_machine_;
};

} // namespace voip::application
