#include "pch.h"
#include "DeviceResources.h"
#include "DirectXHelper.h"
#include <microsoft.ui.xaml.media.dxinterop.h>

#include <winrt/Windows.Graphics.Display.h>

using namespace D2D1;
using namespace winrt;
using namespace DirectX;

namespace DisplayMetrics
{
	// High resolution displays can require a lot of GPU and battery power to render.
	// High resolution phones, for example, may suffer from poor battery life if
	// games attempt to render at 60 frames per second at full fidelity.
	// The decision to render at full fidelity across all platforms and form factors
	// should be deliberate.
	static const bool SupportHighResolutions = false;

	// The default thresholds that define a "high resolution" display. If the thresholds
	// are exceeded and SupportHighResolutions is false, the dimensions will be scaled
	// by 50%.
	static const float DpiThreshold = 192.0f;		// 200% of standard desktop display.
	static const float WidthThreshold = 1920.0f;	// 1080p width.
	static const float HeightThreshold = 1080.0f;	// 1080p height.
};

// Constants used to calculate screen rotations.
namespace ScreenRotation
{
	// 0-degree Z-rotation
	static const XMFLOAT4X4 Rotation0(
		1.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 1.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f
	);

	// 90-degree Z-rotation
	static const XMFLOAT4X4 Rotation90(
		0.0f, 1.0f, 0.0f, 0.0f,
		-1.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f
	);

	// 180-degree Z-rotation
	static const XMFLOAT4X4 Rotation180(
		-1.0f, 0.0f, 0.0f, 0.0f,
		0.0f, -1.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f
	);

	// 270-degree Z-rotation
	static const XMFLOAT4X4 Rotation270(
		0.0f, -1.0f, 0.0f, 0.0f,
		1.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f
	);
};

DX::DeviceResources::DeviceResources() : 
	m_screenViewport(),
	m_d3dFeatureLevel(D3D_FEATURE_LEVEL_11_1),
	m_d3dRenderTargetSize(),
	m_outputSize(),
	m_logicalSize(),
	m_dpi(-1.0f),
	m_effectiveDpi(-1.0f),
	m_compositionScaleX(1.0f),
	m_compositionScaleY(1.0f),
	m_deviceNotify(nullptr)
{
	CreateDeviceIndependentResources();
	CreateDeviceResources();
}

void DX::DeviceResources::CreateDeviceIndependentResources()
{
	// Initialize Direct2D resources.
	D2D1_FACTORY_OPTIONS options;
	ZeroMemory(&options, sizeof(D2D1_FACTORY_OPTIONS));
	// Initialize the Direct2D Factory.
	DX::ThrowIfFailed(
		D2D1CreateFactory(
			D2D1_FACTORY_TYPE_SINGLE_THREADED,
			__uuidof(ID2D1Factory3),
			&options,
			m_d2dFactory.put_void()
		)
	);
	// Initialize the DirectWrite Factory.

	DX::ThrowIfFailed(
		DWriteCreateFactory(
			DWRITE_FACTORY_TYPE_SHARED,
			__uuidof(IDWriteFactory3),
			reinterpret_cast<IUnknown**>(m_dwriteFactory.put_void())
		)
	);

	// Initialize the Windows Imaging Component (WIC) Factory.
	DX::ThrowIfFailed(
		CoCreateInstance(
			CLSID_WICImagingFactory2,
			nullptr,
			CLSCTX_INPROC_SERVER,
			IID_PPV_ARGS(&m_wicFactory)
		)
	);
}

