#include "pch.h"
#include "ResidencyManager.h"
#include "SampleSettings.h"
#include <Common\DirectXHelper.h>
//#include "..\..\..\DirectXTK\Src\PlatformHelpers.h"
//#include "..\..\..\DirectXTK\Src\BinaryReader.h"

using namespace Sampling;
using namespace concurrency;
using namespace DirectX;
using namespace Windows::Foundation;

ResidencyManager::ResidencyManager(const std::shared_ptr<DX::DeviceResources>& deviceResources) :
    m_deviceResources(deviceResources),
    m_debugMode(false),
    m_activeTileLoadingOperations(0),
    m_reservedTiles(0),
    m_defaultTileIndex(-1)
{
    CreateDeviceDependentResources();
}

void ResidencyManager::CreateDeviceDependentResources()
{
    auto device = m_deviceResources->GetD3DDevice();
    // Create the tile pool.
    D3D11_BUFFER_DESC tilePoolDesc;
    ZeroMemory(&tilePoolDesc, sizeof(tilePoolDesc));
    tilePoolDesc.ByteWidth = SampleSettings::TileSizeInBytes * SampleSettings::TileResidency::PoolSizeInTiles;
    tilePoolDesc.Usage = D3D11_USAGE_DEFAULT;
    tilePoolDesc.MiscFlags = D3D11_RESOURCE_MISC_TILE_POOL;
    DX::ThrowIfFailed(m_deviceResources->GetD3DDevice()->CreateBuffer(&tilePoolDesc, nullptr, m_tilePool.put()));
}

