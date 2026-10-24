#include "UIManager.h"

#include "Image/ImageDisplay.h"
#include "Widgets/BootScreen.h"
#include "Widgets/FileSelectScreen.h"
#include "Widgets/IdleScreen.h"
#include "Widgets/NumbersPanel.h"

#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"
#include <GLFW/glfw3.h>

namespace ColorValues
{
    ImColor lumonBlue = ImColor(157,227,235,255);
}

class UIManagerImpl : public UIManager
{
public:
    UIManagerImpl()
    {
        imageDisplay = createImageDisplay("./assets/");
        numbersPanel = createNumbersPanel(imageDisplay);
        idleScreen = createIdleScreen(imageDisplay);
        bootScreen = createBootScreen(imageDisplay);
        fileSelectScreen = createFileSelectScreen();
    }

    void init() final {
        // Initialize ImGui
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGui::StyleColorsDark();

        // Initialize ImGui backends
        ImGui_ImplGlfw_InitForOpenGL(glfwGetCurrentContext(), true);
        ImGui_ImplOpenGL3_Init("#version 130");

        numbersPanel->init();
    }

    void update() final
    {
        if (!bootScreen->isComplete()) {
            bootScreen->update();
            return;
        }

        if (!fileSelectScreen->isComplete()) {
            fileSelectScreen->update();
            if (fileSelectScreen->isComplete()) {
                numbersPanel->openFile(fileSelectScreen->getSelectedFile());
            }
            return;
        }

        // Return to the file picker with 'ESCAPE'; each file keeps its progress
        if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
            idleMode = false;
            fileSelectScreen->reset();
            return;
        }

        // Toggle settings mode with 'TAB'
        if (ImGui::IsKeyPressed(ImGuiKey_Tab)) {
            settingsMode = !settingsMode;
        }

        // Toggle idle mode with 'I'
        if (ImGui::IsKeyPressed(ImGuiKey_I)) {
            idleMode = !idleMode;
        }

        if (idleMode) {
            idleScreen->update();

            // Exit idle mode with 'LEFT CLICK'
            if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                idleMode = false;
                numbersPanel->triggerLoadAnimation();
            }
        } else {
            numbersPanel->update();
        }
    }

    void draw() final
    {
        ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImVec2 viewportSize = viewport->WorkSize;
        ImVec2 viewportPos = viewport->WorkPos;

        static float settingsWidthRatio = 0.4f;

        // Draw widgets
        ImGui::SetNextWindowPos(ImVec2(viewportPos.x, viewportPos.y));
        ImGui::SetNextWindowSize(ImVec2(viewportSize.x, viewportSize.y));
        if (ImGui::Begin("Main", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
        {
            if (!bootScreen->isComplete()) {
                bootScreen->drawBootScreen();
            } else if (!fileSelectScreen->isComplete()) {
                fileSelectScreen->drawFileSelectScreen();
            } else if (idleMode) {
                idleScreen->drawIdleScreen();
            } else {
                numbersPanel->drawNumbersPanel();
            }
        }
        ImGui::End();

        if (settingsMode && fileSelectScreen->isComplete()) {
            ImGui::SetNextWindowPos(viewportPos);
            ImGui::SetNextWindowSize(ImVec2(viewportSize.x * settingsWidthRatio, viewportSize.y));
            if (ImGui::Begin("Settings")) {
                numbersPanel->drawSettings();
            }
            ImGui::End();
        }
    }

    void cleanup() final {
        // Cleanup ImGui
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
    }

private:
    std::shared_ptr<ImageDisplay> imageDisplay;
    std::shared_ptr<NumbersPanel> numbersPanel;
    std::shared_ptr<IdleScreen> idleScreen;
    std::shared_ptr<BootScreen> bootScreen;
    std::shared_ptr<FileSelectScreen> fileSelectScreen;

    bool settingsMode = false;
    bool idleMode = false;
};

std::shared_ptr<UIManager> createUIManager()
{
    return std::make_shared<UIManagerImpl>();
}