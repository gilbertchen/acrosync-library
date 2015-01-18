#ifndef TESTUTIL_NEWDELETEMONITOR
#define TESTUTIL_NEWDELETEMONITOR

#include <cstdio>
#include <cstdlib>
#include <new>

//============================================================================
//             New/Delete Overriding for Checking Memory Leak
//============================================================================

namespace {

static int BLOCKS_IN_USE = -1;

void checkMemoryOnExit()
{
    if (BLOCKS_IN_USE) {
        std::fprintf(stderr, "Memory Leak: %d block(s) not released\n", BLOCKS_IN_USE);
    }
}

void *allocateMemory(std::size_t size) throw (std::bad_alloc)
{
    if (BLOCKS_IN_USE == -1) {
        BLOCKS_IN_USE = 0;
        std::atexit(&checkMemoryOnExit);
    }
    ++BLOCKS_IN_USE;
    void *ptr = std::malloc(size);
    if (!ptr) {
        throw std::bad_alloc();
    }
    return ptr;
}

void releaseMemory(void *ptr)
{
    --BLOCKS_IN_USE;
    std::free(ptr);
}

} // unnamed namespace

void *operator new(std::size_t size) throw (std::bad_alloc)
{
    return allocateMemory(size);
}

void *operator new[] (std::size_t size) throw (std::bad_alloc)
{
    return allocateMemory(size);
}

void operator delete(void *ptr) throw()
{
    releaseMemory(ptr);
}

void operator delete[](void *ptr) throw()
{
    releaseMemory(ptr);
}

#endif // TESTUTIL_NEWDELETEMONITOR
