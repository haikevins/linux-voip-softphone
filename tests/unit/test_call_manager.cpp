#include "application/CallManager.hpp"
#include "application/SipEngine.hpp"
#include "core/CallState.hpp"
#include "core/StateChangeSink.hpp"

#include <gtest/gtest.h>

#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

class RecordingSink final : public voip::StateChangeSink
{
  public:
    void on_state_changed(const voip::StateTransition transition) override
    {
        transitions.push_back(transition);
    }

    std::vector<voip::StateTransition> transitions;
};

class FakeSipEngine final : public voip::application::SipEngine
{
  public:
    void set_observer(voip::application::SipEngineObserver *observer) noexcept override
    {
        observer_ = observer;
    }

    void register_account() override
    {
        ++register_calls;
        if (throw_on_register) {
            throw std::runtime_error("registration transport failed");
        }
    }

    void make_call(const std::string &destination) override
    {
        ++make_call_calls;
        last_destination = destination;
        if (throw_on_make_call) {
            throw std::runtime_error("call setup failed");
        }
    }

    void answer_call() override
    {
        ++answer_calls;
    }

    void hangup_call() override
    {
        ++hangup_calls;
    }

    void registration_succeeded()
    {
        ASSERT_NE(observer_, nullptr);
        observer_->on_registration_succeeded();
    }

    void incoming_call(std::string peer)
    {
        ASSERT_NE(observer_, nullptr);
        observer_->on_incoming_call(std::move(peer));
    }

    void remote_ringing()
    {
        ASSERT_NE(observer_, nullptr);
        observer_->on_remote_ringing();
    }

    void call_connected()
    {
        ASSERT_NE(observer_, nullptr);
        observer_->on_call_connected();
    }

    void call_ended()
    {
        ASSERT_NE(observer_, nullptr);
        observer_->on_call_ended();
    }

    [[nodiscard]] bool has_observer() const noexcept
    {
        return observer_ != nullptr;
    }

    bool throw_on_register{false};
    bool throw_on_make_call{false};
    int register_calls{0};
    int make_call_calls{0};
    int answer_calls{0};
    int hangup_calls{0};
    std::string last_destination;

  private:
    voip::application::SipEngineObserver *observer_{nullptr};
};

void make_ready(voip::application::CallManager &manager, FakeSipEngine &engine)
{
    ASSERT_TRUE(manager.register_account());
    engine.registration_succeeded();
    ASSERT_EQ(manager.state(), voip::CallState::Ready);
}

} // namespace

TEST(CallManagerTest, RegistersAndTracksReadyState)
{
    FakeSipEngine engine;
    RecordingSink sink;
    voip::application::CallManager manager{engine, sink};

    EXPECT_TRUE(engine.has_observer());
    EXPECT_TRUE(manager.register_account());
    EXPECT_EQ(engine.register_calls, 1);
    EXPECT_EQ(manager.state(), voip::CallState::Registering);

    engine.registration_succeeded();

    EXPECT_EQ(manager.state(), voip::CallState::Ready);
    ASSERT_EQ(sink.transitions.size(), 2U);
    EXPECT_EQ(sink.transitions.back().to, voip::CallState::Ready);
}

TEST(CallManagerTest, HandlesOutgoingCallLifecycle)
{
    FakeSipEngine engine;
    RecordingSink sink;
    voip::application::CallManager manager{engine, sink};
    make_ready(manager, engine);

    EXPECT_TRUE(manager.make_call("1002"));
    EXPECT_EQ(engine.make_call_calls, 1);
    EXPECT_EQ(engine.last_destination, "1002");
    EXPECT_EQ(manager.state(), voip::CallState::Calling);
    EXPECT_EQ(manager.peer(), std::optional<std::string>{"1002"});
    EXPECT_FALSE(manager.incoming_call());

    engine.remote_ringing();
    EXPECT_EQ(manager.state(), voip::CallState::Ringing);

    engine.call_connected();
    EXPECT_EQ(manager.state(), voip::CallState::Connected);

    EXPECT_TRUE(manager.hangup_call());
    EXPECT_EQ(engine.hangup_calls, 1);
    EXPECT_EQ(manager.state(), voip::CallState::Terminating);

    engine.call_ended();
    EXPECT_EQ(manager.state(), voip::CallState::Ready);
    EXPECT_FALSE(manager.peer().has_value());
}

TEST(CallManagerTest, HandlesIncomingCallAnswerAndRemoteEnd)
{
    FakeSipEngine engine;
    RecordingSink sink;
    voip::application::CallManager manager{engine, sink};
    make_ready(manager, engine);

    engine.incoming_call("1003");

    EXPECT_EQ(manager.state(), voip::CallState::Ringing);
    EXPECT_EQ(manager.peer(), std::optional<std::string>{"1003"});
    EXPECT_TRUE(manager.incoming_call());

    EXPECT_TRUE(manager.answer_call());
    EXPECT_EQ(engine.answer_calls, 1);

    engine.call_connected();
    EXPECT_EQ(manager.state(), voip::CallState::Connected);

    engine.call_ended();
    EXPECT_EQ(manager.state(), voip::CallState::Ready);
    EXPECT_FALSE(manager.peer().has_value());
    EXPECT_FALSE(manager.incoming_call());
}

TEST(CallManagerTest, ConvertsEngineExceptionIntoFailedState)
{
    FakeSipEngine engine;
    engine.throw_on_register = true;
    RecordingSink sink;
    voip::application::CallManager manager{engine, sink};

    EXPECT_FALSE(manager.register_account());
    EXPECT_EQ(manager.state(), voip::CallState::Failed);
    EXPECT_EQ(manager.last_error(),
              std::optional<std::string>{"register account: registration transport failed"});

    EXPECT_TRUE(manager.reset());
    EXPECT_EQ(manager.state(), voip::CallState::Idle);
    EXPECT_FALSE(manager.last_error().has_value());
}

TEST(CallManagerTest, RejectsInvalidOperationsBeforeCallingEngine)
{
    FakeSipEngine engine;
    RecordingSink sink;
    voip::application::CallManager manager{engine, sink};

    EXPECT_FALSE(manager.make_call(""));
    EXPECT_FALSE(manager.make_call("1002"));
    EXPECT_FALSE(manager.answer_call());
    EXPECT_FALSE(manager.hangup_call());

    EXPECT_EQ(engine.make_call_calls, 0);
    EXPECT_EQ(engine.answer_calls, 0);
    EXPECT_EQ(engine.hangup_calls, 0);
    EXPECT_EQ(manager.state(), voip::CallState::Idle);
}
