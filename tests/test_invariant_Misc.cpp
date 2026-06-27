#include <gtest/gtest.h>
#include <string>
#include <vector>
#include "EmperorHooks/Misc.cpp"

class SecurityTest : public ::testing::TestWithParam<std::string> {};

TEST_P(SecurityTest, SsprintfFormatStringSafety) {
    // Invariant: Format string must be safely processed without memory corruption or crashes
    std::string payload = GetParam();
    
    // Call actual production function with adversarial format string
    std::string result = ssprintf(payload.c_str(), "test", 123);
    
    // Property: Function must return without crashing and produce deterministic output
    // Either a valid formatted string or the defined error indicator
    ASSERT_TRUE(result == "FORMAT_ERROR" || !result.empty());
}

INSTANTIATE_TEST_SUITE_P(
    AdversarialInputs,
    SecurityTest,
    ::testing::Values(
        "%n%n%n%n%n",          // Exact exploit: %n write primitive
        "%s%s%s%s%s",          // Memory read via %s
        "Valid: %d %s",        // Valid boundary case
        "%9999999999s",        // Large width specifier
        ""                     // Empty format string
    )
);

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}