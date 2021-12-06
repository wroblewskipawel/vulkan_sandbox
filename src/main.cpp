#include <iostream>

#include "app.h"

using namespace std::string_literals;

int main() {
    try {
        vks::Application app{1024, 786, "vulkan_sandbox"s, "shaders/diffuse"s};
        app.run();
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
    }
    return EXIT_SUCCESS;
}