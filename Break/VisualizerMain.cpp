#include "pch.h"
#include "VisualizerMain.h"
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.System.Threading.h>
using namespace Sampling;

using namespace winrt;
using namespace Visualizer;
VisualizerMain::VisualizerMain(const std::shared_ptr<DX::DeviceResources>& deviceResources) :
	m_deviceResources(deviceResources), m_pointerLocationX(0.0f)
{
	m_deviceResources->RegisterDeviceNotify(this);
	m_sceneRenderer = std::unique_ptr<Sample3DSceneRenderer>(new Sample3DSceneRenderer(m_deviceResources));
    m_residencyManager = std::unique_ptr<ResidencyManager>(new ResidencyManager(m_deviceResources));

    CreateDeviceDependentResourcesAsync().then([this]()
        {
        });
}

VisualizerMain::~VisualizerMain()
{
    // Deregister device notification
    m_deviceResources->RegisterDeviceNotify(nullptr);
}

void VisualizerMain::CreateWindowSizeDependentResources()
{
    // TODO: Replace this with the size-dependent initialization of your app's content.
    m_sceneRenderer->CreateWindowSizeDependentResources();
}

void VisualizerMain::StartRenderLoop()
{
    // If the animation render loop is already running then do not start another thread.
    if (m_renderLoopWorker && m_renderLoopWorker.Status() == winrt::Windows::Foundation::AsyncStatus::Started)
    {
        return;
    }

    // Create a task that will be run on a background thread.
    auto workItemHandler = [this](winrt::Windows::Foundation::IAsyncAction const& action)
        {
            // Calculate the updated frame and render once per vertical blanking interval.
            while (action.Status() == winrt::Windows::Foundation::AsyncStatus::Started)
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                Update();
                if (Render())
                {
                    m_deviceResources->Present();
                }
            }
        };

    // Run task on a dedicated high priority background thread.
    m_renderLoopWorker = winrt::Windows::System::Threading::ThreadPool::RunAsync(
        winrt::Windows::System::Threading::WorkItemHandler(workItemHandler),
        winrt::Windows::System::Threading::WorkItemPriority::High,
        winrt::Windows::System::Threading::WorkItemOptions::TimeSliced
    );
}

void VisualizerMain::StopRenderLoop()
{
    m_renderLoopWorker.Cancel();
}

void VisualizerMain::OnDeviceLost()
{
    m_sceneRenderer->ReleaseDeviceDependentResources();
}

void VisualizerMain::OnDeviceRestored()
{
    m_sceneRenderer->CreateDeviceDependentResources();
    CreateWindowSizeDependentResources();
}

concurrency::task<void> Visualizer::VisualizerMain::CreateDeviceDependentResourcesAsync()
{
    auto residencyManagerCreationTask = m_residencyManager->CreateDeviceDependentResourcesAsync();
    auto diffuseResidencyMap = m_residencyManager->ManageTexture(m_sceneRenderer->GetDiffuseTexture());
    m_sceneRenderer->SetDiffuseResidencyMap(diffuseResidencyMap);
    return (residencyManagerCreationTask);
}

void VisualizerMain::ProcessInput()
{
}

void VisualizerMain::Update()
{
    ProcessInput();

    // Update scene objects.
    m_timer.Tick([&]()
        {
            // TODO: Replace this with your app's content update functions.
            m_sceneRenderer->Update(m_timer);
        });
}

bool VisualizerMain::Render()
{
    if (m_timer.GetFrameCount() == 0)
    {
        return false;
    }
    auto context = m_deviceResources->GetD3DDeviceContext();

    // Reset the viewport to target the whole screen.
    auto viewport = m_deviceResources->GetScreenViewport();
    context->RSSetViewports(1, &viewport);

    // Reset render targets to the screen.
    ID3D11RenderTargetView* const targets[1] = { m_deviceResources->GetBackBufferRenderTargetView() };
    context->OMSetRenderTargets(1, targets, m_deviceResources->GetDepthStencilView());

    // Clear the back buffer and depth stencil view.
    context->ClearRenderTargetView(m_deviceResources->GetBackBufferRenderTargetView(), DirectX::Colors::CornflowerBlue);
    context->ClearDepthStencilView(m_deviceResources->GetDepthStencilView(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

    // Render the scene objects.
    // TODO: Replace this with your app's content rendering functions.
    m_sceneRenderer->Render();
}
