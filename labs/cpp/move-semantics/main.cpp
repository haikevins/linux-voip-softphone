#include <algorithm>
#include <cstddef>
#include <iostream>
#include <memory>
#include <utility>

namespace {

class PacketBuffer
{
  public:
    explicit PacketBuffer(const std::size_t size)
        : data_(std::make_unique<std::byte[]>(size)), size_(size)
    {}

    PacketBuffer(const PacketBuffer &) = delete;
    PacketBuffer &operator=(const PacketBuffer &) = delete;

    PacketBuffer(PacketBuffer &&other) noexcept
        : data_(std::move(other.data_)), size_(std::exchange(other.size_, 0U))
    {}

    PacketBuffer &operator=(PacketBuffer &&other) noexcept
    {
        if (this != &other) {
            data_ = std::move(other.data_);
            size_ = std::exchange(other.size_, 0U);
        }
        return *this;
    }

    [[nodiscard]] std::size_t size() const noexcept
    {
        return size_;
    }

    void fill(const std::byte value)
    {
        std::fill_n(data_.get(), size_, value);
    }

  private:
    std::unique_ptr<std::byte[]> data_;
    std::size_t size_{0};
};

} // namespace

int main()
{
    PacketBuffer original{1'024};
    original.fill(std::byte{0x2A});

    std::cout << "original before move: " << original.size() << '\n';

    PacketBuffer moved{std::move(original)};
    // Intentional: PacketBuffer explicitly defines moved-from size as zero.
    // NOLINTNEXTLINE(bugprone-use-after-move)
    std::cout << "original after move: " << original.size() << '\n';
    std::cout << "moved buffer size: " << moved.size() << '\n';

    PacketBuffer assigned{128};
    assigned = std::move(moved);
    // NOLINTNEXTLINE(bugprone-use-after-move)
    std::cout << "moved after assignment: " << moved.size() << '\n';
    std::cout << "assigned buffer size: " << assigned.size() << '\n';

    return 0;
}
