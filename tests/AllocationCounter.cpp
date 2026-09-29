#include "AllocationCounter.h"
#include <atomic>
#include <cstdlib>
#include <new>

namespace
{
    std::atomic<bool> counting { false };
    std::atomic<size_t> allocations { 0 };

    inline void note() noexcept
    {
        if (counting.load (std::memory_order_relaxed)) allocations.fetch_add (1, std::memory_order_relaxed);
    }
}

#if defined(AFQ_WRAP_MALLOC)
extern "C"
{
    void* __real_malloc (size_t);
    void* __real_calloc (size_t, size_t);
    void* __real_realloc (void*, size_t);
    void* __wrap_malloc (size_t n)            { note(); return __real_malloc (n); }
    void* __wrap_calloc (size_t a, size_t b)  { note(); return __real_calloc (a, b); }
    void* __wrap_realloc (void* p, size_t n)  { note(); return __real_realloc (p, n); }
}
namespace { inline void* rawAlloc (size_t n) { return __real_malloc (n != 0 ? n : 1); } }
#else
namespace { inline void* rawAlloc (size_t n) { return std::malloc (n != 0 ? n : 1); } }
#endif

void* operator new (size_t n)                                { void* p = rawAlloc (n); if (p == nullptr) throw std::bad_alloc(); note(); return p; }
void* operator new[] (size_t n)                              { void* p = rawAlloc (n); if (p == nullptr) throw std::bad_alloc(); note(); return p; }
void* operator new (size_t n, const std::nothrow_t&) noexcept   { note(); return rawAlloc (n); }
void* operator new[] (size_t n, const std::nothrow_t&) noexcept { note(); return rawAlloc (n); }
void operator delete (void* p) noexcept                      { std::free (p); }
void operator delete[] (void* p) noexcept                    { std::free (p); }
void operator delete (void* p, size_t) noexcept              { std::free (p); }
void operator delete[] (void* p, size_t) noexcept            { std::free (p); }

namespace afq::test
{
    void startCountingAllocations() { allocations.store (0); counting.store (true); }

    size_t stopCountingAllocations()
    {
        counting.store (false);
        return allocations.load();
    }

    bool countsMalloc()
    {
       #if defined(AFQ_WRAP_MALLOC)
        return true;
       #else
        return false;
       #endif
    }
}
