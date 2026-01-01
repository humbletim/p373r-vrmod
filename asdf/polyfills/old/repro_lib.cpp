#include <boost/signals2.hpp>
#include <iostream>

// This mimics the Snapshot's behavior (compiled expecting int*)
// We will compile this into fsdata.obj
void trigger_signal(boost::signals2::signal<void(int*)>& sig) {
    std::cout << "Lib: Triggering signal..." << std::endl;
    int value = 42;
    sig(&value);
    std::cout << "Lib: Signal triggered." << std::endl;
}
