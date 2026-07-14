/*
 * main.cpp - Application Entry Point
 *
 * z80cpmw - Z80 CP/M Emulator for Windows
 */

#include "pch.h"
#include "MainWindow.h"
#include "EmulatorEngine.h"
#include "CrashHandler.h"

extern "C" void emu_io_set_log_path(const char* path);

int WINAPI wWinMain(
    _In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPWSTR lpCmdLine,
    _In_ int nCmdShow)
{
    UNREFERENCED_PARAMETER(hInstance);
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    // Crash reporting and the diagnostics log both live in the user data
    // directory (getUserDataDirectory creates it). Installed before anything
    // that could fault: this is a GUI app, so an unhandled exception is
    // otherwise completely invisible to the user.
    std::string userDir = EmulatorEngine::getUserDataDirectory();
    InstallCrashHandler(userDir);
    emu_io_set_log_path((userDir + "\\z80cpmw.log").c_str());

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
