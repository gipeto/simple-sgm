#pragma once
#include <memory>
#include <wrl.h>
#include <wincodec.h>
#include <wincodecsdk.h>
#pragma comment(lib, "Windowscodecs.lib")
#include "ErrorHandling.h"
#include <Shlwapi.h>
#pragma comment(lib, "Shlwapi.lib")

#include <iostream>
#include <vector>
#include <bitset>
#include <array>
#include <string>
#include <intrin.h>




namespace utils
{
	using namespace std;
	using namespace Microsoft::WRL;

	class PerformanceTimer
	{
	public:

		double m_PCFreq;
		__int64 m_CounterStart;
		LPCTSTR m_p;

		PerformanceTimer(LPCTSTR p) :
			m_PCFreq(0),
			m_CounterStart(0),
			m_p(p)
		{
			LARGE_INTEGER li;

			if (!QueryPerformanceFrequency(&li))
			{
			}

			m_PCFreq = double(li.QuadPart) / 1000.0;

			QueryPerformanceCounter(&li);
			m_CounterStart = li.QuadPart;

		}

		~PerformanceTimer()
		{
			LARGE_INTEGER li;
			QueryPerformanceCounter(&li);
			double Diff = double(li.QuadPart - m_CounterStart) / m_PCFreq;
			wprintf(L"\n-------------------------------------------\n");
			wprintf(L"%s execute in %12.8lf ms \n", m_p, Diff);
			wprintf(L"-------------------------------------------\n");
		}
	};



	template<class T>
	struct aligned_deleter
	{
		void operator()(T * p) const
		{
			_aligned_free(p);
		}
	};

	template<typename T>
	using unique_ptr_aligned = typename unique_ptr<T[], aligned_deleter<T>>;

	

	template<typename T,size_t Alignment = 16>
	auto make_unique_aligned(size_t n) -> std::enable_if_t<std::is_arithmetic<T>::value,unique_ptr_aligned<T>>
	{
		auto p = _aligned_malloc(n*sizeof(T), Alignment);
		ZeroMemory(p, n*sizeof(T));
		return unique_ptr_aligned<T>(static_cast<T*>(p));
	}


	
	auto static CreateWICFactory() -> ComPtr<IWICImagingFactory2>
	{
	
		ComPtr<IWICImagingFactory2> Factory;


		// Create the COM imaging factory
		HR(CoCreateInstance(
			CLSID_WICImagingFactory,
			NULL,
			CLSCTX_INPROC_SERVER,
			__uuidof(IWICImagingFactory2),
			reinterpret_cast<void **>(Factory.GetAddressOf())
			));

			return Factory;
	}


	
	struct SimpleImage
	{
		unique_ptr_aligned<BYTE> Buffer;
		size_t Width;
		size_t Height;

		bool operator== (const SimpleImage & Other)
		{
			return (Width == Other.Width) && (Height == Other.Height);
		}

		explicit  operator bool()
		{
			return nullptr != Buffer;
		
		}


	};


	SimpleImage ReadImage(const ComPtr<IWICImagingFactory2> & Factory, LPCWSTR FileName)
	{
	
		ASSERT(Factory);
		ComPtr<IWICBitmapDecoder>     Decoder;
		ComPtr<IWICBitmapFrameDecode> BitmapFrame;
		
		HR(Factory->CreateDecoderFromFilename(FileName, NULL, GENERIC_READ, WICDecodeMetadataCacheOnDemand, Decoder.GetAddressOf()));
		Decoder->GetFrame(0, BitmapFrame.GetAddressOf());


		WICPixelFormatGUID pixelFormat = GUID_WICPixelFormatUndefined;
		HR(BitmapFrame->GetPixelFormat(&pixelFormat));

		UINT Width, Height = 0;

		HR(BitmapFrame->GetSize(&Width, &Height));


		if (GUID_WICPixelFormat8bppGray != pixelFormat)
		{
			ComPtr<IWICFormatConverter> Converter;
			
		   HR(Factory->CreateFormatConverter(Converter.GetAddressOf()));
		   HR( Converter->Initialize(BitmapFrame.Get(), 
								GUID_WICPixelFormat8bppGray,
			                    WICBitmapDitherTypeNone,  
								NULL,                           
								0.f,                             
			                    WICBitmapPaletteTypeCustom));

		   auto Buffer = make_unique_aligned<BYTE>(Width*Height);

		  HR(Converter->CopyPixels(NULL, Width*sizeof(BYTE), Width*Height, Buffer.get()));

		  return {std::move(Buffer),Width,Height};

		}

		auto Buffer = make_unique_aligned<BYTE>(Width*Height);
		HR(BitmapFrame->CopyPixels(NULL, Width*sizeof(BYTE), Width*Height, Buffer.get()));

		return{ std::move(Buffer),Width,Height };

	}


	
	void SaveImage(const SimpleImage & Image, ComPtr<IWICImagingFactory2> & Factory, LPCWSTR FileName)
	{
		
		ComPtr <IStream >FileStream;
		HR(SHCreateStreamOnFile(FileName, STGM_CREATE | STGM_WRITE, FileStream.GetAddressOf()));


		ComPtr<IWICBitmap> OutputImage;
		HR(Factory->CreateBitmapFromMemory(Image.Width, Image.Height, GUID_WICPixelFormat8bppGray, Image.Width*sizeof(BYTE), Image.Width*Image.Height, Image.Buffer.get(), OutputImage.GetAddressOf()));
		
		ComPtr<IWICBitmapEncoder> Encoder;
		HR(Factory->CreateEncoder(GUID_ContainerFormatPng,NULL,Encoder.GetAddressOf()));
	
		HR(Encoder->Initialize(FileStream.Get(), WICBitmapEncoderNoCache));

    	ComPtr<IWICBitmapFrameEncode> TargetFrame;
    	ComPtr<IPropertyBag2> PropertyBag;
		HR(Encoder->CreateNewFrame(TargetFrame.GetAddressOf(), PropertyBag.GetAddressOf()));

		HR(TargetFrame->Initialize(PropertyBag.Get())); // no properties
		HR(TargetFrame->WriteSource(OutputImage.Get(), nullptr));
		HR(TargetFrame->Commit());
		HR(Encoder->Commit());

	
	}


