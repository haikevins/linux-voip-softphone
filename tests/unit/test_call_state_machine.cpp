#include "core/CallStateMachine.hpp"
#include "core/Event.hpp"
#include "core/StateChangeSink.hpp"

#include <gtest/gtest.h>

#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace {

class RecordingSink final : public voip::StateChangeSink
{
  public:
    void on_state_changed(const voip::StateTransition transition) override
    {
        changes.push_back(transition);
    }

    std::vector<voip::StateTransition> changes;
};

voip::CallStateMachine make_ready_machine(RecordingSink &sink)
{
    voip::CallStateMachine machine{sink};
    EXPECT_TRUE(machine.handle(voip::RegisterRequested{}));
    EXPECT_TRUE(machine.handle(voip::RegistrationSucceeded{}));
    return machine;
}

} // namespace

TEST(CallStateMachineTest, StartsIdleWithoutPeerOrError)
{
    RecordingSink sink;
    const voip::CallStateMachine machine{sink};

    EXPECT_EQ(machine.state(), voip::CallState::Idle);
    EXPECT_FALSE(machine.peer().has_value());
    EXPECT_FALSE(machine.last_error().has_value());
    ASSERT_EQ(machine.history().size(), 1U);
    EXPECT_EQ(machine.history().front(), voip::CallState::Idle);
}

TEST(CallStateMachineTest, HandlesOutgoingCallLifecycle)
{
    RecordingSink sink;
    auto machine = make_ready_machine(sink);

    EXPECT_TRUE(machine.handle(voip::OutgoingCallRequested{"1002"}));
    EXPECT_EQ(machine.peer(), std::optional<std::string>{"1002"});

    EXPECT_TRUE(machine.handle(voip::RemoteRinging{}));
    EXPECT_TRUE(machine.handle(voip::CallConnected{}));
    EXPECT_TRUE(machine.handle(voip::HangupRequested{}));
    EXPECT_TRUE(machine.handle(voip::CallEnded{}));

    EXPECT_EQ(machine.state(), voip::CallState::Ready);
    EXPECT_FALSE(machine.peer().has_value());
    EXPECT_EQ(sink.changes.size(), 7U);
}

TEST(CallStateMachineTest, RejectsInvalidEventWithoutChangingState)
{
    RecordingSink sink;
    voip::CallStateMachine machine{sink};

    EXPECT_FALSE(machine.handle(voip::CallConnected{}));
    EXPECT_EQ(machine.state(), voip::CallState::Idle);
    EXPECT_TRUE(sink.changes.empty());
}

TEST(CallStateMachineTest, StoresFailureReasonAndCanReset)
{
    RecordingSink sink;
    voip::CallStateMachine machine{sink};

    EXPECT_TRUE(machine.handle(voip::RegisterRequested{}));
    EXPECT_TRUE(machine.handle(voip::RegistrationFailed{"authentication failed"}));
    EXPECT_EQ(machine.last_error(), std::optional<std::string>{"authentication failed"});
    EXPECT_EQ(machine.state(), voip::CallState::Failed);

    EXPECT_TRUE(machine.handle(voip::ResetRequested{}));
    EXPECT_EQ(machine.state(), voip::CallState::Idle);
    EXPECT_FALSE(machine.last_error().has_value());
}

TEST(CallStateMachineTest, HandlesIncomingCallFromReadyState)
{
    RecordingSink sink;
    auto machine = make_ready_machine(sink);

    EXPECT_TRUE(machine.handle(voip::IncomingCallReceived{"1002"}));
    EXPECT_EQ(machine.state(), voip::CallState::Ringing);
    EXPECT_EQ(machine.peer(), std::optional<std::string>{"1002"});
}
