#include "pch.h"
#include "Sample3DSceneRenderer.h"

#include "..\Common\DirectXHelper.h"

using namespace Sampling;

using namespace winrt;
using namespace DirectX;
using namespace winrt::Windows::Foundation;

// Loads vertex and pixel shaders from files and instantiates the cube geometry.
Sample3DSceneRenderer::Sample3DSceneRenderer(const std::shared_ptr<DX::DeviceResources>& deviceResources) :
	m_loadingComplete(false),
	m_degreesPerSecond(45),
	m_indexCount(0),
	m_tracking(false),
	m_deviceResources(deviceResources)
{
	CreateDeviceDependentResources();
	CreateWindowSizeDependentResources();
}

// Initializes view parameters when the window size changes.
void Sample3DSceneRenderer::CreateWindowSizeDependentResources()
{
	Size outputSize = m_deviceResources->GetOutputSize();
	float aspectRatio = outputSize.Width / outputSize.Height;
	float fovAngleY = 70.0f * XM_PI / 180.0f;

	// This is a simple example of change that can be made when the app is in
	// portrait or snapped view.
	if (aspectRatio < 1.0f)
	{
		fovAngleY *= 2.0f;
	}

	// Note that the OrientationTransform3D matrix is post-multiplied here
	// in order to correctly orient the scene to match the display orientation.
	// This post-multiplication step is required for any draw calls that are
	// made to the swap chain render target. For draw calls to other targets,
	// this transform should not be applied.

	// This sample makes use of a right-handed coordinate system using row-major matrices.
	XMMATRIX perspectiveMatrix = XMMatrixPerspectiveFovRH(
		fovAngleY,
		aspectRatio,
		0.01f,
		100.0f
	);

	XMFLOAT4X4 orientation = m_deviceResources->GetOrientationTransform3D();

	XMMATRIX orientationMatrix = XMLoadFloat4x4(&orientation);

	XMStoreFloat4x4(
		&m_constantBufferData.projection,
		XMMatrixTranspose(perspectiveMatrix * orientationMatrix)
	);

	// Eye is at (0,0.7,1.5), looking at point (0,-0.1,0) with the up-vector along the y-axis.
	static const XMVECTORF32 eye = { 0.0f, 0.7f, 4.5f, 0.0f };
	static const XMVECTORF32 at = { 0.0f, -0.1f, 0.0f, 0.0f };
	static const XMVECTORF32 up = { 0.0f, 1.0f, 0.0f, 0.0f };

	XMStoreFloat4x4(&m_constantBufferData.view, XMMatrixTranspose(XMMatrixLookAtRH(eye, at, up)));
}

// Called once per frame, rotates the cube and calculates the model and view matrices.
void Sample3DSceneRenderer::Update(DX::StepTimer const& timer)
{
	if (!m_tracking)
	{
		// Convert degrees to radians, then convert seconds to rotation angle
		float radiansPerSecond = XMConvertToRadians(m_degreesPerSecond);
		double totalRotation = timer.GetTotalSeconds() * radiansPerSecond;
		float radians = static_cast<float>(fmod(totalRotation, XM_2PI));

		Rotate(radians);
	}
}

// Rotate the 3D cube model a set amount of radians.
void Sample3DSceneRenderer::Rotate(float radians)
{
	// Prepare to pass the updated model matrix to the shader
	XMStoreFloat4x4(&m_constantBufferData.model, XMMatrixTranspose(XMMatrixRotationY(radians)));
}

void Sample3DSceneRenderer::StartTracking()
{
	m_tracking = true;
}

// When tracking, the 3D cube can be rotated around its Y axis by tracking pointer position relative to the output screen width.
void Sample3DSceneRenderer::TrackingUpdate(float positionX)
{
	if (m_tracking)
	{
		float radians = XM_2PI * 2.0f * positionX / m_deviceResources->GetOutputSize().Width;
		Rotate(radians);
	}
}

void Sample3DSceneRenderer::StopTracking()
{
	m_tracking = false;
}

