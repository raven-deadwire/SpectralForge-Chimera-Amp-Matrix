#include "TesterCode.h"
#include "TesterCodeFixtures.h"
#include <iostream>

// The input and key are existing PUBLIC test fixtures, never customer codes.
int main()
{
    using namespace spectralforge::beta::tester;
    constexpr bool expected = CHIMERA_EXPECT_TESTER_ENABLED != 0;
    if (enabled() != expected || verify(testercode::zielCode).valid != expected
        || verify("CHT1-NOBODY-000000").valid)
    {
        std::cerr << "FAIL: configured tester verifier did not match its isolated header\n";
        return 1;
    }
    if constexpr (expected)
    {
        const auto grant = verify(testercode::zielCode);
        if (grant.name != "ZIEL" || canonical(grant) != testercode::zielCode)
            return 1;
    }
    std::cout << "PASS: generated header and real product verifier agree\n";
    return 0;
}
