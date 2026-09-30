#include "gui/GuiViewModel.hpp"
#include "gui/QtRegistrationBackend.hpp"

#include <QCoreApplication>
#include <QEventLoop>
#include <QObject>
#include <QThread>

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <thread>
#include <utility>

namespace voip::gui {
namespace {

struct FakeSessionState
{
    std::atomic<int> started{0};
    std::atomic<int> stopped{0};
    std::atomic<bool> result{true};
    std::atomic<bool> ran_on_worker{false};
};

class FakeSession final : public RegistrationSession
{
  public:
    explicit FakeSession(std::shared_ptr<FakeSessionState> state) : state_(std::move(state)) {}

    void start() override
    {
        state_->ran_on_worker = QThread::currentThread() != QCoreApplication::instance()->thread();
        ++state_->started;
    }

    [[nodiscard]] bool wait_for_registration(std::chrono::milliseconds /*timeout*/) override
    {
        return state_->result;
    }

    void stop() noexcept override
    {
        ++state_->stopped;
    }

  private:
    std::shared_ptr<FakeSessionState> state_;
};

[[nodiscard]] QtRegistrationBackend::SessionFactory
make_factory(const std::shared_ptr<FakeSessionState> &state)
{
    return [state] { return std::make_unique<FakeSession>(state); };
}

[[nodiscard]] bool wait_for_event(const std::function<bool()> &predicate)
{
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds{2};
    while (!predicate() && std::chrono::steady_clock::now() < deadline) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
        std::this_thread::sleep_for(std::chrono::milliseconds{1});
    }
    return predicate();
}

TEST(QtRegistrationBackendTest, InertGuiCannotRequestRegistration)
{
    GuiViewModel view_model;
    EXPECT_FALSE(view_model.backendAvailable());
    EXPECT_FALSE(view_model.registrationActionsEnabled());
    EXPECT_FALSE(view_model.registerAccount());
    EXPECT_FALSE(view_model.callActionsEnabled());
}

TEST(QtRegistrationBackendTest, SuccessRunsOnWorkerAndDisablesUnsupportedCalls)
{
    auto state = std::make_shared<FakeSessionState>();
    {
        QtRegistrationBackend backend(make_factory(state));
        GuiViewModel view_model(backend);
        bool delivered = false;
        bool delivered_on_ui_thread = false;
        QObject::connect(&backend, &QtRegistrationBackend::registrationFinished, &view_model,
                         [&view_model, &delivered, &delivered_on_ui_thread](const bool success) {
                             delivered_on_ui_thread =
                                 QThread::currentThread() == QCoreApplication::instance()->thread();
                             delivered = true;
                             if (success) {
                                 static_cast<void>(
                                     view_model.manager().on_registration_succeeded());
                             }
                         });
        EXPECT_TRUE(view_model.registrationActionsEnabled());
        ASSERT_TRUE(view_model.registerAccount());
        EXPECT_FALSE(view_model.registrationActionsEnabled());
        EXPECT_FALSE(view_model.callActionsEnabled());
        ASSERT_TRUE(wait_for_event([&delivered] { return delivered; }));
        EXPECT_TRUE(delivered_on_ui_thread);
        EXPECT_EQ(view_model.registrationStatus(), QStringLiteral("Registered"));
        EXPECT_TRUE(view_model.backendAvailable());
        EXPECT_FALSE(view_model.registrationActionsEnabled());
        EXPECT_FALSE(view_model.callActionsEnabled());
        EXPECT_FALSE(backend.begin_registration()); // No join of the retained worker.
        EXPECT_FALSE(view_model.manager().request_outgoing_call("1001"));
        EXPECT_EQ(state->started.load(), 1);
        EXPECT_EQ(state->stopped.load(), 0); // Endpoint retained until shutdown.
    }
    EXPECT_TRUE(state->ran_on_worker);
    EXPECT_EQ(state->stopped.load(), 1);
}

TEST(QtRegistrationBackendTest, FailureIsQueuedAndDoesNotEnableCalls)
{
    auto state = std::make_shared<FakeSessionState>();
    state->result = false;
    QtRegistrationBackend backend(make_factory(state));
    GuiViewModel view_model(backend);
    bool delivered = false;
    QObject::connect(&backend, &QtRegistrationBackend::registrationFinished, &view_model,
                     [&view_model, &delivered](const bool success) {
                         delivered = true;
                         if (!success) {
                             static_cast<void>(view_model.manager().on_registration_failed(
                                 "SIP registration failed or timed out"));
                         }
                     });
    ASSERT_TRUE(view_model.registerAccount());
    ASSERT_TRUE(wait_for_event([&delivered] { return delivered; }));
    EXPECT_EQ(view_model.registrationStatus(), QStringLiteral("Registration or call failed"));
    EXPECT_TRUE(view_model.registrationActionsEnabled());
    EXPECT_FALSE(view_model.callActionsEnabled());
    EXPECT_EQ(state->stopped.load(), 1);
}

TEST(QtRegistrationBackendTest, FailedAttemptCanRetry)
{
    auto state = std::make_shared<FakeSessionState>();
    state->result = false;
    QtRegistrationBackend backend(make_factory(state));
    GuiViewModel view_model(backend);
    int events = 0;
    QObject::connect(&backend, &QtRegistrationBackend::registrationFinished, &view_model,
                     [&view_model, &events](const bool success) {
                         ++events;
                         if (success) {
                             static_cast<void>(view_model.manager().on_registration_succeeded());
                         } else {
                             static_cast<void>(view_model.manager().on_registration_failed(
                                 "SIP registration failed or timed out"));
                         }
                     });
    ASSERT_TRUE(view_model.registerAccount());
    ASSERT_TRUE(wait_for_event([&events] { return events == 1; }));
    state->result = true;
    ASSERT_TRUE(view_model.registerAccount());
    ASSERT_TRUE(wait_for_event([&events] { return events == 2; }));
    EXPECT_EQ(view_model.registrationStatus(), QStringLiteral("Registered"));
    EXPECT_FALSE(view_model.callActionsEnabled());
    EXPECT_EQ(state->started.load(), 2);
}

} // namespace
} // namespace voip::gui

int main(int argc, char *argv[])
{
    QCoreApplication application(argc, argv);
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
