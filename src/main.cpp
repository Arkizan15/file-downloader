#include "ui_components.hpp"
#include <windows.h>
#include <iostream>

int main() {
    // Aktifkan UTF-8 encoding untuk console Windows
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    // Aktifkan Virtual Terminal Processing untuk ANSI sequences & rendering FTXUI
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut != INVALID_HANDLE_VALUE) {
        DWORD dwMode = 0;
        if (GetConsoleMode(hOut, &dwMode)) {
            dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
            SetConsoleMode(hOut, dwMode);
        }
    }

    try {
        DownloaderApp app;
        app.run();
    } catch (const std::exception& ex) {
        std::cerr << "Terjadi kesalahan: " << ex.what() << std::endl;
        return 1;
    }

    return 0;
}
