#include <iostream>
#include <memory>
#include <string>
#include <utility>

namespace {

class AudioDevice
{
  public:
    explicit AudioDevice(std::string name) : name_(std::move(name))
    {
        std::cout << "AudioDevice created: " << name_ << '\n';
    }

    ~AudioDevice()
    {
        std::cout << "AudioDevice destroyed: " << name_ << '\n';
    }

    [[nodiscard]] const std::string &name() const noexcept
    {
        return name_;
    }

  private:
    std::string name_;
};

class CallContext
{
  public:
    explicit CallContext(std::shared_ptr<AudioDevice> device) : device_(std::move(device)) {}

    void print_device() const
    {
        std::cout << "Call uses: " << device_->name() << '\n';
    }

  private:
    std::shared_ptr<AudioDevice> device_;
};

class AudioMonitor
{
  public:
    explicit AudioMonitor(const std::shared_ptr<AudioDevice> &device) : device_(device) {}

    void inspect() const
    {
        if (const auto device = device_.lock()) {
            std::cout << "Monitor sees: " << device->name() << '\n';
        } else {
            std::cout << "Monitor: device already released\n";
        }
    }

  private:
    std::weak_ptr<AudioDevice> device_;
};

} // namespace

int main()
{
    auto exclusive_buffer = std::make_unique<std::string>("exclusive audio buffer");
    std::cout << *exclusive_buffer << '\n';

    auto device = std::make_shared<AudioDevice>("JBL TUNE115TWS");
    AudioMonitor monitor{device};

    {
        CallContext call{device};
        std::cout << "shared owners during call: " << device.use_count() << '\n';
        call.print_device();
        monitor.inspect();
    }

    std::cout << "shared owners after call: " << device.use_count() << '\n';
    device.reset();
    monitor.inspect();

    return 0;
}