void DX::DeviceResources::CreateDeviceResources()
{
	// This flag adds support for surfaces with a different color channel ordering
	// than the API default. It is required for compatibility with Direct2D.
	UINT creationFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;

#if defined(_DEBUG)
	if (DX::SdkLayersAvailable())
	{
		// If the project is in a debug build, enable debugging via SDK Layers with this flag.
		creationFlags |= D3D11_CREATE_DEVICE_DEBUG;
	}
#endif

	// This array defines the set of DirectX hardware feature levels this app will support.
	// Note the ordering should be preserved.
	// Don't forget to declare your application's minimum required feature level in its
	// description.  All applications are assumed to support 9.1 unless otherwise stated.
	D3D_FEATURE_LEVEL featureLevels[] =
	{
		D3D_FEATURE_LEVEL_12_1,
		D3D_FEATURE_LEVEL_12_0,
		D3D_FEATURE_LEVEL_11_1,
		D3D_FEATURE_LEVEL_11_0,
		D3D_FEATURE_LEVEL_10_1,
		D3D_FEATURE_LEVEL_10_0,
		D3D_FEATURE_LEVEL_9_3,
		D3D_FEATURE_LEVEL_9_2,
		D3D_FEATURE_LEVEL_9_1
	};

	// Create the Direct3D 11 API device object and a corresponding context.
	com_ptr<ID3D11Device> device;
	com_ptr<ID3D11DeviceContext> context;

	HRESULT hr = D3D11CreateDevice(
		nullptr,					// Specify nullptr to use the default adapter.
		D3D_DRIVER_TYPE_HARDWARE,	// Create a device using the hardware graphics driver.
		0,							// Should be 0 unless the driver is D3D_DRIVER_TYPE_SOFTWARE.
		creationFlags,				// Set debug and Direct2D compatibility flags.
		featureLevels,				// List of feature levels this app can support.
		ARRAYSIZE(featureLevels),	// Size of the list above.
		D3D11_SDK_VERSION,			// Always set this to D3D11_SDK_VERSION for Microsoft Store apps.
		device.put(),					// Returns the Direct3D device created.
		&m_d3dFeatureLevel,			// Returns feature level of device created.
		context.put()					// Returns the device immediate context.
	);

	if (FAILED(hr))
	{
		// If the initialization fails, fall back to the WARP device.
		// For more information on WARP, see: 
		// https://go.microsoft.com/fwlink/?LinkId=286690
		DX::ThrowIfFailed(
			D3D11CreateDevice(
				nullptr,
				D3D_DRIVER_TYPE_WARP, // Create a WARP device instead of a hardware device.
				0,
				creationFlags,
				featureLevels,
				ARRAYSIZE(featureLevels),
				D3D11_SDK_VERSION,
				device.put(),
				&m_d3dFeatureLevel,
				context.put()
			)
		);
	}

	// Store pointers to the Direct3D 11.3 API device and immediate context.

	device.as(m_d3dDevice);
	context.as(m_d3dContext);
	// Store pointers to the Direct3D 11.3 API device and immediate context.
	com_ptr<IDXGIDevice3> dxgiDevice;
	m_d3dDevice.as(dxgiDevice);

	DX::ThrowIfFailed(
		m_d2dFactory->CreateDevice(dxgiDevice.get(), m_d2dDevice.put())
	);

	DX::ThrowIfFailed(
		m_d2dDevice->CreateDeviceContext(
			D2D1_DEVICE_CONTEXT_OPTIONS_NONE,
			m_d2dContext.put()
		)
	);
}

