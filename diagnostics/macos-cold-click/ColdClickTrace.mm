#include "ColdClickTrace.h"
#import <Cocoa/Cocoa.h>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <new>
#include <stdexcept>
#include <fcntl.h>
#include <mach/mach_time.h>
#include <pthread.h>
#include <sys/mman.h>
#include <unistd.h>

namespace coldClickTrace {
namespace {
constexpr std::uint64_t capacity=262144;
struct Header {
    char magic[8];
    std::uint64_t version, capacity, recordBytes, pid, mainThread, numerator, denominator;
    std::atomic<std::uint64_t> next, dropped;
    std::uint64_t startTicks;
    std::atomic<std::uint64_t> ended;
    std::uint64_t reserved[4];
};
struct Entry {
    std::atomic<std::uint64_t> published;
    std::uint64_t ticks, thread, event, a, b, c, d;
};
static_assert(sizeof(Header)==128 && sizeof(Entry)==64);
static_assert(std::atomic<std::uint64_t>::is_always_lock_free);
std::atomic<Header*> active{};
Header* mapping{};
CFRunLoopObserverRef early{}, late{};
constexpr auto mappingBytes=sizeof(Header)+capacity*sizeof(Entry);

std::uint64_t threadID() noexcept {
    std::uint64_t result{};
    pthread_threadid_np(nullptr,&result);
    return result;
}
void observe(CFRunLoopObserverRef observer, CFRunLoopActivity activity, void*) {
    record(observer==early ? observerEarly : observerLate,static_cast<std::uint64_t>(activity));
}
}

std::uint64_t record(Event event, std::uint64_t a, std::uint64_t b,
                     std::uint64_t c, std::uint64_t d) noexcept {
    auto* h=active.load(std::memory_order_acquire);
    if(h==nullptr) return 0;
    const auto index=h->next.fetch_add(1,std::memory_order_relaxed);
    if(index>=capacity) {
        h->dropped.fetch_add(1,std::memory_order_relaxed);
        return 0;
    }
    auto* entries=reinterpret_cast<Entry*>(h+1);
    auto& entry=entries[index];
    entry.ticks=mach_absolute_time();
    entry.thread=threadID();
    entry.event=event;
    entry.a=a; entry.b=b; entry.c=c; entry.d=d;
    entry.published.store(index+1,std::memory_order_release);
    return index+1;
}

Session::Session() {
    const auto* path=std::getenv("CHIMERA_COLD_CLICK_TRACE");
    if(path==nullptr || mapping!=nullptr) throw std::runtime_error("Diagnostic trace path/initialization invalid");
    const int fd=::open(path,O_CREAT|O_EXCL|O_RDWR,0600);
    if(fd<0) throw std::runtime_error("Cannot create unique diagnostic trace");
    if(ftruncate(fd,static_cast<off_t>(mappingBytes))!=0) {
        ::close(fd); throw std::runtime_error("Cannot size diagnostic trace");
    }
    void* bytes=mmap(nullptr,mappingBytes,PROT_READ|PROT_WRITE,MAP_SHARED,fd,0);
    ::close(fd);
    if(bytes==MAP_FAILED) throw std::runtime_error("Cannot map diagnostic trace");
    // Allocate and touch the bounded buffer before test setup, never in a click.
    std::memset(bytes,0,mappingBytes);
    mapping=new(bytes) Header{};
    auto* entries=reinterpret_cast<Entry*>(mapping+1);
    for(std::uint64_t i=0;i<capacity;++i) new(entries+i) Entry{};
    std::memcpy(mapping->magic,"CHMTRC01",8);
    mapping->version=1; mapping->capacity=capacity; mapping->recordBytes=sizeof(Entry);
    mapping->pid=static_cast<std::uint64_t>(getpid()); mapping->mainThread=threadID();
    mach_timebase_info_data_t timebase{};
    mach_timebase_info(&timebase);
    mapping->numerator=timebase.numer; mapping->denominator=timebase.denom;
    mapping->startTicks=mach_absolute_time();
    CFRunLoopObserverContext context{};
    early=CFRunLoopObserverCreate(kCFAllocatorDefault,kCFRunLoopAllActivities,true,
        std::numeric_limits<CFIndex>::min(),observe,&context);
    late=CFRunLoopObserverCreate(kCFAllocatorDefault,kCFRunLoopAllActivities,true,
        std::numeric_limits<CFIndex>::max(),observe,&context);
    if(early==nullptr || late==nullptr) throw std::runtime_error("Cannot create passive CF observers");
    CFRunLoopAddObserver(CFRunLoopGetMain(),early,kCFRunLoopDefaultMode);
    CFRunLoopAddObserver(CFRunLoopGetMain(),late,kCFRunLoopDefaultMode);
    active.store(mapping,std::memory_order_release);
    // Read the existing singleton only; do not create it or finish launch.
    record(sessionBegin,address(NSApp),NSApp!=nil && [NSApp isRunning]);
}

Session::~Session() noexcept {
    record(sessionEnd);
    active.store(nullptr,std::memory_order_release);
    CFRunLoopRemoveObserver(CFRunLoopGetMain(),early,kCFRunLoopDefaultMode);
    CFRunLoopRemoveObserver(CFRunLoopGetMain(),late,kCFRunLoopDefaultMode);
    CFRelease(early); CFRelease(late);
    mapping->ended.store(1,std::memory_order_release);
    // Mapping remains alive until process exit so an already-started producer
    // can publish safely. No synchronous flush occurs within a timed assertion.
    msync(mapping,mappingBytes,MS_SYNC);
}
}
