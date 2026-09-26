#include "core/CallState.hpp"

#include <gtest/gtest.h>

TEST(CallStateTest, ConvertsStatesToReadableNames)
{
    EXPECT_EQ(voip::to_string(voip::CallState::Idle), "Idle");
    EXPECT_EQ(voip::to_string(voip::CallState::Connected), "Connected");
    EXPECT_EQ(voip::to_string(voip::CallState::Failed), "Failed");
}

TEST(CallStateTest, AcceptsExpectedRegistrationTransitions)
{
    EXPECT_TRUE(voip::can_transition({
        .from = voip::CallState::Idle,
        .to = voip::CallState::Registering,
    }));
    EXPECT_TRUE(voip::can_transition({
        .from = voip::CallState::Registering,
        .to = voip::CallState::Ready,
    }));
    EXPECT_TRUE(voip::can_transition({
        .from = voip::CallState::Registering,
        .to = voip::CallState::Failed,
    }));
}

TEST(CallStateTest, RejectsInvalidTransitions)
{
    EXPECT_FALSE(voip::can_transition({
        .from = voip::CallState::Idle,
        .to = voip::CallState::Connected,
    }));
    EXPECT_FALSE(voip::can_transition({
        .from = voip::CallState::Ready,
        .to = voip::CallState::Connected,
    }));
    EXPECT_FALSE(voip::can_transition({
        .from = voip::CallState::Connected,
        .to = voip::CallState::Calling,
    }));
}
