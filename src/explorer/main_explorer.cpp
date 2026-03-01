#include "explorer/StarExplorerApp.hpp"
#include <exception>
#include <iostream>

int main(int /*argc*/, char* /*argv*/[]) {
    try {
        astrocore::StarExplorerApp app;
        app.run();
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
