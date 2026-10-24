#pragma once

#include <imgui.h>

// One of the five refinement bins along the bottom of the grid, tracking how many of the
// bad groups assigned to it have been refined
struct Bin
{
    int id;
    ImVec2 pos = ImVec2(0, 0);
    ImVec2 opening = ImVec2(0, 0); // Middle of the box's top edge, where refined numbers fly to

    int badGroupsRefined = 0;  // Counted as soon as a group is clicked
    int groupsLanded = 0;      // Refined groups whose numbers have all arrived; drives the bar
    int maxBadGroups = 0;

    // Animation state
    float shownFraction = 0.f; // Bar fill, easing toward landedFraction()
    float lidOpen = 0.f;       // 0 closed to 1 open
    float lidCloseTimer = 0.f; // Time left before the lid starts closing
    float bump = 0.f;          // 1 when a number lands, decaying to 0
    int incoming = 0;          // Numbers on their way in this update

    ImVec2 updatePos(const ImVec2 &windowSize, const ImVec2 &windowPos, float offsetY)
    {
        pos = ImVec2(windowPos.x + (windowSize.x / 6.f)*id, windowPos.y + windowSize.y - offsetY);
        return pos;
    }

    double fractionRefined() const
    {
        return maxBadGroups > 0 ? static_cast<double>(badGroupsRefined) / maxBadGroups : 0.0;
    }

    float landedFraction() const
    {
        return maxBadGroups > 0 ? static_cast<float>(groupsLanded) / maxBadGroups : 0.f;
    }
};
