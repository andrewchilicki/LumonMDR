#pragma once

#include <functional>
#include <imgui.h>

// Digital breakup drawn over a picture: the frame splits into colour fringes, horizontal bands of it
// jump sideways, and static blocks flicker. It works by drawing the picture several times, so the
// picture must be drawn by a callback that only adds to the given draw list
namespace Glitch
{
    using DrawContent = std::function<void()>;

    // Draws `content` filling areaMin..areaMax, broken up at `intensity` (0 = clean, 1 = falling
    // apart) `time` seconds into the effect. If given, `bleedThrough` shows in some bands instead,
    // more often as intensity rises. Bands are blanked to `background` first. Different seeds give
    // different patterns
    void draw(ImDrawList* drawList, const ImVec2& areaMin, const ImVec2& areaMax, float time, float intensity, unsigned int seed,
              const DrawContent& content, const DrawContent& bleedThrough, ImU32 background);
}
