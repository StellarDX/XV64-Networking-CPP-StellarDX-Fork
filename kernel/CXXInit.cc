#include "CXXInit.h"

typedef void(*func)();
__attribute__((section(".ctors_begin"))) func ctors_begin = nullptr;
__attribute__((section(".ctors_end")))   func ctors_end = nullptr;
__attribute__((section(".dtors_begin"))) func dtors_begin = nullptr;
__attribute__((section(".dtors_end")))   func dtors_end = nullptr;

_EXTERN_C

_ADD_KERN_PRINT_FUNC
_ADD_KALLOC
_ADD_KFREE

void * __dso_handle = 0;

void __cxa_pure_virtual()
{
    // nothing...
    cprintf((char*)"Is pure virtual functions been called?");
}

void CXXInit()
{
    for (func *ctor = &ctors_begin + 1; ctor < &ctors_end; ++ctor)
    {
        (*ctor)();
    }
}

_END_EXTERN_C

void* operator new(size_t size)
{
    return kalloc();
}

void operator delete(void* ptr, size_t size)noexcept
{
    kfree((char*)ptr);
    ptr = nullptr;
}
