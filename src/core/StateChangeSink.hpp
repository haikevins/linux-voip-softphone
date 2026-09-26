#pragma once

#include "core/CallState.hpp"

namespace voip {

class StateChangeSink
{
  public:
    virtual ~StateChangeSink() = default;

    virtual void on_state_changed(StateTransition transition) = 0;
};

class NullStateChangeSink final : public StateChangeSink
{
  public:
    void on_state_changed(StateTransition transition) override;
};

} // namespace voip
