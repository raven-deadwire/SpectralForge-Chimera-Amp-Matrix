#pragma once
// Executable-only C++ allocation probe. No product compile definitions/hooks.
// Does not intercept malloc/realloc, OS/driver allocations or worker threads.
#include <cstdlib>
#include <new>
#include <cstddef>
#if defined(_WIN32)
#include <malloc.h>
#endif
namespace chimera_perf {
inline thread_local bool allocationScope = false;
inline thread_local std::size_t allocationCount = 0;
inline void allocation() noexcept { if (allocationScope) ++allocationCount; }
}
void* operator new(std::size_t n) {
    chimera_perf::allocation();
    if (auto* p = std::malloc(n ? n : 1)) return p;
    throw std::bad_alloc();
}
void* operator new[](std::size_t n) { return ::operator new(n); }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete[](void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
void operator delete[](void* p, std::size_t) noexcept { std::free(p); }
void* operator new(std::size_t n, std::align_val_t a) {
    chimera_perf::allocation();
    const auto alignment = static_cast<std::size_t>(a);
#if defined(_WIN32)
    auto* p = _aligned_malloc(n ? n : 1, alignment);
#else
    void* p = nullptr;
    if (posix_memalign(&p, alignment, n ? n : 1) != 0) p = nullptr;
#endif
    if (!p) throw std::bad_alloc();
    return p;
}
void* operator new[](std::size_t n, std::align_val_t a) { return ::operator new(n,a); }
void operator delete(void* p, std::align_val_t) noexcept {
#if defined(_WIN32)
    _aligned_free(p);
#else
    std::free(p);
#endif
}
void operator delete[](void* p, std::align_val_t a) noexcept { ::operator delete(p,a); }
void operator delete(void* p, std::size_t, std::align_val_t a) noexcept { ::operator delete(p,a); }
void operator delete[](void* p, std::size_t, std::align_val_t a) noexcept { ::operator delete(p,a); }
void* operator new(std::size_t n, const std::nothrow_t&) noexcept { try {return ::operator new(n);} catch (...) {return nullptr;} }
void* operator new[](std::size_t n, const std::nothrow_t& t) noexcept {return ::operator new(n,t);}
void operator delete(void* p, const std::nothrow_t&) noexcept {::operator delete(p);}
void operator delete[](void* p, const std::nothrow_t&) noexcept {::operator delete(p);}
void* operator new(std::size_t n, std::align_val_t a, const std::nothrow_t&) noexcept {try{return ::operator new(n,a);}catch(...){return nullptr;}}
void* operator new[](std::size_t n, std::align_val_t a, const std::nothrow_t& t) noexcept {return ::operator new(n,a,t);}
void operator delete(void* p, std::align_val_t a, const std::nothrow_t&) noexcept {::operator delete(p,a);}
void operator delete[](void* p, std::align_val_t a, const std::nothrow_t&) noexcept {::operator delete(p,a);}
