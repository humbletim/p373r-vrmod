#include <boost/signals2.hpp>
#include <iostream>
#include <windows.h>

// This mimics the Local Mod's behavior (compiled expecting int* const&)
// We declare the external function with the signature WE think it has
void trigger_signal(boost::signals2::signal<void(int* const&)>& sig);

int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow) {
    std::cout << "App: Starting..." << std::endl;

    // Define the signal with the "Mod" signature
    boost::signals2::signal<void(int* const&)> sig;

    sig.connect([](int* const& ptr) {
        std::cout << "App: Slot called! Value: " << *ptr << std::endl;
    });

    std::cout << "App: Calling trigger_signal..." << std::endl;
    trigger_signal(sig); // Linker magic will route this to the Lib's version
    std::cout << "App: Returned from trigger_signal." << std::endl;

    return 0;
}
