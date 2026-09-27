#include <sys/wait.h>
#include <unistd.h>

#include <cerrno>
#include <charconv>
#include <cstring>
#include <iostream>
#include <string>
#include <string_view>
#include <system_error>

namespace {

constexpr int kExecChildSuccessStatus = 43;
constexpr int kExecChildFailureStatus = 65;
constexpr int kForkFailureStatus = 1;
constexpr int kWaitFailureStatus = 2;
constexpr int kExecFailureStatus = 3;
constexpr int kUnexpectedChildStatus = 4;
constexpr std::string_view kSelfTestFlag{"--self-test"};
constexpr std::string_view kExecChildFlag{"--exec-child"};

[[nodiscard]] bool self_test_requested(const int argc, char *const argv[])
{
    return argc == 2 && std::string_view{argv[1]} == kSelfTestFlag;
}

[[nodiscard]] bool parse_pid(const std::string_view text, pid_t &pid)
{
    long long parsed_pid{0};
    const auto result = std::from_chars(text.data(), text.data() + text.size(), parsed_pid);
    if (result.ec != std::errc{} || result.ptr != text.data() + text.size() || parsed_pid <= 0) {
        return false;
    }

    pid = static_cast<pid_t>(parsed_pid);
    return static_cast<long long>(pid) == parsed_pid;
}

[[nodiscard]] int run_exec_child_mode(const int argc, char *const argv[])
{
    if (argc != 3 || std::string_view{argv[1]} != kExecChildFlag) {
        return kExecChildFailureStatus;
    }

    pid_t pid_before_exec{-1};
    if (!parse_pid(argv[2], pid_before_exec)) {
        return kExecChildFailureStatus;
    }

    const pid_t pid_after_exec = ::getpid();
    const bool pid_was_preserved = pid_after_exec == pid_before_exec;

    std::cout << "exec image: pid=" << pid_after_exec << " preserved=" << std::boolalpha
              << pid_was_preserved << '\n';

    return pid_was_preserved ? kExecChildSuccessStatus : kExecChildFailureStatus;
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

[[noreturn]] void replace_child_image(const pid_t child_pid, const bool quiet)
{
    if (!quiet) {
        std::cout << "child before exec: pid=" << child_pid << " replacing process image\n"
                  << std::flush;
    }

    std::string pid_text = std::to_string(static_cast<long long>(child_pid));
    char program_name[] = "lab_exec_child";
    char child_mode[] = "--exec-child";
    char *exec_argv[] = {program_name, child_mode, pid_text.data(), nullptr};

    ::execv("/proc/self/exe", exec_argv);

    std::cerr << "execv failed: " << std::strerror(errno) << '\n';
    ::_exit(kExecFailureStatus);
}

[[nodiscard]] int run_exec_demo(const bool quiet)
{
    if (!quiet) {
        std::cout << "parent: pid=" << ::getpid() << " forking exec child\n" << std::flush;
    }

    const pid_t child_pid = ::fork();
    if (child_pid == -1) {
        std::cerr << "fork failed: " << std::strerror(errno) << '\n';
        return kForkFailureStatus;
    }

    if (child_pid == 0) {
        replace_child_image(::getpid(), quiet);
    }

    int status{0};
    const int wait_result = wait_for_child(child_pid, status);
    if (wait_result != 0) {
        return wait_result;
    }

    if (!WIFEXITED(status)) {
        std::cerr << "exec child did not exit normally\n";
        return kUnexpectedChildStatus;
    }

    const int child_exit_status = WEXITSTATUS(status);
    if (!quiet) {
        std::cout << "parent: child pid=" << child_pid << " exit_status=" << child_exit_status
                  << '\n';
    }

    if (child_exit_status != kExecChildSuccessStatus) {
        std::cerr << "exec child validation failed with status " << child_exit_status << '\n';
        return kUnexpectedChildStatus;
    }

    if (!quiet) {
        std::cout << "exec replacement: PASS\n";
    }

    return 0;
}

} // namespace

int main(const int argc, char *argv[])
{
    if (argc >= 2 && std::string_view{argv[1]} == kExecChildFlag) {
        return run_exec_child_mode(argc, argv);
    }

    return run_exec_demo(self_test_requested(argc, argv));
}
