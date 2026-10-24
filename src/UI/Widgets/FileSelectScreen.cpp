#include "FileSelectScreen.h"

#include "Files.h"
#include "../UIManager.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <imgui.h>
#include <string>
#include <vector>

namespace
{
    constexpr float kPromptPadding = 60.f;
    constexpr float kFontScale = 0.5f;

    constexpr size_t kMaxFileNameLength = 24;
    constexpr float kCursorBlinkInterval = 0.5f;
    constexpr float kFileNotFoundDuration = 1.5f;

    constexpr int kWheelVisibleRows = 5;
    constexpr float kWheelRowSpacing = 1.6f;
    constexpr float kWheelFramePadding = 40.f;
    constexpr float kWheelFrameThickness = 2.f;
    constexpr float kWheelIdleSpeed = 0.6f;
    constexpr int kWheelSpinLoops = 1;
    constexpr float kWheelSpinDuration = 2.5f;
    constexpr float kHoldAfterSelect = 1.2f;
    constexpr float kFadeOutDuration = 0.6f;
    constexpr float kWheelFadeInDuration = 0.8f;

    float fadeOutStart() { return kWheelSpinDuration + kHoldAfterSelect; }

    ImVec4 lumonBlueWithAlpha(float alpha)
    {
        ImVec4 color = ColorValues::lumonBlue.Value;
        color.w *= alpha;
        return color;
    }

    int wrapIndex(int i, int n)
    {
        return ((i % n) + n) % n;
    }

    float easeOutCubic(float t)
    {
        float inv = 1.f - t;
        return 1.f - inv * inv * inv;
    }

    std::string normalizeFileName(const std::string& name)
    {
        size_t first = name.find_first_not_of(' ');
        if (first == std::string::npos) {
            return "";
        }
        size_t last = name.find_last_not_of(' ');

        std::string normalized = name.substr(first, last - first + 1);
        std::transform(normalized.begin(), normalized.end(), normalized.begin(), [](unsigned char c) { return std::tolower(c); });
        return normalized;
    }

    // Index of `name` in kFiles, ignoring case and surrounding spaces, or -1 if absent
    int findFileIndex(const std::string& name)
    {
        std::string target = normalizeFileName(name);
        for (size_t i = 0; i < kFiles.size(); i++) {
            if (normalizeFileName(kFiles[i].name) == target) {
                return static_cast<int>(i);
            }
        }
        return -1;
    }
}

enum class FileSelectState
{
    Prompt,
    Spinning,
    Done
};

