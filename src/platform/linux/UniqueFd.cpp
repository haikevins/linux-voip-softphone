#include "platform/linux/UniqueFd.hpp"

#include <utility>

#include <unistd.h>

namespace voip::platform {

UniqueFd::UniqueFd(const int fd) noexcept : fd_(fd) {}

UniqueFd::~UniqueFd()
{
    reset();
}

UniqueFd::UniqueFd(UniqueFd &&other) noexcept : fd_(std::exchange(other.fd_, invalid_fd)) {}

UniqueFd &UniqueFd::operator=(UniqueFd &&other) noexcept
{
    if (this != &other) {
        reset(std::exchange(other.fd_, invalid_fd));
    }

    return *this;
}

int UniqueFd::get() const noexcept
{
    return fd_;
}

UniqueFd::operator bool() const noexcept
{
    return fd_ != invalid_fd;
}

int UniqueFd::release() noexcept
{
    return std::exchange(fd_, invalid_fd);
}

void UniqueFd::reset(const int new_fd) noexcept
{
    if (fd_ == new_fd) {
        return;
    }

    if (fd_ != invalid_fd) {
        static_cast<void>(::close(fd_));
    }

    fd_ = new_fd;
}

} // namespace voip::platform
