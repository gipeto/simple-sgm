#pragma once

#include "Utils.h"

namespace sgm
{

	using namespace utils;

	template<typename T>
	inline T Min(T const & left, T const & right)
	{
		return left < right ? left : right;
	}


	/*
	  Implementation of the SemiGlobal Matching algorithm [1], with the following limitations:
	  
	  - The matching cost is a simple absolute difference of the pixel luminance
	  - Only 8 path are considered
	  - Penality P2 are not weighted by the image gradient
	  - No consistency checks and disparity sub-pixel interpolation
	  
	  [1] Hirschmuller, H. (2005). Accurate and Efficient Stereo Processing by Semi Global Matching and Mutual Information. CVPR .
	
	*/

	template<typename T, size_t DMax>
	class SemiGlobalMatching
	{

		static_assert(std::is_integral<T>::value, "T must be an integral type");

		template<int cnt, int N>
		struct Loop
		{

			inline static void EvaluateMin(T * Lmin, T & GlobalMin, T * Lp, T P1) noexcept
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


			inline static T GetMinIdx(T & GlobalMin, T * Lp, int d) noexcept
			{

				if (Lp[cnt] < GlobalMin)
				{
					d = cnt;
					GlobalMin = Lp[cnt];
				}

				return Loop<cnt + 1, N>::GetMinIdx(GlobalMin, Lp, d);

			}


			
			inline static void EvaluateMinAVX2(T * Lmin, __m256i & GlobalMin, T * Lp, __m256i & P1) noexcept
			{

				auto _Lp      = _mm256_load_si256(reinterpret_cast<__m256i *>(Lp));
				auto _Lp_plus = _mm256_lddqu_si256(reinterpret_cast<__m256i *>(Lp+1));
				
				GlobalMin = _mm256_min_epu16(GlobalMin, _Lp);

				if (0 == cnt)
				{
					auto _min = _mm256_min_epu16(_Lp, _mm256_adds_epu16(_Lp_plus,P1));
					_mm256_store_si256(reinterpret_cast<__m256i *>(Lmin), _min);
					Loop<cnt + 1, N>::EvaluateMinAVX2(Lmin+16, GlobalMin, Lp+16, P1);
					return;
				}

				auto _Lp_minus = _mm256_lddqu_si256(reinterpret_cast<__m256i *>(Lp - 1));


				if (N - 16 == cnt)
				{
					auto _min = _mm256_min_epu16(_Lp, _mm256_adds_epu16(_Lp_minus, P1));
					_mm256_store_si256(reinterpret_cast<__m256i *>(Lmin), _min);
					Loop<cnt + 1, N>::EvaluateMinAVX2(Lmin + 16, GlobalMin, Lp + 16, P1);
					return;
				}

				auto _min = _mm256_min_epu16(_Lp, _mm256_adds_epu16( _mm256_min_epu16(_Lp_minus,_Lp_plus) ,P1) ) ;
				_mm256_store_si256(reinterpret_cast<__m256i *>(Lmin), _min);
				Loop<cnt + 1, N>::EvaluateMinAVX2(Lmin + 16, GlobalMin, Lp + 16, P1);
			

			}




		};

		template<int N>
		struct Loop<N, N>
		{
			inline static void EvaluateMin(T *, T &, T *, T) noexcept
			{}

			inline static T GetMinIdx(T &, T *, int d) noexcept
			{
				return d;
			}


			inline static void EvaluateMinAVX2(T * , __m256i & , T * , __m256i &) noexcept
			{}

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



		SemiGlobalMatching(SimpleImage && _Left, SimpleImage && _Right) :
			Left(std::move(_Left)), Right(std::move(_Right))
		{
			Width = Left.Width;
			Height = Left.Height;

			C = make_unique_aligned<T,Alignment>(Width*Height*DMax);
			S = make_unique_aligned<T, Alignment>(Width*Height*DMax);
			PathStorage[0] = make_unique_aligned<T, Alignment>(DMax);
			PathStorage[1] = make_unique_aligned<T, Alignment>(Width*DMax);
			PathStorage[2] = make_unique_aligned<T, Alignment>(Width*DMax);
			PathStorage[3] = make_unique_aligned<T, Alignment>(Width*DMax);
			min_Lp_r = make_unique_aligned<T, Alignment>(DMax);
			initC();
		}

		
		inline void SetPenalities(T P1, T P2)
		{
			m_P1 = P1;
			m_P2 = P2;
		}


		inline void initC()
		{

			for (auto iy = 0; iy < Height; iy++)
			{
				for (auto ix = 0; ix < DMax; ix++)
				{
					auto iidx = ix + Width*iy;

					for (auto d = 0; d < ix; d++)
					{
						auto idx = d + iidx * DMax;
						C[idx] = abs(Left.Buffer[iidx] - Right.Buffer[iidx - d]);
					}

					for (auto d = ix; d < DMax; d++)
					{
						auto idx = d + iidx * DMax;
						C[idx] = std::numeric_limits<T>::max() / 2;
					}

				}


				for (auto ix = DMax; ix < Width ; ix++)
				{
					auto iidx = ix + Width*iy;

					for (auto d = 0; d < DMax; d++)
					{
						auto idx = d + iidx * DMax;
						C[idx] = abs(Left.Buffer[iidx] - Right.Buffer[iidx - d]);
					}
				}		

			}

		}


		SimpleImage GetDisparity()
		{
			ForwardPass();
			BackwardPass();

			auto Output = make_unique_aligned<BYTE>(Width*Height);


			for (auto i = 0; i < Height*Width; i++)
			{

				T MinVal = std::numeric_limits<T>::max();
				Output[i] = Loop<0, DMax>::GetMinIdx(MinVal, S.get() + i*DMax, 0) * 255 / DMax;

			}


			return{ std::move(Output),Width,Height };

		}

