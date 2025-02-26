// Copyright (c) Microsoft Corporation. All rights reserved.
//
// C11 atomic support routines
#pragma once

#ifdef __cplusplus
// this header should never be included in c++ mode, but if it is
// we need to catch it because the content of this header is provided by
// the STL's <atomic> header in C++
#error "vcruntime_c11_atomic_support.h is a C-only header"
#endif // __cplusplus

#include <crtdbg.h>
#include <intrin0.h>
#include <stdint.h>

// code from xatomic.h
#define _CONCATX(x, y) x##y
#define _CONCAT(x, y)  _CONCATX(x, y)

// Interlocked intrinsic mapping for _nf/_acq/_rel
#if defined(_M_CEE_PURE) || defined(_M_IX86) || (defined(_M_X64) && !defined(_M_ARM64EC))
#define _INTRIN_RELAXED(x) x
#define _INTRIN_ACQUIRE(x) x
#define _INTRIN_RELEASE(x) x
#define _INTRIN_ACQ_REL(x) x
#ifdef _M_CEE_PURE
#define _YIELD_PROCESSOR()
#else // ^^^ _M_CEE_PURE / !_M_CEE_PURE vvv
#define _YIELD_PROCESSOR() _mm_pause()
#endif // ^^^ !_M_CEE_PURE ^^^

#elif defined(_M_ARM) || defined(_M_ARM64) || defined(_M_ARM64EC)
#define _INTRIN_RELAXED(x) _CONCAT(x, _nf)
#define _INTRIN_ACQUIRE(x) _CONCAT(x, _acq)
#define _INTRIN_RELEASE(x) _CONCAT(x, _rel)
// We don't have interlocked intrinsics for acquire-release ordering, even on
// ARM32/ARM64, so fall back to sequentially consistent.
#define _INTRIN_ACQ_REL(x) x
#define _YIELD_PROCESSOR() __yield()

#else // ^^^ ARM32/ARM64 / unsupported hardware vvv
#error Unsupported hardware
#endif // hardware
// end code from xatomic.h


// The following is modified from the _CRT_SECURE_INVALID_PARAMETER macro in
// corecrt.h. We need to do this because this header must be C, not C++, but we
// still want to report invalid parameters in the same way as C++ does. The
// macro in the CRT expands to C++ code because it contains global namespace
// qualification. This can be fixed in the ucrt by using a mechanism that
// defines something like _GLOBAL_NAMESPACE to :: in c++ mode and nothing in C
// mode.
#ifndef _ATOMIC_INVALID_PARAMETER
#ifdef _DEBUG
#define _ATOMIC_INVALID_PARAMETER(expr) _invalid_parameter(_CRT_WIDE(#expr), L"", __FILEW__, __LINE__, 0)
#else
// By default, _ATOMIC_INVALID_PARAMETER in retail invokes
// _invalid_parameter_noinfo_noreturn(), which is marked
// __declspec(noreturn) and does not return control to the application.
// Even if _set_invalid_parameter_handler() is used to set a new invalid
// parameter handler which does return control to the application,
// _invalid_parameter_noinfo_noreturn() will terminate the application
// and invoke Watson. You can overwrite the definition of
// _ATOMIC_INVALID_PARAMETER if you need.
#define _ATOMIC_INVALID_PARAMETER(expr) _invalid_parameter_noinfo_noreturn()
#endif
#endif

// The following code is SHARED between the STL's <atomic> header and vcruntime's
// vcruntime_c11_atomic_support.h header. Any updates should be mirrored.
// Also: if any macros are added they should be #undefed in both headers

enum {
    _Atomic_memory_order_relaxed,
    _Atomic_memory_order_consume,
    _Atomic_memory_order_acquire,
    _Atomic_memory_order_release,
    _Atomic_memory_order_acq_rel,
    _Atomic_memory_order_seq_cst,
};

#ifndef _INVALID_MEMORY_ORDER
#ifdef _DEBUG
#define _INVALID_MEMORY_ORDER                              \
    do {                                                   \
        _RPTF0(_CRT_ASSERT, "Invalid memory order");       \
        _ATOMIC_INVALID_PARAMETER("Invalid memory order"); \
    } while (0)
#else // ^^^ _DEBUG / !_DEBUG vvv
#define _INVALID_MEMORY_ORDER
#endif // _DEBUG
#endif // _INVALID_MEMORY_ORDER

inline void _Check_memory_order(const unsigned int _Order) {
    if (_Order > _Atomic_memory_order_seq_cst) {
        _INVALID_MEMORY_ORDER;
    }
}