	// https://msdn.microsoft.com/en-us/library/hskdteyh.aspx
	class InstructionSet
	{
		// forward declarations
		class InstructionSet_Internal;

	public:
		// getters
		static std::string Vendor(void) { return CPU_Rep.vendor_; }
		static std::string Brand(void) { return CPU_Rep.brand_; }

		static bool SSE3(void) { return CPU_Rep.f_1_ECX_[0]; }
		static bool PCLMULQDQ(void) { return CPU_Rep.f_1_ECX_[1]; }
		static bool MONITOR(void) { return CPU_Rep.f_1_ECX_[3]; }
		static bool SSSE3(void) { return CPU_Rep.f_1_ECX_[9]; }
		static bool FMA(void) { return CPU_Rep.f_1_ECX_[12]; }
		static bool CMPXCHG16B(void) { return CPU_Rep.f_1_ECX_[13]; }
		static bool SSE41(void) { return CPU_Rep.f_1_ECX_[19]; }
		static bool SSE42(void) { return CPU_Rep.f_1_ECX_[20]; }
		static bool MOVBE(void) { return CPU_Rep.f_1_ECX_[22]; }
		static bool POPCNT(void) { return CPU_Rep.f_1_ECX_[23]; }
		static bool AES(void) { return CPU_Rep.f_1_ECX_[25]; }
		static bool XSAVE(void) { return CPU_Rep.f_1_ECX_[26]; }
		static bool OSXSAVE(void) { return CPU_Rep.f_1_ECX_[27]; }
		static bool AVX(void) { return CPU_Rep.f_1_ECX_[28]; }
		static bool F16C(void) { return CPU_Rep.f_1_ECX_[29]; }
		static bool RDRAND(void) { return CPU_Rep.f_1_ECX_[30]; }

		static bool MSR(void) { return CPU_Rep.f_1_EDX_[5]; }
		static bool CX8(void) { return CPU_Rep.f_1_EDX_[8]; }
		static bool SEP(void) { return CPU_Rep.f_1_EDX_[11]; }
		static bool CMOV(void) { return CPU_Rep.f_1_EDX_[15]; }
		static bool CLFSH(void) { return CPU_Rep.f_1_EDX_[19]; }
		static bool MMX(void) { return CPU_Rep.f_1_EDX_[23]; }
		static bool FXSR(void) { return CPU_Rep.f_1_EDX_[24]; }
		static bool SSE(void) { return CPU_Rep.f_1_EDX_[25]; }
		static bool SSE2(void) { return CPU_Rep.f_1_EDX_[26]; }

		static bool FSGSBASE(void) { return CPU_Rep.f_7_EBX_[0]; }
		static bool BMI1(void) { return CPU_Rep.f_7_EBX_[3]; }
		static bool HLE(void) { return CPU_Rep.isIntel_ && CPU_Rep.f_7_EBX_[4]; }
		static bool AVX2(void) { return CPU_Rep.f_7_EBX_[5]; }
		static bool BMI2(void) { return CPU_Rep.f_7_EBX_[8]; }
		static bool ERMS(void) { return CPU_Rep.f_7_EBX_[9]; }
		static bool INVPCID(void) { return CPU_Rep.f_7_EBX_[10]; }
		static bool RTM(void) { return CPU_Rep.isIntel_ && CPU_Rep.f_7_EBX_[11]; }
		static bool AVX512F(void) { return CPU_Rep.f_7_EBX_[16]; }
		static bool RDSEED(void) { return CPU_Rep.f_7_EBX_[18]; }
		static bool ADX(void) { return CPU_Rep.f_7_EBX_[19]; }
		static bool AVX512PF(void) { return CPU_Rep.f_7_EBX_[26]; }
		static bool AVX512ER(void) { return CPU_Rep.f_7_EBX_[27]; }
		static bool AVX512CD(void) { return CPU_Rep.f_7_EBX_[28]; }
		static bool SHA(void) { return CPU_Rep.f_7_EBX_[29]; }

