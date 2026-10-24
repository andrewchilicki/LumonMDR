#include "FileCompleteScreen.h"

#include "../UIManager.h"

#include <algorithm>
#include <cfloat>
#include <imgui.h>
#include <iostream>

namespace
{
    // Placeholder messages until the real screens are designed
    const char* kFirstCompletionText = "Congratulations!";
    const char* kAlreadyCompleteText = "File complete";

    constexpr float kFontScale = 0.8f;
    constexpr float kFadeInDuration = 0.6f;

    // On first completion, a boxed "100%" grows in over the grid from kPopInStartScale, holds,
    // then the screen fades to the background before the message fades in
    const char* kPopupText = "100%";
    constexpr float kPopupFontSize = 140.f; // Loaded at this size so it stays crisp
    constexpr float kPopupPaddingX = 90.f;
    constexpr float kPopupPaddingY = 40.f;
    constexpr float kPopupBoxThickness = 4.f;
    constexpr float kPopInDuration = 0.35f;
    constexpr float kPopInStartScale = 0.8f;
    constexpr float kPopupDuration = 2.f;  // From appearing to the start of the fade
    constexpr float kFadeToBackgroundDuration = 0.6f;

    float messageStart() { return kPopupDuration + kFadeToBackgroundDuration; }

    float easeOutCubic(float t)
    {
        float inv = 1.f - t;
        return 1.f - inv * inv * inv;
    }

    ImU32 lumonBlueWithAlpha(float alpha)
    {
        ImVec4 color = ColorValues::lumonBlue.Value;
        color.w *= alpha;
        return ImGui::ColorConvertFloat4ToU32(color);
    }

    ImU32 backgroundWithAlpha(float alpha)
    {
        ImVec4 color = ImGui::GetStyleColorVec4(ImGuiCol_WindowBg);
        color.w = alpha;
        return ImGui::ColorConvertFloat4ToU32(color);
    }
}

class FileCompleteScreenImpl : public FileCompleteScreen
{
public:
    void init() final
    {
        popupFont = ImGui::GetIO().Fonts->AddFontFromFileTTF("./assets/Montserrat-Bold.ttf", kPopupFontSize);
        if (popupFont == nullptr) {
            std::cerr << "Failed to load the completion popup font; using the default font." << std::endl;
        }
    }

    void show(Kind kind_) final
    {
        kind = kind_;
        timer = 0.f;
    }

    void update() final
    {
        timer += ImGui::GetIO().DeltaTime;
    }

    void drawFileCompleteScreen() final
    {
        if (kind == Kind::FirstCompletion && showsGridBehind()) {
            drawPopup();
            return;
        }

        float messageTime = kind == Kind::FirstCompletion ? timer - messageStart() : timer;
        drawMessage(kind == Kind::FirstCompletion ? kFirstCompletionText : kAlreadyCompleteText, std::clamp(messageTime / kFadeInDuration, 0.f, 1.f));
    }

    bool showsGridBehind() const final
    {
        return kind == Kind::FirstCompletion && timer < messageStart();
    }

private:
    void drawPopup()
    {
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        ImVec2 windowPos = ImGui::GetWindowPos();
        ImVec2 windowSize = ImGui::GetWindowSize();
        ImVec2 center(windowPos.x + windowSize.x * 0.5f, windowPos.y + windowSize.y * 0.5f);

        float popProgress = std::clamp(timer / kPopInDuration, 0.f, 1.f);
        float scale = kPopInStartScale + (1.f - kPopInStartScale) * easeOutCubic(popProgress);
        float alpha = popProgress;

        ImFont* font = popupFont ? popupFont : ImGui::GetFont();
        float fontSize = kPopupFontSize * scale;
        ImVec2 textSize = font->CalcTextSizeA(fontSize, FLT_MAX, 0.f, kPopupText);
        ImVec2 boxHalf(textSize.x * 0.5f + kPopupPaddingX * scale, textSize.y * 0.5f + kPopupPaddingY * scale);
        ImVec2 boxMin(center.x - boxHalf.x, center.y - boxHalf.y);
        ImVec2 boxMax(center.x + boxHalf.x, center.y + boxHalf.y);

        // Box blanks out the grid behind it
        drawList->AddRectFilled(boxMin, boxMax, backgroundWithAlpha(alpha));
        drawList->AddRect(boxMin, boxMax, lumonBlueWithAlpha(alpha), 0.f, ImDrawFlags_None, kPopupBoxThickness);
        drawList->AddText(font, fontSize, ImVec2(center.x - textSize.x * 0.5f, center.y - textSize.y * 0.5f), lumonBlueWithAlpha(alpha), kPopupText);

        // Fade grid and popup out together to the plain background
        float fade = std::clamp((timer - kPopupDuration) / kFadeToBackgroundDuration, 0.f, 1.f);
        if (fade > 0.f) {
            drawList->AddRectFilled(windowPos, ImVec2(windowPos.x + windowSize.x, windowPos.y + windowSize.y), backgroundWithAlpha(fade));
        }
    }

    void drawMessage(const char* text, float alpha)
    {
        ImGui::SetWindowFontScale(kFontScale);

        ImVec2 textSize = ImGui::CalcTextSize(text);
        ImVec2 windowPos = ImGui::GetWindowPos();
        ImVec2 windowSize = ImGui::GetWindowSize();
        ImVec2 textPos(windowPos.x + (windowSize.x - textSize.x) * 0.5f, windowPos.y + (windowSize.y - textSize.y) * 0.5f);
        ImGui::GetWindowDrawList()->AddText(ImGui::GetFont(), ImGui::GetFontSize(), textPos, lumonBlueWithAlpha(alpha), text);
    }

    Kind kind = Kind::AlreadyComplete;
    float timer = 0.f;
    ImFont* popupFont = nullptr;
};

std::shared_ptr<FileCompleteScreen> createFileCompleteScreen()
{
    return std::make_shared<FileCompleteScreenImpl>();
}
