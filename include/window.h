#pragma once

#define GLFW_INCLUDE_VULKAN

#include <GLFW/glfw3.h>

#include <exception>
#include <functional>
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

namespace vks {

class Window {
   public:
    Window(uint32_t width, uint32_t height, const std::string& name)
        : m_name{name}, m_width{width}, m_height{height} {
        glfwSetErrorCallback((GLFWerrorfun)Window::glfwErrorCallback);
        if (!glfwInit()) {
            throw std::runtime_error("Failed to initialize GLFW");
        }
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
        m_window =
            glfwCreateWindow(width, height, name.c_str(), nullptr, nullptr);
        if (!m_window) {
            if (!windows_opened) glfwTerminate();
            throw std::runtime_error("Failed to create GLFW window");
        }
        glfwSetKeyCallback(m_window, (GLFWkeyfun)glfwKeyEventCallback);
        glfwSetCursorPosCallback(m_window,
                                 (GLFWcursorposfun)glfwMousePosCallback);
        glfwSetWindowCloseCallback(m_window,
                                   (GLFWwindowclosefun)glfwWindowCloseCallback);
        windows_opened++;
    }
    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;
    Window& operator=(Window&&) = delete;
    Window(Window&& other)
        : m_name{std::move(other.m_name)},
          m_width{other.m_width},
          m_height{other.m_height},
          m_window{other.m_window} {
        other.m_window = nullptr;
    }

    ~Window() {
        if (m_window) {
            key_callbacks.erase(m_window);
            mouse_callbacks.erase(m_window);
            close_callbacks.erase(m_window);
            glfwDestroyWindow(m_window);
            windows_opened--;
            if (!windows_opened) glfwTerminate();
        }
    }

    void poolEvents() { glfwPollEvents(); }

    void registerKeyCallback(
        std::function<void(int, int, int, int)>&& callback) {
        key_callbacks[m_window].emplace_back(std::move(callback));
    }

    void registerMouseCallback(std::function<void(double, double)>&& callback) {
        mouse_callbacks[m_window].emplace_back(std::move(callback));
    }

    void registerCloseCallback(std::function<void(void)>&& callback) {
        close_callbacks[m_window].emplace_back(std::move(callback));
    }

    const std::string m_name;
    const uint32_t m_width;
    const uint32_t m_height;

    float aspect() const { return static_cast<float>(m_width) / m_height; }

    static void glfwErrorCallback(int err, const char* desc) {
        std::cout << "GLFW Error: " << err << ": " << desc << std::endl;
    };
    static void glfwKeyEventCallback(GLFWwindow* window, int key, int scancode,
                                     int action, int mods) {
        for (auto& callback : key_callbacks[window]) {
            callback(key, scancode, action, mods);
        };
    }

    static void glfwMousePosCallback(GLFWwindow* window, double x_pos,
                                     double y_pos) {
        for (auto& callback : mouse_callbacks[window]) {
            callback(x_pos, y_pos);
        };
    }

    static void glfwWindowCloseCallback(GLFWwindow* window) {
        for (auto& callback : close_callbacks[window]) {
            callback();
        };
    }

   private:
    friend class Device;

    void createVkSurface(VkInstance instance, VkSurfaceKHR& surface) {
        if (glfwCreateWindowSurface(instance, m_window, nullptr, &surface) !=
            VK_SUCCESS) {
            throw std::runtime_error("Failed to create Vulkan surface");
        }
    }
    const char** requiredSurfaceExtensions(uint32_t& extension_count) {
        return glfwGetRequiredInstanceExtensions(&extension_count);
    }

    inline static uint32_t windows_opened;

    inline static std::unordered_map<
        GLFWwindow*, std::vector<std::function<void(int, int, int, int)>>>
        key_callbacks;

    inline static std::unordered_map<
        GLFWwindow*, std::vector<std::function<void(double, double)>>>
        mouse_callbacks;

    inline static std::unordered_map<GLFWwindow*,
                                     std::vector<std::function<void(void)>>>
        close_callbacks;

    GLFWwindow* m_window;
};
}  // namespace vks