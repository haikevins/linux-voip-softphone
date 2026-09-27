#include "platform/linux/UniqueFd.hpp"

#include <cerrno>
#include <utility>

#include <fcntl.h>
#include <gtest/gtest.h>
#include <unistd.h>

namespace {

struct PipeEnds
{
    int read_fd;
    int write_fd;
};

PipeEnds make_pipe()
{
    int descriptors[2]{};
    EXPECT_EQ(::pipe(descriptors), 0);
    return {
        .read_fd = descriptors[0],
        .write_fd = descriptors[1],
    };
}

bool is_closed(const int fd)
{
    errno = 0;
    const int result = ::fcntl(fd, F_GETFD);
    return result == -1 && errno == EBADF;
}

} // namespace

TEST(UniqueFdTest, StartsInvalidByDefault)
{
    const voip::platform::UniqueFd descriptor;

    EXPECT_FALSE(static_cast<bool>(descriptor));
    EXPECT_EQ(descriptor.get(), voip::platform::UniqueFd::invalid_fd);
}

TEST(UniqueFdTest, ClosesOwnedDescriptorOnDestruction)
{
    const PipeEnds pipe = make_pipe();

    {
        const voip::platform::UniqueFd descriptor{pipe.read_fd};
        EXPECT_TRUE(static_cast<bool>(descriptor));
        EXPECT_EQ(descriptor.get(), pipe.read_fd);
    }

    EXPECT_TRUE(is_closed(pipe.read_fd));
    EXPECT_EQ(::close(pipe.write_fd), 0);
}

TEST(UniqueFdTest, MoveConstructionTransfersOwnership)
{
    const PipeEnds pipe = make_pipe();
    voip::platform::UniqueFd source{pipe.read_fd};

    const voip::platform::UniqueFd destination{std::move(source)};

    EXPECT_FALSE(static_cast<bool>(source)); // NOLINT(bugprone-use-after-move)
    EXPECT_EQ(source.get(),
              voip::platform::UniqueFd::invalid_fd); // NOLINT(bugprone-use-after-move)
    EXPECT_EQ(destination.get(), pipe.read_fd);

    EXPECT_EQ(::close(pipe.write_fd), 0);
}

TEST(UniqueFdTest, MoveAssignmentClosesPreviousDescriptor)
{
    const PipeEnds first_pipe = make_pipe();
    const PipeEnds second_pipe = make_pipe();

    voip::platform::UniqueFd destination{first_pipe.read_fd};
    voip::platform::UniqueFd source{second_pipe.read_fd};

    destination = std::move(source);

    EXPECT_TRUE(is_closed(first_pipe.read_fd));
    EXPECT_FALSE(static_cast<bool>(source)); // NOLINT(bugprone-use-after-move)
    EXPECT_EQ(destination.get(), second_pipe.read_fd);

    EXPECT_EQ(::close(first_pipe.write_fd), 0);
    EXPECT_EQ(::close(second_pipe.write_fd), 0);
}

TEST(UniqueFdTest, ReleaseReturnsDescriptorWithoutClosingIt)
{
    const PipeEnds pipe = make_pipe();
    voip::platform::UniqueFd descriptor{pipe.read_fd};

    const int released = descriptor.release();

    EXPECT_EQ(released, pipe.read_fd);
    EXPECT_FALSE(static_cast<bool>(descriptor));
    EXPECT_FALSE(is_closed(released));

    EXPECT_EQ(::close(released), 0);
    EXPECT_EQ(::close(pipe.write_fd), 0);
}

TEST(UniqueFdTest, ResetClosesOldDescriptorAndOwnsNewDescriptor)
{
    const PipeEnds first_pipe = make_pipe();
    const PipeEnds second_pipe = make_pipe();
    voip::platform::UniqueFd descriptor{first_pipe.read_fd};

    descriptor.reset(second_pipe.read_fd);

    EXPECT_TRUE(is_closed(first_pipe.read_fd));
    EXPECT_EQ(descriptor.get(), second_pipe.read_fd);

    EXPECT_EQ(::close(first_pipe.write_fd), 0);
    EXPECT_EQ(::close(second_pipe.write_fd), 0);
}
