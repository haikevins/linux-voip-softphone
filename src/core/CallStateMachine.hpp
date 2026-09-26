#pragma once

#include "core/CallState.hpp"
#include "core/Event.hpp"
#include "core/StateChangeSink.hpp"

#include <optional>
#include <string>
#include <vector>

namespace voip {

class CallStateMachine
{
  public:
    explicit CallStateMachine(StateChangeSink &sink);

    [[nodiscard]] CallState state() const noexcept;
    [[nodiscard]] const std::optional<std::string> &peer() const noexcept;
    [[nodiscard]] const std::optional<std::string> &last_error() const noexcept;
    [[nodiscard]] const std::vector<CallState> &history() const noexcept;

    [[nodiscard]] bool handle(const Event &event);

  private:
    [[nodiscard]] bool transition_to(CallState next);

    StateChangeSink &sink_;
    CallState state_{CallState::Idle};
    std::optional<std::string> peer_;
    std::optional<std::string> last_error_;
    std::vector<CallState> history_{CallState::Idle};
};

} // namespace voip