ID3D11Texture3D* Sampling::Sample3DSceneRenderer::GetDiffuseTexture()
{
	return m_volumeTexture.get();
}

void Sampling::Sample3DSceneRenderer::SetDiffuseResidencyMap(ID3D11ShaderResourceView* view)
{
	m_diffuseTextureResidencyView = view;
}

// Renders one frame using the vertex and pixel shaders.
void Sample3DSceneRenderer::Render()
{
	// Loading is asynchronous. Only draw geometry after it's loaded.
	if (!m_loadingComplete)
	{
		return;
	}

	auto context = m_deviceResources->GetD3DDeviceContext();

	// Prepare the constant buffer to send it to the graphics device.
	context->UpdateSubresource1(
		m_constantBuffer.get(),
		0,
		NULL,
		&m_constantBufferData,
		0,
		0,
		0
	);

	// Each vertex is one instance of the VertexPositionColor struct.
	UINT stride = sizeof(VertexPositionColor);
	UINT offset = 0;
	ID3D11Buffer* vertexBuffer = m_vertexBuffer.get();
	context->IASetVertexBuffers(
		0,
		1,
		&vertexBuffer,
		&stride,
		&offset
	);

	context->IASetIndexBuffer(
		m_indexBuffer.get(),
		DXGI_FORMAT_R16_UINT, // Each index is one 16-bit unsigned integer (short).
		0
	);

	context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	context->IASetInputLayout(m_inputLayout.get());

	// Attach our vertex shader.
	context->VSSetShader(
		m_vertexShader.get(),
		nullptr,
		0
	);

	// Send the constant buffer to the graphics device.
	ID3D11Buffer* constantBuffer = m_constantBuffer.get();
	context->VSSetConstantBuffers1(
		0,
		1,
		&constantBuffer,
		nullptr,
		nullptr
	);

	// Attach our pixel shader.
	context->PSSetShader(
		m_pixelShader.get(),
		nullptr,
		0
	);

	// Draw the objects.
	context->DrawIndexed(
		m_indexCount,
		0,
		0
	);
	ID3D11ShaderResourceView* textureViews[] =
	{
		m_volumeTextureView.get(),
		m_diffuseTextureResidencyView
	};
	context->PSSetShaderResources(0, ARRAYSIZE(textureViews), textureViews);
}

