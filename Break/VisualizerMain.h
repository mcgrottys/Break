#pragma once
#include "Common\DeviceResources.h"
#include "Common\StepTimer.h"
#include "Content\Sample3DSceneRenderer.h"
#include "Content\ResidencyManager.h"
#include <mutex> // Include for std::mutex

namespace Visualizer
{
    class VisualizerMain : public DX::IDeviceNotify
    {
    public:
        VisualizerMain(const std::shared_ptr<DX::DeviceResources>& deviceResources);
        ~VisualizerMain();
        void CreateWindowSizeDependentResources();
        void StartRenderLoop();
        void StopRenderLoop();
        std::mutex& GetMutex() { return m_mutex; }
        // Rendering loop timer.
        // IDeviceNotify
        virtual void OnDeviceLost();
        virtual void OnDeviceRestored();
    private:
        concurrency::task<void> CreateDeviceDependentResourcesAsync();
        void ProcessInput();
        void Update();
        bool Render();

        // Cached pointer to device resources.
        std::shared_ptr<DX::DeviceResources> m_deviceResources;

        // TODO: Replace with your own content renderers.
        std::unique_ptr<Sampling::Sample3DSceneRenderer> m_sceneRenderer;
        std::unique_ptr<Sampling::ResidencyManager> m_residencyManager;

        winrt::Windows::Foundation::IAsyncAction m_renderLoopWorker;
        std::mutex m_mutex;
        DX::StepTimer m_timer;
        // Track current input pointer position.
        float m_pointerLocationX = 0.0f;
    };
}
