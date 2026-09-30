#include "application/CallManager.hpp"

#include <gtest/gtest.h>

#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace voip::application {
namespace {

class FakeCallBackend : public CallBackend
{
  public:
    bool registration_accepted{true};
    bool call_accepted{true};
    bool hangup_accepted{true};
    bool registration_throws{false};
    int registration_requests{0};
    int outgoing_requests{0};
    int hangup_requests{0};
    std::string destination;

    bool begin_registration() override
    {
        ++registration_requests;
        if (registration_throws) {
            throw std::runtime_error("untrusted backend exception");
        }
        return registration_accepted;
    }

    bool begin_outgoing_call(std::string_view target) override
    {
        ++outgoing_requests;
        destination = target;
        return call_accepted;
    }

    bool begin_hangup() override
    {
        ++hangup_requests;
        return hangup_accepted;
    }
};

TEST(CallManagerTest, ShellDoesNotPretendToHaveSIP)
{
    CallManager manager;
    EXPECT_EQ(manager.snapshot().state, CallState::Idle);
    EXPECT_FALSE(manager.snapshot().backend_available);
    EXPECT_FALSE(manager.snapshot().call_actions_enabled);
    EXPECT_FALSE(manager.request_registration());
    EXPECT_FALSE(manager.request_outgoing_call("1001"));
    EXPECT_EQ(manager.snapshot().state, CallState::Idle);
    EXPECT_EQ(manager.snapshot().error, "SIP backend is not connected");
}

TEST(CallManagerTest, RegistrationIsPendingUntilExplicitBackendCompletion)
{
    FakeCallBackend backend;
    CallManager manager(&backend);
    ASSERT_TRUE(manager.request_registration());
    EXPECT_EQ(backend.registration_requests, 1);
    EXPECT_EQ(manager.snapshot().state, CallState::Registering);
    EXPECT_FALSE(manager.snapshot().call_actions_enabled);
    EXPECT_FALSE(manager.request_registration());
    EXPECT_EQ(backend.registration_requests, 1);
    ASSERT_TRUE(manager.on_registration_succeeded());
    EXPECT_TRUE(manager.snapshot().call_actions_enabled);
    EXPECT_EQ(manager.snapshot().state, CallState::Ready);
    EXPECT_FALSE(manager.on_registration_succeeded());
}

TEST(CallManagerTest, RegistrationOnlyBackendNeverAdvertisesCalls)
{
    class RegistrationOnlyBackend final : public FakeCallBackend
    {
      public:
        [[nodiscard]] bool supports_calls() const noexcept override
        {
            return false;
        }
    };

    RegistrationOnlyBackend backend;
    CallManager manager(&backend);
    ASSERT_TRUE(manager.request_registration());
    ASSERT_TRUE(manager.on_registration_succeeded());
    EXPECT_EQ(manager.snapshot().state, CallState::Ready);
    EXPECT_TRUE(manager.snapshot().backend_available);
    EXPECT_FALSE(manager.snapshot().call_actions_enabled);
    EXPECT_FALSE(manager.request_registration()); // Must not re-enter a retained session.
    EXPECT_EQ(backend.registration_requests, 1);
    EXPECT_FALSE(manager.request_outgoing_call("1001"));
    EXPECT_EQ(backend.outgoing_requests, 0);
    EXPECT_EQ(manager.snapshot().state, CallState::Ready);
}

TEST(CallManagerTest, RejectedRegistrationFailsWithoutReportingReady)
{
    FakeCallBackend backend;
    backend.registration_accepted = false;
    CallManager manager(&backend);
    EXPECT_FALSE(manager.request_registration());
    EXPECT_EQ(manager.snapshot().state, CallState::Failed);
    EXPECT_FALSE(manager.snapshot().call_actions_enabled);
    EXPECT_EQ(manager.snapshot().error, "SIP backend rejected the registration request");
    EXPECT_FALSE(manager.request_registration());
    ASSERT_TRUE(manager.reset_after_failure());
    EXPECT_EQ(manager.snapshot().state, CallState::Idle);
    EXPECT_TRUE(manager.snapshot().error.empty());
    backend.registration_accepted = true;
    EXPECT_TRUE(manager.request_registration());
}

TEST(CallManagerTest, BackendExceptionDoesNotExposeItsMessage)
{
    FakeCallBackend backend;
    backend.registration_throws = true;
    CallManager manager(&backend);
    EXPECT_FALSE(manager.request_registration());
    EXPECT_EQ(manager.snapshot().state, CallState::Failed);
    EXPECT_EQ(manager.snapshot().error, "SIP backend rejected the registration request");
}

TEST(CallManagerTest, AsynchronousRegistrationFailureIsPublishedAndRecoverable)
{
    FakeCallBackend backend;
    CallManager manager(&backend);
    ASSERT_TRUE(manager.request_registration());
    ASSERT_TRUE(manager.on_registration_failed("Registration rejected"));
    EXPECT_EQ(manager.snapshot().state, CallState::Failed);
    EXPECT_EQ(manager.snapshot().error, "Registration rejected");
    EXPECT_FALSE(manager.snapshot().call_actions_enabled);
    ASSERT_TRUE(manager.reset_after_failure());
    ASSERT_TRUE(manager.request_registration());
    ASSERT_TRUE(manager.on_registration_succeeded());
    EXPECT_TRUE(manager.snapshot().error.empty());
}

TEST(CallManagerTest, InvalidDestinationCannotStartAnOutgoingCall)
{
    FakeCallBackend backend;
    CallManager manager(&backend);
    ASSERT_TRUE(manager.request_registration());
    ASSERT_TRUE(manager.on_registration_succeeded());
    EXPECT_FALSE(manager.request_outgoing_call(""));
    EXPECT_FALSE(manager.request_outgoing_call("sip:1001@example.com"));
    EXPECT_FALSE(manager.request_outgoing_call(std::string(65, '1')));
    EXPECT_EQ(backend.outgoing_requests, 0);
    EXPECT_EQ(manager.snapshot().state, CallState::Ready);
    ASSERT_TRUE(manager.request_outgoing_call("*1001#"));
    EXPECT_EQ(backend.outgoing_requests, 1);
    EXPECT_EQ(backend.destination, "*1001#");
    EXPECT_EQ(manager.snapshot().peer, "*1001#");
    EXPECT_TRUE(manager.snapshot().error.empty());
}

TEST(CallManagerTest, OutgoingCallFlowUsesExplicitAsynchronousEvents)
{
    FakeCallBackend backend;
    CallManager manager(&backend);
    ASSERT_TRUE(manager.request_registration());
    ASSERT_TRUE(manager.on_registration_succeeded());
    ASSERT_TRUE(manager.request_outgoing_call("1001"));
    EXPECT_EQ(manager.snapshot().state, CallState::Calling);
    EXPECT_FALSE(manager.snapshot().call_actions_enabled);
    ASSERT_TRUE(manager.on_remote_ringing());
    ASSERT_TRUE(manager.on_call_connected());
    EXPECT_EQ(manager.snapshot().state, CallState::Connected);
    ASSERT_TRUE(manager.request_hangup());
    EXPECT_EQ(manager.snapshot().state, CallState::Terminating);
    EXPECT_EQ(backend.hangup_requests, 1);
    ASSERT_TRUE(manager.on_call_ended());
    EXPECT_EQ(manager.snapshot().state, CallState::Ready);
    EXPECT_TRUE(manager.snapshot().peer.empty());
    EXPECT_TRUE(manager.snapshot().call_actions_enabled);
}

TEST(CallManagerTest, IncomingCallAndRemoteDisconnectReturnToReady)
{
    FakeCallBackend backend;
    CallManager manager(&backend);
    ASSERT_TRUE(manager.request_registration());
    ASSERT_TRUE(manager.on_registration_succeeded());
    ASSERT_TRUE(manager.on_incoming_call("1002"));
    EXPECT_EQ(manager.snapshot().state, CallState::Ringing);
    EXPECT_EQ(manager.snapshot().peer, "1002");
    ASSERT_TRUE(manager.on_call_connected());
    ASSERT_TRUE(manager.on_call_ended());
    EXPECT_EQ(manager.snapshot().state, CallState::Ready);
    EXPECT_EQ(backend.hangup_requests, 0);
    EXPECT_TRUE(manager.snapshot().peer.empty());
}

TEST(CallManagerTest, RejectedBackendCallReportsFailureNotConnection)
{
    FakeCallBackend backend;
    backend.call_accepted = false;
    CallManager manager(&backend);
    ASSERT_TRUE(manager.request_registration());
    ASSERT_TRUE(manager.on_registration_succeeded());
    EXPECT_FALSE(manager.request_outgoing_call("1001"));
    EXPECT_EQ(manager.snapshot().state, CallState::Failed);
    EXPECT_EQ(manager.snapshot().error, "SIP backend rejected the outgoing call request");
    ASSERT_TRUE(manager.reset_after_failure());
    EXPECT_TRUE(manager.snapshot().peer.empty());
}

TEST(CallManagerTest, UnexpectedEventsCannotFabricateState)
{
    FakeCallBackend backend;
    CallManager manager(&backend);
    EXPECT_FALSE(manager.on_registration_succeeded());
    EXPECT_FALSE(manager.on_incoming_call("1001"));
    EXPECT_FALSE(manager.on_call_connected());
    EXPECT_FALSE(manager.on_call_ended());
    EXPECT_FALSE(manager.on_call_failed("unexpected"));
    EXPECT_EQ(manager.snapshot().state, CallState::Idle);
}

TEST(CallManagerTest, HangupRejectionReportsFailure)
{
    FakeCallBackend backend;
    backend.hangup_accepted = false;
    CallManager manager(&backend);
    ASSERT_TRUE(manager.request_registration());
    ASSERT_TRUE(manager.on_registration_succeeded());
    ASSERT_TRUE(manager.request_outgoing_call("1001"));
    EXPECT_FALSE(manager.request_hangup());
    EXPECT_EQ(manager.snapshot().state, CallState::Failed);
    EXPECT_EQ(manager.snapshot().error, "SIP backend rejected the hangup request");
}

TEST(CallManagerTest, ObserverSeesFinalPeerCleanupAndErrorState)
{
    FakeCallBackend backend;
    CallManager manager(&backend);
    std::vector<CallSnapshot> updates;
    manager.set_observer([&updates](const CallSnapshot &snapshot) { updates.push_back(snapshot); });
    ASSERT_TRUE(manager.request_registration());
    ASSERT_TRUE(manager.on_registration_succeeded());
    ASSERT_TRUE(manager.request_outgoing_call("1001"));
    ASSERT_TRUE(manager.on_call_ended());
    ASSERT_FALSE(updates.empty());
    EXPECT_EQ(updates.back().state, CallState::Ready);
    EXPECT_TRUE(updates.back().peer.empty());
    EXPECT_EQ(updates.back().error, "");
    EXPECT_EQ(updates.size(), 4U);
}

} // namespace
} // namespace voip::application
