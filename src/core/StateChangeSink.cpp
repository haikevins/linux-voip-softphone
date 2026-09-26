#include "core/StateChangeSink.hpp"

namespace voip {

void NullStateChangeSink::on_state_changed(const StateTransition transition)
{
    static_cast<void>(transition);
}

} // namespace voip