// this is different from the STL
// we are the MSVC runtime so we need not support clang here
#define _Compiler_barrier()                                                                   \
    _Pragma("warning(push)") _Pragma("warning(disable : 4996)") /* was declared deprecated */ \
        _ReadWriteBarrier() _Pragma("warning(pop)")

#if defined(_M_ARM) || defined(_M_ARM64) || defined(_M_ARM64EC)
#define _Memory_barrier()             __dmb(0xB) // inner shared data memory barrier
#define _Compiler_or_memory_barrier() _Memory_barrier()
#elif defined(_M_IX86) || defined(_M_X64)
// x86/x64 hardware only emits memory barriers inside _Interlocked intrinsics
#define _Compiler_or_memory_barrier() _Compiler_barrier()
#else // ^^^ x86/x64 / unsupported hardware vvv
#error Unsupported hardware
#endif // hardware

#if defined(_M_IX86) || (defined(_M_X64) && !defined(_M_ARM64EC))
#define _ATOMIC_CHOOSE_INTRINSIC(_Order, _Result, _Intrinsic, ...) \
    _Check_memory_order(_Order);                                   \
    _Result = _Intrinsic(__VA_ARGS__)
#elif defined(_M_ARM) || defined(_M_ARM64) || defined(_M_ARM64EC)
#define _ATOMIC_CHOOSE_INTRINSIC(_Order, _Result, _Intrinsic, ...) \
    switch (_Order) {                                              \
    case _Atomic_memory_order_relaxed:                             \
        _Result = _INTRIN_RELAXED(_Intrinsic)(__VA_ARGS__);        \
        break;                                                     \
    case _Atomic_memory_order_consume:                             \
    case _Atomic_memory_order_acquire:                             \
        _Result = _INTRIN_ACQUIRE(_Intrinsic)(__VA_ARGS__);        \
        break;                                                     \
    case _Atomic_memory_order_release:                             \
        _Result = _INTRIN_RELEASE(_Intrinsic)(__VA_ARGS__);        \
        break;                                                     \
    default:                                                       \
        _INVALID_MEMORY_ORDER;                                     \
        /* [[fallthrough]]; */                                     \
    case _Atomic_memory_order_acq_rel:                             \
    case _Atomic_memory_order_seq_cst:                             \
        _Result = _Intrinsic(__VA_ARGS__);                         \
        break;                                                     \
    }
#endif // hardware

// note: these macros are _not_ always safe to use with a trailing semicolon,
// we avoid wrapping them in do {} while (0) because MSVC generates code for such loops
// in debug mode.
#define _ATOMIC_LOAD_VERIFY_MEMORY_ORDER(_Order_var) \
    switch (_Order_var) {                            \
    case _Atomic_memory_order_relaxed:               \
        break;                                       \
    case _Atomic_memory_order_consume:               \
    case _Atomic_memory_order_acquire:               \
    case _Atomic_memory_order_seq_cst:               \
        _Compiler_or_memory_barrier();               \
        break;                                       \
    case _Atomic_memory_order_release:               \
    case _Atomic_memory_order_acq_rel:               \
    default:                                         \
        _INVALID_MEMORY_ORDER;                       \
        break;                                       \
    }

#define _ATOMIC_STORE_PREFIX(_Width, _Ptr, _Desired)      \
    case _Atomic_memory_order_relaxed:                    \
        __iso_volatile_store##_Width((_Ptr), (_Desired)); \
        return;                                           \
    case _Atomic_memory_order_release:                    \
        _Compiler_or_memory_barrier();                    \
        __iso_volatile_store##_Width((_Ptr), (_Desired)); \
        return;                                           \
    default:                                              \
    case _Atomic_memory_order_consume:                    \
    case _Atomic_memory_order_acquire:                    \
    case _Atomic_memory_order_acq_rel:                    \
        _INVALID_MEMORY_ORDER;                            \
        /* [[fallthrough]]; */


#define _ATOMIC_STORE_SEQ_CST_ARM(_Width, _Ptr, _Desired) \
    _Memory_barrier();                                    \
    __iso_volatile_store##_Width((_Ptr), (_Desired));     \
    _Memory_barrier();
#define _ATOMIC_STORE_SEQ_CST_X86_X64(_Width, _Ptr, _Desired) (void) _InterlockedExchange##_Width((_Ptr), (_Desired));
#define _ATOMIC_STORE_32_SEQ_CST_X86_X64(_Ptr, _Desired) \
    (void) _InterlockedExchange((volatile long*) (_Ptr), (long) (_Desired));

