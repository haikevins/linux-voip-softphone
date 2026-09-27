#pragma once

namespace voip::platform {

class UniqueFd
{
  public:
    static constexpr int invalid_fd = -1;

    UniqueFd() noexcept = default;
    explicit UniqueFd(int fd) noexcept;
    ~UniqueFd();

    UniqueFd(const UniqueFd &) = delete;
    UniqueFd &operator=(const UniqueFd &) = delete;

    UniqueFd(UniqueFd &&other) noexcept;
    UniqueFd &operator=(UniqueFd &&other) noexcept;

    [[nodiscard]] int get() const noexcept;
    [[nodiscard]] explicit operator bool() const noexcept;

    [[nodiscard]] int release() noexcept;
    void reset(int new_fd = invalid_fd) noexcept;

  private:
    int fd_{invalid_fd};
};

} // namespace voip::platform
