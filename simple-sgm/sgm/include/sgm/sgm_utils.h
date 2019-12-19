#pragma once

#include <cstdlib>
#include <memory>
#include <type_traits>

namespace sgm
{
template <class T>
struct aligned_deleter
{
    void operator()(T* p) const
    {
        std::free(p);
    }
};

template <typename T>
using unique_ptr_aligned = typename std::unique_ptr<T[], aligned_deleter<T>>;

template <typename T, size_t Alignment = 16>
auto make_unique_aligned(size_t n) -> std::enable_if_t<std::is_arithmetic<T>::value, unique_ptr_aligned<T>>
{
    auto p = std::aligned_alloc(Alignment, n * sizeof(T));
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

    bool operator==(const SimpleImage& Other) noexcept
    {
        return (Width == Other.Width) && (Height == Other.Height);
    }

    explicit operator bool() noexcept
    {
        return nullptr != Buffer;
    }
};

}  // namespace sgm