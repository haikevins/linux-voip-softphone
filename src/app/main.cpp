#include "core/BuildInfo.hpp"

#include <iostream>

int main()
{
    std::cout << voip::project_name() << '\n'
              << voip::current_phase() << '\n'
              << "Standard: " << voip::cpp_standard() << '\n'
              << "Status: project skeleton is ready" << '\n';
    return 0;
}
