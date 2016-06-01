#pragma once

#include <stdio.h>
#include <stdarg.h>
#include <wchar.h>
#include <wtypes.h>
#include <tchar.h>
#include <varargs.h>




#if WINAPI_FAMILY_PHONE_APP != WINAPI_FAMILY
#define ASSERT(expression) _ASSERTE(expression)
#else
#ifdef _DEBUG
#define ASSERT(expression) {if(!(expression)) throw win::Exception::CreateException(E_FAIL);}
#else
#define ASSERT(expression) ((void)0)
#endif
#endif

#ifdef _DEBUG

#define VERIFY(expression) ASSERT(expression)
#define HR(expression) ASSERT(S_OK==(expression))

#define DEBUG_OUTPUT(x) OutputDebugPrintf x

inline int  OutputDebugPrintf(LPCTSTR format, ...)
{
	va_list argList;
	static TCHAR szBuffer[8192];
	va_start(argList, format);

	auto iLength = _vsntprintf_s(szBuffer, sizeof(szBuffer) / sizeof(szBuffer[0]), sizeof(szBuffer) / sizeof(szBuffer[0]) - 1, format, argList);
	OutputDebugString(szBuffer);
	va_end(argList);
	return iLength;
}

inline void TRACE(WCHAR const * const format, ...)
{
	va_list args;
	va_start(args, format);

	WCHAR output[512];
	vswprintf_s(output, format, args);
	OutputDebugString(output);

	va_end(args);
}

#else

#define DEBUG_OUTPUT(x)

#define VERIFY(expression) (expression)
struct ComException
{
	HRESULT const hr;
	ComException(HRESULT const value) : hr(value){}
};

#ifdef __cplusplus_winrt

inline void HR(HRESULT const hr)
{
	if (S_OK != hr) throw Platform::Exception::CreateException(hr);
}

#else

inline void HR(HRESULT const hr)
{
	if (S_OK != hr) throw ComException(hr);
}

#endif

#define TRACE __noop
#define OutputDebugPrintf __noop

#endif
