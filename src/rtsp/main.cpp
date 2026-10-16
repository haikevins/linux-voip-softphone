#include <iostream>
#include <string_view>

namespace {

void print_usage(std::ostream &output)
{
    output
        << "Usage: rtsp-lab --help\n"
        << "\n"
        << "Phase 10 RTSP mini-lab.\n"
        << "RTSP/1.0 message parsing is available; session behavior is added in later commits.\n";
}

} // namespace

int main(int argc, char **argv)
{
    if (argc == 2 && std::string_view{argv[1]} == "--help") {
        print_usage(std::cout);
        return 0;
    }

    print_usage(std::cerr);
    return 2;
}
