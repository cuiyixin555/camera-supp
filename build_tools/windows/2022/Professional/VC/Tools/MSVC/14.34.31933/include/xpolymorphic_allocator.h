// xpolymorphic_allocator.h internal header

// Copyright (c) Microsoft Corporation.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once
#ifndef _XPOLYMORPHIC_ALLOCATOR_H
#define _XPOLYMORPHIC_ALLOCATOR_H
#include <yvals_core.h>
#if _STL_COMPILER_PREPROCESSOR
#include <tuple>
#include <type_traits>
#include <utility>
#include <xmemory>

#pragma pack(push, _CRT_PACKING)
#pragma warning(push, _STL_WARNING_LEVEL)
#pragma warning(disable : _STL_DISABLED_WARNINGS)
_STL_DISABLE_CLANG_WARNINGS
#pragma push_macro("new")
#undef new

_STD_BEGIN

#if !_HAS_CXX20
template <class _Ty, class _Outer_alloc, class _Inner_alloc, class... _Types,
    enable_if_t<!_Is_specialization_v<_Ty, pair>, int> = 0>
void _Uses_allocator_construct(_Ty* const _Ptr, _Outer_alloc& _Outer, _Inner_alloc& _Inner, _Types&&... _Args) {
    // uses-allocator construction of *_Ptr by alloc _Outer propagating alloc _Inner, non-pair case
    if constexpr (uses_allocator_v<_Ty, _Inner_alloc>) {
        if constexpr (is_constructible_v<_Ty, allocator_arg_t, _Inner_alloc&, _Types...>) {
            allocator_traits<_Outer_alloc>::construct(
                _Outer, _Ptr, allocator_arg, _Inner, _STD forward<_Types>(_Args)...);
        } else {
            static_assert(is_constructible_v<_Ty, _Types..., _Inner_alloc&>,
                "N4901 [allocator.uses.trait]/1 requires "
                "is_constructible_v<T, Args..., Alloc&> when uses_allocator_v<T, Alloc> is true and "
                "is_constructible_v<T, allocator_arg_t, Alloc&, Args...> is false");
            allocator_traits<_Outer_alloc>::construct(_Outer, _Ptr, _STD forward<_Types>(_Args)..., _Inner);
        }
    } else {
        static_assert(is_constructible_v<_Ty, _Types...>,
            "N4901 [allocator.uses.trait]/1 requires "
            "is_constructible_v<T, Args...> when uses_allocator_v<T, Alloc> is false");
        allocator_traits<_Outer_alloc>::construct(_Outer, _Ptr, _STD forward<_Types>(_Args)...);
    }
}

template <class _Ty, class _Alloc, class... _Types>
decltype(auto) _Uses_allocator_piecewise(_Alloc& _Al, tuple<_Types...>&& _Tuple) {
    if constexpr (uses_allocator_v<_Ty, _Alloc>) {
        if constexpr (is_constructible_v<_Ty, allocator_arg_t, _Alloc&, _Types...>) {
            return _STD tuple_cat(tuple<allocator_arg_t, _Alloc&>(allocator_arg, _Al), _STD move(_Tuple));
        } else {
            return _STD tuple_cat(_STD move(_Tuple), tuple<_Alloc&>(_Al));
        }
    } else {
        return _STD move(_Tuple);
    }
}

template <class _Ty1, class _Ty2, class _Outer_alloc, class _Inner_alloc, class... _Types1, class... _Types2>
void _Uses_allocator_construct_pair(pair<_Ty1, _Ty2>* const _Ptr, _Outer_alloc& _Outer, _Inner_alloc& _Inner,
    tuple<_Types1...>&& _Val1, tuple<_Types2...>&& _Val2) {
    // uses-allocator construction of pair from _Val1 and _Val2 by alloc _Outer propagating alloc _Inner
    allocator_traits<_Outer_alloc>::construct(_Outer, _Ptr, piecewise_construct,
        _Uses_allocator_piecewise<_Ty1>(_Inner, _STD move(_Val1)),
        _Uses_allocator_piecewise<_Ty2>(_Inner, _STD move(_Val2)));
}

template <class _Ty1, class _Ty2, class _Outer_alloc, class _Inner_alloc, class... _Types1, class... _Types2>
void _Uses_allocator_construct(pair<_Ty1, _Ty2>* const _Ptr, _Outer_alloc& _Outer, _Inner_alloc& _Inner,
    piecewise_construct_t, tuple<_Types1...> _Val1, tuple<_Types2...> _Val2) {
    // uses-allocator construction of pair by alloc _Outer propagating alloc _Inner, piecewise case
    _Uses_allocator_construct_pair(_Ptr, _Outer, _Inner, _STD move(_Val1), _STD move(_Val2));
}

