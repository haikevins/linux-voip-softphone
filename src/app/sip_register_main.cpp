#include "pjsip/SipRegistrationCli.hpp"
#include "pjsip/SipRegistrationClient.hpp"

#include <cstdlib>
#include <exception>
#include <iostream>
#include <string_view>
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

} // namespace

int main(const int argc, char *argv[])
{
    try {
        auto options = voip::pjsip::parse_registration_cli(command_arguments(argc, argv));
        if (options.show_help) {
            std::cout << voip::pjsip::registration_cli_usage();
            return 0;
        }

        const auto *password = std::getenv(options.password_environment.c_str());
        if (password == nullptr || password[0] == '\0') {
            std::cerr << "registration error: environment variable " << options.password_environment
                      << " is not set or is empty\n";
            return 2;
        }
        options.account.password = password;

        std::cout << "[REGISTER] account=" << options.account.identity_uri() << '\n'
                  << "[REGISTER] registrar=" << options.account.registrar_uri() << '\n'
                  << "[REGISTER] transport=" << voip::pjsip::to_string(options.endpoint.transport)
                  << " listen-port=" << options.endpoint.listen_port << '\n';

        voip::pjsip::SipRegistrationClient client{options.endpoint, options.account};
        client.start();

        const auto result = client.wait_for_registration(options.timeout);
        if (result.timed_out) {
            std::cerr << "[REGISTER] timeout after " << options.timeout.count() << " ms\n";
            return 1;
        }

        std::cout << "[REGISTER] status=" << result.sip_status_code
                  << " active=" << (result.registered ? "yes" : "no");
        if (!result.reason.empty()) {
            std::cout << " reason=\"" << result.reason << '"';
        }
        std::cout << '\n';

        return result.registered ? 0 : 1;
    } catch (const pj::Error &error) {
        std::cerr << "PJSIP error: " << error.info() << '\n';
        return 1;
    } catch (const std::exception &error) {
        std::cerr << "registration error: " << error.what() << '\n';
        return 2;
    }
}