void DX::DeviceResources::CreateWindowSizeDependentResources()
{
	// Clear the previous window size specific context.
	ID3D11RenderTargetView* nullViews[] = { nullptr };
	m_d3dContext->OMSetRenderTargets(ARRAYSIZE(nullViews), nullViews, nullptr);
	m_d3dRenderTargetView = nullptr;
	m_d2dContext->SetTarget(nullptr);
	m_d2dTargetBitmap = nullptr;
	m_d3dDepthStencilView = nullptr;
	m_d3dContext->Flush1(D3D11_CONTEXT_TYPE_ALL, nullptr);

	UpdateRenderTargetSize();

	// The width and height of the swap chain must be based on the window's
	// natively-oriented width and height. If the window is not in the native
	// orientation, the dimensions must be reversed.
	DXGI_MODE_ROTATION displayRotation = ComputeDisplayRotation();

	bool swapDimensions = displayRotation == DXGI_MODE_ROTATION_ROTATE90 || displayRotation == DXGI_MODE_ROTATION_ROTATE270;
	m_d3dRenderTargetSize.Width = swapDimensions ? m_outputSize.Height : m_outputSize.Width;
	m_d3dRenderTargetSize.Height = swapDimensions ? m_outputSize.Width : m_outputSize.Height;

	if (m_swapChain != nullptr)
	{
		// If the swap chain already exists, resize it.
		HRESULT hr = m_swapChain->ResizeBuffers(
			2, // Double-buffered swap chain.
			lround(m_d3dRenderTargetSize.Width),
			lround(m_d3dRenderTargetSize.Height),
			DXGI_FORMAT_B8G8R8A8_UNORM,
			0
		);

		if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET)
		{
			// If the device was removed for any reason, a new device and swap chain will need to be created.
			HandleDeviceLost();

			// Everything is set up now. Do not continue execution of this method. HandleDeviceLost will reenter this method 
			// and correctly set up the new device.
			return;
		}
		else
		{
			DX::ThrowIfFailed(hr);
		}
	}
	else
	{
		// Otherwise, create a new one using the same adapter as the existing Direct3D device.
		DXGI_SCALING scaling = DisplayMetrics::SupportHighResolutions ? DXGI_SCALING_NONE : DXGI_SCALING_STRETCH;
		DXGI_SWAP_CHAIN_DESC1 swapChainDesc = { 0 };

		swapChainDesc.Width = lround(m_d3dRenderTargetSize.Width);		// Match the size of the window.
		swapChainDesc.Height = lround(m_d3dRenderTargetSize.Height);
		swapChainDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;				// This is the most common swap chain format.
		swapChainDesc.Stereo = false;
		swapChainDesc.SampleDesc.Count = 1;								// Don't use multi-sampling.
		swapChainDesc.SampleDesc.Quality = 0;
		swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		swapChainDesc.BufferCount = 2;									// Use double-buffering to minimize latency.
		swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;	// All Microsoft Store apps must use _FLIP_ SwapEffects.
		swapChainDesc.Flags = 0;
		swapChainDesc.Scaling = scaling;
		swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;

		// This sequence obtains the DXGI factory that was used to create the Direct3D device above.
		com_ptr<IDXGIDevice3> dxgiDevice;
		m_d3dDevice.as(dxgiDevice);

		com_ptr<IDXGIAdapter> dxgiAdapter;
		DX::ThrowIfFailed(
			dxgiDevice->GetAdapter(dxgiAdapter.put())
		);

		com_ptr<IDXGIFactory4> dxgiFactory;
		DX::ThrowIfFailed(
			dxgiAdapter->GetParent(IID_PPV_ARGS(&dxgiFactory))
		);

		// When using XAML interop, the swap chain must be created for composition.
		com_ptr<IDXGISwapChain1> swapChain;
		DX::ThrowIfFailed(
			dxgiFactory->CreateSwapChainForComposition(
				m_d3dDevice.get(),
				&swapChainDesc,
				nullptr,
				swapChain.put()
			)
		);

		swapChain.as(m_swapChain);
		auto panelNative{ m_swapChainPanel.as<ISwapChainPanelNative>() };

		winrt::check_hresult(
			panelNative->SetSwapChain(swapChain.get())
		);
		// Ensure that DXGI does not queue more than one frame at a time. This both reduces latency and
		// ensures that the application will only render after each VSync, minimizing power consumption.
		DX::ThrowIfFailed(
			dxgiDevice->SetMaximumFrameLatency(1)
		);
	}

	// Set the proper orientation for the swap chain, and generate 2D and
	// 3D matrix transformations for rendering to the rotated swap chain.
	// Note the rotation angle for the 2D and 3D transforms are different.
	// This is due to the difference in coordinate spaces.  Additionally,
	// the 3D matrix is specified explicitly to avoid rounding errors.

	switch (DXGI_MODE_ROTATION_IDENTITY)
	{
	case DXGI_MODE_ROTATION_IDENTITY:
		m_orientationTransform2D = Matrix3x2F::Identity();
		m_orientationTransform3D = ScreenRotation::Rotation0;
		break;

	case DXGI_MODE_ROTATION_ROTATE90:
		m_orientationTransform2D =
			Matrix3x2F::Rotation(90.0f) *
			Matrix3x2F::Translation(m_logicalSize.Height, 0.0f);
		m_orientationTransform3D = ScreenRotation::Rotation270;
		break;

	case DXGI_MODE_ROTATION_ROTATE180:
		m_orientationTransform2D =
			Matrix3x2F::Rotation(180.0f) *
			Matrix3x2F::Translation(m_logicalSize.Width, m_logicalSize.Height);
		m_orientationTransform3D = ScreenRotation::Rotation180;
		break;

	case DXGI_MODE_ROTATION_ROTATE270:
		m_orientationTransform2D =
			Matrix3x2F::Rotation(270.0f) *
			Matrix3x2F::Translation(0.0f, m_logicalSize.Width);
		m_orientationTransform3D = ScreenRotation::Rotation90;
		break;

	default:
		throw;
	}

	DX::ThrowIfFailed(
		m_swapChain->SetRotation(displayRotation)
	);

	// Setup inverse scale on the swap chain
	DXGI_MATRIX_3X2_F inverseScale = { 0 };
	inverseScale._11 = 1.0f / m_effectiveCompositionScaleX;
	inverseScale._22 = 1.0f / m_effectiveCompositionScaleY;
	com_ptr<IDXGISwapChain2> spSwapChain2;
	m_swapChain.as(spSwapChain2);

	DX::ThrowIfFailed(
		spSwapChain2->SetMatrixTransform(&inverseScale)
	);

	// Create a render target view of the swap chain back buffer.
	com_ptr<ID3D11Texture2D1> backBuffer;
	DX::ThrowIfFailed(
		m_swapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer))
	);

	DX::ThrowIfFailed(
		m_d3dDevice->CreateRenderTargetView1(
			backBuffer.get(),
			nullptr,
			m_d3dRenderTargetView.put()
		)
	);

	// Create a depth stencil view for use with 3D rendering if needed.
	CD3D11_TEXTURE2D_DESC1 depthStencilDesc(
		DXGI_FORMAT_D24_UNORM_S8_UINT,
		lround(m_d3dRenderTargetSize.Width),
		lround(m_d3dRenderTargetSize.Height),
		1, // This depth stencil view has only one texture.
		1, // Use a single mipmap level.
		D3D11_BIND_DEPTH_STENCIL
	);

	com_ptr<ID3D11Texture2D1> depthStencil;
	DX::ThrowIfFailed(
		m_d3dDevice->CreateTexture2D1(
			&depthStencilDesc,
			nullptr,
			depthStencil.put()
		)
	);

	CD3D11_DEPTH_STENCIL_VIEW_DESC depthStencilViewDesc(D3D11_DSV_DIMENSION_TEXTURE2D);
	DX::ThrowIfFailed(
		m_d3dDevice->CreateDepthStencilView(
			depthStencil.get(),
			&depthStencilViewDesc,
			m_d3dDepthStencilView.put()
		)
	);

	// Set the 3D rendering viewport to target the entire window.
	m_screenViewport = CD3D11_VIEWPORT(
		0.0f,
		0.0f,
		m_d3dRenderTargetSize.Width,
		m_d3dRenderTargetSize.Height
	);

	m_d3dContext->RSSetViewports(1, &m_screenViewport);

	// Create a Direct2D target bitmap associated with the
	// swap chain back buffer and set it as the current target.
	D2D1_BITMAP_PROPERTIES1 bitmapProperties2 =
		D2D1::BitmapProperties1(
			D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
			D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED),
			m_dpi,
			m_dpi
		);

	com_ptr<IDXGISurface2> dxgiBackBuffer;
	DX::ThrowIfFailed(
		m_swapChain->GetBuffer(0, IID_PPV_ARGS(&dxgiBackBuffer))
	);

	DX::ThrowIfFailed(
		m_d2dContext->CreateBitmapFromDxgiSurface(
			dxgiBackBuffer.get(),
			&bitmapProperties2,
			m_d2dTargetBitmap.put()
		)
	);

	m_d2dContext->SetTarget(m_d2dTargetBitmap.get());
	m_d2dContext->SetDpi(m_effectiveDpi, m_effectiveDpi);

	// Grayscale text anti-aliasing is recommended for all Microsoft Store apps.
	m_d2dContext->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);