template <class _Ty1, class _Ty2, class _Outer_alloc, class _Inner_alloc>
void _Uses_allocator_construct(pair<_Ty1, _Ty2>* const _Ptr, _Outer_alloc& _Outer, _Inner_alloc& _Inner) {
    // uses-allocator construction of pair by alloc _Outer propagating alloc _Inner, zero-argument case
    _Uses_allocator_construct_pair(_Ptr, _Outer, _Inner, tuple<>{}, tuple<>{});
}

template <class _Ty1, class _Ty2, class _Outer_alloc, class _Inner_alloc, class _Uty, class _Vty>
void _Uses_allocator_construct(
    pair<_Ty1, _Ty2>* const _Ptr, _Outer_alloc& _Outer, _Inner_alloc& _Inner, _Uty&& _Arg1, _Vty&& _Arg2) {
    // uses-allocator construction of pair by alloc _Outer propagating alloc _Inner, two-argument case
    _Uses_allocator_construct_pair(_Ptr, _Outer, _Inner, _STD forward_as_tuple(_STD forward<_Uty>(_Arg1)),
        _STD forward_as_tuple(_STD forward<_Vty>(_Arg2)));
}

template <class _Ty1, class _Ty2, class _Outer_alloc, class _Inner_alloc, class _Uty, class _Vty>
void _Uses_allocator_construct(
    pair<_Ty1, _Ty2>* const _Ptr, _Outer_alloc& _Outer, _Inner_alloc& _Inner, const pair<_Uty, _Vty>& _Pair) {
    // uses-allocator construction of pair by alloc _Outer propagating alloc _Inner, lvalue pair argument
    _Uses_allocator_construct_pair(
        _Ptr, _Outer, _Inner, _STD forward_as_tuple(_Pair.first), _STD forward_as_tuple(_Pair.second));
}

template <class _Ty1, class _Ty2, class _Outer_alloc, class _Inner_alloc, class _Uty, class _Vty>
void _Uses_allocator_construct(
    pair<_Ty1, _Ty2>* const _Ptr, _Outer_alloc& _Outer, _Inner_alloc& _Inner, pair<_Uty, _Vty>&& _Pair) {
    // uses-allocator construction of pair by alloc _Outer propagating alloc _Inner, rvalue pair argument
    _Uses_allocator_construct_pair(_Ptr, _Outer, _Inner, _STD forward_as_tuple(_STD forward<_Uty>(_Pair.first)),
        _STD forward_as_tuple(_STD forward<_Vty>(_Pair.second)));
}

template <class _Ty1, class _Ty2, class _Outer_alloc, class _Inner_alloc, class _Uty,
    enable_if_t<!_Is_deducible_as_pair<_Uty>, int> = 0>
void _Uses_allocator_construct(pair<_Ty1, _Ty2>* const _Ptr, _Outer_alloc& _Outer, _Inner_alloc& _Inner, _Uty&& _Ux) {
    // uses-allocator construction of pair by alloc _Outer propagating alloc _Inner, non-pair argument
    static_assert(_Is_normally_bindable<pair<_Ty1, _Ty2>, _Uty>,
        "The argument must be bindable to a reference to the std::pair type.");

    using _Pair_ref_t     = _Normally_bound_ref<pair<_Ty1, _Ty2>, _Uty>;
    _Pair_ref_t _Pair_ref = _STD forward<_Uty>(_Ux);
    if constexpr (is_same_v<_Pair_ref_t, const pair<_Ty1, _Ty2>&>) {
        _Uses_allocator_construct_pair(
            _Ptr, _Outer, _Inner, _STD forward_as_tuple(_Pair_ref.first), _STD forward_as_tuple(_Pair_ref.second));
    } else {
        _Uses_allocator_construct_pair(_Ptr, _Outer, _Inner, _STD forward_as_tuple(_STD forward<_Ty1>(_Pair_ref.first)),
            _STD forward_as_tuple(_STD forward<_Ty2>(_Pair_ref.second)));
    }
}
#endif // !_HAS_CXX20

#if _HAS_CXX17
namespace pmr {
    class __declspec(novtable) memory_resource {
    public:
        virtual ~memory_resource() noexcept = default;

