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
    bool reachedMax = false;

    int binIdx = 0;
    bool refined = false;
};

using BadGroupPtr = std::shared_ptr<BadGroup>;

struct NumberDisplayInfos
{
    explicit NumberDisplayInfos(bool horizontalOffset) : horizontalOffset(horizontalOffset) {}

    bool horizontalOffset;
    float centerX = -1.f, centerY = -1.f;
    float refinedX = -1.f, refinedY = -1.f;
    bool isVisible = false;
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