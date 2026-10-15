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

TEST(BuildInfoTest, ReportsPhaseSeven)
{
    EXPECT_EQ(voip::current_phase(), "Phase 7 - Qt/QML GUI");
}
