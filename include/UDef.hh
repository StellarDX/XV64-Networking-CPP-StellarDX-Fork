/****************************************************************************
* UDef.h -- Basic Type Definitions                                          *                         *
****************************************************************************/

#pragma once

#ifndef UDEF_H
#define UDEF_H

#define _EXTERN_C extern "C" {
#define _END_EXTERN_C }
#define _EXTERN_CXX //extern "C++" {
#define _END_EXTERN_CXX //}

#define __declspec(attrib) __attribute__((attrib))

#ifdef __cplusplus
_EXTERN_C
#endif

#include "types.h"

// ---------- M$ style types ---------- //

#define __interface struct

typedef unsigned long       ULONG;
typedef ULONG*              PULONG;
typedef unsigned short      USHORT;
typedef USHORT*             PUSHORT;
typedef unsigned char       UCHAR;
typedef UCHAR*              PUCHAR;
typedef char*               PSZ;

typedef unsigned long long  QWORD;
typedef unsigned int        DWORD;
#ifdef __cplusplus
typedef bool                BOOL;
#else
typedef _Bool               BOOL;
#endif
typedef unsigned char       BYTE;
typedef unsigned short      WORD;
typedef float               FLOAT;
typedef FLOAT*              PFLOAT;
typedef double              REAL;
typedef REAL*               PREAL;
typedef BOOL*               PBOOL, *LPBOOL;
typedef BYTE*               PBYTE, *LPBYTE;
typedef int*                PINT, *LPINT;
typedef WORD*               PWORD, *LPWORD;
typedef long*               LPLONG;
typedef DWORD*              PDWORD, *LPDWORD;
typedef QWORD*              PQWORD, *LPQWORD;
typedef void*               LPVOID;
typedef const void*         LPCVOID;

typedef int                 INT;
typedef unsigned int        UINT;
typedef unsigned int*       PUINT;

typedef char*               LPSTR;
typedef const char*         LPCSTR;

// -------------------------------------------

#ifdef __cplusplus

using size_t = decltype(sizeof(0));
using ptrdiff_t = decltype(static_cast<int*>(nullptr) - static_cast<int*>(nullptr));

#define _ADD_KERN_PRINT_FUNC void cprintf(char*, ...);
#define _ADD_KALLOC          char* kalloc(void);
#define _ADD_KFREE           void kfree(char*);
#define _ADD_PANIC           void panic(char*) __attribute__((noreturn));
#define _ADD_DELAY           void microdelay(int);
#define _ADD_PICENABLE       void picenable(int);
#define _ADD_IOAPICENABLE    void ioapicenable(int irq, int cpu);
#define _ADD_INITLOCK        void initlock(struct spinlock*, char*);
#define _ADD_ACQUIRE         void acquire(struct spinlock*);
#define _ADD_RELEASE         void release(struct spinlock*);

static const QWORD _KERNBASE = 0xFFFFFFFF80000000;
static const QWORD _DEVBASE  = 0xFFFFFFFF40000000;
static const QWORD _DEVSPACE = 0xFE000000;

inline LPVOID MMIOToVirtualAddress(QWORD Address)
{
    return LPVOID(Address + _DEVBASE - _DEVSPACE);
}

inline QWORD VirtualAddressToPhysical(LPCVOID Address)
{
    return QWORD(Address) - _KERNBASE;
}

inline LPVOID PhysicalAddressToVirtual(QWORD Address)
{
    return LPVOID(Address + _KERNBASE);
}

union IntegerSplitter
{
    QWORD I64;
    struct {DWORD Lo; DWORD Hi;};
    IntegerSplitter(QWORD _I64)
        : I64(_I64) {}
    operator QWORD() {return I64;}
};

typedef QWORD time_t;
_ADD_ACQUIRE
_ADD_RELEASE
extern struct spinlock tickslock;
extern uint ticks;

inline time_t time(time_t* Time)
{
    time_t Temp;
    acquire(&tickslock);
    Temp = ticks / 100;
    release(&tickslock);
    if (Time) {*Time = Temp;}
    return Temp;
}

#endif

#ifdef __cplusplus
_END_EXTERN_C
#endif

#ifdef __cplusplus

// Templates

template<class T, T v>
struct integral_constant
{
    static constexpr T value = v;
    using value_type = T;
    using type = integral_constant; // using injected-class-name
    constexpr operator value_type() const noexcept { return value; }
    constexpr value_type operator()() const noexcept { return value; } // since c++14
};

template <bool B>                                   // C++14
using bool_constant = integral_constant<bool, B>;   // C++14
typedef bool_constant<true> true_type;              // C++14
typedef bool_constant<false> false_type;

template <class _Tp, class _Up> struct IsSame : public false_type {};
template <class _Tp>            struct IsSame<_Tp, _Tp> : public true_type {};

// int128
using int128_t = __int128;
using uint128_t = unsigned __int128;

// CPP20 concepts

template <typename Base, typename Derived>
concept IsBaseOf = requires(Base* p1, Derived* p2)
{
    p1 = p2;
};

#endif

#endif // UDEF_H
