#pragma once

#include "..\Common\DeviceResources.h"
#include "ShaderStructures.h"
#include "SampleSettings.h"
#include "..\Common\StepTimer.h"

using namespace winrt;
namespace Sampling
{
	// This sample renderer instantiates a basic rendering pipeline.
	class Sample3DSceneRenderer
	{
	public:
		Sample3DSceneRenderer(const std::shared_ptr<DX::DeviceResources>& deviceResources);
		void CreateDeviceDependentResources();
		void CreateWindowSizeDependentResources();
		void ReleaseDeviceDependentResources();
		void Update(DX::StepTimer const& timer);
		void Render();
		void StartTracking();
		void TrackingUpdate(float positionX);
		void StopTracking();
		bool IsTracking() { return m_tracking; }

		ID3D11Texture3D* GetDiffuseTexture();
		void SetDiffuseResidencyMap(ID3D11ShaderResourceView* view);


	private:
		void Rotate(float radians);

	private:
		// Cached pointer to device resources.
		std::shared_ptr<DX::DeviceResources> m_deviceResources;
		com_ptr<ID3D11SamplerState> samplerState;

		// Direct3D resources for cube geometry.
		com_ptr<ID3D11InputLayout>	m_inputLayout = nullptr;
		com_ptr<ID3D11Buffer>		m_vertexBuffer = nullptr;
		com_ptr<ID3D11Buffer>		m_indexBuffer = nullptr;
		com_ptr<ID3D11VertexShader>	m_vertexShader = nullptr;
		com_ptr<ID3D11PixelShader>	m_pixelShader = nullptr;
		com_ptr<ID3D11Buffer>		m_constantBuffer = nullptr;

		// System resources for cube geometry.
		ModelViewProjectionConstantBuffer	m_constantBufferData;
		UINT32	m_indexCount;

		// Variables used with the rendering loop.
		bool	m_loadingComplete;
		float	m_degreesPerSecond;
		bool	m_tracking;

		com_ptr<ID3D11Texture3D> m_volumeTexture = nullptr;
		com_ptr<ID3D11ShaderResourceView> m_volumeTextureView = nullptr;
		ID3D11ShaderResourceView* m_diffuseTextureResidencyView;
	};
}