#define _ATOMIC_STORE_64_SEQ_CST_IX86(_Ptr, _Desired) \
    _Compiler_barrier();                              \
    __iso_volatile_store64((_Ptr), (_Desired));       \
    _Atomic_thread_fence(_Atomic_memory_order_seq_cst);

#if defined(_M_ARM) || defined(_M_ARM64) || defined(_M_ARM64EC)
#define _ATOMIC_STORE_SEQ_CST(_Width, _Ptr, _Desired) _ATOMIC_STORE_SEQ_CST_ARM(_Width, (_Ptr), (_Desired))
#define _ATOMIC_STORE_32_SEQ_CST(_Ptr, _Desired)      _ATOMIC_STORE_SEQ_CST_ARM(32, (_Ptr), (_Desired))
#define _ATOMIC_STORE_64_SEQ_CST(_Ptr, _Desired)      _ATOMIC_STORE_SEQ_CST_ARM(64, (_Ptr), (_Desired))
#else // ^^^ ARM32/ARM64/ARM64EC hardware / x86/x64 hardware vvv
#define _ATOMIC_STORE_SEQ_CST(_Width, _Ptr, _Desired) _ATOMIC_STORE_SEQ_CST_X86_X64(_Width, (_Ptr), (_Desired))
#define _ATOMIC_STORE_32_SEQ_CST(_Ptr, _Desired)      _ATOMIC_STORE_32_SEQ_CST_X86_X64((_Ptr), (_Desired))
#ifdef _M_IX86
#define _ATOMIC_STORE_64_SEQ_CST(_Ptr, _Desired) _ATOMIC_STORE_64_SEQ_CST_IX86((_Ptr), (_Desired))
#else // ^^^ x86 / x64 vvv
#define _ATOMIC_STORE_64_SEQ_CST(_Ptr, _Desired) _ATOMIC_STORE_SEQ_CST_X86_X64(64, (_Ptr), (_Desired))
#endif // x86/x64
#endif // hardware

inline void _Atomic_thread_fence(const unsigned int _Order) {
    if (_Order == _Atomic_memory_order_relaxed) {
        return;
    }

#if defined(_M_IX86) || (defined(_M_X64) && !defined(_M_ARM64EC))
    _Compiler_barrier();
    if (_Order == _Atomic_memory_order_seq_cst) {
        volatile long _Guard; // Not initialized to avoid an unnecessary operation; the value does not matter

        // _mm_mfence could have been used, but it is not supported on older x86 CPUs and is slower on some recent CPUs.
        // The memory fence provided by interlocked operations has some exceptions, but this is fine:
        // std::atomic_thread_fence works with respect to other atomics only; it may not be a full fence for all ops.
#pragma warning(suppress : 6001) // "Using uninitialized memory '_Guard'"
#pragma warning(suppress : 28113) // "Accessing a local variable _Guard via an Interlocked function: This is an unusual
                                  // usage which could be reconsidered."
        (void) _InterlockedIncrement(&_Guard);
        _Compiler_barrier();
    }
#elif defined(_M_ARM) || defined(_M_ARM64) || defined(_M_ARM64EC)
    _Memory_barrier();
#else // ^^^ ARM32/ARM64/ARM64EC / unsupported hardware vvv
#error Unsupported hardware
#endif // unsupported hardware
}
// End of code shared with vcruntime


inline void _Atomic_store8(volatile char* _Ptr, char _Desired, int _Order) {
    switch (_Order) {
        _ATOMIC_STORE_PREFIX(8, _Ptr, _Desired)
    case _Atomic_memory_order_seq_cst:
        _ATOMIC_STORE_SEQ_CST(8, _Ptr, _Desired)
        return;
    }
}

inline void _Atomic_store16(volatile short* _Ptr, short _Desired, int _Order) {
    switch (_Order) {
        _ATOMIC_STORE_PREFIX(16, _Ptr, _Desired)
    case _Atomic_memory_order_seq_cst:
        _ATOMIC_STORE_SEQ_CST(16, _Ptr, _Desired)
        return;
    }
}

inline void _Atomic_store32(volatile int* _Ptr, int _Desired, int _Order) {
    switch (_Order) {
        _ATOMIC_STORE_PREFIX(32, _Ptr, _Desired)
    case _Atomic_memory_order_seq_cst:
        _ATOMIC_STORE_32_SEQ_CST(_Ptr, _Desired)
        return;
    }
}

inline void _Atomic_store64(volatile long long* _Ptr, long long _Desired, int _Order) {
    switch (_Order) {
        _ATOMIC_STORE_PREFIX(64, _Ptr, _Desired)
    case _Atomic_memory_order_seq_cst:
        _ATOMIC_STORE_64_SEQ_CST(_Ptr, _Desired)
        return;
    }
}