class FileSelectScreenImpl : public FileSelectScreen
{
public:
    void update() final
    {
        fadeInTimer += ImGui::GetIO().DeltaTime;

        switch (state) {
            case FileSelectState::Prompt: {
                float dt = ImGui::GetIO().DeltaTime;
                wheelPos = std::fmod(wheelPos + kWheelIdleSpeed * dt, static_cast<float>(kFiles.size()));
                fileNotFoundTimer = std::max(0.f, fileNotFoundTimer - dt);
                updatePromptInput();
                break;
            }

            case FileSelectState::Spinning: {
                // Enter or a click lands the wheel and skips the hold, fading straight out to the grid.
                // Ignores key repeat so the Enter that submitted the name can't also skip
                bool skipPressed = ImGui::IsKeyPressed(ImGuiKey_Enter, false) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter, false) || ImGui::IsMouseClicked(ImGuiMouseButton_Left);
                if (skipPressed && spinTimer < fadeOutStart()) {
                    spinTimer = fadeOutStart();
                }

                spinTimer += ImGui::GetIO().DeltaTime;
                float progress = std::clamp(spinTimer / kWheelSpinDuration, 0.f, 1.f);
                wheelPos = spinFrom + (spinTo - spinFrom) * easeOutCubic(progress);
                if (spinTimer >= fadeOutStart() + kFadeOutDuration) {
                    state = FileSelectState::Done;
                }
                break;
            }

            case FileSelectState::Done:
                break;
        }
    }

    void drawFileSelectScreen() final
    {
        ImGui::SetWindowFontScale(kFontScale);

        ImVec2 windowPos = ImGui::GetWindowPos();
        ImVec2 windowSize = ImGui::GetWindowSize();
        ImVec2 center(windowPos.x + windowSize.x * 0.5f, windowPos.y + windowSize.y * 0.5f);

        ImDrawList* drawList = ImGui::GetWindowDrawList();
        ImFont* font = ImGui::GetFont();
        float fontSize = ImGui::GetFontSize();
        float lineHeight = ImGui::GetTextLineHeightWithSpacing();

        // Everything fades out together once the wheel has landed and held
        float fadeOutAlpha = 1.f - std::clamp((spinTimer - fadeOutStart()) / kFadeOutDuration, 0.f, 1.f);
        ImU32 textColor = ImGui::ColorConvertFloat4ToU32(lumonBlueWithAlpha(fadeOutAlpha));

        // The wheel and its box also fade in when the screen first appears
        float wheelAlpha = std::clamp(fadeInTimer / kWheelFadeInDuration, 0.f, 1.f) * fadeOutAlpha;
        ImU32 wheelColor = ImGui::ColorConvertFloat4ToU32(lumonBlueWithAlpha(wheelAlpha));

        // Prompt, top-left like the boot text
        bool cursorVisible = state == FileSelectState::Prompt && std::fmod(ImGui::GetTime(), 2.0 * kCursorBlinkInterval) < kCursorBlinkInterval;
        std::string prompt = "> " + promptInput + (cursorVisible ? "_" : "");
        ImVec2 promptPos(windowPos.x + kPromptPadding, windowPos.y + kPromptPadding);
        drawList->AddText(font, fontSize, promptPos, textColor, prompt.c_str());

        if (fileNotFoundTimer > 0.f) {
            ImU32 errorColor = ImGui::ColorConvertFloat4ToU32(lumonBlueWithAlpha(fileNotFoundTimer / kFileNotFoundDuration));
            drawList->AddText(font, fontSize, ImVec2(promptPos.x, promptPos.y + lineHeight), errorColor, "FILE NOT FOUND");
        }

        // Wheel: rows fade linearly toward the top and bottom edges
        float rowHeight = ImGui::GetTextLineHeight() * kWheelRowSpacing;
        float halfVisible = kWheelVisibleRows * 0.5f;
        int n = static_cast<int>(kFiles.size());
        for (int i = static_cast<int>(std::floor(wheelPos - halfVisible)); i <= static_cast<int>(std::ceil(wheelPos + halfVisible)); i++) {
            float offset = static_cast<float>(i) - wheelPos;
            float alpha = 1.f - std::abs(offset) / halfVisible;
            if (alpha <= 0.f) {
                continue;
            }
            alpha *= wheelAlpha;

            const std::string& name = kFiles[wrapIndex(i, n)].name;
            ImVec2 textSize = ImGui::CalcTextSize(name.c_str());
            ImVec2 textPos(center.x - textSize.x * 0.5f, center.y + offset * rowHeight - textSize.y * 0.5f);
            drawList->AddText(font, fontSize, textPos, ImGui::ColorConvertFloat4ToU32(lumonBlueWithAlpha(alpha)), name.c_str());
        }

        // Selection box around the center row, sized to the widest name so it stays fixed while spinning
        float maxNameWidth = 0.f;
        for (const FileInfo& file : kFiles) {
            maxNameWidth = std::max(maxNameWidth, ImGui::CalcTextSize(file.name.c_str()).x);
        }
        float frameHalfWidth = maxNameWidth * 0.5f + kWheelFramePadding;
        float frameHalfHeight = rowHeight * 0.5f;
        drawList->AddRect(ImVec2(center.x - frameHalfWidth, center.y - frameHalfHeight), ImVec2(center.x + frameHalfWidth, center.y + frameHalfHeight), wheelColor, 0.f, ImDrawFlags_None, kWheelFrameThickness);
    }

    bool isComplete() const final
    {
        return state == FileSelectState::Done;
    }

    const std::string& getSelectedFile() const final
    {
        return selectedFile;
    }

    // Back to an empty prompt with the wheel fading in again; the wheel keeps its position
    void reset() final
    {
        state = FileSelectState::Prompt;
        promptInput.clear();
        selectedFile.clear();
        fileNotFoundTimer = 0.f;
        spinTimer = 0.f;
        fadeInTimer = 0.f;
    }

private:
    void updatePromptInput()
    {
        for (ImWchar c : ImGui::GetIO().InputQueueCharacters) {
            if (c >= 32 && c < 127 && promptInput.size() < kMaxFileNameLength) {
                promptInput.push_back(static_cast<char>(c));
            }
        }

        if (ImGui::IsKeyPressed(ImGuiKey_Backspace) && !promptInput.empty()) {
            promptInput.pop_back();
        }

        if (ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter)) {
            submitFileName();
        }
    }

    void submitFileName()
    {
        if (normalizeFileName(promptInput).empty()) {
            return;
        }

        int index = findFileIndex(promptInput);
        if (index < 0) {
            fileNotFoundTimer = kFileNotFoundDuration;
            promptInput.clear();
            return;
        }

        selectedFile = kFiles[index].name;

        // Spin a few full loops, then land on the first row at or past that point matching `index`
        float n = static_cast<float>(kFiles.size());
        float minTarget = wheelPos + kWheelSpinLoops * n;
        spinFrom = wheelPos;
        spinTo = std::ceil((minTarget - index) / n) * n + index;
        spinTimer = 0.f;
        state = FileSelectState::Spinning;
    }

    FileSelectState state = FileSelectState::Prompt;

    std::string promptInput;
    std::string selectedFile;
    float fileNotFoundTimer = 0.f;

    float wheelPos = 0.f;
    float spinFrom = 0.f;
    float spinTo = 0.f;
    float spinTimer = 0.f;
    float fadeInTimer = 0.f;
};

std::shared_ptr<FileSelectScreen> createFileSelectScreen()
{
    return std::make_shared<FileSelectScreenImpl>();
}
