#include "BootScreen.h"

#include "../UIManager.h"
#include "ImageDisplay.h"

#include <algorithm>
#include <cmath>
#include <imgui.h>
#include <string>
#include <utility>
#include <vector>

namespace
{
    const std::vector<std::string> kTerminalLines = {
        "LUMON OS",
        "parseElements: keyboard: 0 digitizer: 12 pointer: 0 0 scroll: 0 led: 0",
        "GPU initialized: Control ID 16",
        "startupTask",
        "Video has output streams: 1",
        "Reuse output buffer index:2 dev:Code Output offset:184000 size:20800",
        "Audio has output streams: 1",
        "DisableInput = 0",
        "portType = 3",
        "skipCached = 0",
        "PortType: 3, Length: 128",
        "Startup complete. . . ."
    };

    constexpr float kLineInterval = 0.12f;
    constexpr float kHoldAfterPrint = 0.4f;
    constexpr float kBlankBeforeLogo = 0.8f;
    constexpr float kLinePadding = 60.f;
    constexpr float kFontScale = 0.5f;

    constexpr float kCharInterval = 0.008f;
    constexpr float kFinalDotInterval = 0.8f;
    constexpr float kTerminalFadeOutDuration = 0.3f;

    // Periods on the last line ("Startup complete. . .") type slowly, like the system is finishing up
    float charInterval(size_t line, size_t charIndex)
    {
        bool isLastLine = line + 1 == kTerminalLines.size();
        return isLastLine && kTerminalLines[line][charIndex] == '.' ? kFinalDotInterval : kCharInterval;
    }

    constexpr float kTwoPi = 6.28318530718f;

    constexpr float kGlobeAspect = 266.f / 516.f;
    constexpr float kLatitudeFromPoleFraction = 50.f / 133.f;

    constexpr float kMeridianGapHeightFraction = 85.f / 266.f;

    const char* kWordmarkPath = "lumon-text.png";
    constexpr float kWordmarkWidthFraction = 366.f / 516.f;

    constexpr float kMeridianScales[] = {1.0f, 180.f / 258.f, 80.f / 258.f};
    constexpr int kMeridianCount = sizeof(kMeridianScales) / sizeof(kMeridianScales[0]);
    constexpr float kMeridianDuration = 0.8f;
    constexpr float kMeridianStagger = 0.8f;

    constexpr float kLatitudeGap = 0.3f;
    constexpr float kLatitudeDuration = 0.3f;

    constexpr float kTextGap = 0.2f;
    constexpr float kTextFadeDuration = 1.f;
    constexpr float kHoldAfterLogo = 1.5f;
    constexpr float kFadeOutDuration = 0.6f;

    constexpr float kLineThickness = 7.f;

    float meridianStart(int i) { return i * kMeridianStagger; }
    float meridianEnd(int i) { return meridianStart(i) + kMeridianDuration; }
    float latitudeStart() { return meridianEnd(kMeridianCount - 1) + kLatitudeGap; }
    float latitudeEnd() { return latitudeStart() + kLatitudeDuration; }
    float textStart() { return latitudeEnd() + kTextGap; }
    float textEnd() { return textStart() + kTextFadeDuration; }
    float fadeOutStart() { return textEnd() + kHoldAfterLogo; }
    float logoDuration() { return fadeOutStart() + kFadeOutDuration; }

    constexpr float kGlobeRadiusFraction = 0.28f;
    constexpr float kWordmarkGapPadding = 0.8f;

    // Fraction of `duration` elapsed since `start`, clamped to [0, 1].
    float phaseProgress(float time, float start, float duration)
    {
        return std::clamp((time - start) / duration, 0.f, 1.f);
    }

    ImVec4 lumonBlueWithAlpha(float alpha)
    {
        ImVec4 color = ColorValues::lumonBlue.Value;
        color.w *= alpha;
        return color;
    }

