#include "core/BuildInfo.hpp"

#include <gtest/gtest.h>

TEST(BuildInfoTest, ReportsProjectName)
{
    EXPECT_EQ(voip::project_name(), "linux-voip-softphone");
}

TEST(BuildInfoTest, ReportsCpp20)
{
    EXPECT_EQ(voip::cpp_standard(), "C++20");
}

TEST(BuildInfoTest, ReportsPhaseZero)
{
    EXPECT_NE(voip::current_phase().find("Phase 0"), std::string_view::npos);
}
