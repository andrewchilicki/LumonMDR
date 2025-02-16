#include "BootSequence.h"

#include "ImageDisplay.h"
#include <imgui.h>
#include <imgui_internal.h>
#include <iostream>
#include <fstream>
#include <string>

#include "../UIManager.h"

class BootSequenceImpl : public BootSequence
{
public:
    explicit BootSequenceImpl(std::shared_ptr<ImageDisplay> imageDisplay_)
        : imageDisplay(std::move(imageDisplay_))
    {
        // Read lines from boot.txt into 'lines'
        std::ifstream file(bootTextPath);
        if (!file.is_open())
        {
            assert(false && "Failed to open boot.txt file.");
        }

        std::string line;
        while (std::getline(file, line))
        {
            lines.push_back(line);
        }
        file.close();
    }

    BootSequence::BootState getState() const override
    {
        return state;
    }

    void init() final
    {
        // Load custom font
        ImGuiIO &io = ImGui::GetIO();
        font = io.Fonts->AddFontFromFileTTF("./assets/Montserrat-Bold.ttf", 50.f);
        io.Fonts->Build();

        if (font == nullptr)
        {
            font = ImGui::GetDefaultFont();
            std::cerr << "Failed to load 'Montserrat-Bold.ttf' font.\n";
        }
    }

    void updateTextState()
    {
        if (currentLineIndex < lines.size())
        {
            while (timeAccumulator >= timePerLetter)
            {
                timeAccumulator -= timePerLetter;
                currentLineProgress++;

                if (currentLineProgress > lines[currentLineIndex].length())
                {
                    currentLineIndex++;
                    currentLineProgress = 0;

                    if (currentLineIndex >= lines.size())
                        break;
                }
            }
        }
        else
        {
            state = BootState::Logo;
        }
    }

    void updateFolderNameState()
    {
        if (currentLineProgress < folder_name.size())
        {
            while (folderNameAccumulator >= folderTimePerLetter)
            {
                folderNameAccumulator -= folderTimePerLetter;
                currentLineProgress++;
            }
        }
        else
        {
            ImGuiIO &io = ImGui::GetIO();
            folderWaitAccumulator += io.DeltaTime;

            if (folderWaitAccumulator >= 2.0f)
            {
                state = BootState::EndState;
            }
        }
    }

    void update() final
    {
        // Handle window scaling adjustments
        ImVec2 viewportSize = ImGui::GetMainViewport()->Size;
        if (lastViewportSize.x != viewportSize.x || lastViewportSize.y != viewportSize.y)
        {
            float oldScaleFactor = lastViewportSize.x / 1280.f;
            float newScaleFactor = viewportSize.x / 1280.f;
            float adjustedScale = newScaleFactor / oldScaleFactor;

            scale *= adjustedScale;
            speed *= adjustedScale;
            lastViewportSize = viewportSize;
        }

        ImGuiIO &io = ImGui::GetIO();

        switch (state)
        {
        case BootState::Text:
            timeAccumulator += io.DeltaTime;
            updateTextState();
            break;
        case BootState::Logo:
            logoTimeAccumulator += io.DeltaTime;
            if (logoTimeAccumulator >= 5.0f)
            {
                state = BootState::FolderName;
                currentLineProgress = 0;
            }
            break;
        case BootState::FolderName:
            folderNameAccumulator += io.DeltaTime;
            updateFolderNameState();
            break;
        }
    }

    void drawTextState(ImDrawList *draw_list)
    {
        for (int i = 0; i < lines.size(); i++)
        {
            if (i > currentLineIndex)
                break;

            if (i < currentLineIndex)
            {
                draw_list->AddText(font, 23.f, ImVec2(20, 50 + i * 30), ColorValues::lumonBlue, lines[i].c_str());
            }
            else
            {
                std::string currentText = lines[i].substr(0, currentLineProgress);
                draw_list->AddText(font, 23.f, ImVec2(20, 50 + i * 30), ColorValues::lumonBlue, currentText.c_str());
            }
        }
    }

    void drawLogoState(ImDrawList *draw_list)
    {
        ImVec2 windowSize = ImGui::GetWindowSize();
        ImVec2 windowPos = ImGui::GetWindowPos();

        auto [logoWidth, logoHeight] = imageDisplay->getImageSize(logoPath);

        float scaledWidth = logoWidth * scale;
        float scaledHeight = logoHeight * scale;

        ImVec2 centeredPos = ImVec2(
            (windowSize.x - scaledWidth) * 0.5f,
            (windowSize.y - scaledHeight) * 0.5f);

        ImGui::SetCursorPos(centeredPos);
        imageDisplay->drawImGuiImage(logoPath, scale, ColorValues::lumonBlue);
    }

    void drawFolderNameState(ImDrawList *draw_list)
    {
        std::string currentText = "> " + folder_name.substr(0, currentLineProgress);
        ImVec2 textPos(20, 50);
        draw_list->AddText(font, 23.f, textPos, ColorValues::lumonBlue, currentText.c_str());

        ImVec2 textSize = font->CalcTextSizeA(23.f, FLT_MAX, 0.0f, currentText.c_str(), NULL, NULL);

        ImVec2 cursorPos = ImVec2(textPos.x + textSize.x + 5, textPos.y);

        ImVec2 cursorSize = ImVec2(10, 20);

        float blinkPeriod = 1.0f;
        float time = ImGui::GetTime();
        if (fmod(time, blinkPeriod) < blinkPeriod * 0.5f)
        {
            draw_list->AddRectFilled(cursorPos,
                                     ImVec2(cursorPos.x + cursorSize.x, cursorPos.y + cursorSize.y),
                                     ColorValues::lumonBlue);
        }
    }

    void drawBootSequence() final
    {
        ImDrawList *draw_list = ImGui::GetWindowDrawList();

        switch (state)
        {
        case BootState::Text:
            drawTextState(draw_list);
            break;
        case BootState::Logo:
            drawLogoState(draw_list);
            break;
        case BootState::FolderName:
            drawFolderNameState(draw_list);
            break;
        }
    }

private:
    std::shared_ptr<ImageDisplay> imageDisplay;

    int currentLineIndex = 0;
    int currentLineProgress = 0;
    float timeAccumulator = 0.0f;
    float timePerLetter = 0.01f;

    float folderTimePerLetter = 0.4f;

    float logoTimeAccumulator = 0.0f;

    float folderNameAccumulator = 0.0f;
    float folderWaitAccumulator = 0.0f;

    float speed = 0.5f;
    float scale = 0.5f;
    ImVec2 lastViewportSize = ImVec2(1280, 720);

    std::string bootTextPath = "./assets/boot.txt";
    std::vector<std::string> lines;

    std::string folder_name = "Cold Harbor";

    std::string logoPath = "lumon-logo.png";
    ImVec2 logoSize;

    ImFont *font = nullptr;

    BootSequence::BootState state = BootSequence::BootState::Text;
};

std::shared_ptr<BootSequence> createBootSequence(const std::shared_ptr<ImageDisplay> &imageDisplay)
{
    return std::make_shared<BootSequenceImpl>(imageDisplay);
}