//	// Create a Direct2D target bitmap associated with the
//// swap chain back buffer, and set it as the current target.
//	D2D1_BITMAP_PROPERTIES1 bitmapProperties =
//		D2D1::BitmapProperties1(
//			D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
//			D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED),
//			96.f,
//			96.f
//		);
//
//	winrt::com_ptr<::IDXGISurface> dxgiBackBuffer2;
//	m_swapChain->GetBuffer(0, __uuidof(dxgiBackBuffer2), dxgiBackBuffer2.put_void());
//
//	winrt::com_ptr< ::ID2D1Bitmap1 > targetBitmap;
//	winrt::check_hresult(
//		m_d2dContext->CreateBitmapFromDxgiSurface(
//			dxgiBackBuffer2.get(),
//			&bitmapProperties,
//			targetBitmap.put()
//		)
//	);
//
//	m_d2dContext->SetTarget(targetBitmap.get());
//
//	// Draw using Direct2D context.
//	m_d2dContext->BeginDraw();
//
//	m_d2dContext->Clear(D2D1::ColorF(D2D1::ColorF::Red));
//
//	winrt::com_ptr<::ID2D1SolidColorBrush> brush;
//	winrt::check_hresult(m_d2dContext->CreateSolidColorBrush(
//	    D2D1::ColorF(D2D1::ColorF::Chocolate),
//	    D2D1::BrushProperties(0.8f),
//	    brush.put()));
//
//	D2D1_SIZE_F const size{ 350, 500 };
//	D2D1_RECT_F const rect{ 100.0f, 100.0f, size.width - 100.0f, size.height - 100.0f };
//	m_d2dContext->DrawRectangle(rect, brush.get(), 100.0f);
//
//	m_d2dContext->EndDraw();
//
//	m_swapChain->Present(1, 0);
}

