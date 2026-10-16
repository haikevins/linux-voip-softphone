#pragma once

#include "rtsp/RtspMessage.hpp"

namespace voip::rtsp {

class Connection
{
  public:
    virtual ~Connection() = default;

    Connection(const Connection &) = delete;
    Connection &operator=(const Connection &) = delete;
    Connection(Connection &&) = delete;
    Connection &operator=(Connection &&) = delete;

    virtual void connect() = 0;
    virtual void disconnect() noexcept = 0;
    [[nodiscard]] virtual bool is_connected() const noexcept = 0;
    [[nodiscard]] virtual Response transact(const Request &request) = 0;

  protected:
    Connection() = default;
};

} // namespace voip::rtsp
