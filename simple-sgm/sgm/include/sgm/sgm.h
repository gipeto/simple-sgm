#pragma once

#include <cassert>
#include <sgm/sgm_utils.h>

namespace sgm
{
/*
  Implementation of the SemiGlobal Matching algorithm [1], with the following limitations:

  - The matching cost is a simple absolute difference of the pixel luminance
  - Only 8 path are considered
  - Penality P2 are not weighted by the image gradient
  - No consistency checks

  [1] Hirschmuller, H. (2005). Accurate and Efficient Stereo Processing by Semi Global Matching and Mutual Information.
  CVPR .

*/
template <size_t DMax, size_t DMin = 0, bool UseAVX2 = false>
class SemiGlobalMatching
{
    using T = unsigned short;
    auto static constexpr DInt = DMax - DMin;

    static_assert(DMin >= 0 && DMax >= 0, "DMin and DMax must be positive");
    static_assert(DMax > DMin, "DMax must be larger than DMin");
    static_assert(0 == DMin % 16, "DMin must be a multiple of 16");
    static_assert(0 == DMax % 16, "DMax must be a multiple of 16");

    template <int cnt, int N>
    struct Loop
    {
        inline static void EvaluateMin(T* Lmin, T& GlobalMin, T* Lp, T P1) noexcept
        {
            GlobalMin = Min(GlobalMin, Lp[cnt]);

            if (0 == cnt)
            {
                Lmin[cnt] = Min(Lp[cnt], static_cast<T>(Lp[cnt + 1] + P1));
                Loop<cnt + 1, N>::EvaluateMin(Lmin, GlobalMin, Lp, P1);
                return;
            }

            if (N - 1 == cnt)
            {
                Lmin[cnt] = Min(static_cast<T>(Lp[cnt - 1] + P1), Lp[cnt]);
                Loop<cnt + 1, N>::EvaluateMin(Lmin, GlobalMin, Lp, P1);
                return;
            }

            Lmin[cnt] = Min(static_cast<T>(Min(Lp[cnt - 1], Lp[cnt + 1]) + P1), Lp[cnt]);
            Loop<cnt + 1, N>::EvaluateMin(Lmin, GlobalMin, Lp, P1);
        }

        inline static T GetMinIdx(T& GlobalMin, T* Lp, int d) noexcept
        {
            if (Lp[cnt] < GlobalMin)
            {
                d = cnt;
                GlobalMin = Lp[cnt];
            }

            return Loop<cnt + 1, N>::GetMinIdx(GlobalMin, Lp, d);
        }

        inline static void EvaluateMinAVX2(T* Lmin, __m256i& GlobalMin, T* Lp, __m256i& P1) noexcept
        {
            auto _Lp = _mm256_load_si256(reinterpret_cast<__m256i*>(Lp));
            auto _Lp_plus = _mm256_lddqu_si256(reinterpret_cast<__m256i*>(Lp + 1));

            GlobalMin = _mm256_min_epu16(GlobalMin, _Lp);

            if (0 == cnt)
            {
                auto _min = _mm256_min_epu16(_Lp, _mm256_adds_epu16(_Lp_plus, P1));
                _mm256_store_si256(reinterpret_cast<__m256i*>(Lmin), _min);
                Loop<cnt + 1, N>::EvaluateMinAVX2(Lmin + 16, GlobalMin, Lp + 16, P1);
                return;
            }

            auto _Lp_minus = _mm256_lddqu_si256(reinterpret_cast<__m256i*>(Lp - 1));

            if (N - 16 == cnt)
            {
                auto _min = _mm256_min_epu16(_Lp, _mm256_adds_epu16(_Lp_minus, P1));
                _mm256_store_si256(reinterpret_cast<__m256i*>(Lmin), _min);
                Loop<cnt + 1, N>::EvaluateMinAVX2(Lmin + 16, GlobalMin, Lp + 16, P1);
                return;
            }

            auto _min = _mm256_min_epu16(_Lp, _mm256_adds_epu16(_mm256_min_epu16(_Lp_minus, _Lp_plus), P1));
            _mm256_store_si256(reinterpret_cast<__m256i*>(Lmin), _min);
            Loop<cnt + 1, N>::EvaluateMinAVX2(Lmin + 16, GlobalMin, Lp + 16, P1);
        }
    };

