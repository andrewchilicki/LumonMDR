#include "FileCompleteScreen.h"

#include "AnimatedVideo.h"
#include "TendrilReveal.h"
#include "../Fonts.h"
#include "../UIManager.h"

#include <algorithm>
#include <cfloat>
#include <imgui.h>
#include <iostream>
#include <random>

namespace
{
    // Placeholder messages until the real screens are designed. The first-completion message only
    // shows if the reveal animation failed to load or is still decoding
    const char* kFirstCompletionText = "Congratulations!";
    const char* kAlreadyCompleteText = "File complete";

    constexpr float kFontScale = 0.8f;
    constexpr float kFadeInDuration = 0.6f;

    // On first completion, a boxed "100%" grows in over the grid from kPopInStartScale and holds,
    // then the Kier animation is revealed over everything in tendrils spreading from the middle
    const char* kPopupText = "100%";
    constexpr float kPopupFontSize = 140.f; // Loaded at this size so it stays crisp
    constexpr float kPopupPaddingX = 90.f;
    constexpr float kPopupPaddingY = 40.f;
    constexpr float kPopupBoxThickness = 4.f;
    constexpr float kPopInDuration = 0.35f;
    constexpr float kPopInStartScale = 0.8f;
    constexpr float kPopupDuration = 2.f;  // From appearing to the start of the reveal

    const char* kRevealAnimationPath = "./assets/KierEaganEnding.mp4";

    // The animation plays once, cutting back to the logo kCutBeforeEnd s before it ends, after
    // fading out (with the message) to the background over kFadeOutDuration
    constexpr float kCutBeforeEnd = 0.5f;
    constexpr float kFadeOutDuration = 1.f;

    // Once the animation is fully revealed, a message fades in over it near the bottom, at
    // kMessageFontSize px or smaller to fit kMessageMaxWidth of the window's width
    const char* kRevealMessage = "I love you!";
    constexpr float kMessageFontSize = 110.f;
    constexpr float kMessageMaxWidth = 0.85f;
    constexpr float kMessageBottomMargin = 30.f;
    constexpr float kMessageFadeInDuration = 1.f;
    constexpr float kMessageShadowOffset = 3.f;
    constexpr float kMessageShadowAlpha = 0.6f;

    // Without the animation, the screen instead fades to the background and shows the message
    constexpr float kFadeToBackgroundDuration = 0.6f;
    float fallbackMessageStart() { return kPopupDuration + kFadeToBackgroundDuration; }

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
        popupFont = Fonts::loadUiFont(kPopupFontSize);

