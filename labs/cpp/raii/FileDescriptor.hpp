#pragma once

#include <unistd.h>

#include <utility>

namespace voip::lab {

class FileDescriptor
{
  public:
    FileDescriptor() noexcept = default;

    explicit FileDescriptor(const int fd) noexcept : fd_(fd) {}

    ~FileDescriptor()
    {
        reset();
    }

    FileDescriptor(const FileDescriptor &) = delete;
    FileDescriptor &operator=(const FileDescriptor &) = delete;

    FileDescriptor(FileDescriptor &&other) noexcept : fd_(std::exchange(other.fd_, -1)) {}

    FileDescriptor &operator=(FileDescriptor &&other) noexcept
    {
        if (this != &other) {
            reset();
            fd_ = std::exchange(other.fd_, -1);
        }
        return *this;
    }

    [[nodiscard]] int get() const noexcept
    {
        return fd_;
    }

    [[nodiscard]] explicit operator bool() const noexcept
    {
        return fd_ >= 0;
    }

    [[nodiscard]] int release() noexcept
    {
        return std::exchange(fd_, -1);
    }

    void reset(const int replacement = -1) noexcept
    {
        if (fd_ >= 0) {
            static_cast<void>(::close(fd_));
        }
        fd_ = replacement;
    }

  private:
    int fd_{-1};
};

} // namespace voip::lab