    template <int N>
    struct Loop<N, N>
    {
        inline static void EvaluateMin(T*, T&, T*, T) noexcept
        {
        }

        inline static T GetMinIdx(T&, T*, int d) noexcept
        {
            return d;
        }

        inline static void EvaluateMinAVX2(T*, __m256i&, T*, __m256i&) noexcept
        {
        }
    };

    using BufferPtr = typename unique_ptr_aligned<T>;
    auto static constexpr Alignment = 32;

    BufferPtr C;
    BufferPtr S;

    BufferPtr PathStorage[4];

    BufferPtr min_Lp_r;

    size_t Width;
    size_t Height;

    SimpleImage Left;
    SimpleImage Right;

    T m_P1 = 5;
    T m_P2 = 30;

public:
    SemiGlobalMatching(SimpleImage&& _Left, SimpleImage&& _Right)
          : Left(std::move(_Left))
          , Right(std::move(_Right))
    {
        Width = Left.Width;
        Height = Left.Height;

        C = make_unique_aligned<T, Alignment>(Width * Height * DInt);
        S = make_unique_aligned<T, Alignment>(Width * Height * DInt);
        PathStorage[0] = make_unique_aligned<T, Alignment>(DInt);
        PathStorage[1] = make_unique_aligned<T, Alignment>(Width * DInt);
        PathStorage[2] = make_unique_aligned<T, Alignment>(Width * DInt);
        PathStorage[3] = make_unique_aligned<T, Alignment>(Width * DInt);
        min_Lp_r = make_unique_aligned<T, Alignment>(DInt);
        ComputeCost();
    }

    inline void SetPenalities(T P1, T P2)
    {
        m_P1 = P1;
        m_P2 = P2;
    }

    inline void ComputeCost()
    {
        for (auto iy = 0; iy < Height; iy++)
        {
            for (auto ix = 0; ix < DMin; ix++)
            {
                auto iidx = ix + Width * iy;

                for (auto d = 0; d < DInt; d++)
                {
                    auto idx = d + iidx * DInt;
                    C[idx] = (1 << 11);
                    assert(idx >= 0 && idx < Width * Height * DInt);
                }
            }

            for (auto ix = DMin; ix < DMax; ix++)
            {
                auto iidx = ix + Width * iy;

                for (auto d = DMin; d < ix; d++)
                {
                    auto idx = d - DMin + iidx * DInt;
                    C[idx] = abs(static_cast<short>(Left.Buffer[iidx]) - static_cast<short>(Right.Buffer[iidx - d]));
                    assert(idx >= 0 && idx < Width * Height * DInt);
                    assert(iidx - d >= 0 && iidx - d < Width * Height);
                }

                for (auto d = ix; d < DMax; d++)
                {
                    auto idx = d - DMin + iidx * DInt;
                    C[idx] = (1 << 11);
                    assert(idx >= 0 && idx < Width * Height * DInt);
                }
            }

            for (auto ix = DMax; ix < Width; ix++)
            {
                auto iidx = ix + Width * iy;

                for (auto d = 0; d < DInt; d++)
                {
                    auto idx = d + iidx * DInt;
                    C[idx] =
                        abs(static_cast<short>(Left.Buffer[iidx]) - static_cast<short>(Right.Buffer[iidx - d - DMin]));
                    assert(idx >= 0 && idx < Width * Height * DInt);
                    assert(iidx - d - DMin >= 0 && iidx - d - DMin < Width * Height);
                }
            }
        }
    }

