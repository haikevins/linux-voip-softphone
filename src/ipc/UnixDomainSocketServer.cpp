#include "ipc/UnixDomainSocketServer.hpp"

#include <cerrno>
#include <cstring>
#include <stdexcept>
#include <system_error>
#include <utility>

#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

namespace voip::ipc {
namespace {

void remove_stale_socket(const std::string &path)
{
    struct stat metadata
    {
    };

    if (::lstat(path.c_str(), &metadata) == 0) {
        if (!S_ISSOCK(metadata.st_mode)) {
            throw std::runtime_error("refusing to remove non-socket path: " + path);
        }

        if (::unlink(path.c_str()) == -1) {
            throw std::system_error(errno, std::generic_category(), "unlink stale Unix socket");
        }
        return;
    }

    if (errno != ENOENT) {
        throw std::system_error(errno, std::generic_category(), "inspect Unix socket path");
    }
}

sockaddr_un make_address(const std::string &path)
{
    sockaddr_un address{};
    address.sun_family = AF_UNIX;

    if (path.empty()) {
        throw std::invalid_argument("Unix socket path must not be empty");
    }

    if (path.size() >= sizeof(address.sun_path)) {
        throw std::invalid_argument("Unix socket path is too long");
    }

    std::memcpy(address.sun_path, path.c_str(), path.size() + 1U);
    return address;
}

} // namespace

UnixDomainSocketServer::UnixDomainSocketServer(std::string socket_path)
    : socket_path_(std::move(socket_path))
{}

UnixDomainSocketServer::~UnixDomainSocketServer()
{
    stop();
}

void UnixDomainSocketServer::start()
{
    if (listener_) {
        return;
    }

    const auto address = make_address(socket_path_);
    remove_stale_socket(socket_path_);

    platform::UniqueFd listener{::socket(AF_UNIX, SOCK_STREAM, 0)};
    if (!listener) {
        throw std::system_error(errno, std::generic_category(), "create Unix domain socket");
    }

    if (::bind(listener.get(), reinterpret_cast<const sockaddr *>(&address),
               static_cast<socklen_t>(sizeof(address))) == -1) {
        throw std::system_error(errno, std::generic_category(), "bind Unix domain socket");
    }

    owns_socket_path_ = true;

    if (::listen(listener.get(), SOMAXCONN) == -1) {
        const int error = errno;
        remove_socket_file();
        throw std::system_error(error, std::generic_category(), "listen on Unix domain socket");
    }

    listener_ = std::move(listener);
}

void UnixDomainSocketServer::stop() noexcept
{
    listener_.reset();
    remove_socket_file();
}

platform::UniqueFd UnixDomainSocketServer::accept_one() const
{
    if (!listener_) {
        throw std::logic_error("Unix domain socket server is not listening");
    }

    int accepted_fd = -1;
    do {
        accepted_fd = ::accept(listener_.get(), nullptr, nullptr);
    } while (accepted_fd == -1 && errno == EINTR);

    if (accepted_fd == -1) {
        throw std::system_error(errno, std::generic_category(), "accept Unix domain socket client");
    }

    return platform::UniqueFd{accepted_fd};
}

bool UnixDomainSocketServer::is_listening() const noexcept
{
    return static_cast<bool>(listener_);
}

const std::string &UnixDomainSocketServer::path() const noexcept
{
    return socket_path_;
}

void UnixDomainSocketServer::remove_socket_file() noexcept
{
    if (!owns_socket_path_) {
        return;
    }

    static_cast<void>(::unlink(socket_path_.c_str()));
    owns_socket_path_ = false;
}

} // namespace voip::ipc
