#ifndef TESTUTIL_ASSERT
#define TESTUTIL_ASSERT

#include <iostream>
#include <cstdlib>
#include <ctime>

//============================================================================
//                         Standard Test Macros
//============================================================================

unsigned int testutil_seed = 0;
static int ASSERT_COUNT = 0;
static void TESTUTIL_ASSERT_FUNCT(int c, const char *msg, const char *file, int line)
{
    if (c) {
        std::cout << "[" << testutil_seed << "] Error " << file
                  << "(" << line << "): " << msg
                  << "    (failed)" << std::endl;
        ++ASSERT_COUNT;
    }
}
#define ASSERT(X) { TESTUTIL_ASSERT_FUNCT(!(X), #X, __FILE__, __LINE__); }

#define TESTUTIL_INIT_RAND                                           \
{                                                                    \
    testutil_seed = static_cast<unsigned int>(std::time(0));         \
    std::srand(testutil_seed);                                       \
}

#endif // TESTUTIL_ASSERT
