#include "core/BuildInfo.hpp"

namespace voip {

std::string_view project_name() noexcept
{
    return "linux-voip-softphone";
}

std::string_view current_phase() noexcept
{
    return "Phase 7 - Qt/QML GUI";
}

std::string_view cpp_standard() noexcept
{
    return "C++20";
}

} // namespace voip
