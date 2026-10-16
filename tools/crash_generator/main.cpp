#include <cstdlib>
#include <cstdint>
#include <iostream>
#include <memory>
#include <string_view>

namespace {
constexpr std::string_view kNullDereferenceScenario{"null-deref"};
constexpr std::string_view kUseAfterFreeScenario{"use-after-free"};

void printUsage(std::ostream& output, std::string_view program_name)
{
    output << "Usage: " << program_name << " <scenario>\n"
           << "Scenarios:\n"
           << "  " << kNullDereferenceScenario
           << "  intentionally dereference a null pointer and terminate with SIGSEGV\n"
           << "  " << kUseAfterFreeScenario
           << "  exercise the repaired lifetime path that previously triggered a heap use-after-free\n";
}

[[noreturn]] void triggerNullDereference()
{
    std::cout << "Triggering intentional null dereference for the Phase 9 debugging lab.\n";
    std::cout.flush();

    volatile std::uintptr_t address = 0U;
    auto* pointer = reinterpret_cast<volatile int*>(address);
    *pointer = 42;

    std::abort();
}

#if defined(__GNUC__) || defined(__clang__)
__attribute__((noinline))
#endif
int readOwnedValue(const int* pointer)
{
    return *pointer;
}

bool verifyRepairedLifetime()
{
    std::cout << "Exercising repaired heap lifetime for the Phase 9 ASan regression.\n";

    auto owner = std::make_unique<int>(42);
    const int* observed_pointer = owner.get();

    const int observed_value = readOwnedValue(observed_pointer);
    if (observed_value != 42) {
        std::cerr << "Unexpected owned value: " << observed_value << '\n';
        return false;
    }

    owner.reset();
    observed_pointer = nullptr;

    std::cout << "Repaired lifetime path completed without accessing freed memory.\n";
    return true;
}
} // namespace

int main(int argc, char* argv[])
{
    const std::string_view program_name = argc > 0 ? argv[0] : "crash_generator";

    if (argc == 2) {
        const std::string_view argument{argv[1]};
        if (argument == "--help" || argument == "-h") {
            printUsage(std::cout, program_name);
            return EXIT_SUCCESS;
        }

        if (argument == kNullDereferenceScenario) {
            triggerNullDereference();
        }

        if (argument == kUseAfterFreeScenario) {
            return verifyRepairedLifetime() ? EXIT_SUCCESS : EXIT_FAILURE;
        }
    }

    printUsage(std::cerr, program_name);
    return EXIT_FAILURE;
}
