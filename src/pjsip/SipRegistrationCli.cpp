#include "pjsip/SipRegistrationCli.hpp"

#include <charconv>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <type_traits>

namespace voip::pjsip {
namespace {

[[nodiscard]] std::string_view require_value(const std::span<const std::string_view> arguments,
                                             std::size_t &index, const std::string_view option)
{
    if (index + 1U >= arguments.size()) {
        throw std::invalid_argument("missing value for " + std::string{option});
    }

    ++index;
    if (arguments[index].empty()) {
        throw std::invalid_argument("empty value for " + std::string{option});
    }
    return arguments[index];
}

struct IntegerArgument
{
    std::string_view value;
    std::string_view option;
};

template <typename Integer>
[[nodiscard]] Integer parse_integer(const IntegerArgument argument, const Integer minimum,
                                    const Integer maximum)
{
    const auto value = argument.value;
    const auto option = argument.option;
    static_assert(std::is_integral_v<Integer>);

    unsigned long long parsed_value{0U};
    const auto *const first = value.data();
    const auto *const last = value.data() + value.size();
    const auto [position, error] = std::from_chars(first, last, parsed_value);

    if (error != std::errc{} || position != last ||
        parsed_value > static_cast<unsigned long long>(maximum) ||
        parsed_value < static_cast<unsigned long long>(minimum)) {
        throw std::invalid_argument("invalid numeric value for " + std::string{option});
    }

    return static_cast<Integer>(parsed_value);
}

[[nodiscard]] SipTransport parse_transport(const std::string_view value)
{
    if (value == "udp") {
        return SipTransport::Udp;
    }
    if (value == "tcp") {
        return SipTransport::Tcp;
    }
    throw std::invalid_argument("--transport must be udp or tcp");
}

} // namespace

RegistrationCliOptions parse_registration_cli(const std::span<const std::string_view> arguments)
{
    RegistrationCliOptions options;

    for (std::size_t index = 0U; index < arguments.size(); ++index) {
        const auto argument = arguments[index];

        if (argument == "--help" || argument == "-h") {
            options.show_help = true;
            continue;
        }
        if (argument == "--username") {
            options.account.username = require_value(arguments, index, argument);
            continue;
        }
        if (argument == "--server") {
            options.account.server = require_value(arguments, index, argument);
            continue;
        }
        if (argument == "--server-port") {
            const auto value = require_value(arguments, index, argument);
            options.account.server_port = parse_integer<std::uint16_t>(
                {value, argument}, 1U, std::numeric_limits<std::uint16_t>::max());
            continue;
        }
        if (argument == "--listen-port") {
            const auto value = require_value(arguments, index, argument);
            options.endpoint.listen_port = parse_integer<std::uint16_t>(
                {value, argument}, 0U, std::numeric_limits<std::uint16_t>::max());
            continue;
        }
        if (argument == "--transport") {
            const auto value = require_value(arguments, index, argument);
            options.endpoint.transport = parse_transport(value);
            options.account.transport = options.endpoint.transport;
            continue;
        }
        if (argument == "--realm") {
            options.account.realm = require_value(arguments, index, argument);
            continue;
        }
        if (argument == "--timeout-ms") {
            const auto value = require_value(arguments, index, argument);
            const auto milliseconds = parse_integer<long long>(
                {value, argument}, 1LL, std::chrono::milliseconds::max().count());
            options.timeout = std::chrono::milliseconds{milliseconds};
            continue;
        }
        if (argument == "--password-env") {
            options.password_environment = require_value(arguments, index, argument);
            continue;
        }

        throw std::invalid_argument("unknown option: " + std::string{argument});
    }

    if (!options.show_help) {
        if (options.account.username.empty()) {
            throw std::invalid_argument("--username is required");
        }
        if (options.account.server.empty()) {
            throw std::invalid_argument("--server is required");
        }
    }

    return options;
}

std::string registration_cli_usage()
{
    return "Usage: voip-sip-register --username USER --server HOST [options]\n"
           "\n"
           "Required:\n"
           "  --username USER       SIP account username\n"
           "  --server HOST         SIP registrar hostname or IPv4 address\n"
           "\n"
           "Options:\n"
           "  --server-port PORT    Registrar port (default: 5060)\n"
           "  --listen-port PORT    Local SIP port; 0 lets PJSIP choose (default: 0)\n"
           "  --transport TYPE      udp or tcp (default: udp)\n"
           "  --realm REALM         Digest authentication realm (default: *)\n"
           "  --timeout-ms MS       Registration wait timeout (default: 10000)\n"
           "  --password-env NAME   Environment variable holding the password\n"
           "                        (default: VOIP_SIP_PASSWORD)\n"
           "  -h, --help            Show this help\n"
           "\n"
           "The password is intentionally read from an environment variable instead of\n"
           "the command line so it is not exposed in the process argument list.\n";
}

} // namespace voip::pjsip
