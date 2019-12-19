#pragma once

#include <cstring>
#include <memory>
#include <type_traits>

#if defined(_MSC_VER)
#include <intrin.h>
#define __avx2_dispatch
#else
#include <immintrin.h>
#define __avx2_dispatch __attribute__((__target__("avx2")))
#endif

namespace sgm
{
template <class T>
struct aligned_deleter
{
    void operator()(T* p) const
    {
        _mm_free(p);
        p = nullptr;
    }
};

template <typename T>
using unique_ptr_aligned = typename std::unique_ptr<T[], aligned_deleter<T>>;

template <typename T, size_t Alignment = 16>
auto make_unique_aligned(size_t n) -> std::enable_if_t<std::is_arithmetic<T>::value, unique_ptr_aligned<T>>
{
    auto p = _mm_malloc(n * sizeof(T), Alignment);

    if (nullptr == p)
    {
        throw std::runtime_error("Failed to allocate aligned memory");
    }
    std::memset(p, 0, n * sizeof(T));

    return unique_ptr_aligned<T>(static_cast<T*>(p));
}

template <typename T>
inline T Min(T const& left, T const& right)
{
    return left < right ? left : right;
}

struct SimpleImage
{
    unique_ptr_aligned<uint8_t> Buffer;
    size_t Width;
    size_t Height;

    bool operator==(const SimpleImage& Other) const noexcept
    {
        return (Width == Other.Width) && (Height == Other.Height);
    }

    bool operator!=(const SimpleImage& Other) const noexcept
    {
        return !this->operator==(Other);
    }

    explicit operator bool() const noexcept
    {
        return nullptr != Buffer;
    }
};

}  // namespace sgm