void DX::DeviceResources::UpdateRenderTargetSize()
{
	m_effectiveDpi = m_dpi;
	m_effectiveCompositionScaleX = m_compositionScaleX;
	m_effectiveCompositionScaleY = m_compositionScaleY;

	// To improve battery life on high resolution devices, render to a smaller render target
	// and allow the GPU to scale the output when it is presented.
	if (!DisplayMetrics::SupportHighResolutions && m_dpi > DisplayMetrics::DpiThreshold)
	{
		float width = DX::ConvertDipsToPixels(m_logicalSize.Width, m_dpi);
		float height = DX::ConvertDipsToPixels(m_logicalSize.Height, m_dpi);

		// When the device is in portrait orientation, height > width. Compare the
		// larger dimension against the width threshold and the smaller dimension
		// against the height threshold.
		if (std::max(width, height) > DisplayMetrics::WidthThreshold && std::min(width, height) > DisplayMetrics::HeightThreshold)
		{
			// To scale the app we change the effective DPI. Logical size does not change.
			m_effectiveDpi /= 2.0f;
			m_effectiveCompositionScaleX /= 2.0f;
			m_effectiveCompositionScaleY /= 2.0f;
		}
	}

	// Calculate the necessary render target size in pixels.
	m_outputSize.Width = DX::ConvertDipsToPixels(m_logicalSize.Width, m_effectiveDpi);
	m_outputSize.Height = DX::ConvertDipsToPixels(m_logicalSize.Height, m_effectiveDpi);

	// Prevent zero size DirectX content from being created.
	m_outputSize.Width = std::max(m_outputSize.Width, 1.0f);
	m_outputSize.Height = std::max(m_outputSize.Height, 1.0f);
}

void DX::DeviceResources::SetSwapChainPanel(Microsoft::UI::Xaml::Controls::SwapChainPanel panel)
{

	m_swapChainPanel = panel;
	m_logicalSize = Windows::Foundation::Size(static_cast<float>(panel.ActualWidth()), static_cast<float>(panel.ActualHeight()));
	m_compositionScaleX = 1;
	m_compositionScaleY = 1;
	m_dpi = 100;
	m_d2dContext->SetDpi(m_dpi, m_dpi);

	CreateWindowSizeDependentResources();
}

void DX::DeviceResources::SetLogicalSize(Windows::Foundation::Size logicalSize)
{
	if (m_logicalSize != logicalSize)
	{
		m_logicalSize = logicalSize;
		CreateWindowSizeDependentResources();
	}
}

void DX::DeviceResources::SetDpi(float dpi)
{
	if (dpi != m_dpi)
	{
		m_dpi = dpi;
		m_d2dContext->SetDpi(m_dpi, m_dpi);
		CreateWindowSizeDependentResources();
	}
}

void DX::DeviceResources::SetCompositionScale(float compositionScaleX, float compositionScaleY)
{
	if (m_compositionScaleX != compositionScaleX ||
		m_compositionScaleY != compositionScaleY)
	{
		m_compositionScaleX = compositionScaleX;
		m_compositionScaleY = compositionScaleY;
		CreateWindowSizeDependentResources();
	}
}

