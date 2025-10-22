#include <windows.h> // actual sdk filename is named "Windows.h"
#pragma comment(lib, "user32.lib") // actual sdk filename is "User32.Lib"

int WinMain (HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd) {
    MessageBox(NULL, L"Hello from Clang++ " __clang_version__ " on Windows!", L"Case Sensitivity SDK Test", MB_OK);
    return 0;
}