		private:


		inline  void EvaluateMinAVX2Proxy(T * Lmin, T & GlobalMin, T * Lp, T P1) noexcept
		{
		
			__m256i _GlobalMin = _mm256_set1_epi16(GlobalMin);
			__m256i _P1 = _mm256_set1_epi16(P1);
		
			Loop<0, DMax /16 >::EvaluateMinAVX2(Lmin, _GlobalMin,  Lp, _P1);
				
			T LastMin[16];
			_mm256_store_si256(reinterpret_cast<__m256i *>(&LastMin), _GlobalMin);

			for (auto & i : LastMin)
			{
				if (i < GlobalMin)
				{
					GlobalMin = i;
				}
			}
		
		}



		template<int P, bool init = false>
		inline void UpdatePath(T * pS, T * const pC, size_t widx) noexcept
		{
			auto pshift = (0 == P) ? 0 : widx*DMax;
			auto path_vector = PathStorage[P].get() + pshift;

			if (init)
			{

				for (auto d = 0; d < DMax; d++)
				{
					auto path_cost = pC[d];
					pS[d] += path_cost;
					path_vector[d] = path_cost;
				}

				return;

			}


			T LGmin = std::numeric_limits<T>::max();
		//	Loop<0, DMax>::EvaluateMin(min_Lp_r.get(), LGmin, path_vector, m_P1);
			EvaluateMinAVX2Proxy(min_Lp_r.get(), LGmin, path_vector, m_P1);


			for (auto d = 0; d < DMax; d++)
			{
				auto path_cost = pC[d] + Min(min_Lp_r[d], static_cast<T>(LGmin + m_P2)) - LGmin;
				pS[d] += path_cost;
				path_vector[d] = path_cost;
			}


		}


		inline void ForwardPass() noexcept
		{
			// top left corner
			UpdatePath<0, true>(S.get(), C.get(), 0);
			UpdatePath<1, true>(S.get(), C.get(), 0);
			UpdatePath<2, true>(S.get(), C.get(), 0);
			UpdatePath<3, true>(S.get(), C.get(), 0);

			// first line
			for (auto ix = 1; ix < Width; ix++)
			{
				UpdatePath<0>(S.get() + ix*DMax, C.get() + ix*DMax, ix);
				UpdatePath<1, true>(S.get() + ix*DMax, C.get() + ix*DMax, ix);
				UpdatePath<2, true>(S.get() + ix*DMax, C.get() + ix*DMax, ix);
				UpdatePath<3, true>(S.get() + ix*DMax, C.get() + ix*DMax, ix);
			}


			for (auto iy = 1; iy < Height; iy++)
			{
				// first pixel of the line
				auto idy = Width*iy;

				UpdatePath<0, true>(S.get() + idy*DMax, C.get() + idy*DMax, 0);
				UpdatePath<1, true>(S.get() + idy*DMax, C.get() + idy*DMax, 0);
				UpdatePath<2>(S.get() + idy*DMax, C.get() + idy*DMax, 0);
				UpdatePath<3>(S.get() + idy*DMax, C.get() + idy*DMax, 0);

				for (auto ix = 1; ix < Width; ix++)
				{
					auto idx = ix + idy;
					UpdatePath<0>(S.get() + idx*DMax, C.get() + idx*DMax, ix);
					UpdatePath<1>(S.get() + idx*DMax, C.get() + idx*DMax, ix);
					UpdatePath<2>(S.get() + idx*DMax, C.get() + idx*DMax, ix);
					UpdatePath<3>(S.get() + idx*DMax, C.get() + idx*DMax, ix);
				}

			}

		}

		inline void BackwardPass() noexcept
		{
			auto last = Width*Height - 1;

			// bottom right corner
			UpdatePath<0, true>(S.get() + last*DMax, C.get() + last*DMax, 0);
			UpdatePath<1, true>(S.get() + last*DMax, C.get() + last*DMax, 0);
			UpdatePath<2, true>(S.get() + last*DMax, C.get() + last*DMax, 0);
			UpdatePath<3, true>(S.get() + last*DMax, C.get() + last*DMax, 0);

			// last line
			for (auto ix = 1; ix < Width; ix++)
			{
				UpdatePath<0>(S.get() + (last - ix)*DMax, C.get() + (last - ix)*DMax, ix);
				UpdatePath<1, true>(S.get() + (last - ix)*DMax, C.get() + (last - ix)*DMax, ix);
				UpdatePath<2, true>(S.get() + (last - ix)*DMax, C.get() + (last - ix)*DMax, ix);
				UpdatePath<3, true>(S.get() + (last - ix)*DMax, C.get() + (last - ix)*DMax, ix);
			}


			for (auto iy = 1; iy < Height; iy++)
			{
				// first pixel of the line
				auto idy = last - Width*iy;

				UpdatePath<0, true>(S.get() + idy*DMax, C.get() + idy*DMax, 0);
				UpdatePath<1, true>(S.get() + idy*DMax, C.get() + idy*DMax, 0);
				UpdatePath<2>(S.get() + idy*DMax, C.get() + idy*DMax, 0);
				UpdatePath<3>(S.get() + idy*DMax, C.get() + idy*DMax, 0);

				for (auto ix = 1; ix < Width; ix++)
				{
					auto idx = idy - ix;
					UpdatePath<0>(S.get() + idx*DMax, C.get() + idx*DMax, ix);
					UpdatePath<1>(S.get() + idx*DMax, C.get() + idx*DMax, ix);
					UpdatePath<2>(S.get() + idx*DMax, C.get() + idx*DMax, ix);
					UpdatePath<3>(S.get() + idx*DMax, C.get() + idx*DMax, ix);
				}

			}

		}


	};



}
