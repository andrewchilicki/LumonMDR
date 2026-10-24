#pragma once

#include <memory>
#include <vector>

struct BadGroup
{
    BadGroup(int id, std::vector<int> numberIds, int binIdx) : id(id), numberIds(std::move(numberIds)), binIdx(binIdx) {}

    int id;
    std::vector<int> numberIds;

    bool isActive = false;
    bool superActive = false;
    double scale = 0;
    bool reachedMax = false; // Done holding at full size and shrinking away
    float holdTime = 0.f;

    float cursorProximity = 0.f; // 0 (cursor far away) to 1 (cursor on the group), smoothed over time
    float agitation = 0.f;       // 0 to 1: how hard the numbers shake, rising while hovered and settling after

    bool highlighted = false;    // One of the last few unrefined groups: stays active and is pointed out when off screen
    float hintAlpha = 0.f;       // Fade level of the off-screen arrow pointing at it

    int binIdx = 0;
    bool refined = false;
};

using BadGroupPtr = std::shared_ptr<BadGroup>;

struct NumberDisplayInfos
{
    explicit NumberDisplayInfos(bool horizontalOffset) : horizontalOffset(horizontalOffset) {}

    bool horizontalOffset;
    float centerX = -1.f, centerY = -1.f;
    // Flight into a bin once refined: start point (-1 while not flying), a delay so a group's numbers
    // stream in one after another, and progress from 0 to 1 over the flight's duration
    float refinedX = -1.f, refinedY = -1.f;
    float flightDelay = 0.f;
    float flightProgress = 0.f;
    float flightDuration = 1.f;
    bool isVisible = false;

    // Set each update and applied when drawing: drift/jitter offset from center, and cursor-proximity scale
    float offsetX = 0.f, offsetY = 0.f;
    float hoverScale = 1.f;

    // Fades the number out as it nears the edge of the grid area, so scrolling doesn't pop numbers in and out
    float edgeAlpha = 1.f;
};

struct Number
{
    Number(int id, int gridX, int gridY, int num, bool horizontalOffset) : id(id), gridX(gridX), gridY(gridY), num(num), displayInfos(NumberDisplayInfos(horizontalOffset)) {}

    int id;
    int gridX, gridY;
    int num;
    NumberDisplayInfos displayInfos;

    BadGroupPtr badGroup = nullptr;

    float regenerateScale = 0.f;
    float regenerateDelay = 0.f;
    float regenerateDuration = 1.f;
};

using NumberPtr = std::shared_ptr<Number>;