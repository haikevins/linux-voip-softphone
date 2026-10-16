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
           << "  intentionally read freed heap memory for the AddressSanitizer lab\n";
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
int readFreedValue(const int* pointer)
{
    return *pointer;
}

void triggerUseAfterFree()
{
    std::cout << "Triggering intentional heap use-after-free for the Phase 9 ASan lab.\n";
    std::cout.flush();

    auto owner = std::make_unique<int>(42);
    const int* dangling = owner.get();
    owner.reset();

    volatile int observed = readFreedValue(dangling);
    std::cout << "Unexpected freed value: " << observed << '\n';
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
            triggerUseAfterFree();
            return EXIT_SUCCESS;
        }
    }

    printUsage(std::cerr, program_name);
    return EXIT_FAILURE;
}
