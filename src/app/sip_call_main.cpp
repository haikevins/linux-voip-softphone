#include "pjsip/SipOutgoingCallCli.hpp"
#include "pjsip/SipOutgoingCallClient.hpp"

#include <chrono>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <string_view>
#include <thread>
#include <vector>

namespace {

[[nodiscard]] std::vector<std::string_view> command_arguments(const int argc, char *argv[])
{
    std::vector<std::string_view> arguments;
    arguments.reserve(static_cast<std::size_t>(argc > 0 ? argc - 1 : 0));

    for (int index = 1; index < argc; ++index) {
        arguments.emplace_back(argv[index]);
    }
    return arguments;
}

void print_registration_result(const voip::pjsip::RegistrationResult &result)
{
    std::cout << "[REGISTER] status=" << result.sip_status_code
              << " active=" << (result.registered ? "yes" : "no");
    if (!result.reason.empty()) {
        std::cout << " reason=\"" << result.reason << '"';
    }
    std::cout << '\n';
}

void print_call_result(const voip::pjsip::OutgoingCallResult &result)
{
    std::cout << "[CALL] state=" << (result.state.empty() ? "UNKNOWN" : result.state)
              << " status=" << result.sip_status_code
              << " connected=" << (result.connected ? "yes" : "no")
              << " disconnected=" << (result.disconnected ? "yes" : "no");
    if (!result.reason.empty()) {
        std::cout << " reason=\"" << result.reason << '"';
    }
    if (!result.remote_uri.empty()) {
        std::cout << " remote=\"" << result.remote_uri << '"';
    }
    std::cout << '\n';
}

} // namespace

int main(const int argc, char *argv[])
{
    try {
        auto options = voip::pjsip::parse_outgoing_call_cli(command_arguments(argc, argv));
        if (options.show_help) {
            std::cout << voip::pjsip::outgoing_call_cli_usage();
            return 0;
        }

        const auto *password = std::getenv(options.password_environment.c_str());
        if (password == nullptr || password[0] == '\0') {
            std::cerr << "call error: environment variable " << options.password_environment
                      << " is not set or is empty\n";
            return 2;
        }
        options.account.password = password;

        const auto destination_uri = options.destination_uri();
        std::cout << "[CALL] account=" << options.account.identity_uri() << '\n'
                  << "[CALL] registrar=" << options.account.registrar_uri() << '\n'
                  << "[CALL] target=" << destination_uri << '\n'
                  << "[CALL] transport=" << voip::pjsip::to_string(options.endpoint.transport)
                  << " listen-port=" << options.endpoint.listen_port << '\n';

        voip::pjsip::SipOutgoingCallClient client{options.endpoint, options.account};
        client.start();

        const auto registration = client.wait_for_registration(options.registration_timeout);
        if (registration.timed_out) {
            std::cerr << "[REGISTER] timeout after " << options.registration_timeout.count()
                      << " ms\n";
            return 1;
        }
        print_registration_result(registration);
        if (!registration.registered) {
            return 1;
        }

        client.make_call(destination_uri);
        auto call_result = client.wait_for_call(options.call_timeout);
        if (call_result.timed_out) {
            std::cerr << "[CALL] timeout after " << options.call_timeout.count() << " ms\n";
            client.hangup();
            static_cast<void>(client.wait_for_disconnection(std::chrono::seconds{5}));
            return 1;
        }

        print_call_result(call_result);
        if (!call_result.connected) {
            return 1;
        }

        if (options.hold_duration > std::chrono::milliseconds::zero()) {
            std::cout << "[CALL] holding for " << options.hold_duration.count() << " ms\n";
            std::this_thread::sleep_for(options.hold_duration);
        }

        client.hangup();
        if (!client.wait_for_disconnection(std::chrono::seconds{5})) {
            std::cerr << "[CALL] hangup timed out\n";
            return 1;
        }

        std::cout << "[CALL] disconnected=yes\n";
        return 0;
    } catch (const pj::Error &error) {
        std::cerr << "PJSIP error: " << error.info() << '\n';
        return 1;
    } catch (const std::exception &error) {
        std::cerr << "call error: " << error.what() << '\n';
        return 2;
    }
}
