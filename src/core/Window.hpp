#pragma once

#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <string>
#include <functional>

namespace astrocore {

struct WindowConfig {
    std::string title = "AstroCore";
    int width = 1920;
    int height = 1080;
    bool vsync = true;
    bool fullscreen = false;
    int samples = 4;  // MSAA samples
};

class Window {
public:
    using ResizeCallback = std::function<void(int, int)>;
    using KeyCallback = std::function<void(int, int, int, int)>;
    using MouseButtonCallback = std::function<void(int, int, int)>;
    using CursorPosCallback = std::function<void(double, double)>;
    using ScrollCallback = std::function<void(double, double)>;

    explicit Window(const WindowConfig& config = {});
    ~Window();

    // Prevent copying
    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    // Window operations
    void pollEvents();
    void swapBuffers();
    bool shouldClose() const;
    void close();

    // Getters
    GLFWwindow* getHandle() const { return m_window; }
    int getWidth() const { return m_width; }
    int getHeight() const { return m_height; }
    float getAspectRatio() const { return static_cast<float>(m_width) / static_cast<float>(m_height); }
    double getTime() const;

    // Input state
    bool isKeyPressed(int key) const;
    bool isMouseButtonPressed(int button) const;
    void getCursorPos(double& x, double& y) const;

    // Callbacks
    void setResizeCallback(ResizeCallback callback) { m_resizeCallback = std::move(callback); }
    void setKeyCallback(KeyCallback callback) { m_keyCallback = std::move(callback); }
    void setMouseButtonCallback(MouseButtonCallback callback) { m_mouseButtonCallback = std::move(callback); }
    void setCursorPosCallback(CursorPosCallback callback) { m_cursorPosCallback = std::move(callback); }
    void setScrollCallback(ScrollCallback callback) { m_scrollCallback = std::move(callback); }

private:
    GLFWwindow* m_window = nullptr;
    int m_width = 0;
    int m_height = 0;

    // Callbacks
    ResizeCallback m_resizeCallback;
    KeyCallback m_keyCallback;
    MouseButtonCallback m_mouseButtonCallback;
    CursorPosCallback m_cursorPosCallback;
    ScrollCallback m_scrollCallback;

    // Static GLFW callbacks
    static void framebufferSizeCallback(GLFWwindow* window, int width, int height);
    static void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
    static void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods);
    static void cursorPosCallback(GLFWwindow* window, double xpos, double ypos);
    static void scrollCallback(GLFWwindow* window, double xoffset, double yoffset);
    static void errorCallback(int error, const char* description);
};

}  // namespace astrocore
