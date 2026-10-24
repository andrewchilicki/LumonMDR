#pragma once

#include <imgui.h>

// One of the five refinement bins along the bottom of the grid, tracking how many of the
// bad groups assigned to it have been refined
struct Bin
{
    int id;
    ImVec2 pos = ImVec2(0, 0);

    int badGroupsRefined = 0;
    int maxBadGroups = 0;

    ImVec2 updatePos(const ImVec2 &windowSize, const ImVec2 &windowPos, float offsetY)
    {
        pos = ImVec2(windowPos.x + (windowSize.x / 6.f)*id, windowPos.y + windowSize.y - offsetY);
        return pos;
    }

    double fractionRefined() const
    {
        return maxBadGroups > 0 ? static_cast<double>(badGroupsRefined) / maxBadGroups : 0.0;
    }
};
