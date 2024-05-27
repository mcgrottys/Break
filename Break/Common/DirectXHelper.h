#pragma once

#include <ppltasks.h>	// For create_task
#include <winrt/Windows.Storage.h>
#include <winrt/Windows.Storage.Streams.h>
#include <winrt/Windows.ApplicationModel.h>
#include <vector>
#include <string>
#include <future>

using namespace winrt;
using namespace winrt::Windows::Storage;
using namespace winrt::Windows::Storage::Streams;
using namespace winrt::Windows::ApplicationModel;

namespace DX
{
	inline void ThrowIfFailed(HRESULT hr)
	{
		if (FAILED(hr))
		{
			// Set a breakpoint on this line to catch Win32 API errors.
			//throw Platform::Exception::CreateException(hr);
		}
	}

	inline std::future<std::vector<byte>> ReadDataAsync(const std::wstring& filename)
	{
		auto folder = Package::Current().InstalledLocation();
		auto file = co_await folder.GetFileAsync(filename);
		auto fileBuffer = co_await FileIO::ReadBufferAsync(file);

		std::vector<byte> returnBuffer(fileBuffer.Length());
		DataReader::FromBuffer(fileBuffer).ReadBytes(returnBuffer);
		co_return returnBuffer;
	}
	// Function that reads from a binary file asynchronously.
	inline Concurrency::task<std::vector<byte>> ReadDataAsync2(const std::wstring& filename)
	{
		using namespace Concurrency;

		HANDLE file = CreateFile2(
			filename.c_str(),
			GENERIC_READ,
			FILE_SHARE_READ,
			OPEN_EXISTING,
			NULL
		);

		DX::ThrowIfFailed(file != INVALID_HANDLE_VALUE ? S_OK : HRESULT_FROM_WIN32(GetLastError()));

		return create_task([file]() {
			BY_HANDLE_FILE_INFORMATION info;
			if (!GetFileInformationByHandle(file, &info)) DX::ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));

			std::vector<byte> ret(info.nFileSizeLow);
			DWORD read = 0;
			DX::ThrowIfFailed(ReadFile(file, ret.data(), info.nFileSizeLow, &read, NULL) ? S_OK : HRESULT_FROM_WIN32(GetLastError()));
			DX::ThrowIfFailed(read == info.nFileSizeLow ? S_OK : E_FAIL);

			CloseHandle(file);

			return ret;
			});
	}
	// Converts a length in device-independent pixels (DIPs) to a length in physical pixels.
	inline float ConvertDipsToPixels(float dips, float dpi)
	{
		static const float dipsPerInch = 96.0f;
		return floorf(dips * dpi / dipsPerInch + 0.5f); // Round to nearest integer.
	}

#if defined(_DEBUG)
	// Check for SDK Layer support.
	inline bool SdkLayersAvailable()
	{
		HRESULT hr = D3D11CreateDevice(
			nullptr,
			D3D_DRIVER_TYPE_NULL,       // There is no need to create a real hardware device.
			0,
			D3D11_CREATE_DEVICE_DEBUG,  // Check for the SDK layers.
			nullptr,                    // Any feature level will do.
			0,
			D3D11_SDK_VERSION,          // Always set this to D3D11_SDK_VERSION for Microsoft Store apps.
			nullptr,                    // No need to keep the D3D device reference.
			nullptr,                    // No need to know the feature level.
			nullptr                     // No need to keep the D3D device context reference.
			);

		return SUCCEEDED(hr);
	}
#endif
}
