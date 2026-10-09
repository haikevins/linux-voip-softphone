#include "gui/GuiViewModel.hpp"
#include "gui/QtRegistrationBackend.hpp"

#include <QCoreApplication>
#include <QEventLoop>
#include <QObject>
#include <QThread>

#include <gtest/gtest.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <memory>
#include <string_view>
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

struct OutgoingFakeState
{
    std::atomic<int> registration_count{0};
    std::atomic<int> call_count{0};
    std::atomic<int> hangup_count{0};
    std::atomic<int> stopped_count{0};
    std::atomic<int> diagnostics_read_count{0};
    std::atomic<bool> remote_disconnected{false};
    std::atomic<bool> hung_up{false};
    std::atomic<bool> fail_next_call{false};
    std::atomic<bool> disconnect_after_connect_before_publication{false};
    std::atomic<bool> reject_after_ringing{false};
    std::atomic<int> reject_after_ringing_step{0};
    std::atomic<bool> on_worker{false};
};

class OutgoingFakeSession final : public RegistrationSession
{
  public:
    explicit OutgoingFakeSession(std::shared_ptr<OutgoingFakeState> state)
        : state_(std::move(state))
    {}

    void start() override
    {
        state_->on_worker = QThread::currentThread() != QCoreApplication::instance()->thread();
        ++state_->registration_count;
    }
    [[nodiscard]] bool wait_for_registration(std::chrono::milliseconds /*timeout*/) override
    {
        return true;
    }
    void stop() noexcept override
    {
        ++state_->stopped_count;
    }
    [[nodiscard]] bool supports_calls() const noexcept override
    {
        return true;
    }
    void start_call(std::string_view /*destination*/) override
    {
        state_->remote_disconnected = false;
        state_->hung_up = false;
        state_->reject_after_ringing_step = 0;
        ++state_->call_count;
    }
    [[nodiscard]] RtpDiagnosticsSnapshot read_rtp_diagnostics() override
    {
        ++state_->diagnostics_read_count;
        return {.available = true,
                .codec = "PCMU",
                .packets_received = 1200,
                .packets_sent = 1180,
                .packets_lost = 3,
                .packet_loss_percent = 0.25,
                .jitter_ms = 1.5,
                .round_trip_time_ms = 0.8};
    }
    [[nodiscard]] CallProgress poll_call(std::chrono::milliseconds /*timeout*/) override
    {
        if (state_->fail_next_call.exchange(false)) {
            state_->remote_disconnected = true;
            return CallProgress::Disconnected;
        }
        if (state_->disconnect_after_connect_before_publication.exchange(false)) {
            state_->remote_disconnected = true;
            return CallProgress::DisconnectedAfterConnect;
        }
        if (state_->reject_after_ringing.load()) {
            if (state_->reject_after_ringing_step.fetch_add(1) == 0) {
                return CallProgress::Ringing;
            }
            state_->remote_disconnected = true;
            return CallProgress::Disconnected;
        }
        return state_->remote_disconnected || state_->hung_up ? CallProgress::Disconnected
                                                              : CallProgress::ConnectedWithMedia;
    }
    void hangup_call() noexcept override
    {
        ++state_->hangup_count;
        state_->hung_up = true;
    }
    [[nodiscard]] bool wait_for_disconnection(std::chrono::milliseconds timeout) override
    {
        if (state_->hung_up || state_->remote_disconnected) {
            return true;
        }
        std::this_thread::sleep_for(std::min(timeout, std::chrono::milliseconds{2}));
        return state_->hung_up || state_->remote_disconnected;
    }
    [[nodiscard]] bool release_disconnected_call() override
    {
        return state_->hung_up || state_->remote_disconnected;
    }

  private:
    std::shared_ptr<OutgoingFakeState> state_;
};