        _NODISCARD_RAW_PTR_ALLOC __declspec(allocator) void* allocate(_CRT_GUARDOVERFLOW const size_t _Bytes,
            const size_t _Align = alignof(max_align_t)) { // allocate _Bytes bytes of memory with alignment _Align
            _STL_ASSERT(_Is_pow_2(_Align), "memory_resource::allocate(): Alignment must be a power of two.");
            void* _Ptr = do_allocate(_Bytes, _Align);
            return ::operator new(_Bytes, _Ptr);
        }

        void deallocate(void* const _Ptr, const size_t _Bytes, const size_t _Align = alignof(max_align_t)) {
            // deallocate _Ptr, which was returned from allocate(_Bytes, _Align)
            _STL_ASSERT(_Is_pow_2(_Align), "memory_resource::deallocate(): Alignment must be a power of two.");
            return do_deallocate(_Ptr, _Bytes, _Align);
        }

        _NODISCARD bool is_equal(const memory_resource& _That) const noexcept {
            // determine if *this and _That can both deallocate memory allocated by either
            return do_is_equal(_That);
        }

    private:
        virtual void* do_allocate(size_t _Bytes, size_t _Align)               = 0;
        virtual void do_deallocate(void* _Ptr, size_t _Bytes, size_t _Align)  = 0;
        virtual bool do_is_equal(const memory_resource& _That) const noexcept = 0;
    };

    _NODISCARD inline bool operator==(const memory_resource& _Left, const memory_resource& _Right) noexcept {
        return &_Left == &_Right || _Left.is_equal(_Right);
    }

#if !_HAS_CXX20
    _NODISCARD inline bool operator!=(const memory_resource& _Left, const memory_resource& _Right) noexcept {
        return !(_Left == _Right);
    }
#endif // !_HAS_CXX20

    extern "C" _CRT_SATELLITE_1 memory_resource* __cdecl _Aligned_get_default_resource() noexcept;
    extern "C" _CRT_SATELLITE_1 memory_resource* __cdecl _Unaligned_get_default_resource() noexcept;

    _NODISCARD inline memory_resource* get_default_resource() noexcept {
#ifdef __cpp_aligned_new
        return _Aligned_get_default_resource();
#else // ^^^ __cpp_aligned_new / !__cpp_aligned_new vvv
        return _Unaligned_get_default_resource();
#endif // __cpp_aligned_new
    }

#if _HAS_CXX20 && defined(__cpp_lib_byte)
    template <class _Ty = byte>
#else
    template <class _Ty>
#endif // _HAS_CXX20 && defined(__cpp_lib_byte)
    class polymorphic_allocator {
    public:
        template <class>
        friend class polymorphic_allocator;

        using value_type = _Ty;

        polymorphic_allocator() noexcept = default;

        /* implicit */ polymorphic_allocator(memory_resource* const _Resource_) noexcept // strengthened
            : _Resource{_Resource_} { // initialize with _Resource_
            _STL_ASSERT(_Resource, "Cannot initialize polymorphic_allocator with null resource (N4810 20.12.3.1 "
                                   "[mem.poly.allocator.ctor]/2)");
        }

        polymorphic_allocator(const polymorphic_allocator&) = default;

        template <class _Uty>
        polymorphic_allocator(const polymorphic_allocator<_Uty>& _That) noexcept
            : _Resource{_That._Resource} {} // initialize with _That's resource

        polymorphic_allocator& operator=(const polymorphic_allocator&) = delete;

        _NODISCARD_RAW_PTR_ALLOC __declspec(allocator) _Ty* allocate(_CRT_GUARDOVERFLOW const size_t _Count) {
            // get space for _Count objects of type _Ty from _Resource
            void* const _Vp = _Resource->allocate(_Get_size_of_n<sizeof(_Ty)>(_Count), alignof(_Ty));
            return static_cast<_Ty*>(_Vp);
        }

        void deallocate(_Ty* const _Ptr, const size_t _Count) noexcept /* strengthened */ {
            // return space for _Count objects of type _Ty to _Resource
            // No need to verify that size_t can represent the size of _Ty[_Count].
            _Resource->deallocate(_Ptr, _Count * sizeof(_Ty), alignof(_Ty));
        }

#if _HAS_CXX20
        _NODISCARD_RAW_PTR_ALLOC __declspec(allocator) void* allocate_bytes(
            const size_t _Bytes, const size_t _Align = alignof(max_align_t)) {
            return _Resource->allocate(_Bytes, _Align);
        }

        void deallocate_bytes(void* const _Ptr, const size_t _Bytes,
            const size_t _Align = alignof(max_align_t)) noexcept /* strengthened */ {
            _Resource->deallocate(_Ptr, _Bytes, _Align);
        }

