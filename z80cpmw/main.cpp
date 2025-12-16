/*
 * main.cpp - Application Entry Point
 *
 * z80cpmw - Z80 CP/M Emulator for Windows
 */

#include "pch.h"
#include "MainWindow.h"

int WINAPI wWinMain(
    _In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPWSTR lpCmdLine,
    _In_ int nCmdShow)
{
    UNREFERENCED_PARAMETER(hInstance);
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    // Enable high DPI awareness
    SetProcessDPIAware();

    // Initialize COM (needed for some Windows features)
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(hr)) {
        return 1;
    }

    // Create and show main window
    MainWindow mainWindow;
    if (!mainWindow.create()) {
        CoUninitialize();
        return 1;
    }

    mainWindow.show(nCmdShow);

    // Run message loop
    int result = mainWindow.run();

    CoUninitialize();
    return result;
}
