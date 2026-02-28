#include "core/Window.hpp"
#include "core/Logger.hpp"
#include <stdexcept>

namespace astrocore {

Window::Window(const WindowConfig& config) {
    glfwSetErrorCallback(errorCallback);

    if (!glfwInit()) {
        throw std::runtime_error("Failed to initialize GLFW");
    }

#ifdef ASTRO_METAL
    // Metal path — tell GLFW not to create an OpenGL context.
    // The MetalRenderer will attach a CAMetalLayer to the NSView directly.
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
#else
    // OpenGL 4.5 Core Profile
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);

    if (config.samples > 0) {
        glfwWindowHint(GLFW_SAMPLES, config.samples);
    }
#endif

    GLFWmonitor* monitor = config.fullscreen ? glfwGetPrimaryMonitor() : nullptr;
    m_window = glfwCreateWindow(config.width, config.height,
                                config.title.c_str(), monitor, nullptr);
    if (!m_window) {
        glfwTerminate();
        throw std::runtime_error("Failed to create GLFW window");
    }

#ifndef ASTRO_METAL
    glfwMakeContextCurrent(m_window);

    int version = gladLoadGL(glfwGetProcAddress);
    if (version == 0) {
        glfwDestroyWindow(m_window);
        glfwTerminate();
        throw std::runtime_error("Failed to initialize GLAD");
    }

    LOG_INFO("OpenGL {}.{} loaded",
             GLAD_VERSION_MAJOR(version), GLAD_VERSION_MINOR(version));
    LOG_INFO("Renderer: {}",
             reinterpret_cast<const char*>(glGetString(GL_RENDERER)));
    LOG_INFO("Vendor: {}",
             reinterpret_cast<const char*>(glGetString(GL_VENDOR)));

    glfwSwapInterval(config.vsync ? 1 : 0);

    if (config.samples > 0) glEnable(GL_MULTISAMPLE);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
#endif

    glfwGetFramebufferSize(m_window, &m_width, &m_height);
    glfwSetWindowUserPointer(m_window, this);

    glfwSetFramebufferSizeCallback(m_window, framebufferSizeCallback);
    glfwSetKeyCallback(m_window, keyCallback);
    glfwSetMouseButtonCallback(m_window, mouseButtonCallback);
    glfwSetCursorPosCallback(m_window, cursorPosCallback);
    glfwSetScrollCallback(m_window, scrollCallback);

    LOG_INFO("Window created: {}x{}", m_width, m_height);
}

Window::~Window() {
    if (m_window) glfwDestroyWindow(m_window);
    glfwTerminate();
    LOG_INFO("Window destroyed");
}

void Window::pollEvents()  { glfwPollEvents(); }

void Window::swapBuffers() {
#ifndef ASTRO_METAL
    glfwSwapBuffers(m_window);
#endif
    // Metal: presentation is handled by MetalRenderer::endFrame via CAMetalDrawable
}

bool Window::shouldClose() const { return glfwWindowShouldClose(m_window); }
void Window::close()             { glfwSetWindowShouldClose(m_window, GLFW_TRUE); }
double Window::getTime()   const { return glfwGetTime(); }

bool Window::isKeyPressed(int key)         const { return glfwGetKey(m_window, key) == GLFW_PRESS; }
bool Window::isMouseButtonPressed(int btn) const { return glfwGetMouseButton(m_window, btn) == GLFW_PRESS; }
void Window::getCursorPos(double& x, double& y) const { glfwGetCursorPos(m_window, &x, &y); }

void Window::framebufferSizeCallback(GLFWwindow* window, int width, int height) {
    auto* self   = static_cast<Window*>(glfwGetWindowUserPointer(window));
    self->m_width  = width;
    self->m_height = height;
#ifndef ASTRO_METAL
    glViewport(0, 0, width, height);
#endif
    if (self->m_resizeCallback) self->m_resizeCallback(width, height);
}

void Window::keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    auto* self = static_cast<Window*>(glfwGetWindowUserPointer(window));
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
        glfwSetWindowShouldClose(window, GLFW_TRUE);
    if (self->m_keyCallback) self->m_keyCallback(key, scancode, action, mods);
}

void Window::mouseButtonCallback(GLFWwindow* window, int button, int action, int mods) {
    auto* self = static_cast<Window*>(glfwGetWindowUserPointer(window));
    if (self->m_mouseButtonCallback) self->m_mouseButtonCallback(button, action, mods);
}

void Window::cursorPosCallback(GLFWwindow* window, double xpos, double ypos) {
    auto* self = static_cast<Window*>(glfwGetWindowUserPointer(window));
    if (self->m_cursorPosCallback) self->m_cursorPosCallback(xpos, ypos);
}

void Window::scrollCallback(GLFWwindow* window, double xoffset, double yoffset) {
    auto* self = static_cast<Window*>(glfwGetWindowUserPointer(window));
    if (self->m_scrollCallback) self->m_scrollCallback(xoffset, yoffset);
}

void Window::errorCallback(int error, const char* description) {
    LOG_ERROR("GLFW Error {}: {}", error, description);
}

}  // namespace astrocore