void Sample3DSceneRenderer::CreateDeviceDependentResources()
{
	// Load shaders asynchronously.
	auto vertexShaderFuture = std::async(std::launch::async, DX::ReadDataAsync, L"SampleVertexShader.cso");
	vertexShaderFuture.wait();
	auto vertexShaderData = vertexShaderFuture.get().get();

	DX::ThrowIfFailed(
		m_deviceResources->GetD3DDevice()->CreateVertexShader(
			vertexShaderData.data(),
			vertexShaderData.size(),
			nullptr,
			m_vertexShader.put()
		)
	);

	static const D3D11_INPUT_ELEMENT_DESC vertexDesc[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};

	DX::ThrowIfFailed(
		m_deviceResources->GetD3DDevice()->CreateInputLayout(
			vertexDesc,
			ARRAYSIZE(vertexDesc),
			vertexShaderData.data(),
			vertexShaderData.size(),
			m_inputLayout.put()
		)
	);

	// Create pixel shader
	auto pixelShaderFuture = std::async(std::launch::async, DX::ReadDataAsync, L"SamplePixelShader.cso");
	pixelShaderFuture.wait();
	auto pixelShaderData = pixelShaderFuture.get().get();

	DX::ThrowIfFailed(
		m_deviceResources->GetD3DDevice()->CreatePixelShader(
			pixelShaderData.data(),
			pixelShaderData.size(),
			nullptr,
			m_pixelShader.put()
		)
	);

	CD3D11_BUFFER_DESC constantBufferDesc(sizeof(ModelViewProjectionConstantBuffer), D3D11_BIND_CONSTANT_BUFFER);
	DX::ThrowIfFailed(
		m_deviceResources->GetD3DDevice()->CreateBuffer(
			&constantBufferDesc,
			nullptr,
			m_constantBuffer.put()
		)
	);

	// Once both shaders are loaded, create the mesh.

// Load mesh vertices. Each vertex has a position and a color.
	static const VertexPositionColor cubeVertices[] =
	{
		// Front face
		{ { -1.0f, -1.0f,  1.0f }, { 0.0f, 0.0f, 1.0f } }, // Bottom Left 
		{ { -1.0f,  1.0f,  1.0f }, { 0.0f, 1.0f, 1.0f } }, // Top Left
		{ {  1.0f,  1.0f,  1.0f }, { 1.0f, 1.0f, 1.0f } }, // Top Right
		{ {  1.0f, -1.0f,  1.0f }, { 1.0f, 0.0f, 1.0f } }, // Bottom Right

		// Back face
		{ { -1.0f, -1.0f, -1.0f }, { 1.0f, 0.0f, 0.0f } }, // Bottom Left 
		{ {  1.0f, -1.0f, -1.0f }, { 0.0f, 0.0f, 0.0f } }, // Bottom Right
		{ {  1.0f,  1.0f, -1.0f }, { 0.0f, 1.0f, 0.0f } }, // Top Right
		{ { -1.0f,  1.0f, -1.0f }, { 1.0f, 1.0f, 0.0f } }, // Top Left

		// Top face
		{ { -1.0f,  1.0f, -1.0f }, { 0.0f, 1.0f, 0.0f } }, // Top Left 
		{ {  1.0f,  1.0f, -1.0f }, { 1.0f, 1.0f, 0.0f } }, // Top Right
		{ {  1.0f,  1.0f,  1.0f }, { 1.0f, 1.0f, 1.0f } }, // Bottom Right
		{ { -1.0f,  1.0f,  1.0f }, { 0.0f, 1.0f, 1.0f } }, // Bottom Left

		// Bottom face
		{ { -1.0f, -1.0f, -1.0f }, { 0.0f, 0.0f, 0.0f } }, // Top Left 
		{ { -1.0f, -1.0f,  1.0f }, { 0.0f, 0.0f, 1.0f } }, // Bottom Left
		{ {  1.0f, -1.0f,  1.0f }, { 1.0f, 0.0f, 1.0f } }, // Bottom Right
		{ {  1.0f, -1.0f, -1.0f }, { 1.0f, 0.0f, 0.0f } }, // Top Right

		// Left face
		{ { -1.0f, -1.0f, -1.0f }, { 1.0f, 0.0f, 0.0f } }, // Bottom Left 
		{ { -1.0f,  1.0f, -1.0f }, { 1.0f, 1.0f, 0.0f } }, // Top Left
		{ { -1.0f,  1.0f,  1.0f }, { 0.0f, 1.0f, 1.0f } }, // Top Right
		{ { -1.0f, -1.0f,  1.0f }, { 0.0f, 0.0f, 1.0f } }, // Bottom Right

		// Right face
		{ {  1.0f, -1.0f, -1.0f }, { 0.0f, 0.0f, 0.0f } }, // Bottom Left 
		{ {  1.0f, -1.0f,  1.0f }, { 0.0f, 0.0f, 1.0f } }, // Bottom Right
		{ {  1.0f,  1.0f,  1.0f }, { 1.0f, 1.0f, 1.0f } }, // Top Right
		{ {  1.0f,  1.0f, -1.0f }, { 1.0f, 1.0f, 0.0f } }, // Top Left
	};

	D3D11_SUBRESOURCE_DATA vertexBufferData = {0};
	vertexBufferData.pSysMem = cubeVertices;
	vertexBufferData.SysMemPitch = 0;
	vertexBufferData.SysMemSlicePitch = 0;
	CD3D11_BUFFER_DESC vertexBufferDesc(sizeof(cubeVertices), D3D11_BIND_VERTEX_BUFFER);
	DX::ThrowIfFailed(
		m_deviceResources->GetD3DDevice()->CreateBuffer(
			&vertexBufferDesc,
			&vertexBufferData,
			m_vertexBuffer.put()
			)
		);

	// Load mesh indices. Each trio of indices represents
	// a triangle to be rendered on the screen.
	// For example: 0,2,1 means that the vertices with indexes
	// 0, 2 and 1 from the vertex buffer compose the 
	// first triangle of this mesh.
	static const unsigned short cubeIndices[] =
	{
		// Front face
		0, 1, 2, 0, 2, 3,
		// Back face
		4, 5, 6, 4, 6, 7,
		// Top face
		8, 9, 10, 8, 10, 11,
		// Bottom face
		12, 13, 14, 12, 14, 15,
		// Left face
		16, 17, 18, 16, 18, 19,
		// Right face
		20, 21, 22, 20, 22, 23,
	};

	m_indexCount = ARRAYSIZE(cubeIndices);

	D3D11_SUBRESOURCE_DATA indexBufferData = {0};
	indexBufferData.pSysMem = cubeIndices;
	indexBufferData.SysMemPitch = 0;
	indexBufferData.SysMemSlicePitch = 0;
	CD3D11_BUFFER_DESC indexBufferDesc(sizeof(cubeIndices), D3D11_BIND_INDEX_BUFFER);
	DX::ThrowIfFailed(
		m_deviceResources->GetD3DDevice()->CreateBuffer(
			&indexBufferDesc,
			&indexBufferData,
			m_indexBuffer.put()
			)
		);

	// Create a tiled texture and view for the 3D volume.
	UINT maxLOD = 0;
	UINT dimension = SampleSettings::TileResidency::Resolution / 32;
	while (dimension > 1) {
		dimension /= 2;
		maxLOD++;
	}
	D3D11_TEXTURE3D_DESC volumeTextureDesc;
	ZeroMemory(&volumeTextureDesc, sizeof(volumeTextureDesc));
	volumeTextureDesc.Width = SampleSettings::TileResidency::Resolution;
	volumeTextureDesc.Height = SampleSettings::TileResidency::Resolution;
	volumeTextureDesc.Depth = SampleSettings::TileResidency::Resolution / 2; // Set the depth for the 3D volume.
	volumeTextureDesc.MipLevels = maxLOD + 1; // Set the number of MIP levels.
	volumeTextureDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
	volumeTextureDesc.Usage = D3D11_USAGE_DEFAULT;
	volumeTextureDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	volumeTextureDesc.MiscFlags = D3D11_RESOURCE_MISC_TILED;
	DX::ThrowIfFailed(m_deviceResources->GetD3DDevice()->CreateTexture3D(&volumeTextureDesc, nullptr, m_volumeTexture.put()));

	D3D11_SHADER_RESOURCE_VIEW_DESC volumeTextureViewDesc;
	ZeroMemory(&volumeTextureViewDesc, sizeof(volumeTextureViewDesc));
	volumeTextureViewDesc.Format = volumeTextureDesc.Format;
	volumeTextureViewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE3D;
	volumeTextureViewDesc.Texture3D.MostDetailedMip = 0;
	volumeTextureViewDesc.Texture3D.MipLevels = volumeTextureDesc.MipLevels; // Use the full MIP chain.
	DX::ThrowIfFailed(m_deviceResources->GetD3DDevice()->CreateShaderResourceView(m_volumeTexture.get(), &volumeTextureViewDesc, m_volumeTextureView.put()));
	// Once the cube is loaded, the object is ready to be rendered.

	m_loadingComplete = true;
}

void Sample3DSceneRenderer::ReleaseDeviceDependentResources()
{
	m_loadingComplete = false;
	m_vertexShader = nullptr;
	m_inputLayout = nullptr;
	m_pixelShader = nullptr;
	m_constantBuffer = nullptr;
	m_vertexBuffer = nullptr;
	m_indexBuffer = nullptr;
}