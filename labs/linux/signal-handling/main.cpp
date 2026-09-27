#include <signal.h>
#include <unistd.h>

#include <cerrno>
#include <csignal>
#include <cstring>
#include <iostream>
#include <string_view>

namespace {

constexpr int kSetupFailureStatus = 1;
constexpr int kSendFailureStatus = 2;
constexpr int kWaitFailureStatus = 3;
constexpr int kUnexpectedSignalStatus = 4;
constexpr std::string_view kSelfTestFlag{"--self-test"};

volatile std::sig_atomic_t shutdown_requested = 0;
volatile std::sig_atomic_t last_signal = 0;

void shutdown_signal_handler(const int signal_number) noexcept
{
    last_signal = signal_number;
    shutdown_requested = 1;
}

[[nodiscard]] bool self_test_requested(const int argc, char *const argv[])
{
    return argc == 2 && std::string_view{argv[1]} == kSelfTestFlag;
}

[[nodiscard]] bool add_shutdown_signals(sigset_t &signal_set)
{
    if (::sigemptyset(&signal_set) == -1) {
        return false;
    }

    return ::sigaddset(&signal_set, SIGINT) != -1 && ::sigaddset(&signal_set, SIGTERM) != -1;
}

[[nodiscard]] bool install_handler(const int signal_number)
{
    struct sigaction action
    {
    };
    action.sa_handler = shutdown_signal_handler;
    action.sa_flags = 0;

    if (::sigemptyset(&action.sa_mask) == -1) {
        return false;
    }

    return ::sigaction(signal_number, &action, nullptr) != -1;
}

[[nodiscard]] bool install_shutdown_handlers()
{
    return install_handler(SIGINT) && install_handler(SIGTERM);
}

[[nodiscard]] bool block_shutdown_signals(sigset_t &previous_mask)
{
    sigset_t shutdown_set{};
    if (!add_shutdown_signals(shutdown_set)) {
        return false;
    }

    return ::sigprocmask(SIG_BLOCK, &shutdown_set, &previous_mask) != -1;
}

[[nodiscard]] bool restore_signal_mask(const sigset_t &previous_mask)
{
    return ::sigprocmask(SIG_SETMASK, &previous_mask, nullptr) != -1;
}

[[nodiscard]] bool wait_for_shutdown_signal(const sigset_t &wait_mask)
{
    while (shutdown_requested == 0) {
        errno = 0;
        if (::sigsuspend(&wait_mask) == -1 && errno != EINTR) {
            return false;
        }
    }

    return true;
}

void reset_signal_state() noexcept
{
    shutdown_requested = 0;
    last_signal = 0;
}

[[nodiscard]] const char *signal_name(const int signal_number) noexcept
{
    switch (signal_number) {
    case SIGINT:
        return "SIGINT";
    case SIGTERM:
        return "SIGTERM";
    default:
        return "UNKNOWN";
    }
}

[[nodiscard]] int wait_for_one_signal(const int expected_signal, const sigset_t &wait_mask,
                                      const bool quiet)
{
    reset_signal_state();

    if (::kill(::getpid(), expected_signal) == -1) {
        std::cerr << "kill failed: " << std::strerror(errno) << '\n';
        return kSendFailureStatus;
    }

    if (!wait_for_shutdown_signal(wait_mask)) {
        std::cerr << "sigsuspend failed: " << std::strerror(errno) << '\n';
        return kWaitFailureStatus;
    }

    const int observed_signal = static_cast<int>(last_signal);
    if (observed_signal != expected_signal) {
        std::cerr << "expected signal " << expected_signal << " but observed " << observed_signal
                  << '\n';
        return kUnexpectedSignalStatus;
    }

    if (!quiet) {
        std::cout << "graceful shutdown requested by " << signal_name(observed_signal) << '\n';
    }

    return 0;
}

[[nodiscard]] int run_signal_demo(const bool self_test)
{
    sigset_t previous_mask{};
    if (!block_shutdown_signals(previous_mask)) {
        std::cerr << "sigprocmask failed: " << std::strerror(errno) << '\n';
        return kSetupFailureStatus;
    }

    if (!install_shutdown_handlers()) {
        std::cerr << "sigaction failed: " << std::strerror(errno) << '\n';
        static_cast<void>(restore_signal_mask(previous_mask));
        return kSetupFailureStatus;
    }

    if (self_test) {
        int result = wait_for_one_signal(SIGINT, previous_mask, true);
        if (result == 0) {
            result = wait_for_one_signal(SIGTERM, previous_mask, true);
        }

        if (!restore_signal_mask(previous_mask)) {
            std::cerr << "failed to restore signal mask: " << std::strerror(errno) << '\n';
            return kSetupFailureStatus;
        }

        if (result == 0) {
            std::cout << "signal handling: PASS\n";
        }
        return result;
    }

    std::cout << "signal demo: pid=" << ::getpid() << " waiting for SIGINT or SIGTERM\n"
              << std::flush;

    reset_signal_state();
    if (!wait_for_shutdown_signal(previous_mask)) {
        std::cerr << "sigsuspend failed: " << std::strerror(errno) << '\n';
        static_cast<void>(restore_signal_mask(previous_mask));
        return kWaitFailureStatus;
    }

    const int observed_signal = static_cast<int>(last_signal);
    if (!restore_signal_mask(previous_mask)) {
        std::cerr << "failed to restore signal mask: " << std::strerror(errno) << '\n';
        return kSetupFailureStatus;
    }

    std::cout << "graceful shutdown requested by " << signal_name(observed_signal) << '\n';
    return 0;
}

} // namespace

int main(const int argc, char *argv[])
{
    return run_signal_demo(self_test_requested(argc, argv));
}
