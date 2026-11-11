#include "Glitch.h"

#include "../UIManager.h"

#include <algorithm>
#include <random>

namespace
{
    // The pattern changes this often; between changes the frame holds still
    constexpr float kStepInterval = 0.05f;

    // Chance a step is glitched at all, rising to certain at full intensity
    constexpr float kMinBurstChance = 0.15f;

    // Displaced bands
    constexpr int kMinBands = 2;
    constexpr int kMaxBands = 14;
    constexpr float kMinBandHeight = 4.f;
    constexpr float kMaxBandHeight = 140.f;
    constexpr float kMinShift = 8.f;
    constexpr float kMaxShift = 180.f;
    constexpr float kMaxBleedChance = 0.6f;

    // Red and cyan copies offset either side of the frame
    constexpr float kMinSplit = 3.f;
    constexpr float kMaxSplit = 18.f;
    constexpr float kSplitAlpha = 0.45f;

    // Static: small flickering blocks and thin lines
    constexpr int kMaxNoiseBlocks = 30;
    constexpr float kMaxNoiseWidth = 160.f;
    constexpr float kMaxNoiseHeight = 10.f;

    // Chance at full intensity that the whole frame dims for a step
    constexpr float kMaxDimChance = 0.3f;
    constexpr float kDimAlpha = 0.6f;

    float lerp(float a, float b, float t) { return a + (b - a) * t; }

    // Runs `draw`, then moves everything it added `offsetX` px sideways and multiplies its colours by `tint`
    void drawShifted(ImDrawList* drawList, const Glitch::DrawContent& draw, float offsetX, const ImVec4& tint)
    {
        int start = drawList->VtxBuffer.Size;
        draw();
        bool tinted = tint.x != 1.f || tint.y != 1.f || tint.z != 1.f || tint.w != 1.f;
        for (int i = start; i < drawList->VtxBuffer.Size; i++) {
            ImDrawVert& vertex = drawList->VtxBuffer[i];
            vertex.pos.x += offsetX;
            if (tinted) {
                ImVec4 color = ImGui::ColorConvertU32ToFloat4(vertex.col);
                vertex.col = ImGui::ColorConvertFloat4ToU32(ImVec4(color.x * tint.x, color.y * tint.y, color.z * tint.z, color.w * tint.w));
            }
        }
    }
}

namespace Glitch
{
    void draw(ImDrawList* drawList, const ImVec2& areaMin, const ImVec2& areaMax, float time, float intensity, unsigned int seed,
              const DrawContent& content, const DrawContent& bleedThrough, ImU32 background)
    {
        content();
        intensity = std::clamp(intensity, 0.f, 1.f);
        if (intensity <= 0.f) {
            return;
        }

        unsigned int step = static_cast<unsigned int>(std::max(time, 0.f) / kStepInterval);
        std::mt19937 rng(seed * 7919u + step);
        std::uniform_real_distribution<float> random(0.f, 1.f);

        if (random(rng) >= lerp(kMinBurstChance, 1.f, intensity)) {
            return;
        }

        ImVec2 size(areaMax.x - areaMin.x, areaMax.y - areaMin.y);

        float split = lerp(kMinSplit, kMaxSplit, intensity) * lerp(0.5f, 1.f, random(rng));
        drawShifted(drawList, content, split, ImVec4(1.f, 0.2f, 0.2f, kSplitAlpha));
        drawShifted(drawList, content, -split, ImVec4(0.2f, 1.f, 1.f, kSplitAlpha));

        int bands = kMinBands + static_cast<int>((kMaxBands - kMinBands) * intensity * random(rng));
        float maxBandHeight = lerp(kMinBandHeight * 3.f, kMaxBandHeight, intensity);
        for (int i = 0; i < bands; i++) {
            float y = areaMin.y + random(rng) * size.y;
            float height = lerp(kMinBandHeight, maxBandHeight, random(rng));
            float shift = lerp(kMinShift, kMaxShift, intensity) * random(rng) * (random(rng) < 0.5f ? -1.f : 1.f);
            bool bleed = bleedThrough && random(rng) < kMaxBleedChance * intensity * intensity;

            ImVec2 bandMin(areaMin.x, y);
            ImVec2 bandMax(areaMax.x, std::min(y + height, areaMax.y));
            drawList->PushClipRect(bandMin, bandMax, true);
            drawList->AddRectFilled(bandMin, bandMax, background);
            drawShifted(drawList, bleed ? bleedThrough : content, shift, ImVec4(1.f, 1.f, 1.f, 1.f));
            drawList->PopClipRect();
        }

        int noiseBlocks = static_cast<int>(kMaxNoiseBlocks * intensity * random(rng));
        for (int i = 0; i < noiseBlocks; i++) {
            ImVec2 blockMin(areaMin.x + random(rng) * size.x, areaMin.y + random(rng) * size.y);
            ImVec2 blockMax(blockMin.x + lerp(4.f, kMaxNoiseWidth, random(rng)), blockMin.y + lerp(1.f, kMaxNoiseHeight, random(rng)));
            ImVec4 color = random(rng) < 0.5f ? ColorValues::lumonBlue.Value : ImVec4(0.f, 0.f, 0.f, 1.f);
            color.w = lerp(0.3f, 1.f, random(rng));
            drawList->AddRectFilled(blockMin, blockMax, ImGui::ColorConvertFloat4ToU32(color));
        }

        if (random(rng) < kMaxDimChance * intensity) {
            ImVec4 dim = ImGui::ColorConvertU32ToFloat4(background);
            dim.w = kDimAlpha;
            drawList->AddRectFilled(areaMin, areaMax, ImGui::ColorConvertFloat4ToU32(dim));
        }
    }
}
