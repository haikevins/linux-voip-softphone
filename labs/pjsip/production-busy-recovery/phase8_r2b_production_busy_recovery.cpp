// P8-R2B: opt-in lab experiment on the actual production outgoing Account.
// Non-forced shutdown is attempted while the Call is active, before stop().
// This executable must not be included in unattended/CI live tests.
#include "pjsip/SipOutgoingCallClient.hpp"

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <iostream>

namespace {

using namespace std::chrono_literals;
using Client = voip::pjsip::SipOutgoingCallClient;

enum class LabResult : std::uint8_t
{
    Pass,
    Fail,
    Inconclusive
};

void marker(const char *line)
{
    std::cout << line << '\n';
    std::cout.flush();
}

[[nodiscard]] bool registered(Client &client, bool first)
{
    const auto result = client.wait_for_registration(10s);
    const bool ok = !result.timed_out && result.registered && result.sip_status_code == 200;
    if (first) {
        marker(ok ? "P8R2B_FIRST_REGISTRATION=PASS" : "P8R2B_FIRST_REGISTRATION=FAIL");
    } else {
        marker(ok ? "P8R2B_SECOND_REGISTRATION=PASS" : "P8R2B_SECOND_REGISTRATION=FAIL");
    }
    return ok;
}

[[nodiscard]] LabResult run_lab()
{
    const char *password = std::getenv("VOIP_SIP_PASSWORD");
    if (password == nullptr || password[0] == '\0') {
        marker("P8R2B_PASSWORD=MISSING");
        return LabResult::Fail;
    }

    voip::pjsip::SipEndpointConfig endpoint;
    endpoint.transport = voip::pjsip::SipTransport::Udp;
    endpoint.listen_port = 0;

    voip::pjsip::SipAccountConfig account;
    account.username = "1002";
    account.password = password;
    account.server = "192.168.1.23";
    account.transport = voip::pjsip::SipTransport::Udp;

    {
        Client client{endpoint, account};
        client.start();
        if (!registered(client, true)) {
            client.stop();
            return LabResult::Fail;
        }

        // Call::makeCall creates the PJSUA call slot synchronously. Do not
        // wait for remote response: this lab needs a still-active call slot.
        client.make_call("sip:1001@192.168.1.23:5060");
        marker("P8R2B_CALL_ATTEMPT=STARTED");

        const auto observed = client.lab_try_shutdown_while_call_active();
        switch (observed) {
        case Client::LabBusyShutdownResult::PreconditionsNotMet:
            marker("P8R2B_FIRST_SHUTDOWN=PRECONDITIONS_NOT_MET");
            break;
        case Client::LabBusyShutdownResult::BusyWithOwnersRetained:
            marker("P8R2B_FIRST_SHUTDOWN=PJ_EBUSY");
            marker("P8R2B_OWNERS_RETAINED=PASS");
            break;
        case Client::LabBusyShutdownResult::BusyOwnerCheckFailed:
            marker("P8R2B_FIRST_SHUTDOWN=PJ_EBUSY");
            marker("P8R2B_OWNERS_RETAINED=FAIL");
            break;
        case Client::LabBusyShutdownResult::UnexpectedSuccess:
            marker("P8R2B_FIRST_SHUTDOWN=UNEXPECTED_SUCCESS");
            break;
        case Client::LabBusyShutdownResult::OtherFailure:
            marker("P8R2B_FIRST_SHUTDOWN=OTHER_ERROR");
            break;
        }

        // Restore the normal production shutdown ordering even when the lab
        // early attempt was inconclusive. No force=true or owner release here.
        client.hangup();
        const bool disconnected = client.wait_for_disconnection(6s);
        marker(disconnected ? "P8R2B_CALL_DISCONNECTED=PASS" : "P8R2B_CALL_DISCONNECTED=FAIL");
        client.stop();
        marker("P8R2B_STOP_RETRY=RETURNED");

        if (!disconnected) {
            return LabResult::Fail;
        }
        if (observed != Client::LabBusyShutdownResult::BusyWithOwnersRetained) {
            return observed == Client::LabBusyShutdownResult::PreconditionsNotMet ||
                           observed == Client::LabBusyShutdownResult::UnexpectedSuccess
                       ? LabResult::Inconclusive
                       : LabResult::Fail;
        }

        // start() rejects any retained Account, Endpoint, Call or lib state.
        client.start();
        marker("P8R2B_SAME_OBJECT_RESTART=PASS");
        if (!registered(client, false)) {
            client.stop();
            return LabResult::Fail;
        }
        client.stop();
        marker("P8R2B_FINAL_STOP=RETURNED");
    }

    marker("P8R2B_DESTRUCTOR=RETURNED");
    return LabResult::Pass;
}

} // namespace

int main()
{
    try {
        switch (run_lab()) {
        case LabResult::Pass:
            marker("P8R2B_RESULT=PASS");
            return EXIT_SUCCESS;
        case LabResult::Inconclusive:
            marker("P8R2B_RESULT=INCONCLUSIVE");
            return 2;
        case LabResult::Fail:
            marker("P8R2B_RESULT=FAIL");
            return EXIT_FAILURE;
        }
    } catch (const std::exception &) {
        // Do not echo vendor exceptions: they can include SIP/private details.
        marker("P8R2B_RESULT=FAIL_EXCEPTION");
    } catch (...) {
        marker("P8R2B_RESULT=FAIL_UNKNOWN");
    }
    return EXIT_FAILURE;
}
