#pragma once

#include "MainWindow.g.h"
#include "Common\DeviceResources.h"
#include "Common\StepTimer.h"
#include <mutex> // Include for std::mutex
#include <VisualizerMain.h>

namespace winrt::Break::implementation
{
    struct MainWindow : MainWindowT<MainWindow>
    {
        MainWindow();

        int32_t MyProperty();
        void MyProperty(int32_t value);

        void myButton_Click(IInspectable const& sender, Microsoft::UI::Xaml::RoutedEventArgs const& args);
        std::shared_ptr<DX::DeviceResources> m_deviceResources;
        std::unique_ptr<Visualizer::VisualizerMain> m_main;
        void swapChainPanel_SizeChanged(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::SizeChangedEventArgs const& e);
    };
}

namespace winrt::Break::factory_implementation
{
    struct MainWindow : MainWindowT<MainWindow, implementation::MainWindow>
    {
    };
}
