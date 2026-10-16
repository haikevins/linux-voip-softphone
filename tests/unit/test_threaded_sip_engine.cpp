#include "application/ThreadedSipEngine.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <future>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>

namespace {

using namespace std::chrono_literals;
using voip::application::SipEngine;
using voip::application::SipEngineObserver;
using voip::application::ThreadedSipEngine;

struct BackendState
{
    std::mutex mutex;
    std::thread::id construction_thread;
    std::thread::id destruction_thread;
    std::thread::id register_thread;
    std::thread::id make_call_thread;
    std::string destination;
    bool throw_on_register{false};
    bool throw_on_make_call{false};
};

class RecordingSipEngine final : public SipEngine
{
  public:
    explicit RecordingSipEngine(BackendState &state) : state_(state)
    {
        const std::lock_guard lock{state_.mutex};
        state_.construction_thread = std::this_thread::get_id();
    }

    ~RecordingSipEngine() override
    {
        const std::lock_guard lock{state_.mutex};
        state_.destruction_thread = std::this_thread::get_id();
    }

    void set_observer(SipEngineObserver *observer) noexcept override
    {
        observer_ = observer;
    }

    void register_account() override
    {
        {
            const std::lock_guard lock{state_.mutex};
            state_.register_thread = std::this_thread::get_id();
            if (state_.throw_on_register) {
                throw std::runtime_error("registration transport failed");
            }
        }

        if (observer_ != nullptr) {
            observer_->on_registration_succeeded();
        }
    }

    void make_call(const std::string &destination) override
    {
        {
            const std::lock_guard lock{state_.mutex};
            state_.make_call_thread = std::this_thread::get_id();
            state_.destination = destination;
            if (state_.throw_on_make_call) {
                throw std::runtime_error("call setup failed");
            }
        }

        if (observer_ != nullptr) {
            observer_->on_remote_ringing();
        }
    }

    void answer_call() override {}
    void hangup_call() override {}

  private:
    BackendState &state_;
    SipEngineObserver *observer_{nullptr};
};

class RecordingObserver final : public SipEngineObserver
{
  public:
    void on_registration_succeeded() override
    {
        registration_succeeded.set_value(std::this_thread::get_id());
    }

    void on_registration_failed(std::string reason) override
    {
        registration_failed.set_value(std::move(reason));
    }

    void on_incoming_call(std::string /*peer*/) override {}

    void on_remote_ringing() override
    {
        remote_ringing.set_value(std::this_thread::get_id());
    }

    void on_call_connected() override {}
    void on_call_ended() override {}

    void on_call_failed(std::string reason) override
    {
        call_failed.set_value(std::move(reason));
    }

    std::promise<std::thread::id> registration_succeeded;
    std::promise<std::string> registration_failed;
    std::promise<std::thread::id> remote_ringing;
    std::promise<std::string> call_failed;
};

[[nodiscard]] ThreadedSipEngine make_engine(BackendState &state)
{
    return ThreadedSipEngine{[&state]() { return std::make_unique<RecordingSipEngine>(state); }};
}

} // namespace

TEST(ThreadedSipEngineTest, OwnsBackendLifecycleOnDedicatedWorkerThread)
{
    BackendState state;
    std::thread::id worker_thread;

    {
        auto engine = make_engine(state);
        worker_thread = engine.worker_thread_id();

        const std::lock_guard lock{state.mutex};
        EXPECT_EQ(state.construction_thread, worker_thread);
        EXPECT_NE(state.construction_thread, std::this_thread::get_id());
    }

    const std::lock_guard lock{state.mutex};
    EXPECT_EQ(state.destruction_thread, worker_thread);
}

TEST(ThreadedSipEngineTest, DispatchesCommandsAndCallbacksOnWorkerThread)
{
    BackendState state;
    auto engine = make_engine(state);
    RecordingObserver observer;
    engine.set_observer(&observer);

    auto registered = observer.registration_succeeded.get_future();
    engine.register_account();
    ASSERT_EQ(registered.wait_for(1s), std::future_status::ready);
    EXPECT_EQ(registered.get(), engine.worker_thread_id());

    auto ringing = observer.remote_ringing.get_future();
    engine.make_call("1002");
    ASSERT_EQ(ringing.wait_for(1s), std::future_status::ready);
    EXPECT_EQ(ringing.get(), engine.worker_thread_id());

    const std::lock_guard lock{state.mutex};
    EXPECT_EQ(state.register_thread, engine.worker_thread_id());
    EXPECT_EQ(state.make_call_thread, engine.worker_thread_id());
    EXPECT_EQ(state.destination, "1002");
}

TEST(ThreadedSipEngineTest, ConvertsRegistrationExceptionIntoObserverFailure)
{
    BackendState state;
    state.throw_on_register = true;
    auto engine = make_engine(state);
    RecordingObserver observer;
    engine.set_observer(&observer);

    auto failed = observer.registration_failed.get_future();
    EXPECT_NO_THROW(engine.register_account());
    ASSERT_EQ(failed.wait_for(1s), std::future_status::ready);
    EXPECT_EQ(failed.get(), "register account: registration transport failed");
}

TEST(ThreadedSipEngineTest, ConvertsCallExceptionIntoObserverFailure)
{
    BackendState state;
    state.throw_on_make_call = true;
    auto engine = make_engine(state);
    RecordingObserver observer;
    engine.set_observer(&observer);

    auto failed = observer.call_failed.get_future();
    EXPECT_NO_THROW(engine.make_call("1002"));
    ASSERT_EQ(failed.wait_for(1s), std::future_status::ready);
    EXPECT_EQ(failed.get(), "make call: call setup failed");
}

TEST(ThreadedSipEngineTest, RejectsEmptyFactory)
{
    EXPECT_THROW(static_cast<void>(ThreadedSipEngine{ThreadedSipEngine::Factory{}}),
                 std::invalid_argument);
}
