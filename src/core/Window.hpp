#pragma once

#include <GLFW/glfw3.h>
#include <string>
#include <functional>

namespace astrocore {

struct WindowConfig {
    std::string title      = "AstroCore";
    int         width      = 1920;
    int         height     = 1080;
    bool        vsync      = true;
    bool        fullscreen = false;
    int         samples    = 4;
};

class Window {
public:
    using ResizeCallback      = std::function<void(int, int)>;
    using KeyCallback         = std::function<void(int, int, int, int)>;
    using MouseButtonCallback = std::function<void(int, int, int)>;
    using CursorPosCallback   = std::function<void(double, double)>;
    using ScrollCallback      = std::function<void(double, double)>;

    explicit Window(const WindowConfig& config = {});
    ~Window();

    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    void pollEvents();
    void swapBuffers();
    bool shouldClose() const;
    void close();

    GLFWwindow* getHandle()      const { return m_window; }
    int         getWidth()       const { return m_width; }
    int         getHeight()      const { return m_height; }
    float       getAspectRatio() const { return static_cast<float>(m_width) / static_cast<float>(m_height); }
    double      getTime()        const;

    bool isKeyPressed(int key)          const;
    bool isMouseButtonPressed(int button) const;
    void getCursorPos(double& x, double& y) const;

    void setResizeCallback(ResizeCallback cb)           { m_resizeCallback = std::move(cb); }
    void setKeyCallback(KeyCallback cb)                 { m_keyCallback    = std::move(cb); }
    void setMouseButtonCallback(MouseButtonCallback cb) { m_mouseButtonCallback = std::move(cb); }
    void setCursorPosCallback(CursorPosCallback cb)     { m_cursorPosCallback   = std::move(cb); }
    void setScrollCallback(ScrollCallback cb)           { m_scrollCallback      = std::move(cb); }

private:
    GLFWwindow* m_window = nullptr;
    int m_width  = 0;
    int m_height = 0;

    ResizeCallback      m_resizeCallback;
    KeyCallback         m_keyCallback;
    MouseButtonCallback m_mouseButtonCallback;
    CursorPosCallback   m_cursorPosCallback;
    ScrollCallback      m_scrollCallback;

    static void framebufferSizeCallback(GLFWwindow*, int, int);
    static void keyCallback(GLFWwindow*, int, int, int, int);
    static void mouseButtonCallback(GLFWwindow*, int, int, int);
    static void cursorPosCallback(GLFWwindow*, double, double);
    static void scrollCallback(GLFWwindow*, double, double);
    static void errorCallback(int, const char*);
};

}  // namespace astrocore