TEST(QtRegistrationBackendTest, OutgoingCallAndHangupUseRetainedWorker)
{
    auto state = std::make_shared<OutgoingFakeState>();
    {
        QtRegistrationBackend backend(
            [state] { return std::make_unique<OutgoingFakeSession>(state); }, true);
        GuiViewModel model(backend);
        int connections = 0;
        int endings = 0;
        QObject::connect(&backend, &QtRegistrationBackend::registrationFinished, &model,
                         [&model](const bool success) {
                             if (success) {
                                 static_cast<void>(model.manager().on_registration_succeeded());
                             }
                         });
        QObject::connect(&backend, &QtRegistrationBackend::callConnected, &model,
                         [&model, &connections] {
                             ++connections;
                             static_cast<void>(model.manager().on_call_connected());
                         });
        QObject::connect(&backend, &QtRegistrationBackend::callEnded, &model, [&model, &endings] {
            ++endings;
            static_cast<void>(model.manager().on_call_ended());
        });

        ASSERT_TRUE(model.registerAccount());
        ASSERT_TRUE(wait_for_event([&model] { return model.callActionsEnabled(); }));
        EXPECT_EQ(model.registrationStatus(), QStringLiteral("Registered"));
        ASSERT_TRUE(model.startCall(QStringLiteral("1002")));
        EXPECT_FALSE(model.callActionsEnabled());
        ASSERT_TRUE(wait_for_event([&connections] { return connections == 1; }));
        EXPECT_EQ(model.callStatus(), QStringLiteral("Connected (audio active)"));
        EXPECT_TRUE(model.callConnected());
        ASSERT_TRUE(model.hangupCall());
        ASSERT_TRUE(wait_for_event([&endings] { return endings == 1; }));
        EXPECT_TRUE(model.callActionsEnabled());
        EXPECT_EQ(model.callStatus(), QStringLiteral("Ready to call"));
        EXPECT_FALSE(model.callConnected());
        EXPECT_TRUE(state->on_worker);
        EXPECT_EQ(state->registration_count.load(), 1);
        EXPECT_EQ(state->hangup_count.load(), 1);

        // Reuse the same fake account after deterministic call cleanup.
        ASSERT_TRUE(model.startCall(QStringLiteral("1003")));
        ASSERT_TRUE(wait_for_event([&connections] { return connections == 2; }));
        state->remote_disconnected = true;
        ASSERT_TRUE(wait_for_event([&endings] { return endings == 2; }));
        EXPECT_EQ(state->registration_count.load(), 1);
        EXPECT_EQ(state->call_count.load(), 2);
    }
    EXPECT_EQ(state->stopped_count.load(), 1);
}

TEST(QtRegistrationBackendTest, ConnectedCallPublishesAndClearsRtpDiagnostics)
{
    auto state = std::make_shared<OutgoingFakeState>();
    QtRegistrationBackend backend([state] { return std::make_unique<OutgoingFakeSession>(state); },
                                  true);
    GuiViewModel model(backend);
    int endings = 0;
    bool diagnostics_on_ui_thread = false;

    QObject::connect(&backend, &QtRegistrationBackend::registrationFinished, &model,
                     [&model](const bool success) {
                         if (success) {
                             static_cast<void>(model.manager().on_registration_succeeded());
                         }
                     });
    QObject::connect(&backend, &QtRegistrationBackend::callConnected, &model,
                     [&model] { static_cast<void>(model.manager().on_call_connected()); });
    QObject::connect(&backend, &QtRegistrationBackend::rtpDiagnosticsUpdated, &model,
                     [&model, &diagnostics_on_ui_thread](
                         const QString &codec, const qulonglong received, const qulonglong sent,
                         const qulonglong lost, const double loss_percent, const double jitter_ms,
                         const double rtt_ms) {
                         diagnostics_on_ui_thread =
                             QThread::currentThread() == QCoreApplication::instance()->thread();
                         model.updateRtpDiagnostics(codec, received, sent, lost, loss_percent,
                                                    jitter_ms, rtt_ms);
                     });
    QObject::connect(&backend, &QtRegistrationBackend::rtpDiagnosticsCleared, &model,
                     [&model] { model.clearRtpDiagnostics(); });
    QObject::connect(&backend, &QtRegistrationBackend::callEnded, &model, [&model, &endings] {
        ++endings;
        static_cast<void>(model.manager().on_call_ended());
    });

    ASSERT_TRUE(model.registerAccount());
    ASSERT_TRUE(wait_for_event([&model] { return model.callActionsEnabled(); }));
    ASSERT_TRUE(model.startCall(QStringLiteral("1001")));
    ASSERT_TRUE(wait_for_event([&model] { return model.callConnected(); }));
    ASSERT_TRUE(wait_for_event([&model] { return model.rtpDiagnosticsAvailable(); }));

    EXPECT_TRUE(diagnostics_on_ui_thread);
    EXPECT_EQ(model.rtpCodec(), QStringLiteral("PCMU"));
    EXPECT_EQ(model.rtpPacketsReceived(), 1200U);
    EXPECT_EQ(model.rtpPacketsSent(), 1180U);
    EXPECT_EQ(model.rtpPacketsLost(), 3U);
    EXPECT_DOUBLE_EQ(model.rtpPacketLossPercent(), 0.25);
    EXPECT_DOUBLE_EQ(model.rtpJitterMs(), 1.5);
    EXPECT_DOUBLE_EQ(model.rtpRoundTripTimeMs(), 0.8);
    EXPECT_GE(state->diagnostics_read_count.load(), 1);

    state->remote_disconnected = true;
    ASSERT_TRUE(wait_for_event([&endings] { return endings == 1; }));
    EXPECT_FALSE(model.rtpDiagnosticsAvailable());
    EXPECT_TRUE(model.rtpCodec().isEmpty());
    EXPECT_EQ(model.rtpPacketsReceived(), 0U);
}

