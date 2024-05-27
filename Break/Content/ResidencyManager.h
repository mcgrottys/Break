//// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
//// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
//// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
//// PARTICULAR PURPOSE.
////
//// Copyright (c) Microsoft Corporation. All rights reserved

#pragma once
//
#include "..\Common\DeviceResources.h"
#include <ppltasks.h>	// For create_task
//#include "FreeCamera.h"
//#include "SamplingRenderer.h"
//#include "TileLoader.h"


namespace Sampling
{
    // Data and metadata for a tiled resource layer.
    struct ManagedTiledResource
    {
        ID3D11Texture3D* texture;
        D3D11_TEXTURE3D_DESC textureDesc;
        UINT totalTiles;
        D3D11_PACKED_MIP_DESC packedMipDesc;
        D3D11_TILE_SHAPE tileShape;
        std::vector<D3D11_SUBRESOURCE_TILING> subresourceTilings;
        //std::unique_ptr<TileLoader> loader;
        std::vector<byte> residency[6];
        com_ptr<ID3D11Texture3D> residencyTexture;
        com_ptr<ID3D11ShaderResourceView> residencyTextureView;
    };

    // Unique identifier for a tile.
    struct TileKey
    {
        D3D11_TILED_RESOURCE_COORDINATE coordinate;
        ID3D11Texture3D* resource;
    };

    // Define the < relational operator for use as a key in std::map.
    static bool operator<(const Sampling::TileKey& a, const Sampling::TileKey& b)
    {
        if (a.resource < b.resource) return true;
        if (a.resource > b.resource) return false;
        if (a.coordinate.Subresource < b.coordinate.Subresource) return true;
        if (a.coordinate.Subresource > b.coordinate.Subresource) return false;
        if (a.coordinate.Z < b.coordinate.Z) return true;
        if (a.coordinate.Z > b.coordinate.Z) return false;
        if (a.coordinate.Y < b.coordinate.Y) return true;
        if (a.coordinate.Y > b.coordinate.Y) return false;
        return a.coordinate.X < b.coordinate.X;
    }

    enum class TileState
    {
        Seen,
        Loading,
        Loaded,
        Mapped
    };

    struct TrackedTile
    {
        ManagedTiledResource* managedResource;
        D3D11_TILED_RESOURCE_COORDINATE coordinate;
        short mipLevel;
        short face;
        UINT physicalTileOffset;
        unsigned int lastSeen;
        std::vector<byte> tileData;
        TileState state;
    };

    class ResidencyManager
    {
    public:
        ResidencyManager(const std::shared_ptr<DX::DeviceResources>& deviceResources);
        void CreateDeviceDependentResources();
        Concurrency::task<void> CreateDeviceDependentResourcesAsync();
        void ReleaseDeviceDependentResources();

        ID3D11ShaderResourceView* ManageTexture(ID3D11Texture3D* texture);
        concurrency::task<void> InitializeManagedResourcesAsync();
        //void EnqueueSamples(const std::vector<DecodedSample>& samples);
        void ProcessQueues();

        void RenderVisualization();

        void SetDebugMode(bool value);

        void Reset();

        void SetBorderMode(bool value);

    private:
        // Cached pointer to device resources.
        std::shared_ptr<DX::DeviceResources> m_deviceResources;

        com_ptr<ID3D11Buffer> m_viewerVertexBuffer;
        com_ptr<ID3D11Buffer> m_viewerIndexBuffer;
        com_ptr<ID3D11InputLayout> m_viewerInputLayout;
        com_ptr<ID3D11VertexShader> m_viewerVertexShader;
        com_ptr<ID3D11PixelShader> m_viewerPixelShader;
        com_ptr<ID3D11SamplerState> m_sampler;
        com_ptr<ID3D11Buffer> m_viewerVertexShaderConstantBuffer;

        unsigned int m_indexCount;

        // Set of resources managed by this class.
        std::vector<std::shared_ptr<ManagedTiledResource>> m_managedResources;

        // Tiled Resource tile pool.
        com_ptr<ID3D11Buffer> m_tilePool;

        // Map of all tracked tiles.
        std::map<TileKey, std::shared_ptr<TrackedTile>> m_trackedTiles;

        // List of seen tiles ready for loading.
        std::list<std::shared_ptr<TrackedTile>> m_seenTileList;

        // List of loading and loaded tiles.
        std::list<std::shared_ptr<TrackedTile>> m_loadingTileList;

        // List of mapped tiles.
        std::list<std::shared_ptr<TrackedTile>> m_mappedTileList;

        volatile LONG m_activeTileLoadingOperations;

        UINT m_reservedTiles;
        UINT m_defaultTileIndex;
        UINT m_TileCount;

        bool m_debugMode;

        std::vector<byte> m_tileDatacacheOld;
        std::unique_ptr<byte[]> m_tileDatacache;
    };

    static bool LoadPredicate(const std::shared_ptr<Sampling::TrackedTile>& a, const std::shared_ptr<Sampling::TrackedTile>& b)
    {
        // Prefer more recently seen tiles.
        if (a->lastSeen > b->lastSeen) return true;
        if (a->lastSeen < b->lastSeen) return false;

        // Break ties by loading less detailed tiles first.
        return a->mipLevel > b->mipLevel;
    }

    static bool MapPredicate(const std::shared_ptr<Sampling::TrackedTile>& a, const std::shared_ptr<Sampling::TrackedTile>& b)
    {
        // Only loaded tiles can be mapped, so put those first.
        if (a->state == TileState::Loaded && b->state == TileState::Loading) return true;
        if (a->state == TileState::Loading && b->state == TileState::Loaded) return false;

        // Then prefer more recently seen tiles.
        if (a->lastSeen > b->lastSeen) return true;
        if (a->lastSeen < b->lastSeen) return false;

        // Break ties by mapping less detailed tiles first.
        return a->mipLevel > b->mipLevel;
    }

    static bool EvictPredicate(const std::shared_ptr<Sampling::TrackedTile>& a, const std::shared_ptr<Sampling::TrackedTile>& b)
    {
        // Evict older tiles first.
        if (a->lastSeen < b->lastSeen) return true;
        if (a->lastSeen > b->lastSeen) return false;

        // To break ties, evict more detailed tiles first.
        return a->mipLevel < b->mipLevel;
    }
}