    SimpleImage GetDisparity()
    {
        ForwardPass<UseAVX2>();
        BackwardPass<UseAVX2>();

        auto Disparity = make_unique_aligned<T>(Width * Height);
        auto Output = make_unique_aligned<BYTE>(Width * Height);

        T MaxDisparity = DMin;
        T MinDisparity = DMax;

        for (auto i = 0; i < Height * Width; i++)
        {
            auto MinVal = std::numeric_limits<T>::max();
            auto d = Loop<0, DInt>::GetMinIdx(MinVal, S.get() + i * DInt, 0);

            if (d > MaxDisparity)
                MaxDisparity = d;
            if (d < MinDisparity)
                MinDisparity = d;

            Disparity[i] = d;
        }

        for (auto i = 0; i < Height * Width; i++)
        {
            Output[i] = (Disparity[i] - MinDisparity) * 255 / (MaxDisparity - MinDisparity);
        }

        return {std::move(Output), Width, Height};
    }

private:
    inline void EvaluateMinAVX2Proxy(T* Lmin, T& GlobalMin, T* Lp, T P1) noexcept
    {
        __m256i _GlobalMin = _mm256_set1_epi16(GlobalMin);
        __m256i _P1 = _mm256_set1_epi16(P1);

        Loop<0, (DInt >> 4)>::EvaluateMinAVX2(Lmin, _GlobalMin, Lp, _P1);

        __declspec(align(32)) T LastMin[16];
        _mm256_store_si256(reinterpret_cast<__m256i*>(&LastMin), _GlobalMin);

        for (auto& i : LastMin)
        {
            if (i < GlobalMin)
            {
                GlobalMin = i;
            }
        }
    }

    template <int P, bool init, bool WithAVX2>
    inline void UpdatePath(T* pS, T* const pC, size_t widx) noexcept
    {
        auto pshift = (0 == P) ? 0 : widx * DInt;
        auto path_vector = PathStorage[P].get() + pshift;

        if (init)
        {
            for (auto d = 0; d < DInt; d++)
            {
                auto path_cost = pC[d];
                pS[d] += path_cost;
                path_vector[d] = path_cost;
            }

            return;
        }

        T LGmin = std::numeric_limits<T>::max();

        if (WithAVX2)
        {
            EvaluateMinAVX2Proxy(min_Lp_r.get(), LGmin, path_vector, m_P1);

            auto _P2 = _mm256_set1_epi16(m_P2);
            auto _LGmin = _mm256_set1_epi16(LGmin);
            auto _Lp_r_far = _mm256_adds_epi16(_P2, _LGmin);

            auto pCtmp = pC;
            auto min_Lp_r_tmp = min_Lp_r.get();
            auto path_vector_tmp = path_vector;
            auto pStmp = pS;

            for (auto d = 0; d < (DInt >> 4); d++)
            {
                auto _pC = _mm256_load_si256(reinterpret_cast<__m256i*>(pCtmp));
                auto _min_Lp_r = _mm256_load_si256(reinterpret_cast<__m256i*>(min_Lp_r_tmp));
                auto _path_vector = _mm256_load_si256(reinterpret_cast<__m256i*>(path_vector_tmp));
                auto _pS = _mm256_load_si256(reinterpret_cast<__m256i*>(pStmp));

                auto _path_cost =
                    _mm256_subs_epu16(_mm256_adds_epu16(_pC, _mm256_min_epu16(_min_Lp_r, _Lp_r_far)), _LGmin);
                _pS = _mm256_adds_epu16(_pS, _path_cost);

                _mm256_store_si256(reinterpret_cast<__m256i*>(pStmp), _pS);
                _mm256_store_si256(reinterpret_cast<__m256i*>(path_vector_tmp), _path_cost);

                pCtmp += 16;
                min_Lp_r_tmp += 16;
                path_vector_tmp += 16;
                pStmp += 16;
            }

            return;
        }

        Loop<0, DInt>::EvaluateMin(min_Lp_r.get(), LGmin, path_vector, m_P1);

        for (auto d = 0; d < DInt; d++)
        {
            auto path_cost = pC[d] + Min(min_Lp_r[d], static_cast<T>(LGmin + m_P2)) - LGmin;
            pS[d] += path_cost;
            path_vector[d] = path_cost;
        }
    }

