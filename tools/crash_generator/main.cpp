#include <cstdlib>
#include <cstdint>
#include <iostream>
#include <string_view>

namespace {
constexpr std::string_view kNullDereferenceScenario{"null-deref"};

void printUsage(std::ostream& output, std::string_view program_name)
{
    output << "Usage: " << program_name << " <scenario>\n"
           << "Scenarios:\n"
           << "  " << kNullDereferenceScenario
           << "  intentionally dereference a null pointer and terminate with SIGSEGV\n";
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
    }

    printUsage(std::cerr, program_name);
    return EXIT_FAILURE;
}
