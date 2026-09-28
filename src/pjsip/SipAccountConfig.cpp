#include "pjsip/SipAccountConfig.hpp"

#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <string_view>

namespace voip::pjsip {
namespace {

[[nodiscard]] bool contains_whitespace(const std::string_view value)
{
    return std::any_of(value.begin(), value.end(), [](const char character) {
        return std::isspace(static_cast<unsigned char>(character)) != 0;
    });
}

[[nodiscard]] bool contains_any(const std::string_view value, const std::string_view characters)
{
    return value.find_first_of(characters) != std::string_view::npos;
}

} // namespace

void SipAccountConfig::validate() const
{
    if (username.empty()) {
        throw std::invalid_argument("SIP username must not be empty");
    }
    if (password.empty()) {
        throw std::invalid_argument("SIP password must not be empty");
    }
    if (server.empty()) {
        throw std::invalid_argument("SIP server must not be empty");
    }
    if (server_port == 0U) {
        throw std::invalid_argument("SIP server port must not be zero");
    }
    if (realm.empty()) {
        throw std::invalid_argument("SIP authentication realm must not be empty");
    }

    if (contains_whitespace(username) || contains_any(username, "@:;<>")) {
        throw std::invalid_argument("SIP username contains unsupported URI characters");
    }
    if (contains_whitespace(server) || contains_any(server, "@:;/<>")) {
        throw std::invalid_argument("SIP server must be a hostname or IPv4 address without scheme, "
                                    "port, or URI parameters");
    }
    if (contains_whitespace(realm)) {
        throw std::invalid_argument("SIP authentication realm must not contain whitespace");
    }
}

std::string SipAccountConfig::identity_uri() const
{
    validate();
    return "sip:" + username + "@" + server;
}

std::string SipAccountConfig::registrar_uri() const
{
    validate();
    return "sip:" + server + ":" + std::to_string(server_port) +
           ";transport=" + std::string{to_string(transport)};
}

pj::AccountConfig SipAccountConfig::to_pjsua2_account_config() const
{
    validate();

    pj::AccountConfig config;
    config.idUri = identity_uri();
    config.regConfig.registrarUri = registrar_uri();
    config.regConfig.registerOnAdd = register_on_add;
    config.sipConfig.authCreds.emplace_back("digest", realm, username, PJSIP_CRED_DATA_PLAIN_PASSWD,
                                            password);

    return config;
}

} // namespace voip::pjsip
