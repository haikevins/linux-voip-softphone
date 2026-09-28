#include "pjsip/SipIncomingCallCli.hpp"
#include "pjsip/SipIncomingCallClient.hpp"

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
    arguments.reserve(static_cast<std::size_t>(argc > 1 ? argc - 1 : 0));

    for (int index = 1; index < argc; ++index) {
        arguments.emplace_back(argv[index]);
    }
    return arguments;
}

void print_registration_result(const voip::pjsip::RegistrationResult &result)
{
    std::cout << "[REGISTER] status=" << result.sip_status_code
              << " active=" << (result.registered ? "yes" : "no") << " reason=\"" << result.reason
              << "\"\n";
}

void print_incoming_result(const voip::pjsip::IncomingCallResult &result)
{
    std::cout << "[INCOMING] state=" << (result.state.empty() ? "UNKNOWN" : result.state)
              << " status=" << result.sip_status_code
              << " received=" << (result.received ? "yes" : "no")
              << " connected=" << (result.connected ? "yes" : "no")
              << " disconnected=" << (result.disconnected ? "yes" : "no") << " reason=\""
              << result.reason << '"';
    if (!result.remote_uri.empty()) {
        std::cout << " remote=\"" << result.remote_uri << '"';
    }
    std::cout << '\n';
}

} // namespace

int main(const int argc, char *argv[])
{
    try {
        auto options = voip::pjsip::parse_incoming_call_cli(command_arguments(argc, argv));
        if (options.show_help) {
            std::cout << voip::pjsip::incoming_call_cli_usage();
            return 0;
        }

        const auto *password = std::getenv(options.password_environment.c_str());
        if (password == nullptr || password[0] == '\0') {
            std::cerr << "listen error: environment variable " << options.password_environment
                      << " is not set or is empty\n";
            return 2;
        }
        options.account.password = password;

        std::cout << "[LISTEN] account=" << options.account.identity_uri() << '\n'
                  << "[LISTEN] registrar=" << options.account.registrar_uri() << '\n'
                  << "[LISTEN] transport=" << voip::pjsip::to_string(options.endpoint.transport)
                  << " listen-port=" << options.endpoint.listen_port << '\n';

        voip::pjsip::SipIncomingCallClient client{options.endpoint, options.account};
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

        auto call_result = client.wait_for_incoming_call(options.incoming_timeout);
        if (call_result.timed_out) {
            std::cerr << "[INCOMING] timeout after " << options.incoming_timeout.count() << " ms\n";
            return 1;
        }
        print_incoming_result(call_result);
        if (!call_result.received || call_result.disconnected) {
            return 1;
        }

        if (options.answer_delay > std::chrono::milliseconds::zero()) {
            std::cout << "[INCOMING] answering in " << options.answer_delay.count() << " ms\n";
            std::this_thread::sleep_for(options.answer_delay);
        }

        client.answer();
        call_result = client.wait_for_call(options.call_timeout);
        if (call_result.timed_out) {
            std::cerr << "[INCOMING] confirmation timeout after " << options.call_timeout.count()
                      << " ms\n";
            client.hangup();
            static_cast<void>(client.wait_for_disconnection(std::chrono::seconds{5}));
            return 1;
        }

        print_incoming_result(call_result);
        if (!call_result.connected) {
            return 1;
        }

        if (options.hold_duration > std::chrono::milliseconds::zero()) {
            std::cout << "[INCOMING] holding for " << options.hold_duration.count() << " ms\n";
            std::this_thread::sleep_for(options.hold_duration);
        }

        client.hangup();
        if (!client.wait_for_disconnection(std::chrono::seconds{5})) {
            std::cerr << "[INCOMING] hangup timed out\n";
            return 1;
        }

        std::cout << "[INCOMING] disconnected=yes\n";
        return 0;
    } catch (const pj::Error &error) {
        std::cerr << "PJSIP error: " << error.info() << '\n';
        return 1;
    } catch (const std::exception &error) {
        std::cerr << "listen error: " << error.what() << '\n';
        return 2;
    }
}
