#pragma once
#include <memory>
#include <wrl.h>
#include <wincodec.h>
#include <wincodecsdk.h>
#pragma comment(lib, "Windowscodecs.lib")
#include "ErrorHandling.h"
#include <Shlwapi.h>
#pragma comment(lib, "Shlwapi.lib")

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
		ZeroMemory(+p, n*sizeof(T));
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








}