    template <bool WithAVX2 = false>
    inline void ForwardPass() noexcept
    {
        // top left corner
        UpdatePath<0, true, WithAVX2>(S.get(), C.get(), 0);
        UpdatePath<1, true, WithAVX2>(S.get(), C.get(), 0);
        UpdatePath<2, true, WithAVX2>(S.get(), C.get(), 0);
        UpdatePath<3, true, WithAVX2>(S.get(), C.get(), 0);

        // first line
        for (auto ix = 1; ix < Width; ix++)
        {
            UpdatePath<0, false, WithAVX2>(S.get() + ix * DInt, C.get() + ix * DInt, ix);
            UpdatePath<1, true, WithAVX2>(S.get() + ix * DInt, C.get() + ix * DInt, ix);
            UpdatePath<2, true, WithAVX2>(S.get() + ix * DInt, C.get() + ix * DInt, ix);
            UpdatePath<3, true, WithAVX2>(S.get() + ix * DInt, C.get() + ix * DInt, ix);
        }

        for (auto iy = 1; iy < Height; iy++)
        {
            // first pixel of the line
            auto idy = Width * iy;

            UpdatePath<0, true, WithAVX2>(S.get() + idy * DInt, C.get() + idy * DInt, 0);
            UpdatePath<1, true, WithAVX2>(S.get() + idy * DInt, C.get() + idy * DInt, 0);
            UpdatePath<2, false, WithAVX2>(S.get() + idy * DInt, C.get() + idy * DInt, 0);
            UpdatePath<3, false, WithAVX2>(S.get() + idy * DInt, C.get() + idy * DInt, 0);

            for (auto ix = 1; ix < Width; ix++)
            {
                auto idx = ix + idy;
                UpdatePath<0, false, WithAVX2>(S.get() + idx * DInt, C.get() + idx * DInt, ix);
                UpdatePath<1, false, WithAVX2>(S.get() + idx * DInt, C.get() + idx * DInt, ix);
                UpdatePath<2, false, WithAVX2>(S.get() + idx * DInt, C.get() + idx * DInt, ix);
                UpdatePath<3, false, WithAVX2>(S.get() + idx * DInt, C.get() + idx * DInt, ix);
            }
        }
    }

    template <bool WithAVX2 = false>
    inline void BackwardPass() noexcept
    {
        auto last = Width * Height - 1;

        // bottom right corner
        UpdatePath<0, true, WithAVX2>(S.get() + last * DInt, C.get() + last * DInt, 0);
        UpdatePath<1, true, WithAVX2>(S.get() + last * DInt, C.get() + last * DInt, 0);
        UpdatePath<2, true, WithAVX2>(S.get() + last * DInt, C.get() + last * DInt, 0);
        UpdatePath<3, true, WithAVX2>(S.get() + last * DInt, C.get() + last * DInt, 0);

        // last line
        for (auto ix = 1; ix < Width; ix++)
        {
            UpdatePath<0, false, WithAVX2>(S.get() + (last - ix) * DInt, C.get() + (last - ix) * DInt, ix);
            UpdatePath<1, true, WithAVX2>(S.get() + (last - ix) * DInt, C.get() + (last - ix) * DInt, ix);
            UpdatePath<2, true, WithAVX2>(S.get() + (last - ix) * DInt, C.get() + (last - ix) * DInt, ix);
            UpdatePath<3, true, WithAVX2>(S.get() + (last - ix) * DInt, C.get() + (last - ix) * DInt, ix);
        }

        for (auto iy = 1; iy < Height; iy++)
        {
            // first pixel of the line
            auto idy = last - Width * iy;

            UpdatePath<0, true, WithAVX2>(S.get() + idy * DInt, C.get() + idy * DInt, 0);
            UpdatePath<1, true, WithAVX2>(S.get() + idy * DInt, C.get() + idy * DInt, 0);
            UpdatePath<2, false, WithAVX2>(S.get() + idy * DInt, C.get() + idy * DInt, 0);
            UpdatePath<3, false, WithAVX2>(S.get() + idy * DInt, C.get() + idy * DInt, 0);

            for (auto ix = 1; ix < Width; ix++)
            {
                auto idx = idy - ix;
                UpdatePath<0, false, WithAVX2>(S.get() + idx * DInt, C.get() + idx * DInt, ix);
                UpdatePath<1, false, WithAVX2>(S.get() + idx * DInt, C.get() + idx * DInt, ix);
                UpdatePath<2, false, WithAVX2>(S.get() + idx * DInt, C.get() + idx * DInt, ix);
                UpdatePath<3, false, WithAVX2>(S.get() + idx * DInt, C.get() + idx * DInt, ix);
            }
        }
    }
};

}  // namespace sgm