TEST(QtRegistrationBackendTest, RemoteHangupAfterConnectedMediaIsNotReportedAsFailure)
{
    auto state = std::make_shared<OutgoingFakeState>();
    state->disconnect_after_connect_before_publication = true;
    QtRegistrationBackend backend([state] { return std::make_unique<OutgoingFakeSession>(state); },
                                  true);
    GuiViewModel model(backend);
    int endings = 0;
    int failures = 0;
    QObject::connect(&backend, &QtRegistrationBackend::registrationFinished, &model,
                     [&model](const bool success) {
                         if (success) {
                             static_cast<void>(model.manager().on_registration_succeeded());
                         }
                     });
    QObject::connect(&backend, &QtRegistrationBackend::callEnded, &model, [&model, &endings] {
        ++endings;
        static_cast<void>(model.manager().on_call_ended());
    });
    QObject::connect(&backend, &QtRegistrationBackend::callFailed, &model, [&model, &failures] {
        ++failures;
        static_cast<void>(model.manager().on_call_failed("Call request was rejected"));
    });

    ASSERT_TRUE(model.registerAccount());
    ASSERT_TRUE(wait_for_event([&model] { return model.callActionsEnabled(); }));
    ASSERT_TRUE(model.startCall(QStringLiteral("1001")));
    ASSERT_TRUE(wait_for_event([&endings] { return endings == 1; }));
    EXPECT_EQ(failures, 0);
    EXPECT_EQ(model.callStatus(), QStringLiteral("Ready to call"));
    EXPECT_TRUE(model.callActionsEnabled());
    EXPECT_FALSE(model.recoveryActionsEnabled());
}

TEST(QtRegistrationBackendTest, RemoteRejectAfterRingingIsNotReportedAsFailure)
{
    auto state = std::make_shared<OutgoingFakeState>();
    state->reject_after_ringing = true;

    QtRegistrationBackend backend([state] { return std::make_unique<OutgoingFakeSession>(state); },
                                  true);
    GuiViewModel model(backend);

    int endings = 0;
    int failures = 0;

    QObject::connect(&backend, &QtRegistrationBackend::registrationFinished, &model,
                     [&model](const bool success) {
                         if (success) {
                             static_cast<void>(model.manager().on_registration_succeeded());
                         }
                     });

    QObject::connect(&backend, &QtRegistrationBackend::callEnded, &model, [&model, &endings] {
        ++endings;
        static_cast<void>(model.manager().on_call_ended());
    });

    QObject::connect(&backend, &QtRegistrationBackend::callFailed, &model, [&model, &failures] {
        ++failures;
        static_cast<void>(model.manager().on_call_failed("Call request was rejected"));
    });

    ASSERT_TRUE(model.registerAccount());
    ASSERT_TRUE(wait_for_event([&model] { return model.callActionsEnabled(); }));

    ASSERT_TRUE(model.startCall(QStringLiteral("1001")));
    ASSERT_TRUE(wait_for_event([&endings] { return endings == 1; }));

    EXPECT_EQ(failures, 0);
    EXPECT_GE(state->reject_after_ringing_step.load(), 2);
    EXPECT_EQ(state->registration_count.load(), 1);
    EXPECT_EQ(state->call_count.load(), 1);
    EXPECT_EQ(model.registrationStatus(), QStringLiteral("Registered"));
    EXPECT_EQ(model.callStatus(), QStringLiteral("Ready to call"));
    EXPECT_TRUE(model.callActionsEnabled());
    EXPECT_FALSE(model.recoveryActionsEnabled());
}