        // Decodes in the background; usually done long before anyone finishes a file
        revealAnimation = loadAnimatedVideo(kRevealAnimationPath, false);
    }

    void show(Kind kind_) final
    {
        kind = kind_;
        timer = 0.f;
        animationEndedAt = -1.f;
        revealSeed = std::random_device{}();
        revealNeedsBuild = true;

        useReveal = revealAnimation->isReady();
        if (kind == Kind::FirstCompletion && !useReveal) {
            std::cerr << "Completion animation isn't available; showing a message instead." << std::endl;
        }

        // Plan the reveal now, while the popup holds still, rather than when it starts. The main
        // window fills the viewport's work area
        if (kind == Kind::FirstCompletion && useReveal) {
            reveal.build(ImGui::GetMainViewport()->WorkSize, revealSeed);
            revealNeedsBuild = false;
        }
    }

    void update() final
    {
        float dt = ImGui::GetIO().DeltaTime;
        float previousTimer = timer;
        timer += dt;

        // The animation plays from its first frame as the reveal starts
        if (kind == Kind::FirstCompletion && useReveal && timer >= kPopupDuration) {
            if (previousTimer < kPopupDuration) {
                revealAnimation->restart();
            } else {
                revealAnimation->update(dt);
                if (animationEndedAt < 0.f && revealAnimation->hasEnded()) {
                    animationEndedAt = timer;
                }
            }
        }
    }

    void drawFileCompleteScreen() final
    {
        if (kind == Kind::AlreadyComplete) {
            drawMessage(kAlreadyCompleteText, std::clamp(timer / kFadeInDuration, 0.f, 1.f));
            return;
        }

        if (!useReveal) {
            drawFallbackFirstCompletion();
            return;
        }

        if (showsGridBehind()) {
            drawPopup();
        }
        if (timer >= kPopupDuration) {
            drawReveal(timer - kPopupDuration);
        }

        // Fade out ahead of the cut back to the logo, staying faded until the cut, which lands a
        // frame after the timer passes returnTime()
        float fade = std::clamp((timer - (returnTime() - kFadeOutDuration)) / kFadeOutDuration, 0.f, 1.f);
        if (fade > 0.f) {
            ImVec2 windowPos = ImGui::GetWindowPos();
            ImVec2 windowSize = ImGui::GetWindowSize();
            ImGui::GetWindowDrawList()->AddRectFilled(windowPos, ImVec2(windowPos.x + windowSize.x, windowPos.y + windowSize.y), backgroundWithAlpha(fade));
        }
    }

    bool showsGridBehind() const final
    {
        if (kind != Kind::FirstCompletion) {
            return false;
        }
        float coveredAt = useReveal ? kPopupDuration + TendrilReveal::totalDuration() : fallbackMessageStart();
        return timer < coveredAt;
    }

    bool isFinished() const final
    {
        return kind == Kind::FirstCompletion && useReveal && timer >= returnTime();
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
    }

    // Where the animation goes: fitted to the window and centered
    void getImageRect(ImVec2& imageMin, ImVec2& imageMax) const
    {
        ImVec2 windowPos = ImGui::GetWindowPos();
        ImVec2 windowSize = ImGui::GetWindowSize();
        float fit = std::min(windowSize.x / revealAnimation->getWidth(), windowSize.y / revealAnimation->getHeight());
        ImVec2 imageSize(revealAnimation->getWidth() * fit, revealAnimation->getHeight() * fit);
        imageMin = ImVec2(windowPos.x + (windowSize.x - imageSize.x) * 0.5f, windowPos.y + (windowSize.y - imageSize.y) * 0.5f);
        imageMax = ImVec2(imageMin.x + imageSize.x, imageMin.y + imageSize.y);
    }

    // The animation spreading in over the popup and grid
    void drawReveal(float revealTime)
    {
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        ImVec2 windowPos = ImGui::GetWindowPos();
        ImVec2 windowSize = ImGui::GetWindowSize();
        ImVec2 imageMin, imageMax;
        getImageRect(imageMin, imageMax);

        if (revealTime >= TendrilReveal::totalDuration()) {
            drawList->AddImage(revealAnimation->getTexture(), imageMin, imageMax);
            drawRevealMessage(std::clamp((revealTime - TendrilReveal::totalDuration()) / kMessageFadeInDuration, 0.f, 1.f));
            return;
        }

        if (revealNeedsBuild || !reveal.isBuiltFor(windowSize)) {
            reveal.build(windowSize, revealSeed);
            revealNeedsBuild = false;
        }
        reveal.draw(drawList, windowPos, revealTime, revealAnimation->getTexture(), imageMin, imageMax, backgroundWithAlpha(1.f));
    }

    void drawRevealMessage(float alpha)
    {
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        ImVec2 windowPos = ImGui::GetWindowPos();
        ImVec2 windowSize = ImGui::GetWindowSize();

        ImFont* font = popupFont ? popupFont : ImGui::GetFont();
        float fontSize = kMessageFontSize;
        float width = font->CalcTextSizeA(fontSize, FLT_MAX, 0.f, kRevealMessage).x;
        if (width > windowSize.x * kMessageMaxWidth) {
            fontSize *= windowSize.x * kMessageMaxWidth / width;
        }
        ImVec2 textSize = font->CalcTextSizeA(fontSize, FLT_MAX, 0.f, kRevealMessage);
        ImVec2 textPos(windowPos.x + (windowSize.x - textSize.x) * 0.5f, windowPos.y + windowSize.y - kMessageBottomMargin - textSize.y);

        drawList->AddText(font, fontSize, ImVec2(textPos.x + kMessageShadowOffset, textPos.y + kMessageShadowOffset), IM_COL32(0, 0, 0, static_cast<int>(kMessageShadowAlpha * alpha * 255.f)), kRevealMessage);
        drawList->AddText(font, fontSize, textPos, lumonBlueWithAlpha(alpha), kRevealMessage);
    }

    void drawFallbackFirstCompletion()
    {
        if (showsGridBehind()) {
            drawPopup();

            // Fade grid and popup out together to the plain background
            float fade = std::clamp((timer - kPopupDuration) / kFadeToBackgroundDuration, 0.f, 1.f);
            if (fade > 0.f) {
                ImVec2 windowPos = ImGui::GetWindowPos();
                ImVec2 windowSize = ImGui::GetWindowSize();
                ImGui::GetWindowDrawList()->AddRectFilled(windowPos, ImVec2(windowPos.x + windowSize.x, windowPos.y + windowSize.y), backgroundWithAlpha(fade));
            }
            return;
        }
        drawMessage(kFirstCompletionText, std::clamp((timer - fallbackMessageStart()) / kFadeInDuration, 0.f, 1.f));
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

    // When to cut back to the logo: kCutBeforeEnd before the animation ends, or when it ends if its
    // length isn't known up front
    float returnTime() const
    {
        float duration = revealAnimation->getDuration();
        if (duration > kCutBeforeEnd) {
            return kPopupDuration + duration - kCutBeforeEnd;
        }
        return animationEndedAt >= 0.f ? animationEndedAt : FLT_MAX;
    }

    Kind kind = Kind::AlreadyComplete;
    float timer = 0.f;
    ImFont* popupFont = nullptr;

    std::shared_ptr<AnimatedVideo> revealAnimation;
    bool useReveal = false; // Whether the animation was ready when this screen was shown
    float animationEndedAt = -1.f; // `timer` when the animation finished playing, once it has
    TendrilReveal reveal;
    unsigned int revealSeed = 0;
    bool revealNeedsBuild = true;
};

std::shared_ptr<FileCompleteScreen> createFileCompleteScreen()
{
    return std::make_shared<FileCompleteScreenImpl>();
}