    // Sweeps an ellipse clockwise from its top pole, leaving out any points inside the
    // centered box `gapHalfExtent`. Angles are sampled at fixed steps regardless of
    // progress so already-drawn points don't shift between frames.
    void drawMeridian(ImDrawList* drawList, const ImVec2& center, float rx, float ry, float progress, ImU32 color, const ImVec2& gapHalfExtent)
    {
        if (progress <= 0.f) {
            return;
        }

        constexpr int kSegments = 128;
        int lastSegment = static_cast<int>(progress * kSegments);

        for (int s = 0; s <= lastSegment; s++) {
            float t = kTwoPi * static_cast<float>(s) / kSegments;
            ImVec2 offset(rx * std::sin(t), -ry * std::cos(t));

            if (std::abs(offset.x) < gapHalfExtent.x && std::abs(offset.y) < gapHalfExtent.y) {
                drawList->PathStroke(color, ImDrawFlags_None, kLineThickness);
                continue;
            }

            drawList->PathLineTo(ImVec2(center.x + offset.x, center.y + offset.y));
        }

        drawList->PathStroke(color, ImDrawFlags_None, kLineThickness);
    }

    float ellipseHalfWidthAtY(float rx, float ry, float y)
    {
        float ratio = y / ry;
        return rx * std::sqrt(std::max(0.f, 1.f - ratio * ratio));
    }
}

enum class BootState
{
    WaitingForEnter,
    Printing,
    Blank,
    Logo,
    Done
};

class BootScreenImpl : public BootScreen
{
public:
    explicit BootScreenImpl(std::shared_ptr<ImageDisplay> imageDisplay_) : imageDisplay(std::move(imageDisplay_)) {}

    void update() final
    {
        // Enter or a click starts the boot, then skips ahead one stage: terminal text straight to the
        // logo, and the logo straight to its fade-out. Ignores key repeat so holding Enter can't skip
        // several stages at once
        bool enterPressed = ImGui::IsKeyPressed(ImGuiKey_Enter, false) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter, false);
        bool skipPressed = enterPressed || ImGui::IsMouseClicked(ImGuiMouseButton_Left);
        if (skipPressed && (state == BootState::Printing || state == BootState::Blank)) {
            state = BootState::Logo;
            logoTimer = 0.f;
            return;
        }
        if (skipPressed && state == BootState::Logo && logoTimer < fadeOutStart()) {
            logoTimer = fadeOutStart();
            return;
        }

        switch (state) {
            case BootState::WaitingForEnter:
                if (skipPressed) {
                    state = BootState::Printing;
                }
                break;

            case BootState::Printing: {
                timer += ImGui::GetIO().DeltaTime;
                advanceTyping();

                if (currentLine >= kTerminalLines.size()) {
                    holdTimer += ImGui::GetIO().DeltaTime;
                    if (holdTimer >= kHoldAfterPrint + kTerminalFadeOutDuration) {
                        state = BootState::Blank;
                        blankTimer = 0.f;
                    }
                }
                break;
            }

            case BootState::Blank:
                blankTimer += ImGui::GetIO().DeltaTime;
                if (blankTimer >= kBlankBeforeLogo) {
                    state = BootState::Logo;
                    logoTimer = 0.f;
                }
                break;

            case BootState::Logo:
                logoTimer += ImGui::GetIO().DeltaTime;
                if (logoTimer >= logoDuration()) {
                    state = BootState::Done;
                }
                break;

            case BootState::Done:
                break;
        }
    }

    void drawBootScreen() final
    {
        if (state == BootState::WaitingForEnter) {
            return;
        }

        if (state == BootState::Printing) {
            drawTerminalLines();
        } else if (state == BootState::Logo) {
            drawLogo();
        }
    }

    bool isComplete() const final
    {
        return state == BootState::Done;
    }

    void replayLogo() final
    {
        state = BootState::Logo;
        logoTimer = 0.f;
    }

