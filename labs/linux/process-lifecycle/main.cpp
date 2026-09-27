#include <sys/wait.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <iostream>
#include <string_view>

namespace {

constexpr int kChildSuccessStatus = 42;
constexpr int kChildFailureStatus = 64;
constexpr int kForkFailureStatus = 1;
constexpr int kWaitFailureStatus = 2;
constexpr int kUnexpectedChildStatus = 3;

[[nodiscard]] bool self_test_requested(const int argc, char *const argv[])
{
    return argc == 2 && std::string_view{argv[1]} == "--self-test";
}

[[nodiscard]] int wait_for_child(const pid_t child_pid, int &status)
{
    pid_t waited_pid{-1};
    do {
        waited_pid = ::waitpid(child_pid, &status, 0);
    } while (waited_pid == -1 && errno == EINTR);

    if (waited_pid == -1) {
        std::cerr << "waitpid failed: " << std::strerror(errno) << '\n';
        return kWaitFailureStatus;
    }

    if (waited_pid != child_pid) {
        std::cerr << "waitpid returned an unexpected child pid\n";
        return kWaitFailureStatus;
    }

    return 0;
}

[[nodiscard]] int run_process_lifecycle(const bool quiet)
{
    const pid_t parent_pid = ::getpid();

    if (!quiet) {
        std::cout << "parent: pid=" << parent_pid << " creating child\n" << std::flush;
    }

    const pid_t child_pid = ::fork();
    if (child_pid == -1) {
        std::cerr << "fork failed: " << std::strerror(errno) << '\n';
        return kForkFailureStatus;
    }

    if (child_pid == 0) {
        const pid_t current_pid = ::getpid();
        const pid_t observed_parent_pid = ::getppid();
        const bool lifecycle_is_valid =
            current_pid != parent_pid && observed_parent_pid == parent_pid;

        if (!quiet) {
            std::cout << "child: pid=" << current_pid << " ppid=" << observed_parent_pid << '\n'
                      << std::flush;
        }

        ::_exit(lifecycle_is_valid ? kChildSuccessStatus : kChildFailureStatus);
    }

    int status{0};
    const int wait_result = wait_for_child(child_pid, status);
    if (wait_result != 0) {
        return wait_result;
    }

    if (!WIFEXITED(status)) {
        std::cerr << "child did not exit normally\n";
        return kUnexpectedChildStatus;
    }

    const int child_exit_status = WEXITSTATUS(status);
    if (!quiet) {
        std::cout << "parent: observed child pid=" << child_pid
                  << " exit_status=" << child_exit_status << '\n';
    }

    if (child_exit_status != kChildSuccessStatus) {
        std::cerr << "child lifecycle validation failed with status " << child_exit_status << '\n';
        return kUnexpectedChildStatus;
    }

    if (!quiet) {
        std::cout << "process lifecycle: PASS\n";
    }

    return 0;
}

} // namespace

int main(const int argc, char *argv[])
{
    return run_process_lifecycle(self_test_requested(argc, argv));
}
