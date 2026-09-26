#include "core/BuildInfo.hpp"
#include "core/CallStateMachine.hpp"
#include "core/Event.hpp"
#include "core/StateChangeSink.hpp"

#include <iostream>

namespace {

class ConsoleStateChangeSink final : public voip::StateChangeSink
{
  public:
    void on_state_changed(const voip::StateTransition transition) override
    {
        std::cout << "[STATE] " << voip::to_string(transition.from) << " -> "
                  << voip::to_string(transition.to) << '\n';
    }
};

} // namespace

int main()
{
    std::cout << voip::project_name() << '\n'
              << voip::current_phase() << '\n'
              << "Standard: " << voip::cpp_standard() << "\n\n";

    ConsoleStateChangeSink sink;
    voip::CallStateMachine machine{sink};

    static_cast<void>(machine.handle(voip::RegisterRequested{}));
    static_cast<void>(machine.handle(voip::RegistrationSucceeded{}));
    static_cast<void>(machine.handle(voip::OutgoingCallRequested{"1002"}));
    static_cast<void>(machine.handle(voip::RemoteRinging{}));
    static_cast<void>(machine.handle(voip::CallConnected{}));
    static_cast<void>(machine.handle(voip::HangupRequested{}));
    static_cast<void>(machine.handle(voip::CallEnded{}));

    std::cout << "\nFinal state: " << voip::to_string(machine.state()) << '\n';
    return 0;
}