private:
    // Types each character after its charInterval, pausing kLineInterval after each finished
    // line before starting the next. Loops so several steps can land in one long frame.
    void advanceTyping()
    {
        while (currentLine < kTerminalLines.size()) {
            if (charsRevealed < kTerminalLines[currentLine].size()) {
                float interval = charInterval(currentLine, charsRevealed);
                if (timer < interval) {
                    return;
                }
                timer -= interval;
                charsRevealed++;
            } else {
                bool isLastLine = currentLine + 1 == kTerminalLines.size();
                float pause = isLastLine ? 0.f : kLineInterval;
                if (timer < pause) {
                    return;
                }
                timer -= pause;
                currentLine++;
                charsRevealed = 0;
            }
        }
    }

    void drawTerminalLines()
    {
        ImGui::SetWindowFontScale(kFontScale);

        float lineHeight = ImGui::GetTextLineHeightWithSpacing();
        float fadeOutAlpha = 1.f - phaseProgress(holdTimer, kHoldAfterPrint, kTerminalFadeOutDuration);
        ImGui::PushStyleColor(ImGuiCol_Text, lumonBlueWithAlpha(fadeOutAlpha));
        for (size_t i = 0; i < kTerminalLines.size() && i <= currentLine; i++) {
            const std::string& line = kTerminalLines[i];
            size_t visibleChars = i < currentLine ? line.size() : charsRevealed;
            ImGui::SetCursorPos(ImVec2(kLinePadding, kLinePadding + i * lineHeight));
            ImGui::TextUnformatted(line.c_str(), line.c_str() + visibleChars);
        }
        ImGui::PopStyleColor();
    }

    void drawLogo()
    {
        ImVec2 windowPos = ImGui::GetWindowPos();
        ImVec2 windowSize = ImGui::GetWindowSize();
        ImVec2 center(windowPos.x + windowSize.x * 0.5f, windowPos.y + windowSize.y * 0.5f);

        float rx = std::min(windowSize.x, windowSize.y) * kGlobeRadiusFraction;
        float ry = rx * kGlobeAspect;

        auto [textWidth, textHeight] = imageDisplay->getImageSize(kWordmarkPath);
        float wordmarkWidth = rx * 2.f * kWordmarkWidthFraction;
        float wordmarkScale = textWidth > 0 ? wordmarkWidth / static_cast<float>(textWidth) : 0.f;
        float wordmarkHeight = textHeight * wordmarkScale;

        float fadeOutAlpha = 1.f - phaseProgress(logoTimer, fadeOutStart(), kFadeOutDuration);
        ImU32 lineColor = ImGui::ColorConvertFloat4ToU32(lumonBlueWithAlpha(fadeOutAlpha));
        ImDrawList* drawList = ImGui::GetWindowDrawList();

        // Interior meridians break around the wordmark; the outer border stays whole
        ImVec2 wordmarkGap(wordmarkWidth + wordmarkHeight * kWordmarkGapPadding, ry * kMeridianGapHeightFraction);
        for (int i = 0; i < kMeridianCount; i++) {
            float progress = phaseProgress(logoTimer, meridianStart(i), kMeridianDuration);
            ImVec2 gap = i == 0 ? ImVec2(0.f, 0.f) : wordmarkGap;
            drawMeridian(drawList, center, rx * kMeridianScales[i], ry, progress, lineColor, gap);
        }

        if (logoTimer > latitudeStart()) {
            float latitudeY = ry * (1.f - kLatitudeFromPoleFraction);
            float halfLength = phaseProgress(logoTimer, latitudeStart(), kLatitudeDuration) * ellipseHalfWidthAtY(rx, ry, latitudeY);
            for (float y : {center.y - latitudeY, center.y + latitudeY}) {
                drawList->AddLine(ImVec2(center.x - halfLength, y), ImVec2(center.x + halfLength, y), lineColor, kLineThickness);
            }
        }

        if (logoTimer > textStart()) {
            float alpha = phaseProgress(logoTimer, textStart(), kTextFadeDuration) * fadeOutAlpha;
            ImGui::SetCursorPos(ImVec2((windowSize.x - wordmarkWidth) * 0.5f, (windowSize.y - wordmarkHeight) * 0.5f));
            imageDisplay->drawImGuiImage(kWordmarkPath, wordmarkScale, lumonBlueWithAlpha(alpha));
        }
    }

    std::shared_ptr<ImageDisplay> imageDisplay;

    BootState state = BootState::WaitingForEnter;

    float timer = 0.f;
    float holdTimer = 0.f;
    size_t currentLine = 0;
    size_t charsRevealed = 0;

    float blankTimer = 0.f;
    float logoTimer = 0.f;
};

std::shared_ptr<BootScreen> createBootScreen(const std::shared_ptr<ImageDisplay>& imageDisplay)
{
    return std::make_shared<BootScreenImpl>(imageDisplay);
}