struct IncomingFakeState
{
    std::atomic<int> registration_count{0};
    std::atomic<int> answer_count{0};
    std::atomic<int> decline_count{0};
    std::atomic<int> hangup_count{0};
    std::atomic<int> stopped_count{0};
    std::atomic<bool> offer_pending{false};
    std::atomic<bool> call_active{false};
    std::atomic<bool> answered{false};
    std::atomic<bool> remote_disconnected{false};
    std::atomic<bool> hung_up{false};
};

class IncomingFakeSession final : public RegistrationSession
{
  public:
    explicit IncomingFakeSession(std::shared_ptr<IncomingFakeState> state)
        : state_(std::move(state))
    {}

    void start() override
    {
        ++state_->registration_count;
    }
    [[nodiscard]] bool wait_for_registration(std::chrono::milliseconds /*timeout*/) override
    {
        return true;
    }
    void stop() noexcept override
    {
        ++state_->stopped_count;
    }
    [[nodiscard]] bool supports_calls() const noexcept override
    {
        return true;
    }
    [[nodiscard]] bool supports_incoming_calls() const noexcept override
    {
        return true;
    }

    [[nodiscard]] IncomingCallNotice
    poll_incoming_call(const std::chrono::milliseconds timeout) override
    {
        if (state_->offer_pending.exchange(false)) {
            state_->call_active = true;
            state_->answered = false;
            state_->remote_disconnected = false;
            state_->hung_up = false;
            return {.received = true, .disconnected = false, .peer = "1001"};
        }
        std::this_thread::sleep_for(std::min(timeout, std::chrono::milliseconds{2}));
        return {};
    }

    void answer_call() override
    {
        ++state_->answer_count;
        state_->answered = true;
    }

    void decline_call() override
    {
        ++state_->decline_count;
        state_->hung_up = true;
    }

    [[nodiscard]] CallProgress poll_call(std::chrono::milliseconds /*timeout*/) override
    {
        if (state_->remote_disconnected || state_->hung_up) {
            return CallProgress::Disconnected;
        }
        return state_->answered ? CallProgress::ConnectedWithMedia : CallProgress::Ringing;
    }

    void hangup_call() noexcept override
    {
        ++state_->hangup_count;
        state_->hung_up = true;
    }

    [[nodiscard]] bool wait_for_disconnection(const std::chrono::milliseconds timeout) override
    {
        if (state_->remote_disconnected || state_->hung_up) {
            return true;
        }
        std::this_thread::sleep_for(std::min(timeout, std::chrono::milliseconds{2}));
        return state_->remote_disconnected || state_->hung_up;
    }

    [[nodiscard]] bool release_disconnected_call() override
    {
        if (!state_->remote_disconnected && !state_->hung_up) {
            return false;
        }
        state_->call_active = false;
        state_->answered = false;
        state_->remote_disconnected = false;
        state_->hung_up = false;
        return true;
    }

  private:
    std::shared_ptr<IncomingFakeState> state_;
};