        template <class _Uty>
        _NODISCARD_RAW_PTR_ALLOC __declspec(allocator) _Uty* allocate_object(
            _CRT_GUARDOVERFLOW const size_t _Count = 1) {
            void* const _Vp = allocate_bytes(_Get_size_of_n<sizeof(_Uty)>(_Count), alignof(_Uty));
            return static_cast<_Uty*>(_Vp);
        }

        template <class _Uty>
        void deallocate_object(_Uty* const _Ptr, const size_t _Count = 1) noexcept /* strengthened */ {
            deallocate_bytes(_Ptr, _Count * sizeof(_Uty), alignof(_Uty));
        }

        template <class _Uty, class... _Types>
        _NODISCARD_RAW_PTR_ALLOC __declspec(allocator) _Uty* new_object(_Types&&... _Args) {
            _Uty* const _Ptr = allocate_object<_Uty>();
            _TRY_BEGIN
            construct(_Ptr, _STD forward<_Types>(_Args)...);
            _CATCH_ALL
            deallocate_object(_Ptr);
            _RERAISE;
            _CATCH_END
            return _Ptr;
        }

        template <class _Uty>
        void delete_object(_Uty* const _Ptr) noexcept /* strengthened */ {
            _Destroy_in_place(*_Ptr);
            deallocate_object(_Ptr);
        }
#endif // _HAS_CXX20

        template <class _Uty, class... _Types>
        void construct(_Uty* const _Ptr, _Types&&... _Args) {
            // propagate allocator *this if uses_allocator_v<_Uty, polymorphic_allocator>
#if _HAS_CXX20
            _STD uninitialized_construct_using_allocator(_Ptr, *this, _STD forward<_Types>(_Args)...);
#else // ^^^ _HAS_CXX20 ^^^ / vvv !_HAS_CXX20 vvv
            allocator<char> _Al{};
            _Uses_allocator_construct(_Ptr, _Al, *this, _STD forward<_Types>(_Args)...);
#endif // ^^^ !_HAS_CXX20 ^^^
        }

        template <class _Uty>
        _CXX17_DEPRECATE_POLYMORPHIC_ALLOCATOR_DESTROY void destroy(_Uty* const _Ptr) noexcept /* strengthened */ {
            _Destroy_in_place(*_Ptr);
        }

        _NODISCARD polymorphic_allocator select_on_container_copy_construction() const noexcept /* strengthened */ {
            // don't propagate on copy
            return {};
        }

        _NODISCARD memory_resource* resource() const noexcept /* strengthened */ {
            // retrieve this allocator's memory_resource
            return _Resource;
        }

        _NODISCARD_FRIEND bool operator==(
            const polymorphic_allocator& _Lhs, const polymorphic_allocator& _Rhs) noexcept {
            return *_Lhs._Resource == *_Rhs._Resource;
        }

#if !_HAS_CXX20
        _NODISCARD_FRIEND bool operator!=(
            const polymorphic_allocator& _Lhs, const polymorphic_allocator& _Rhs) noexcept {
            return *_Lhs._Resource != *_Rhs._Resource;
        }
#endif // !_HAS_CXX20

    private:
        memory_resource* _Resource = _STD pmr::get_default_resource();
    };

    template <class _Ty1, class _Ty2>
    _NODISCARD bool operator==(
        const polymorphic_allocator<_Ty1>& _Left, const polymorphic_allocator<_Ty2>& _Right) noexcept {
        // polymorphic_allocators with the same resource are compatible
        return *_Left.resource() == *_Right.resource();
    }

#if !_HAS_CXX20
    template <class _Ty1, class _Ty2>
    _NODISCARD bool operator!=(
        const polymorphic_allocator<_Ty1>& _Left, const polymorphic_allocator<_Ty2>& _Right) noexcept {
        return !(_Left == _Right);
    }
#endif // !_HAS_CXX20

} // namespace pmr

template <class _Ty, class _Ptr>
struct _Has_no_alloc_destroy<pmr::polymorphic_allocator<_Ty>, _Ptr, void> : true_type {
    // polymorphic_allocator technically _does_ have a destroy member, but it's equivalent to the
    // default implementation in allocator_traits so we can optimize it away.
};

#endif // _HAS_CXX17

_STD_END

#pragma pop_macro("new")
_STL_RESTORE_CLANG_WARNINGS
#pragma warning(pop)
#pragma pack(pop)
#endif // _STL_COMPILER_PREPROCESSOR
#endif // _XPOLYMORPHIC_ALLOCATOR_H
