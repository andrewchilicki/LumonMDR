#include "UIManager.h"

#include "Image/ImageDisplay.h"
#include "Widgets/IdleScreen.h"
#include "Widgets/NumbersPanel.h"
#include "Widgets/BootSequence.h"

#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"
#include <GLFW/glfw3.h>

namespace ColorValues
{
    ImColor lumonBlue = ImColor(157, 227, 235, 255);
}

class UIManagerImpl : public UIManager
{
public:
    UIManagerImpl()
    {
        imageDisplay = createImageDisplay("./assets/");
        numbersPanel = createNumbersPanel(imageDisplay);
        idleScreen = createIdleScreen(imageDisplay);
        bootSequence = createBootSequence(imageDisplay);
    }

    enum class UIState
    {
        Idle,
        NumberPanel,
        BootSequence
    };

    void init() final
    {
        // Initialize ImGui
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGui::StyleColorsDark();

        // Initialize ImGui backends
        ImGui_ImplGlfw_InitForOpenGL(glfwGetCurrentContext(), true);
        ImGui_ImplOpenGL3_Init("#version 130");

        numbersPanel->init();
        bootSequence->init();
    }

    void update() final
    {
        // Toggle settings mode with 'TAB'
        if (ImGui::IsKeyPressed(ImGuiKey_Tab))
        {
            settingsMode = !settingsMode;
        }

        // Toggle idle mode with 'I'
        if (ImGui::IsKeyPressed(ImGuiKey_I))
        {

            if (current_state == UIState::Idle)
            {
                current_state = UIState::NumberPanel;
            }
            else
            {
                current_state = UIState::Idle;
            }
        }

        switch (current_state)
        {
        case UIState::Idle:
            idleScreen->update();

            // Exit idle mode with 'LEFT CLICK'
            if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
            {
                current_state = UIState::NumberPanel;
                numbersPanel->triggerLoadAnimation();
            }
            break;

        case UIState::BootSequence:
            bootSequence->update();
            if (bootSequence->getState() == BootSequence::BootState::EndState)
            {
                current_state = UIState::NumberPanel;
            }
            break;
        case UIState::NumberPanel:
            numbersPanel->update();
            break;

        default:
            break;
        }
    }

    void draw() final
    {
        ImGuiViewport *viewport = ImGui::GetMainViewport();
        ImVec2 viewportSize = viewport->WorkSize;
        ImVec2 viewportPos = viewport->WorkPos;

        static float settingsWidthRatio = 0.4f;

        // Draw widgets
        ImGui::SetNextWindowPos(ImVec2(viewportPos.x, viewportPos.y));
        ImGui::SetNextWindowSize(ImVec2(viewportSize.x, viewportSize.y));
        if (ImGui::Begin("Main", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
        {

            switch (current_state)
            {

            case UIState::Idle:
                idleScreen->drawIdleScreen();
                break;
            case UIState::NumberPanel:
                numbersPanel->drawNumbersPanel();
                break;
            case UIState::BootSequence:
                bootSequence->drawBootSequence();
                break;
            }
        }
        ImGui::End();

        if (settingsMode)
        {
            ImGui::SetNextWindowPos(viewportPos);
            ImGui::SetNextWindowSize(ImVec2(viewportSize.x * settingsWidthRatio, viewportSize.y));
            if (ImGui::Begin("Settings"))
            {
                numbersPanel->drawSettings();
            }
            ImGui::End();
        }
    }

    void cleanup() final
    {
        // Cleanup ImGui
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
    }

private:
    std::shared_ptr<ImageDisplay> imageDisplay;
    std::shared_ptr<NumbersPanel> numbersPanel;
    std::shared_ptr<BootSequence> bootSequence;
    std::shared_ptr<IdleScreen> idleScreen;
    UIState current_state = UIState::BootSequence;

    bool settingsMode = false;
};

std::shared_ptr<UIManager> createUIManager()
{
    return std::make_shared<UIManagerImpl>();
}