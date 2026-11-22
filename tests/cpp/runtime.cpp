#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>

// Keep the test runtime's ABI tokens separate from the host CRT's tokens.
#define __dso_handle test_dso_handle
#define __cxa_atexit test_cxa_atexit
#define __cxa_pure_virtual test_cxa_pure_virtual
#include "../../src/cpp/runtime.cpp"

static bool failAllocation;
static unsigned allocations;
static unsigned frees;
static void* lastBase;
static unsigned short lastSize;

extern "C" void* MEM_alloc(unsigned short size)
{
    allocations++;
    lastSize = size;
    if (failAllocation) return nullptr;
    // Deliberately provide only SGDK's two-byte alignment.
    auto* bytes = static_cast<unsigned char*>(std::malloc(size + 2));
    lastBase = bytes ? bytes + 2 : nullptr;
    return lastBase;
}

extern "C" void MEM_free(void* pointer)
{
    assert(pointer == lastBase);
    frees++;
    std::free(static_cast<unsigned char*>(pointer) - 2);
}

extern "C" void SYS_die(char* message, ...)
{
    std::fprintf(stderr, "%s\n", message);
    std::_Exit(77);
}

int main(int argc, char** argv)
{
    if (argc > 1)
    {
        if (std::strcmp(argv[1], "pure") == 0) test_cxa_pure_virtual();
        failAllocation = true;
        void* result = ::operator new(4);
        return result ? 1 : 2;
    }

    const AllocationSize maximum = 0xFFFC - overhead;
    const AllocationSize sizes[] = {0, 1, 2, 3, alignment - 1, alignment, alignment + 1, maximum};
    for (auto size : sizes)
    {
        void* pointer = ::operator new(size);
        assert(reinterpret_cast<__UINTPTR_TYPE__>(pointer) % alignment == 0);
        assert(lastSize == (size ? size : 1) + overhead);
        std::memset(pointer, 0xA5, size);
        ::operator delete(pointer);
    }

    void* pointer = ::operator new[](8);
    ::operator delete[](pointer);
    pointer = ::operator new(8);
    ::operator delete(pointer, AllocationSize(8));
    pointer = ::operator new[](8);
    ::operator delete[](pointer, AllocationSize(8));
    pointer = ::operator new(8, std::nothrow);
    ::operator delete(pointer, std::nothrow);
    pointer = ::operator new[](8, std::nothrow);
    ::operator delete[](pointer, std::nothrow);

    const unsigned previousFrees = frees;
    ::operator delete(nullptr);
    ::operator delete[](nullptr);
    ::operator delete(nullptr, AllocationSize(0));
    ::operator delete[](nullptr, AllocationSize(0));
    ::operator delete(nullptr, std::nothrow);
    ::operator delete[](nullptr, std::nothrow);
    assert(frees == previousFrees);

    const unsigned previousAllocations = allocations;
    assert(::operator new(maximum + 1, std::nothrow) == nullptr);
    assert(::operator new[](65536, std::nothrow) == nullptr);
    volatile AllocationSize largest = AllocationSize(-1);
    assert(::operator new(largest, std::nothrow) == nullptr);
    assert(allocations == previousAllocations);
    failAllocation = true;
    assert(::operator new(8, std::nothrow) == nullptr);
    assert(::operator new[](8, std::nothrow) == nullptr);

    bool called = false;
    assert(test_cxa_atexit([](void* value) { *static_cast<bool*>(value) = true; }, &called, nullptr) == 0);
    assert(!called && test_dso_handle == &test_dso_handle);
    std::puts("PASS: allocation variants, alignment, zero/null, size bounds, OOM and ABI tokens");
}
