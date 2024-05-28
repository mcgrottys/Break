#include "pch.h"
#include "MainWindow.xaml.h"
#if __has_include("MainWindow.g.cpp")
#include "MainWindow.g.cpp"
#endif
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.System.Threading.h>
#include <Common/DirectXHelper.h>
#include <Content/OpenVDBReader.h>
#include <winrt/Windows.Security.Authorization.AppCapabilityAccess.h>
using namespace OpenVDBReader;
using namespace winrt;
using namespace Microsoft::UI::Xaml;
using namespace DX;
using namespace Visualizer;
using namespace Windows::Security::Authorization::AppCapabilityAccess;

// To learn more about WinUI, the WinUI project structure,
// and more about our project templates, see: http://aka.ms/winui-project-info.

namespace winrt::Break::implementation
{
    MainWindow::MainWindow()
	{
        // Create an AppCapability object for broadFileSystemAccess
        auto capability = AppCapability::Create(L"broadFileSystemAccess");

        // Check if the capability is granted
        auto status = capability.CheckAccess();

        if (status == AppCapabilityAccessStatus::DeniedByUser || status == AppCapabilityAccessStatus::DeniedBySystem)
        {
            // Inform the user that permission is required
            printf("Broad file system access is required. Please enable it in Settings -> Privacy -> File system.\n");
        }
        else if (status == AppCapabilityAccessStatus::Allowed)
        {
            // The capability is granted
            printf("Broad file system access is available.\n");
        }

        OpenVDBReaderClass vdbtest;
        vdbtest.LoadVDBFile();
		InitializeComponent();
        m_deviceResources = std::make_shared<DX::DeviceResources>();
        m_deviceResources->SetSwapChainPanel(swapChainPanel());
        m_main = std::unique_ptr<VisualizerMain>(new VisualizerMain(m_deviceResources));
        m_main->StartRenderLoop();
	}

    int32_t MainWindow::MyProperty()
    {
        throw hresult_not_implemented();
    }

    void MainWindow::MyProperty(int32_t /* value */)
    {
        throw hresult_not_implemented();
    }

    void MainWindow::myButton_Click(IInspectable const&, RoutedEventArgs const&)
    {
        myButton().Content(box_value(L"Clicked"));
    }
    void MainWindow::swapChainPanel_SizeChanged(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::SizeChangedEventArgs const& e)
    {
        std::lock_guard<std::mutex> lock(m_main->GetMutex());
        m_deviceResources->SetLogicalSize(e.NewSize());
        m_main->CreateWindowSizeDependentResources();
    }
}
