#pragma once

#include <imgui.h>
#include <vector>

// Reveals an image over whatever's already drawn: angular cracks shoot out from the middle and
// fork, and every crack widens as it goes, fastest at its base. Each cell crossfades straight
// from what's behind it to the image
class TendrilReveal
{
public:
    // Plans the spread over an area of `size` px; different seeds give different cracks
    void build(const ImVec2& size, unsigned int seed);
    bool isBuiltFor(const ImVec2& size) const;

    // Draws the reveal `time` seconds after it started, over the area starting at `areaMin`.
    // The image fills imageMin..imageMax; cells outside it settle to `background`
    void draw(ImDrawList* drawList, const ImVec2& areaMin, float time, ImTextureID image, const ImVec2& imageMin, const ImVec2& imageMax, ImU32 background) const;

    // Seconds from the start until every cell has settled
    static float totalDuration();

private:
    int cols = 0;
    int rows = 0;
    ImVec2 builtSize = ImVec2(0.f, 0.f);
    std::vector<float> arrival; // Seconds until the front reaches each cell
    unsigned int revealSeed = 0;
};
