#pragma once

#include "platform/linux/UniqueFd.hpp"

#include <string>

namespace voip::ipc {

class UnixDomainSocketClient
{
  public:
    explicit UnixDomainSocketClient(std::string socket_path);

    UnixDomainSocketClient(const UnixDomainSocketClient &) = delete;
    UnixDomainSocketClient &operator=(const UnixDomainSocketClient &) = delete;
    UnixDomainSocketClient(UnixDomainSocketClient &&) = delete;
    UnixDomainSocketClient &operator=(UnixDomainSocketClient &&) = delete;

    void connect();
    void disconnect() noexcept;

    [[nodiscard]] bool is_connected() const noexcept;
    [[nodiscard]] int native_handle() const noexcept;
    [[nodiscard]] const std::string &path() const noexcept;

  private:
    std::string socket_path_;
    platform::UniqueFd socket_;
};

} // namespace voip::ipc