void DX::DeviceResources::ValidateDevice()
{
	// The D3D Device is no longer valid if the default adapter changed since the device
	// was created or if the device has been removed.

	// First, get the information for the default adapter from when the device was created.

	com_ptr<IDXGIDevice3> dxgiDevice;
	m_d3dDevice.as(dxgiDevice);

	com_ptr<IDXGIAdapter> deviceAdapter;
	DX::ThrowIfFailed(dxgiDevice->GetAdapter(deviceAdapter.put()));

	com_ptr<IDXGIFactory2> deviceFactory;
	DX::ThrowIfFailed(deviceAdapter->GetParent(IID_PPV_ARGS(&deviceFactory)));

	com_ptr<IDXGIAdapter1> previousDefaultAdapter;
	DX::ThrowIfFailed(deviceFactory->EnumAdapters1(0, previousDefaultAdapter.put()));

	DXGI_ADAPTER_DESC1 previousDesc;
	DX::ThrowIfFailed(previousDefaultAdapter->GetDesc1(&previousDesc));

	// Next, get the information for the current default adapter.

	com_ptr<IDXGIFactory4> currentFactory;
	DX::ThrowIfFailed(CreateDXGIFactory1(IID_PPV_ARGS(&currentFactory)));

	com_ptr<IDXGIAdapter1> currentDefaultAdapter;
	DX::ThrowIfFailed(currentFactory->EnumAdapters1(0, currentDefaultAdapter.put()));

	DXGI_ADAPTER_DESC1 currentDesc;
	DX::ThrowIfFailed(currentDefaultAdapter->GetDesc1(&currentDesc));

	// If the adapter LUIDs don't match, or if the device reports that it has been removed,
	// a new D3D device must be created.

	if (previousDesc.AdapterLuid.LowPart != currentDesc.AdapterLuid.LowPart ||
		previousDesc.AdapterLuid.HighPart != currentDesc.AdapterLuid.HighPart ||
		FAILED(m_d3dDevice->GetDeviceRemovedReason()))
	{
		// Release references to resources related to the old device.
		dxgiDevice = nullptr;
		deviceAdapter = nullptr;
		deviceFactory = nullptr;
		previousDefaultAdapter = nullptr;

		// Create a new device and swap chain.
		HandleDeviceLost();
	}
}

void DX::DeviceResources::HandleDeviceLost()
{
	m_swapChain = nullptr;

	if (m_deviceNotify != nullptr)
	{
		m_deviceNotify->OnDeviceLost();
	}

	CreateDeviceResources();
	m_d2dContext->SetDpi(m_dpi, m_dpi);
	CreateWindowSizeDependentResources();

	if (m_deviceNotify != nullptr)
	{
		m_deviceNotify->OnDeviceRestored();
	}
}

void DX::DeviceResources::RegisterDeviceNotify(IDeviceNotify* deviceNotify)
{
	m_deviceNotify = deviceNotify;
}

void DX::DeviceResources::Trim()
{
	com_ptr<IDXGIDevice3> dxgiDevice;
	m_d3dDevice.as(dxgiDevice);

	dxgiDevice->Trim();
}

void DX::DeviceResources::Present()
{
	// The first argument instructs DXGI to block until VSync, putting the application
// to sleep until the next VSync. This ensures we don't waste any cycles rendering
// frames that will never be displayed to the screen.
	DXGI_PRESENT_PARAMETERS parameters = { 0 };
	HRESULT hr = m_swapChain->Present1(1, 0, &parameters);

	// Discard the contents of the render target.
	// This is a valid operation only when the existing contents will be entirely
	// overwritten. If dirty or scroll rects are used, this call should be modified.
	m_d3dContext->DiscardView1(m_d3dRenderTargetView.get(), nullptr, 0);

	// Discard the contents of the depth stencil.
	m_d3dContext->DiscardView1(m_d3dDepthStencilView.get(), nullptr, 0);

	// If the device was removed either by a disconnection or a driver upgrade, we 
	// must recreate all device resources.
	if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET)
	{
		HandleDeviceLost();
	}
	else
	{
		DX::ThrowIfFailed(hr);
	}
}

DXGI_MODE_ROTATION DX::DeviceResources::ComputeDisplayRotation()
{
	return DXGI_MODE_ROTATION_IDENTITY;
}
