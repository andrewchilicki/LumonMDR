#include "UI/UIManager.h"

#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include <GLFW/glfw3.h>
#include <imgui_impl_opengl3.h>
#include <cstring>
#include <cstdlib>
#include <iostream>

void glfw_error_callback(int error, const char* description) {
    std::cerr << "GLFW Error (" << error << "): " << description << std::endl;
}

// Picks the monitor to use for --full-screen. monitorNumber is 1-indexed as
// shown to the user (--monitor=2 means the second monitor); falls back to the
// primary monitor if the index is out of range.
GLFWmonitor* pickMonitor(int monitorNumber) {
    int monitorCount = 0;
    GLFWmonitor** monitors = glfwGetMonitors(&monitorCount);

    if (monitorNumber >= 1 && monitorNumber <= monitorCount) {
        return monitors[monitorNumber - 1];
    }

    std::cerr << "Monitor " << monitorNumber << " not found (" << monitorCount
               << " monitor(s) detected); using primary monitor." << std::endl;
    return glfwGetPrimaryMonitor();
}

int main(int argc, char** argv) {
    // Set error callback
    glfwSetErrorCallback(glfw_error_callback);

    // Initialize GLFW
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW!" << std::endl;
        return -1;
    }

    bool fullscreen = false;
    int monitorNumber = 1;
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--full-screen") == 0) {
            fullscreen = true;
        } else if (strncmp(argv[i], "--monitor=", 10) == 0) {
            monitorNumber = atoi(argv[i] + 10);
        }
    }

    GLFWwindow* window;
    if (fullscreen)
    {
        GLFWmonitor* targetMonitor = pickMonitor(monitorNumber);
        const GLFWvidmode* mode = glfwGetVideoMode(targetMonitor);
        window = glfwCreateWindow(mode->width, mode->height, "MDR Severance", targetMonitor, nullptr);
    } else
    {
        window = glfwCreateWindow(1920, 1080, "MDR Severance", nullptr, nullptr);
    }

    if (!window) {
        std::cerr << "Failed to create GLFW window!" << std::endl;
        glfwTerminate();
        return -1;
    }

    // Set the OpenGL context
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // Enable vsync

    std::shared_ptr<UIManager> uiManager = createUIManager();
    uiManager->init();
    while (!glfwWindowShouldClose(window)) {
        // Poll events
        glfwPollEvents();

        // Close application with 'ESCAPE' key
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
            glfwSetWindowShouldClose(window, true);
        }

        // Clear the screen
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Start ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // Draw
        uiManager->draw();

        // Update
        uiManager->update();

        // Render ImGui
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        // Swap buffers
        glfwSwapBuffers(window);
    }

    // Cleanup
    uiManager->cleanup();
    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}