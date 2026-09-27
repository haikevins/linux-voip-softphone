#pragma once

#include "platform/linux/UniqueFd.hpp"

#include <string>

namespace voip::ipc {

class UnixDomainSocketServer
{
  public:
    explicit UnixDomainSocketServer(std::string socket_path);
    ~UnixDomainSocketServer();

    UnixDomainSocketServer(const UnixDomainSocketServer &) = delete;
    UnixDomainSocketServer &operator=(const UnixDomainSocketServer &) = delete;
    UnixDomainSocketServer(UnixDomainSocketServer &&) = delete;
    UnixDomainSocketServer &operator=(UnixDomainSocketServer &&) = delete;

    void start();
    void stop() noexcept;

    [[nodiscard]] platform::UniqueFd accept_one() const;
    [[nodiscard]] bool is_listening() const noexcept;
    [[nodiscard]] const std::string &path() const noexcept;

  private:
    void remove_socket_file() noexcept;

    std::string socket_path_;
    platform::UniqueFd listener_;
    bool owns_socket_path_{false};
};

} // namespace voip::ipc
