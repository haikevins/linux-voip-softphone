#pragma once

#include <string>
#include <variant>

namespace voip {

struct RegisterRequested
{
};

struct RegistrationSucceeded
{
};

struct RegistrationFailed
{
    std::string reason;
};

struct OutgoingCallRequested
{
    std::string destination;
};

struct IncomingCallReceived
{
    std::string peer;
};

struct RemoteRinging
{
};

struct CallConnected
{
};

struct HangupRequested
{
};

struct CallEnded
{
};

struct CallFailed
{
    std::string reason;
};

struct RecoverCallRequested
{
};

struct ResetRequested
{
};

using Event =
    std::variant<RegisterRequested, RegistrationSucceeded, RegistrationFailed,
                 OutgoingCallRequested, IncomingCallReceived, RemoteRinging, CallConnected,
                 HangupRequested, CallEnded, CallFailed, RecoverCallRequested, ResetRequested>;

} // namespace voip
