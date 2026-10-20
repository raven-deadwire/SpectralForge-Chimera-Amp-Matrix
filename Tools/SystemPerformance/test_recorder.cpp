#include "CallbackRecorder.h"
#include <cstdlib>
#include <iostream>
#include <new>
// This executable tests the adapter, not Chimera or any audio device.
static bool countAllocations = false;
static std::size_t allocations = 0;
void* operator new(std::size_t size) {
    if (countAllocations) ++allocations;
    if (void* ptr = std::malloc(size ? size : 1)) return ptr;
    throw std::bad_alloc();
}
void operator delete(void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
int main(int argc, char** argv) {
    if (argc != 2) return 2;
    chimera_perf::CallbackRecorder recorder(8);
    countAllocations = true;
    for (int i = 0; i < 9; ++i) {
        const auto stamp = recorder.begin();
        recorder.end(stamp, 128);
    }
    countAllocations = false;
    if (recorder.count() != 8 || recorder.droppedRows() != 1 || allocations != 0) return 1;
    recorder.writeCsv(argv[1]);
    std::cout << "collector-only: 8 rows, 1 retained overflow count, 0 callback C++ allocations\n";
}
