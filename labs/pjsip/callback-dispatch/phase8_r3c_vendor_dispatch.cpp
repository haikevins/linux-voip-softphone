// P8-R3C: opt-in REAL PJPROJECT 2.17 dispatch observation.
// Link ONLY against a separately built, lab-instrumented libpjsua2.
// This probe never dereferences or retains a vendor Call*, never attempts
// destruction while a vendor dispatch is paused, and never proves quiescence.
#include "pjsip/SipOutgoingCallClient.hpp"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <mutex>
#include <thread>

namespace {
using namespace std::chrono_literals;

std::atomic<bool> armed{false};
std::atomic<bool> claimed{false};
std::atomic<unsigned> vendor_lookups{0};
std::atomic<unsigned> matched_app_ingress{0};
thread_local unsigned pending_kind{0};
thread_local int pending_call_id{-1};
std::mutex pause_mutex;
std::condition_variable pause_changed;
bool paused{false};
bool witnessed{false};

void marker(const char *text)
{
    std::cout << text << '\n';
    std::cout.flush();
}

// Only the independently rebuilt PJPROJECT 2.17 endpoint.cpp invokes this.
// No pointer argument, no SIP data, and a strict 500ms upper bound on waiting.
} // namespace

extern "C" void voip_phase8_r3c_vendor_after_lookup(int call_id, unsigned kind) noexcept
{
    if (!armed.load(std::memory_order_acquire)) {
        return;
    }
    pending_kind = kind;
    pending_call_id = call_id;
    vendor_lookups.fetch_add(1, std::memory_order_relaxed);
    if (!claimed.exchange(true, std::memory_order_acq_rel)) {
        std::unique_lock lock{pause_mutex};
        paused = true;
        pause_changed.notify_all();
        static_cast<void>(
            pause_changed.wait_for(lock, std::chrono::milliseconds{500}, [] { return witnessed; }));
        paused = false;
        pause_changed.notify_all();
    }
}

// This lab-only hook executes immediately BEFORE the production client's
// CallbackActivityGate::enter() (on the same callback thread).
extern "C" void voip_phase8_r3c_app_before_gate(int call_id, unsigned kind) noexcept
{
    if (armed.load(std::memory_order_acquire) && pending_kind == kind &&
        pending_call_id == call_id) {
        matched_app_ingress.fetch_add(1, std::memory_order_relaxed);
        pending_kind = 0;
        pending_call_id = -1;
    }
}

namespace {

bool observe_real_dispatch()
{
    const char *password = std::getenv("VOIP_SIP_PASSWORD");
    if (password == nullptr || *password == '\0') {
        marker("P8R3C_RESULT=FAIL");
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

    {
        voip::pjsip::SipOutgoingCallClient client{endpoint, account};
        client.start();
        const auto registration = client.wait_for_registration(10s);
        if (!registration.registered || registration.timed_out ||
            registration.sip_status_code != 200) {
            client.stop();
            marker("P8R3C_REGISTRATION=FAIL");
            return false;
        }
        marker("P8R3C_REGISTRATION=PASS");

        // The observer confirms a REAL worker/thread paused between vendor
        // lookup and vendor dispatch. It does not attempt object retirement.
        std::jthread observer{[] {
            std::unique_lock lock{pause_mutex};
            if (pause_changed.wait_for(lock, 10s, [] { return paused; })) {
                witnessed = true;
                pause_changed.notify_all();
            }
        }};
        armed.store(true, std::memory_order_release);
        client.make_call("sip:1001@192.168.1.23:5060");
        marker("P8R3C_CALL_ATTEMPT=STARTED");
        const auto result = client.wait_for_call(3s);
        if (result.sip_status_code >= 400) {
            client.stop();
            return false;
        }

        client.hangup();
        if (!client.wait_for_disconnection(6s)) {
            client.stop();
            marker("P8R3C_DISCONNECTION=FAIL");
            return false;
        }
        marker("P8R3C_DISCONNECTION=PASS");

        // Only attempt the ordinary release after our *instrumented* pause
        // finishes; this does not claim other vendor dispatches are drained.
        {
            std::unique_lock lock{pause_mutex};
            if (!pause_changed.wait_for(lock, 1s, [] { return !paused; })) {
                marker("P8R3C_RELEASE_DISCONNECTED=FAIL");
                client.stop();
                return false;
            }
        }
        // Exercise the real, previously inventoried outgoing release path.
        if (!client.release_disconnected_call()) {
            client.stop();
            marker("P8R3C_RELEASE_DISCONNECTED=FAIL");
            return false;
        }
        marker("P8R3C_RELEASE_DISCONNECTED=RETURNED");
        client.stop();
        marker("P8R3C_STOP=RETURNED");
        armed.store(false, std::memory_order_release);
        observer.join();
    }

    marker("P8R3C_DESTRUCTOR=RETURNED");
    bool hold_witnessed;
    {
        const std::lock_guard lock{pause_mutex};
        hold_witnessed = witnessed && !paused;
    }
    const bool observed = hold_witnessed && claimed.load(std::memory_order_acquire) &&
                          vendor_lookups.load(std::memory_order_relaxed) > 0 &&
                          matched_app_ingress.load(std::memory_order_relaxed) > 0;
    marker(hold_witnessed ? "P8R3C_REAL_VENDOR_PRE_GATE_PAUSE=OBSERVED"
                          : "P8R3C_REAL_VENDOR_PRE_GATE_PAUSE=INCONCLUSIVE");
    marker(matched_app_ingress.load(std::memory_order_relaxed) > 0
               ? "P8R3C_VENDOR_TO_APP_INGRESS=OBSERVED"
               : "P8R3C_VENDOR_TO_APP_INGRESS=INCONCLUSIVE");
    marker("P8R3C_NO_RETIRE_WHILE_PAUSED=YES");
    marker("P8R3C_EXTERNAL_CALLBACK_QUIESCENCE=UNPROVEN");
    marker("P8R3C_INTEGRATION_READINESS=BLOCKED");
    marker(observed ? "P8R3C_RESULT=PASS" : "P8R3C_RESULT=INCONCLUSIVE");
    return observed;
}
} // namespace

int main()
{
    try {
        return observe_real_dispatch() ? EXIT_SUCCESS : EXIT_FAILURE;
    } catch (const std::exception &) {
        // Vendor exception messages can contain SIP details: never print them.
        marker("P8R3C_RESULT=FAIL_EXCEPTION");
        return EXIT_FAILURE;
    } catch (...) {
        marker("P8R3C_RESULT=FAIL_UNKNOWN");
        return EXIT_FAILURE;
    }
}
