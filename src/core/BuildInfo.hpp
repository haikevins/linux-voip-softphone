#pragma once

#include <string_view>

namespace voip {

[[nodiscard]] std::string_view project_name() noexcept;
[[nodiscard]] std::string_view current_phase() noexcept;
[[nodiscard]] std::string_view cpp_standard() noexcept;

} // namespace voip
