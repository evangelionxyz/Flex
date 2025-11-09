#include <gtest/gtest.h>

#include "Core/Types.h"
#include "Core/UUID.h"

namespace
{
    struct Sample
    {
        int value;
        explicit Sample(int v) : value(v) {}
    };
}

TEST(CoreTypesTest, CreateRefConstructsSharedInstance)
{
    auto object = flex::CreateRef<Sample>(42);
    ASSERT_NE(object, nullptr);
    EXPECT_EQ(object->value, 42);
    EXPECT_EQ(object.use_count(), 1);
}

TEST(CoreTypesTest, CreateScopeConstructsUniqueInstance)
{
    auto object = flex::CreateScope<Sample>(7);
    ASSERT_NE(object, nullptr);
    EXPECT_EQ(object->value, 7);
}

TEST(UUIDTest, ExplicitValueIsPreserved)
{
    constexpr uint64_t expected = 0x1234ABCDEFu;
    const flex::UUID uuid(expected);
    EXPECT_EQ(static_cast<uint64_t>(uuid), expected);
}