TEST(QtRegistrationBackendTest, IncomingCallAnswerAndRemoteHangupUseRetainedWorker)
{
    auto state = std::make_shared<IncomingFakeState>();
    QtRegistrationBackend backend([state] { return std::make_unique<IncomingFakeSession>(state); },
                                  true);
    GuiViewModel model(backend);
    int connections = 0;
    int endings = 0;

    QObject::connect(&backend, &QtRegistrationBackend::registrationFinished, &model,
                     [&model](const bool success) {
                         if (success) {
                             static_cast<void>(model.manager().on_registration_succeeded());
                         }
                     });
    QObject::connect(&backend, &QtRegistrationBackend::incomingCall, &model,
                     [&model](const QString &peer) {
                         static_cast<void>(model.manager().on_incoming_call(peer.toStdString()));
                     });
    QObject::connect(&backend, &QtRegistrationBackend::callConnected, &model,
                     [&model, &connections] {
                         ++connections;
                         static_cast<void>(model.manager().on_call_connected());
                     });
    QObject::connect(&backend, &QtRegistrationBackend::callEnded, &model, [&model, &endings] {
        ++endings;
        static_cast<void>(model.manager().on_call_ended());
    });

    ASSERT_TRUE(model.registerAccount());
    ASSERT_TRUE(wait_for_event([&model] { return model.callActionsEnabled(); }));
    state->offer_pending = true;
    ASSERT_TRUE(wait_for_event([&model] { return model.answerActionsEnabled(); }));
    EXPECT_TRUE(model.incomingCall());
    EXPECT_EQ(model.activePeer(), QStringLiteral("1001"));
    EXPECT_EQ(model.callStatus(), QStringLiteral("Incoming call"));
    EXPECT_TRUE(model.declineActionsEnabled());

    ASSERT_TRUE(model.answerCall());
    EXPECT_FALSE(model.answerActionsEnabled());
    EXPECT_FALSE(model.declineActionsEnabled());
    ASSERT_TRUE(wait_for_event([&connections] { return connections == 1; }));
    EXPECT_EQ(model.callStatus(), QStringLiteral("Connected (audio active)"));
    EXPECT_TRUE(model.callConnected());
    EXPECT_EQ(state->answer_count.load(), 1);

    state->remote_disconnected = true;
    ASSERT_TRUE(wait_for_event([&endings] { return endings == 1; }));
    EXPECT_EQ(model.callStatus(), QStringLiteral("Ready to call"));
    EXPECT_TRUE(model.callActionsEnabled());
    EXPECT_EQ(state->registration_count.load(), 1);
}

TEST(QtRegistrationBackendTest, IncomingCallCanBeDeclinedWithoutReregistering)
{
    auto state = std::make_shared<IncomingFakeState>();
    QtRegistrationBackend backend([state] { return std::make_unique<IncomingFakeSession>(state); },
                                  true);
    GuiViewModel model(backend);
    int endings = 0;

    QObject::connect(&backend, &QtRegistrationBackend::registrationFinished, &model,
                     [&model](const bool success) {
                         if (success) {
                             static_cast<void>(model.manager().on_registration_succeeded());
                         }
                     });
    QObject::connect(&backend, &QtRegistrationBackend::incomingCall, &model,
                     [&model](const QString &peer) {
                         static_cast<void>(model.manager().on_incoming_call(peer.toStdString()));
                     });
    QObject::connect(&backend, &QtRegistrationBackend::callEnded, &model, [&model, &endings] {
        ++endings;
        static_cast<void>(model.manager().on_call_ended());
    });

    ASSERT_TRUE(model.registerAccount());
    ASSERT_TRUE(wait_for_event([&model] { return model.callActionsEnabled(); }));
    state->offer_pending = true;
    ASSERT_TRUE(wait_for_event([&model] { return model.answerActionsEnabled(); }));
    EXPECT_TRUE(model.declineActionsEnabled());
    ASSERT_TRUE(model.declineCall());
    ASSERT_TRUE(wait_for_event([&endings] { return endings == 1; }));
    EXPECT_EQ(state->decline_count.load(), 1);
    EXPECT_EQ(state->hangup_count.load(), 0);
    EXPECT_EQ(state->registration_count.load(), 1);
    EXPECT_EQ(model.callStatus(), QStringLiteral("Ready to call"));
    EXPECT_TRUE(model.callActionsEnabled());
}

TEST(QtRegistrationBackendTest, RejectedCallAllowsExplicitRecoveryWithoutReregistering)
{
    auto state = std::make_shared<OutgoingFakeState>();
    state->fail_next_call = true;
    QtRegistrationBackend backend([state] { return std::make_unique<OutgoingFakeSession>(state); },
                                  true);
    GuiViewModel model(backend);
    bool failed = false;
    QObject::connect(&backend, &QtRegistrationBackend::registrationFinished, &model,
                     [&model](const bool success) {
                         if (success) {
                             static_cast<void>(model.manager().on_registration_succeeded());
                         }
                     });
    QObject::connect(&backend, &QtRegistrationBackend::callFailed, &model, [&model, &failed] {
        failed = true;
        static_cast<void>(model.manager().on_call_failed("Call request was rejected"));
    });
    ASSERT_TRUE(model.registerAccount());
    ASSERT_TRUE(wait_for_event([&model] { return model.callActionsEnabled(); }));
    ASSERT_TRUE(model.startCall(QStringLiteral("1002")));
    ASSERT_TRUE(wait_for_event([&failed] { return failed; }));
    EXPECT_EQ(model.registrationStatus(), QStringLiteral("Registered"));
    EXPECT_TRUE(model.recoveryActionsEnabled());
    EXPECT_FALSE(model.callActionsEnabled());
    ASSERT_TRUE(model.recoverCall());
    EXPECT_TRUE(model.callActionsEnabled());
    EXPECT_EQ(state->registration_count.load(), 1);
}