Concurrency::task<void> ResidencyManager::CreateDeviceDependentResourcesAsync()
{
    // Load and create the vertex shader and input layout.
    auto vsTask = DX::ReadDataAsync2(L"ResidencyViewer.vs.cso").then([this](std::vector<byte> fileData)
        {
            auto device = m_deviceResources->GetD3DDevice();
            D3D11_INPUT_ELEMENT_DESC inputLayoutDesc[] =
            {
                { "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT,       0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0 },
                { "TEXCOORD", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 8,  D3D11_INPUT_PER_VERTEX_DATA, 0 },
            };
            auto address = m_viewerInputLayout.get();
            DX::ThrowIfFailed(device->CreateInputLayout(inputLayoutDesc, ARRAYSIZE(inputLayoutDesc), fileData.data(), fileData.size(), &address));
            auto address2 = m_viewerVertexShader.get();
            DX::ThrowIfFailed(device->CreateVertexShader(fileData.data(), fileData.size(), nullptr, &address2));
        });

    // Load and create the pixel shader.
    auto psTask = DX::ReadDataAsync2(L"ResidencyViewer.ps.cso").then([this](std::vector<byte> fileData)
        {
            auto device = m_deviceResources->GetD3DDevice();
            auto address = m_viewerPixelShader.get();
            DX::ThrowIfFailed(device->CreatePixelShader(fileData.data(), fileData.size(), nullptr, &address));
        });

    return (vsTask && psTask);
}

void ResidencyManager::ReleaseDeviceDependentResources()
{
    m_viewerVertexBuffer = nullptr;
    m_viewerIndexBuffer = nullptr;
    m_viewerInputLayout = nullptr;
    m_viewerVertexShader = nullptr;
    m_viewerPixelShader = nullptr;
    m_sampler = nullptr;
    m_viewerVertexShaderConstantBuffer = nullptr;
    m_tilePool = nullptr;

    m_managedResources.clear();
    m_trackedTiles.clear();
    m_seenTileList.clear();
    m_loadingTileList.clear();
    m_mappedTileList.clear();
}

ID3D11ShaderResourceView* ResidencyManager::ManageTexture(ID3D11Texture3D* texture)
{
    auto device = m_deviceResources->GetD3DDevice();
    auto context = m_deviceResources->GetD3DDeviceContext();


    auto resource = std::shared_ptr<ManagedTiledResource>(new ManagedTiledResource);
    resource->texture = texture;
    texture->GetDesc(&resource->textureDesc);
    UINT subresourceTilings = resource->textureDesc.MipLevels;

    resource->subresourceTilings.resize(subresourceTilings);
    device->GetResourceTiling(
        texture,
        &resource->totalTiles,
        &resource->packedMipDesc,
        &resource->tileShape,
        &subresourceTilings,
        0,
        resource->subresourceTilings.data()
    );
    int width = resource->subresourceTilings[0].WidthInTiles;
    int height = resource->subresourceTilings[0].HeightInTiles;

    // Create the residency texture.
    D3D11_TEXTURE3D_DESC textureDesc;
    ZeroMemory(&textureDesc, sizeof(textureDesc));
    textureDesc.Width = std::max(resource->subresourceTilings[0].WidthInTiles, (UINT)resource->subresourceTilings[0].HeightInTiles);
    textureDesc.Height = textureDesc.Width;
    textureDesc.Depth = resource->subresourceTilings[0].DepthInTiles; // Set the depth.
    textureDesc.MipLevels = 1;
    textureDesc.Format = DXGI_FORMAT_R8_UNORM;
    textureDesc.Usage = D3D11_USAGE_DEFAULT;
    textureDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    textureDesc.MiscFlags = D3D11_RESOURCE_MISC_TILED;
    DX::ThrowIfFailed(device->CreateTexture3D(&textureDesc, nullptr, resource->residencyTexture.put()));

    // Create the shader resource view that will be used by both the terrain renderer and the visualizer.
    D3D11_SHADER_RESOURCE_VIEW_DESC shaderResourceViewDesc;
    ZeroMemory(&shaderResourceViewDesc, sizeof(shaderResourceViewDesc));
    shaderResourceViewDesc.Format = textureDesc.Format;
    shaderResourceViewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE3D;
    shaderResourceViewDesc.Texture3D.MipLevels = 1;
    shaderResourceViewDesc.Texture3D.MostDetailedMip = 0;
    DX::ThrowIfFailed(device->CreateShaderResourceView(resource->residencyTexture.get(), &shaderResourceViewDesc, resource->residencyTextureView.put()));

    com_ptr<ID3D11Resource> Res;
    com_ptr<ID3D11ShaderResourceView> myTexture;
    
	winrt::init_apartment(winrt::apartment_type::single_threaded);
    wchar_t fname[_MAX_PATH];

    int totalWidth = SampleSettings::TileResidency::Resolution;
    int totalHeight = SampleSettings::TileResidency::Resolution;
    int totalDepth = SampleSettings::TileResidency::Resolution/2; 

    // Map the single tile in the buffer to physical tile 0.
    D3D11_TILED_RESOURCE_COORDINATE startCoordinate;
    ZeroMemory(&startCoordinate, sizeof(startCoordinate));
    D3D11_TILE_REGION_SIZE regionSize;
    ZeroMemory(&regionSize, sizeof(regionSize));
    regionSize.NumTiles = 1;
    UINT rangeFlags = D3D11_TILE_RANGE_REUSE_SINGLE_TILE;
    m_defaultTileIndex = m_reservedTiles++;

    // Clear the tile to zeros.
    byte defaultTileData[SampleSettings::TileSizeInBytes];
    ZeroMemory(defaultTileData, sizeof(defaultTileData));
    //context->UpdateTiles(tempTexture.Get(), &startCoordinate, &regionSize, defaultTileData, 0);
    regionSize.NumTiles = 1;
    //regionSize.Height = 2;
    //regionSize.Width = 2;
    //regionSize.b = 2;
    //regionSize.NumTiles = 1;
    auto test = resource->totalTiles;
    const std::wstring foldername = L"Assets\\ConvertedTiles\\";
    int count = 0;
    int maxLOD = resource->packedMipDesc.NumStandardMips;
        // Calculate the max LOD
    //UINT maxLOD = 0;
    //UINT dimension = totalWidth /32;
    //while (dimension > 1) {
    //    dimension /= 2;
    //    maxLOD++;
    //}

    D3D11_TILE_REGION_SIZE size;
    ZeroMemory(&size, sizeof(size));
    size.NumTiles = 1;
    auto tilepoolAddr = m_tilePool.get();
    for (int i = 0; i < maxLOD; i++)
    {
        for (int x = 0; x < std::pow(2, i); x++)
        {
            for (int y = 0; y < std::pow(2, i); y++)
            {
                for (int z = 0; z < std::pow(2, i); z++)
                {

                    // Generate gradient data
                    const int tileWidth = 32; // Assume tile width
                    const int tileHeight = 32; // Assume tile height
                    const int tileDepth = 16; // Assume tile depth
                    int numTiles = std::pow(2, i);
                    float weightPerPixel = (static_cast<float>(totalWidth) / (static_cast<float>(numTiles) * static_cast<float>(tileWidth)));
                    float weightPerPixel2 = (static_cast<float>(totalWidth/2) / (static_cast<float>(numTiles) * static_cast<float>(tileDepth)));
                    float colorWeight = weightPerPixel * (256.000f / static_cast<float>(totalWidth));
                    float colorWeightz = weightPerPixel2 * (256.000f / static_cast<float>(totalWidth/2));
                    float weight = 256.00f / static_cast<float>(tileHeight);
                    float weight2 = 256.00f / static_cast<float>(tileDepth);
                    std::vector<byte> gradientData(tileWidth * tileHeight * tileDepth * 4); // 4 bytes per pixel (BGRA)

                    for (int tz = 0; tz < tileDepth; tz++) {
                        for (int ty = 0; ty < tileHeight; ty++) {
                            for (int tx = 0; tx < tileWidth; tx++) {
                                float fx = static_cast<float>(tx) / (tileWidth - 1);
                                float fy = static_cast<float>(ty) / (tileHeight - 1);
                                float fz = static_cast<float>(tz) / (tileDepth - 1);
                                byte r = static_cast<byte>((((tx + (x*tileWidth)) * colorWeight)));
                                byte g = static_cast<byte>((((ty + (y * tileWidth)) * colorWeight)));
                                byte b = static_cast<byte>((((tz + (z * tileDepth)) * colorWeightz)));
                                byte a = 0;

                                int index = ((tz * tileHeight + ty) * tileWidth + tx) * 4;
                                gradientData[index] = r/2 + g/2;
                                gradientData[index + 1] = 0;
                                gradientData[index + 2] = 0;
                                gradientData[index + 3] = a;
                            }
                        }
                    }

                    startCoordinate.X = x;
                    startCoordinate.Y = y;
                    startCoordinate.Z = z;
                    startCoordinate.Subresource = maxLOD - (i + 1);
                    DX::ThrowIfFailed(
                        context->UpdateTileMappings(
                            resource->texture,
                            1,
                            &startCoordinate,
                            &regionSize,
                            tilepoolAddr,
                            1,
                            &rangeFlags,
                            &m_TileCount,
                            nullptr,
                            0
                        )
                    );
                    context->UpdateTiles(
                        resource->texture,
                        &startCoordinate,
                        &regionSize,
                        gradientData.data(),
                        0
                    );
                    count++;
                    m_TileCount++;
                }
            }
        }
    }



    //startCoordinate.X = 0;
    //startCoordinate.Y = 2;
    //startCoordinate.Subresource = 1;
    //m_defaultTileIndex2 = 1;
    //DX::ThrowIfFailed(
    //    context->UpdateTileMappings(
    //        resource->texture,
    //        1,
    //        &startCoordinate,
    //        &regionSize,
    //        m_tilePool.Get(),
    //        1,
    //        &rangeFlags,
    //        &m_defaultTileIndex2,
    //        nullptr,
    //        0
    //    )
    //);
    //std::vector<byte> arr(0x10000);
    //for (int i = 0; i < 0x10000; i++) {
    //    arr[i] = 70;
    //    if (i % 512 == 1) {
    //        arr[i] = 170;
    //    }

    //    if (i < 512) {
    //        arr[i] = 140;
    //    }
    //}
    ////startCoordinate.X = 1;
    ////startCoordinate.Y = 1;
    //int sizea = sizeof(tempTexture2.Get());
    //startCoordinate.Y = 0;
    //regionSize.NumTiles = 1;
    //context->TiledResourceBarrier(NULL, resource->texture);    
    //std::vector<byte> arr2(0x10000);
    //for (int i = 0; i < 0x10000; i++) {
    //    arr2[i] = 24;
    //    if (i % 512 == 1) {
    //        arr2[i] = 69;
    //    }

    //    if (i < 512) {
    //        arr2[i] = 200;
    //    }
    //}
    //startCoordinate.Y = 2;
    //startCoordinate.Y = 1;
    regionSize.NumTiles = 1;
    //context->TiledResourceBarrier(NULL, resource->texture);

	return nullptr;
}

task<void> ResidencyManager::InitializeManagedResourcesAsync()
{
    auto device = m_deviceResources->GetD3DDevice();
    auto context = m_deviceResources->GetD3DDeviceContext();

    Reset();
    std::vector<task<void>> tileLoadTasks;
    return when_all(tileLoadTasks.begin(), tileLoadTasks.end());
}

void ResidencyManager::ProcessQueues()
{
}

void ResidencyManager::RenderVisualization()
{
}

void ResidencyManager::SetDebugMode(bool value)
{
}

void ResidencyManager::Reset()
{
    auto device = m_deviceResources->GetD3DDevice();
    auto context = m_deviceResources->GetD3DDeviceContext();

    // Clear tracked tiles and residency map.
    m_trackedTiles.clear();
    m_seenTileList.clear();
    m_loadingTileList.clear();
    m_mappedTileList.clear();

    for (auto resource : m_managedResources)
    {
        for (int face = 0; face < 6; face++)
        {
            resource->residency[face].clear();
            resource->residency[face].resize(resource->subresourceTilings[0].WidthInTiles * resource->subresourceTilings[0].HeightInTiles, 0xFF);
        }
    }
    // Reset tile mappings to NULL.
    for (auto resource : m_managedResources)
    {
        UINT flags = D3D11_TILE_RANGE_NULL;
        DX::ThrowIfFailed(
            context->UpdateTileMappings(
                resource->texture,
                1,
                nullptr, // Use default coordinate of all zeros.
                nullptr, // Use default region of entire resource.
                nullptr,
                1,
                &flags,
                nullptr,
                nullptr,
                0
            )
        );
    }
}

void ResidencyManager::SetBorderMode(bool value)
{
}
