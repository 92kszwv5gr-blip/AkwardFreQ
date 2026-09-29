#pragma once

#include <cstddef>

// Counts heap allocations made by the test binary, so tests can assert that real-time code allocates nothing.
// operator new is always replaced; malloc, calloc and realloc are wrapped too where the linker supports it
// (Linux, via -Wl,--wrap in tests/CMakeLists.txt), which also catches JUCE's HeapBlock and Array.
namespace afq::test
{
    void startCountingAllocations();
    size_t stopCountingAllocations(); // returns how many allocations happened since start
    bool countsMalloc();              // whether malloc is covered on this platform
}
