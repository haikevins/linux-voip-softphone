// P8-R2A: Opt-in Asterisk lab observation using the production outgoing client.
// This executable is not built by default and must never be run by CI.
#include "pjsip/SipOutgoingCallClient.hpp"

#include <chrono>
#include <cstdlib>
#include <exception>
#include <iostream>

namespace {

using namespace std::chrono_literals;

void marker(const char *line)
{
    std::cout << line << '\n';
    std::cout.flush();
}

[[nodiscard]] bool registered(voip::pjsip::SipOutgoingCallClient &client, bool first_round)
{
    const auto result = client.wait_for_registration(10s);
    if (result.timed_out || !result.registered || result.sip_status_code != 200) {
        if (first_round) {
            marker("P8R2A_FIRST_REGISTRATION=FAIL");
        } else {
            marker("P8R2A_SECOND_REGISTRATION=FAIL");
        }
        return false;
    }
    if (first_round) {
        marker("P8R2A_FIRST_REGISTRATION=PASS");
    } else {
        marker("P8R2A_SECOND_REGISTRATION=PASS");
    }
    return true;
}

[[nodiscard]] bool live_stop_reuse()
{
    const char *password = std::getenv("VOIP_SIP_PASSWORD");
    if (password == nullptr || password[0] == '\0') {
        marker("P8R2A_PASSWORD=MISSING");
        return false;
    }

    voip::pjsip::SipEndpointConfig endpoint;
    endpoint.transport = voip::pjsip::SipTransport::Udp;
    endpoint.listen_port = 0;

    voip::pjsip::SipAccountConfig account;
    account.username = "1002";
    account.password = password;
    account.server = "192.168.1.23";
    account.transport = voip::pjsip::SipTransport::Udp;

    // Destruction itself is part of the observation: an incomplete stop
    // deliberately invokes the production client's fail-closed policy.
    {
        voip::pjsip::SipOutgoingCallClient client{endpoint, account};
        client.start();
        if (!registered(client, true)) {
            client.stop();
            return false;
        }

        client.make_call("sip:1001@192.168.1.23:5060");
        marker("P8R2A_CALL_ATTEMPT=STARTED");
        const auto result = client.wait_for_call(3s);
        // A remote answer is not required in this stop/reuse lab. A 4xx/5xx
        // rejection, however, is not evidence of a usable call target.
        if (result.sip_status_code >= 400) {
            marker("P8R2A_CALL_RESPONSE=REJECTED");
            client.stop();
            return false;
        }
        if (result.timed_out) {
            marker("P8R2A_CALL_RESPONSE=PENDING");
        } else {
            marker("P8R2A_CALL_RESPONSE=COMPLETED");
        }

        client.hangup();
        if (!client.wait_for_disconnection(6s)) {
            marker("P8R2A_CALL_DISCONNECTED=FAIL");
            client.stop();
            return false;
        }
        marker("P8R2A_CALL_DISCONNECTED=PASS");
        client.stop();
        marker("P8R2A_FIRST_STOP=RETURNED");

        // start() rejects retained teardown state. This is the same object,
        // not a new client hiding a failed stop.
        client.start();
        marker("P8R2A_SAME_OBJECT_RESTART=PASS");
        if (!registered(client, false)) {
            client.stop();
            return false;
        }
        client.stop();
        marker("P8R2A_SECOND_STOP=RETURNED");
    }

    marker("P8R2A_DESTRUCTOR=RETURNED");
    marker("P8R2A_PJ_EBUSY=NOT_EXERCISED");
    return true;
}

} // namespace

int main()
{
    try {
        const bool passed = live_stop_reuse();
        if (passed) {
            marker("P8R2A_RESULT=PASS");
        } else {
            marker("P8R2A_RESULT=FAIL");
        }
        return passed ? EXIT_SUCCESS : EXIT_FAILURE;
    } catch (const std::exception &) {
        // Do not echo vendor exceptions: account/digest details may be present.
        marker("P8R2A_RESULT=FAIL_EXCEPTION");
        return EXIT_FAILURE;
    } catch (...) {
        marker("P8R2A_RESULT=FAIL_UNKNOWN");
        return EXIT_FAILURE;
    }
}