inline char _Atomic_load8(const volatile char* _Ptr, int _Order) {
    char _As_bytes = __iso_volatile_load8(_Ptr);
    _ATOMIC_LOAD_VERIFY_MEMORY_ORDER(_Order);
    return _As_bytes;
}
inline short _Atomic_load16(const volatile short* _Ptr, int _Order) {
    short _As_bytes = __iso_volatile_load16(_Ptr);
    _ATOMIC_LOAD_VERIFY_MEMORY_ORDER(_Order);
    return _As_bytes;
}
inline int _Atomic_load32(const volatile int* _Ptr, int _Order) {
    int _As_bytes = __iso_volatile_load32(_Ptr);
    _ATOMIC_LOAD_VERIFY_MEMORY_ORDER(_Order);
    return _As_bytes;
}
inline long long _Atomic_load64(const volatile long long* _Ptr, int _Order) {
#ifdef _M_ARM
    long long _As_bytes = __ldrexd(_Ptr);
#else
    long long _As_bytes = __iso_volatile_load64(_Ptr);
#endif
    _ATOMIC_LOAD_VERIFY_MEMORY_ORDER(_Order);
    return _As_bytes;
}

inline _Bool _Atomic_compare_exchange_strong8(volatile char* _Ptr, char* _Expected, char _Desired, int _Order) {
    char _Prev_bytes;
    char _Expected_bytes = *_Expected;
    _ATOMIC_CHOOSE_INTRINSIC(_Order, _Prev_bytes, _InterlockedCompareExchange8, _Ptr, _Desired, _Expected_bytes);
    if (_Prev_bytes == _Expected_bytes) {
        return 1;
    }
    *_Expected = _Prev_bytes;
    return 0;
}
inline _Bool _Atomic_compare_exchange_strong16(volatile short* _Ptr, short* _Expected, short _Desired, int _Order) {
    short _Prev_bytes;
    short _Expected_bytes = *_Expected;
    _ATOMIC_CHOOSE_INTRINSIC(_Order, _Prev_bytes, _InterlockedCompareExchange16, _Ptr, _Desired, _Expected_bytes);
    if (_Prev_bytes == _Expected_bytes) {
        return 1;
    }
    *_Expected = _Prev_bytes;
    return 0;
}
inline _Bool _Atomic_compare_exchange_strong32(volatile int* _Ptr, int* _Expected, int _Desired, int _Order) {
    int _Prev_bytes;
    int _Expected_bytes = *_Expected;
    _ATOMIC_CHOOSE_INTRINSIC(
        _Order, _Prev_bytes, _InterlockedCompareExchange, (volatile long*) _Ptr, _Desired, _Expected_bytes);
    if (_Prev_bytes == _Expected_bytes) {
        return 1;
    }
    *_Expected = _Prev_bytes;
    return 0;
}

inline _Bool _Atomic_compare_exchange_strong64(
    volatile long long* _Ptr, long long* _Expected, long long _Desired, int _Order) {
    long long _Prev_bytes;
    long long _Expected_bytes = *_Expected;
    _ATOMIC_CHOOSE_INTRINSIC(_Order, _Prev_bytes, _InterlockedCompareExchange64, _Ptr, _Desired, _Expected_bytes);
    if (_Prev_bytes == _Expected_bytes) {
        return 1;
    }
    *_Expected = _Prev_bytes;
    return 0;
}
#undef _ATOMIC_CHOOSE_INTRINSIC
#undef _ATOMIC_LOAD_VERIFY_MEMORY_ORDER
#undef _ATOMIC_STORE_PREFIX
#undef _ATOMIC_STORE_SEQ_CST_ARM
#undef _ATOMIC_STORE_SEQ_CST_X86_X64
#undef _ATOMIC_STORE_32_SEQ_CST_X86_X64
#undef _ATOMIC_STORE_SEQ_CST
#undef _ATOMIC_STORE_32_SEQ_CST
#undef _ATOMIC_STORE_64_SEQ_CST
#undef _ATOMIC_STORE_64_SEQ_CST_IX86
#undef _ATOMIC_INVALID_PARAMETER

#undef _STD_COMPARE_EXCHANGE_128
#undef _INVALID_MEMORY_ORDER
#undef _Compiler_or_memory_barrier
#undef _Memory_barrier
#undef _Compiler_barrier

#undef _CONCATX
#undef _CONCAT
#undef _INTRIN_RELAXED
#undef _INTRIN_ACQUIRE
#undef _INTRIN_RELEASE
#undef _INTRIN_ACQ_REL
#undef _YIELD_PROCESSOR