// This seam deliberately blocks a registration result while the GUI owner is
// destroyed. No Qt event loop is pumped before the queued result is discarded.
struct RegistrationShutdownGate
{
    std::mutex mutex;
    std::condition_variable changed;
    bool in_wait{false};
    bool release_wait{false};
    std::atomic<int> stops{0};
    std::atomic<bool> stopped_on_worker{false};
};

class GatedRegistrationSession final : public RegistrationSession
{
  public:
    explicit GatedRegistrationSession(std::shared_ptr<RegistrationShutdownGate> gate)
        : gate_(std::move(gate))
    {}

    void start() override {}

    [[nodiscard]] bool wait_for_registration(std::chrono::milliseconds /*timeout*/) override
    {
        std::unique_lock lock{gate_->mutex};
        gate_->in_wait = true;
        gate_->changed.notify_all();
        return gate_->changed.wait_for(lock, std::chrono::seconds{2},
                                       [this] { return gate_->release_wait; });
    }

    void stop() noexcept override
    {
        gate_->stopped_on_worker =
            QThread::currentThread() != QCoreApplication::instance()->thread();
        ++gate_->stops;
    }

  private:
    std::shared_ptr<RegistrationShutdownGate> gate_;
};

TEST(QtRegistrationBackendTest, RejectsAllRequestEntryPointsFromForeignThread)
{
    auto state = std::make_shared<FakeSessionState>();
    QtRegistrationBackend backend(make_factory(state), true);
    bool registration = true;
    bool outgoing = true;
    bool answer = true;
    bool decline = true;
    bool hangup = true;

    std::thread foreign([&] {
        registration = backend.begin_registration();
        outgoing = backend.begin_outgoing_call("1001");
        answer = backend.begin_answer();
        decline = backend.begin_decline();
        hangup = backend.begin_hangup();
    });
    foreign.join();

    EXPECT_FALSE(registration);
    EXPECT_FALSE(outgoing);
    EXPECT_FALSE(answer);
    EXPECT_FALSE(decline);
    EXPECT_FALSE(hangup);
    EXPECT_EQ(state->started.load(), 0);
}

TEST(QtRegistrationBackendTest, DestructionJoinsInFlightSessionWithoutDeliveringQueuedResult)
{
    auto gate = std::make_shared<RegistrationShutdownGate>();
    auto backend = std::make_unique<QtRegistrationBackend>(
        [gate] { return std::make_unique<GatedRegistrationSession>(gate); });
    int delivered = 0;
    QObject::connect(backend.get(), &QtRegistrationBackend::registrationFinished,
                     QCoreApplication::instance(), [&delivered](bool) { ++delivered; });
    ASSERT_TRUE(backend->begin_registration());

    bool entered = false;
    {
        std::unique_lock lock{gate->mutex};
        entered = gate->changed.wait_for(lock, std::chrono::seconds{2},
                                        [&gate] { return gate->in_wait; });
    }
    if (!entered) {
        // Release on every failure path: never leave the destructor waiting on
        // this fake session if CI scheduling is unusually slow.
        {
            const std::lock_guard lock{gate->mutex};
            gate->release_wait = true;
        }
        gate->changed.notify_all();
        backend.reset();
        FAIL() << "fake registration worker never entered its wait";
        return;
    }

    std::thread release([gate] {
        std::this_thread::sleep_for(std::chrono::milliseconds{20});
        {
            const std::lock_guard lock{gate->mutex};
            gate->release_wait = true;
        }
        gate->changed.notify_all();
    });

    // The destructor requests stop, waits for the worker, and stops the fake
    // session. A queued result must not signal after its QObject is gone.
    backend.reset();
    release.join();
    QCoreApplication::processEvents(QEventLoop::AllEvents, 50);

    EXPECT_EQ(gate->stops.load(), 1);
    EXPECT_TRUE(gate->stopped_on_worker.load());
    EXPECT_EQ(delivered, 0);
}

} // namespace
} // namespace voip::gui

int main(int argc, char *argv[])
{
    QCoreApplication application(argc, argv);
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
