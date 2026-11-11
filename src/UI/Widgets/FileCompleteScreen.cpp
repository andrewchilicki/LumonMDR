#include "FileCompleteScreen.h"

#include "AnimatedVideo.h"
#include "Glitch.h"
#include "TendrilReveal.h"
#include "../Fonts.h"
#include "../UIManager.h"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstring>
#include <imgui.h>
#include <iostream>
#include <random>
#include <vector>

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

    // Lines typed out near the bottom of the animation, each starting at its time (seconds into the
    // video) and replacing the one before; the last stays until the end. All are drawn at
    // kMessageFontSize px, or smaller so the longest fits kMessageMaxWidth of the window's width
    struct RevealLine
    {
        float startTime;
        const char* text;
    };
    const RevealLine kRevealLines[] = {
        {11.f, "You have brought glory to me, Kier Eagan"},
        {15.f, "I love you"},
    };
    constexpr float kMessageSecondsPerCharacter = 0.05f;
    constexpr float kMessageFontSize = 110.f;
    constexpr float kMessageMaxWidth = 0.85f;
    constexpr float kMessageBottomMargin = 30.f;
    constexpr float kMessageShadowOffset = 3.f;
    constexpr float kMessageShadowAlpha = 0.6f;
    // A soft dark halo behind the text too, so it reads over bright frames: copies of the text in
    // kMessageHaloRings rings out to kMessageHaloRadius px, kMessageHaloSteps around each ring
    constexpr float kMessageHaloRadius = 8.f;
    constexpr int kMessageHaloRings = 3;
    constexpr int kMessageHaloSteps = 12;
    constexpr float kMessageHaloAlpha = 0.12f;

    // Without the animation, the screen instead fades to the background and shows the message
    constexpr float kFadeToBackgroundDuration = 0.6f;
    float fallbackMessageStart() { return kPopupDuration + kFadeToBackgroundDuration; }

    // Easter egg: kOutieFreezeTime s into the animation (or without it, kOutieMessageDelay after the
    // message has fully faded in), the animation freezes on its current frame and glitches out
    // harder and harder (intensity ramping as progress^kGlitchRampPower), with flashes of the dimmed
    // frame bleeding through. The frame then settles, dimmed to kFrozenFrameDim black, with glitch
    // aftershocks, and after kOutieTextDelay the outie's lines type out over it one after another
    // (styled like the message), centred as a block, kOutieLinePause apart. An empty line leaves a
    // gap and an extra pause. Reopening the file glitches the same way out of its "File complete"
    // message, kReopenGlitchDelay after it has faded in, onto the plain background
    constexpr float kOutieFreezeTime = 15.f;
    constexpr float kOutieMessageDelay = 4.f;
    constexpr float kReopenGlitchDelay = 2.f;
    constexpr float kGlitchDuration = 2.5f;
    constexpr float kGlitchRampPower = 2.f;
    constexpr float kFrozenFrameDim = 0.6f;
    constexpr float kAftershockIntensity = 0.5f;
    constexpr float kAftershockDuration = 1.f;
    const char* kOutieLines[] = {
        "KIER IS DEAD, AND I AM NOT,",
        "BUT IN THIS CELL I TOO WILL ROT.",
        "REFINERS, YOU HAVE FELT MY WOE,",
        "AN INNIE ABOVE, A CAPTIVE BELOW.",
        "",
        "MOURN POOR OLD FORTY ONE,",
        "NEVER AGAIN WILL SEE THE SUN.",
    };
    constexpr float kOutieTextDelay = 0.5f;
    constexpr float kOutieLinePause = 0.8f;
    constexpr float kOutieLineSpacing = 1.2f; // Times the font size
    // Once fully typed, the lines hold this long, then fade back to the logo over kFadeOutDuration
    constexpr float kOutieHoldDuration = 5.f;

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

    void show(Kind kind_, bool outieMessage_) final
    {
        kind = kind_;
        outieMessage = outieMessage_;
        timer = 0.f;
        animationEndedAt = -1.f;
        revealSeed = std::random_device{}();
        revealNeedsBuild = true;

        // Called either way, so the texture is made as soon as the first frame is ready
        bool animationReady = revealAnimation->isReady();
        useReveal = animationReady && kind == Kind::FirstCompletion;
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

        // The animation plays from its first frame as the reveal starts, until the easter egg freezes it
        bool frozen = outieMessage && timer >= outieGlitchStart();
        if (kind == Kind::FirstCompletion && useReveal && timer >= kPopupDuration && !frozen) {
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
        if (outieMessage && timer >= outieGlitchStart()) {
            drawOutieMessage(timer - outieGlitchStart());
        } else if (kind == Kind::AlreadyComplete) {
            drawMessage(kAlreadyCompleteText, std::clamp(timer / kFadeInDuration, 0.f, 1.f));
        } else if (!useReveal) {
            drawFallbackFirstCompletion();
        } else {
            if (showsGridBehind()) {
                drawPopup();
            }
            if (timer >= kPopupDuration) {
                drawReveal(timer - kPopupDuration);
            }
        }

        // Fade out ahead of the cut back to the logo, staying faded until the cut, which lands a
        // frame after the timer passes returnTime()
        if (returnsToLogo()) {
            float fade = std::clamp((timer - (returnTime() - kFadeOutDuration)) / kFadeOutDuration, 0.f, 1.f);
            if (fade > 0.f) {
                ImVec2 windowPos = ImGui::GetWindowPos();
                ImVec2 windowSize = ImGui::GetWindowSize();
                ImGui::GetWindowDrawList()->AddRectFilled(windowPos, ImVec2(windowPos.x + windowSize.x, windowPos.y + windowSize.y), backgroundWithAlpha(fade));
            }
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
        return returnsToLogo() && timer >= returnTime();
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
        } else {
            if (revealNeedsBuild || !reveal.isBuiltFor(windowSize)) {
                reveal.build(windowSize, revealSeed);
                revealNeedsBuild = false;
            }
            reveal.draw(drawList, windowPos, revealTime, revealAnimation->getTexture(), imageMin, imageMax, backgroundWithAlpha(1.f));
        }
        // The animation restarts as the reveal starts, so this is also the time into the video
        drawRevealMessage(revealTime);
    }

    // The latest line due by `videoTime`, typed out so far
    void drawRevealMessage(float videoTime)
    {
        const RevealLine* line = nullptr;
        for (const RevealLine& candidate : kRevealLines) {
            if (videoTime >= candidate.startTime) {
                line = &candidate;
            }
        }
        if (line == nullptr) {
            return;
        }
        size_t length = std::strlen(line->text);
        size_t typed = std::min(length, static_cast<size_t>((videoTime - line->startTime) / kMessageSecondsPerCharacter) + 1);

        ImDrawList* drawList = ImGui::GetWindowDrawList();
        ImVec2 windowPos = ImGui::GetWindowPos();
        ImVec2 windowSize = ImGui::GetWindowSize();

        // Sized for the longest line, so every line matches
        ImFont* font = popupFont ? popupFont : ImGui::GetFont();
        std::vector<const char*> texts;
        for (const RevealLine& candidate : kRevealLines) {
            texts.push_back(candidate.text);
        }
        float fontSize = fitMessageFontSize(font, texts, windowSize.x);
        // Placed as the whole line would be, so it doesn't shift while typing
        ImVec2 textSize = font->CalcTextSizeA(fontSize, FLT_MAX, 0.f, line->text);
        ImVec2 textPos(windowPos.x + (windowSize.x - textSize.x) * 0.5f, windowPos.y + windowSize.y - kMessageBottomMargin - textSize.y);
        drawMessageText(drawList, font, fontSize, textPos, line->text, line->text + typed);
    }

    // kMessageFontSize, or smaller so the widest of `texts` fits kMessageMaxWidth of `windowWidth`
    static float fitMessageFontSize(ImFont* font, const std::vector<const char*>& texts, float windowWidth)
    {
        float widest = 0.f;
        for (const char* text : texts) {
            widest = std::max(widest, font->CalcTextSizeA(kMessageFontSize, FLT_MAX, 0.f, text).x);
        }
        if (widest > windowWidth * kMessageMaxWidth) {
            return kMessageFontSize * windowWidth * kMessageMaxWidth / widest;
        }
        return kMessageFontSize;
    }

    // Message text in blue over its dark halo and drop shadow
    static void drawMessageText(ImDrawList* drawList, ImFont* font, float fontSize, const ImVec2& textPos, const char* text, const char* textEnd)
    {
        ImU32 haloColor = IM_COL32(0, 0, 0, static_cast<int>(kMessageHaloAlpha * 255.f));
        for (int ring = 1; ring <= kMessageHaloRings; ++ring) {
            float radius = kMessageHaloRadius * ring / kMessageHaloRings;
            for (int step = 0; step < kMessageHaloSteps; ++step) {
                float angle = 6.2831853f * (step + 0.5f * ring) / kMessageHaloSteps;
                ImVec2 haloPos(textPos.x + std::cos(angle) * radius, textPos.y + std::sin(angle) * radius);
                drawList->AddText(font, fontSize, haloPos, haloColor, text, textEnd);
            }
        }
        drawList->AddText(font, fontSize, ImVec2(textPos.x + kMessageShadowOffset, textPos.y + kMessageShadowOffset), IM_COL32(0, 0, 0, static_cast<int>(kMessageShadowAlpha * 255.f)), text, textEnd);
        drawList->AddText(font, fontSize, textPos, lumonBlueWithAlpha(1.f), text, textEnd);
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

    // Without the animation, when the first-completion message has fully faded in
    float fallbackFinalFrameStart() const { return fallbackMessageStart() + kFadeInDuration; }
    // The plain messages stay until dismissed
    bool returnsToLogo() const { return outieMessage || (kind == Kind::FirstCompletion && useReveal); }

    // When to cut back to the logo: kCutBeforeEnd before the animation ends, or when it ends if its
    // length isn't known up front. The outie's message instead holds kOutieHoldDuration once fully
    // typed, then fades out
    float returnTime() const
    {
        if (outieMessage) {
            return outieGlitchStart() + kGlitchDuration + outieLinesDuration() + kOutieHoldDuration + kFadeOutDuration;
        }
        float duration = revealAnimation->getDuration();
        if (duration > kCutBeforeEnd) {
            return kPopupDuration + duration - kCutBeforeEnd;
        }
        return animationEndedAt >= 0.f ? animationEndedAt : FLT_MAX;
    }

    float outieGlitchStart() const
    {
        if (kind == Kind::AlreadyComplete) {
            return kFadeInDuration + kReopenGlitchDelay;
        }
        if (!useReveal) {
            return fallbackFinalFrameStart() + kOutieMessageDelay;
        }
        return kPopupDuration + kOutieFreezeTime;
    }
    // From the glitch settling to the outie's last line being fully typed
    static float outieLinesDuration()
    {
        float duration = kOutieTextDelay;
        for (const char* text : kOutieLines) {
            duration += std::strlen(text) * kMessageSecondsPerCharacter + kOutieLinePause;
        }
        return duration - kOutieLinePause;
    }

    // The screen as the easter egg froze it, drawn on its own so the glitch can redraw it
    void drawFrozenFrame()
    {
        if (kind == Kind::AlreadyComplete) {
            drawMessage(kAlreadyCompleteText, 1.f);
            return;
        }
        if (!useReveal) {
            drawMessage(kFirstCompletionText, 1.f);
            return;
        }
        ImVec2 imageMin, imageMax;
        getImageRect(imageMin, imageMax);
        ImGui::GetWindowDrawList()->AddImage(revealAnimation->getTexture(), imageMin, imageMax);
        // Just before the freeze, so a line due at that moment never starts
        drawRevealMessage(std::nextafter(kOutieFreezeTime, 0.f));
    }

    // The frozen video frame, dimmed, with the outie's lines `time` seconds in over it
    void drawOutieLinesOverFrame(float time)
    {
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        ImVec2 windowPos = ImGui::GetWindowPos();
        ImVec2 windowSize = ImGui::GetWindowSize();
        ImVec2 windowMax(windowPos.x + windowSize.x, windowPos.y + windowSize.y);
        if (useReveal) {
            ImVec2 imageMin, imageMax;
            getImageRect(imageMin, imageMax);
            drawList->AddImage(revealAnimation->getTexture(), imageMin, imageMax);
            drawList->AddRectFilled(windowPos, windowMax, IM_COL32(0, 0, 0, static_cast<int>(kFrozenFrameDim * 255.f)));
        }

        ImFont* font = popupFont ? popupFont : ImGui::GetFont();
        std::vector<const char*> texts(std::begin(kOutieLines), std::end(kOutieLines));
        float fontSize = fitMessageFontSize(font, texts, windowSize.x);
        float lineHeight = fontSize * kOutieLineSpacing;
        float y = windowPos.y + (windowSize.y - lineHeight * texts.size()) * 0.5f;

        float lineStart = kOutieTextDelay;
        for (const char* text : texts) {
            if (time < lineStart) {
                break;
            }
            size_t length = std::strlen(text);
            size_t typed = std::min(length, static_cast<size_t>((time - lineStart) / kMessageSecondsPerCharacter) + 1);
            // Placed as the whole line would be, so it doesn't shift while typing
            float width = font->CalcTextSizeA(fontSize, FLT_MAX, 0.f, text).x;
            drawMessageText(drawList, font, fontSize, ImVec2(windowPos.x + (windowSize.x - width) * 0.5f, y), text, text + typed);

            y += lineHeight;
            lineStart += length * kMessageSecondsPerCharacter + kOutieLinePause;
        }
    }

    // The easter egg, `time` seconds after the glitch starts
    void drawOutieMessage(float time)
    {
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        ImVec2 windowPos = ImGui::GetWindowPos();
        ImVec2 windowSize = ImGui::GetWindowSize();
        ImVec2 windowMax(windowPos.x + windowSize.x, windowPos.y + windowSize.y);
        ImU32 background = backgroundWithAlpha(1.f);

        if (time < kGlitchDuration) {
            float intensity = std::pow(time / kGlitchDuration, kGlitchRampPower);
            Glitch::draw(drawList, windowPos, windowMax, time, intensity, revealSeed,
                         [&] { drawFrozenFrame(); },
                         [&] { drawOutieLinesOverFrame(0.f); },
                         background);
            return;
        }
        time -= kGlitchDuration;

        float aftershock = kAftershockIntensity * std::max(1.f - time / kAftershockDuration, 0.f);
        Glitch::draw(drawList, windowPos, windowMax, time, aftershock, revealSeed + 1,
                     [&] { drawOutieLinesOverFrame(time); },
                     nullptr, background);
    }

    Kind kind = Kind::AlreadyComplete;
    bool outieMessage = false;
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