		static bool PREFETCHWT1(void) { return CPU_Rep.f_7_ECX_[0]; }

		static bool LAHF(void) { return CPU_Rep.f_81_ECX_[0]; }
		static bool LZCNT(void) { return CPU_Rep.isIntel_ && CPU_Rep.f_81_ECX_[5]; }
		static bool ABM(void) { return CPU_Rep.isAMD_ && CPU_Rep.f_81_ECX_[5]; }
		static bool SSE4a(void) { return CPU_Rep.isAMD_ && CPU_Rep.f_81_ECX_[6]; }
		static bool XOP(void) { return CPU_Rep.isAMD_ && CPU_Rep.f_81_ECX_[11]; }
		static bool TBM(void) { return CPU_Rep.isAMD_ && CPU_Rep.f_81_ECX_[21]; }

		static bool SYSCALL(void) { return CPU_Rep.isIntel_ && CPU_Rep.f_81_EDX_[11]; }
		static bool MMXEXT(void) { return CPU_Rep.isAMD_ && CPU_Rep.f_81_EDX_[22]; }
		static bool RDTSCP(void) { return CPU_Rep.isIntel_ && CPU_Rep.f_81_EDX_[27]; }
		static bool _3DNOWEXT(void) { return CPU_Rep.isAMD_ && CPU_Rep.f_81_EDX_[30]; }
		static bool _3DNOW(void) { return CPU_Rep.isAMD_ && CPU_Rep.f_81_EDX_[31]; }

	private:
		static const InstructionSet_Internal CPU_Rep;

		class InstructionSet_Internal
		{
		public:
			InstructionSet_Internal()
				: nIds_{ 0 },
				nExIds_{ 0 },
				isIntel_{ false },
				isAMD_{ false },
				f_1_ECX_{ 0 },
				f_1_EDX_{ 0 },
				f_7_EBX_{ 0 },
				f_7_ECX_{ 0 },
				f_81_ECX_{ 0 },
				f_81_EDX_{ 0 },
				data_{},
				extdata_{}
			{
				//int cpuInfo[4] = {-1};
				std::array<int, 4> cpui;

				// Calling __cpuid with 0x0 as the function_id argument
				// gets the number of the highest valid function ID.
				__cpuid(cpui.data(), 0);
				nIds_ = cpui[0];

				for (int i = 0; i <= nIds_; ++i)
				{
					__cpuidex(cpui.data(), i, 0);
					data_.push_back(cpui);
				}

				// Capture vendor string
				char vendor[0x20];
				memset(vendor, 0, sizeof(vendor));
				*reinterpret_cast<int*>(vendor) = data_[0][1];
				*reinterpret_cast<int*>(vendor + 4) = data_[0][3];
				*reinterpret_cast<int*>(vendor + 8) = data_[0][2];
				vendor_ = vendor;
				if (vendor_ == "GenuineIntel")
				{
					isIntel_ = true;
				}
				else if (vendor_ == "AuthenticAMD")
				{
					isAMD_ = true;
				}

				// load bitset with flags for function 0x00000001
				if (nIds_ >= 1)
				{
					f_1_ECX_ = data_[1][2];
					f_1_EDX_ = data_[1][3];
				}

				// load bitset with flags for function 0x00000007
				if (nIds_ >= 7)
				{
					f_7_EBX_ = data_[7][1];
					f_7_ECX_ = data_[7][2];
				}

				// Calling __cpuid with 0x80000000 as the function_id argument
				// gets the number of the highest valid extended ID.
				__cpuid(cpui.data(), 0x80000000);
				nExIds_ = cpui[0];

				char brand[0x40];
				memset(brand, 0, sizeof(brand));

				for (int i = 0x80000000; i <= nExIds_; ++i)
				{
					__cpuidex(cpui.data(), i, 0);
					extdata_.push_back(cpui);
				}

				// load bitset with flags for function 0x80000001
				if (nExIds_ >= 0x80000001)
				{
					f_81_ECX_ = extdata_[1][2];
					f_81_EDX_ = extdata_[1][3];
				}

				// Interpret CPU brand string if reported
				if (nExIds_ >= 0x80000004)
				{
					memcpy(brand, extdata_[2].data(), sizeof(cpui));
					memcpy(brand + 16, extdata_[3].data(), sizeof(cpui));
					memcpy(brand + 32, extdata_[4].data(), sizeof(cpui));
					brand_ = brand;
				}
			};

			int nIds_;
			int nExIds_;
			std::string vendor_;
			std::string brand_;
			bool isIntel_;
			bool isAMD_;
			std::bitset<32> f_1_ECX_;
			std::bitset<32> f_1_EDX_;
			std::bitset<32> f_7_EBX_;
			std::bitset<32> f_7_ECX_;
			std::bitset<32> f_81_ECX_;
			std::bitset<32> f_81_EDX_;
			std::vector<std::array<int, 4>> data_;
			std::vector<std::array<int, 4>> extdata_;
		};
	};

	const InstructionSet::InstructionSet_Internal InstructionSet::CPU_Rep;


}