#include "FileDescriptor.hpp"

#include <fcntl.h>
#include <unistd.h>

#include <cerrno>
#include <iostream>
#include <utility>

int main()
{
    const int raw_fd = ::open("/dev/null", O_WRONLY);
    if (raw_fd < 0) {
        std::cerr << "open(/dev/null) failed\n";
        return 1;
    }

    const int observed_fd = raw_fd;

    {
        voip::lab::FileDescriptor first{raw_fd};
        std::cout << "first owns fd: " << std::boolalpha << static_cast<bool>(first) << '\n';

        voip::lab::FileDescriptor second{std::move(first)};
        // Intentional: FileDescriptor guarantees a moved-from descriptor is non-owning.
        // NOLINTNEXTLINE(bugprone-use-after-move)
        std::cout << "first after move: " << static_cast<bool>(first) << '\n';
        std::cout << "second after move: " << static_cast<bool>(second) << '\n';
    }

    errno = 0;
    const int result = ::fcntl(observed_fd, F_GETFD);
    std::cout << "fd closed by destructor: " << (result == -1 && errno == EBADF) << '\n';
    return 0;
}
