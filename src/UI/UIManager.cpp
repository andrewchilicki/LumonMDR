#include "UIManager.h"

#include "Image/ImageDisplay.h"
#include "Widgets/BootScreen.h"
#include "Widgets/FileCompleteScreen.h"
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
        fileCompleteScreen = createFileCompleteScreen();
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
        fileCompleteScreen->init(); // After the panel, whose font must be added first to stay the default
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
                // Checked straight away so an already-complete file never flashes its grid
                checkFileComplete();
            }
            return;
        }

        // Return to the file picker via the logo animation with 'ESCAPE', or once the completion
        // animation has played out; each file keeps its progress
        if (ImGui::IsKeyPressed(ImGuiKey_Escape) || (showingFileComplete && fileCompleteScreen->isFinished())) {
            idleMode = false;
            showingFileComplete = false;
            bootScreen->replayLogo();
            fileSelectScreen->reset();
            return;
        }

        checkFileComplete();
        if (showingFileComplete) {
            fileCompleteScreen->update();
            if (fileCompleteScreen->showsGridBehind()) {
                numbersPanel->update();
            }
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
            } else if (showingFileComplete) {
                if (fileCompleteScreen->showsGridBehind()) {
                    numbersPanel->drawNumbersPanel();
                }
                fileCompleteScreen->drawFileCompleteScreen();
            } else if (idleMode) {
                idleScreen->drawIdleScreen();
            } else {
                numbersPanel->drawNumbersPanel();
            }
        }
        ImGui::End();

        if (settingsMode && fileSelectScreen->isComplete() && !showingFileComplete) {
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
    // Swaps the grid for the completion screen once the open file is (or was already) complete
    void checkFileComplete()
    {
        FileCompletion completion = numbersPanel->getFileCompletion();
        if (showingFileComplete || completion == FileCompletion::InProgress) {
            return;
        }
        showingFileComplete = true;
        idleMode = false;
        fileCompleteScreen->show(completion == FileCompletion::JustCompleted ? FileCompleteScreen::Kind::FirstCompletion : FileCompleteScreen::Kind::AlreadyComplete);
    }

    std::shared_ptr<ImageDisplay> imageDisplay;
    std::shared_ptr<NumbersPanel> numbersPanel;
    std::shared_ptr<IdleScreen> idleScreen;
    std::shared_ptr<BootScreen> bootScreen;
    std::shared_ptr<FileSelectScreen> fileSelectScreen;
    std::shared_ptr<FileCompleteScreen> fileCompleteScreen;

    bool settingsMode = false;
    bool idleMode = false;
    bool showingFileComplete = false;
};

std::shared_ptr<UIManager> createUIManager()
{
    return std::make_shared<UIManagerImpl>();
}