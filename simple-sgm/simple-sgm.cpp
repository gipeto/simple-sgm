// simple-sgm.cpp : Defines the entry point for the console application.


#include "stdafx.h"
#define NOMINMAX
#include <Windows.h>
#include "Utils.h"
#include "sgm.h"


auto static constexpr DMax = 64;


int main()
{
	using namespace utils;

	CoInitialize(NULL);

	auto Factory = CreateWICFactory();

	auto LeftImage  = ReadImage(Factory, L"im2.png");
	auto RightImage = ReadImage(Factory, L"im6.png");


	ASSERT(LeftImage == RightImage);

	SimpleImage DMap;

	{
		PerformanceTimer Timer(L"Sgm");
		 sgm::SemiGlobalMatching<DMax> Sgm(std::move(LeftImage), std::move(RightImage));
		 Sgm.SetPenalities(10, 80);
		 DMap = Sgm.GetDisparity();
	}

	    SaveImage(DMap,Factory, L"DisparityMap.png");
		return 0;
}

