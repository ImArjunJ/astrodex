#include "core/Application.hpp"
#include "core/Logger.hpp"
#include <exception>
#include <iostream>

int main(int argc, char* argv[]) {
    try {
        astrocore::Application app;
        app.run();
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
