#include <new>
#include <genesis.h>

using AllocationSize = decltype(sizeof(0));
#ifdef __STDCPP_DEFAULT_NEW_ALIGNMENT__
static constexpr AllocationSize alignment = __STDCPP_DEFAULT_NEW_ALIGNMENT__;
#else
static constexpr AllocationSize alignment = __BIGGEST_ALIGNMENT__;
#endif
static constexpr AllocationSize overhead = sizeof(void*) + alignment - 1;

[[noreturn]] static void fatal(const char* message)
{
    SYS_die(const_cast<char*>(message), NULL);
    __builtin_unreachable();
}

static void* allocate(AllocationSize size)
{
    // Leave room for MEM_alloc's 16-bit block header and alignment.
    if (size > 0xFFFC - overhead) return nullptr;
    void* base = MEM_alloc((size ? size : 1) + overhead);
    if (!base) return nullptr;

    // MEM_alloc only guarantees word alignment, not C++ new's default alignment.
    auto address = reinterpret_cast<__UINTPTR_TYPE__>(base);
    address = (address + overhead) & ~(static_cast<__UINTPTR_TYPE__>(alignment) - 1);
    void* result = reinterpret_cast<void*>(address);
    static_cast<void**>(result)[-1] = base;
    return result;
}

void* operator new(AllocationSize size)
{
    void* result = allocate(size);
    if (!result) fatal("C++ allocation failed");
    return result;
}

void* operator new[](AllocationSize size)
{
    return ::operator new(size);
}

void operator delete(void* pointer) noexcept
{
    if (pointer) MEM_free(static_cast<void**>(pointer)[-1]);
}

void operator delete[](void* pointer) noexcept
{
    ::operator delete(pointer);
}

void operator delete(void* pointer, AllocationSize) noexcept
{
    ::operator delete(pointer);
}

void operator delete[](void* pointer, AllocationSize) noexcept
{
    ::operator delete(pointer);
}

namespace std
{
    const nothrow_t nothrow{};
}

void* operator new(AllocationSize size, const std::nothrow_t&) noexcept
{
    return allocate(size);
}

void* operator new[](AllocationSize size, const std::nothrow_t&) noexcept
{
    return allocate(size);
}

void operator delete(void* pointer, const std::nothrow_t&) noexcept
{
    ::operator delete(pointer);
}

void operator delete[](void* pointer, const std::nothrow_t&) noexcept
{
    ::operator delete(pointer);
}

extern "C"
{
    [[noreturn]] void __cxa_pure_virtual()
    {
        fatal("C++ pure virtual call");
    }

    // A console game never exits, so global destructors are not registered.
    int __cxa_atexit(void (*)(void*), void*, void*)
    {
        return 0;
    }

    void* __dso_handle = &__dso_handle;
}
