#include "NumberGrid.h"

#include "PerlinNoise.hpp"

#include <cstdlib>
#include <map>
#include <random>
#include <set>
#include <optional>

class NumberGridImpl : public NumberGrid {
public:
    explicit NumberGridImpl(int gridSize)
    {
        generateGrid(gridSize);
    }

    std::map<int, std::map<int, NumberPtr>> getGrid() final
    {
        return grid;
    }

    void update() final
    {
        visibleBadGroups.clear();
        bool activeGroupStillVisible = false;
        bool newActiveBadGroup = false;

        for (auto it = badGroups.begin(); it != badGroups.end(); ) {
            if (it->second->numberIds.empty()) {
                it = badGroups.erase(it);
            } else {
                ++it;
            }
        }

        // Update visible groups and check if active group is still visible
        for (const auto &[groupId, badGroup] : badGroups) {
            for (const auto &numId : badGroup->numberIds) {
                auto num = numberIdMap.at(numId);
                if (num->displayInfos.isVisible) {
                    visibleBadGroups.emplace(groupId);

                    if (badGroup->isActive && groupId == *activeBadGroup && !badGroup->refined) {
                        activeGroupStillVisible = true;
                    }
                }
            }
        }

        if (activeBadGroup && !activeGroupStillVisible) {
            activeBadGroup.reset();
            newActiveBadGroup = true;
            newBadGroupCountdown = randomNumber(5, 15) * 100;
        }

        // Select a new active group if necessary
        if (!activeBadGroup && !visibleBadGroups.empty() && newBadGroupCountdown == 0) {
            auto randomIndex = randomNumber(0, static_cast<int>(visibleBadGroups.size()) - 1);
            auto it = visibleBadGroups.begin();
            std::advance(it, randomIndex);
            activeBadGroup = *it;
        }

        // Update active groups / their scale
        for (const auto &[groupId, badGroup] : badGroups) {
            badGroup->isActive = activeBadGroup && groupId == *activeBadGroup;
            if (badGroup->isActive) {
                if (newActiveBadGroup) {
                    badGroup->scale = 0;
                } else {
                    if (!badGroup->reachedMax) {
                        if (badGroup->scale < 0.23) {
                            badGroup->scale += (0.0001 * randomNumber(1, 10));
                        }
                    } else {
                        badGroup->scale -= (0.0001 * randomNumber(1, 10));
                    }

                    if (badGroup->scale >= 0.23) {
                        if (!badGroup->superActive || badGroup->scale >= 0.24) {
                            badGroup->reachedMax = true;
                        } else {
                            badGroup->scale += 0.00001;
                        }
                    } else if (badGroup->scale <= 0.0) {
                        badGroup->isActive = false;
                        badGroup->superActive = false;
                        badGroup->reachedMax = false;
                        activeBadGroup.reset();
                        newBadGroupCountdown = randomNumber(5, 15) * 100;
                    }
                }
            } else {
                badGroup->scale = 0;
            }
        }

        if (newBadGroupCountdown > 0) {
            newBadGroupCountdown--;
        }
    }

    NumberPtr getGridNumber(int x, int y) final
    {
        if (auto itr = grid.find(x); itr != grid.end()) {
            if (auto itr2 = itr->second.find(y); itr2 != itr->second.end()) {
                return itr2->second;
            }
        }
        return nullptr;
    }

    NumberPtr getGridNumber(int id) final
    {
        if (auto itr = numberIdMap.find(id); itr != numberIdMap.end()) {
            return itr->second;
        }
        return nullptr;
    }

    std::map<int, BadGroupPtr> getBadGroups() const final
    {
        return badGroups;
    }

private:
    std::map<int, std::map<int, NumberPtr>> grid;
    std::map<int, NumberPtr> numberIdMap;

    std::map<int, BadGroupPtr> badGroups;

    std::set<int> visibleBadGroups;
    std::optional<int> activeBadGroup = std::nullopt;
    int newBadGroupCountdown = 500;

    siv::PerlinNoise perlinBadNumbers{ 505 };
    float badScale = 0.4f;
    float badThresh = 0.7f;
    bool newBad = false;

    void generateGrid(int size)
    {
        int numberId = 0;
        std::set<int> badNumbers;
        for (int x = 0; x < size; x++) {
            for (int y = 0; y < size; y++) {
                grid[x][y] = std::make_shared<Number>(numberId, x, y, randomNumber(0,9), randomBool());
                numberIdMap[numberId] = grid[x][y];

                // Determine if bad
                if (perlinBadNumbers.noise2D_01(x*badScale,y*badScale) > badThresh) {
                    badNumbers.insert(numberId);
                }

                numberId++;
            }
        }

        // Assign 'bad groups'
        auto checkAdjacent = [&](int x, int y) -> BadGroupPtr {
            if (auto gridNum = getGridNumber(x, y)) {
                return gridNum->badGroup;
            }
            return nullptr;
        };

        int badGroup = 0;
        for (const auto &badNumId : badNumbers) {
            auto gridNumber = numberIdMap.at(badNumId);
            if (!gridNumber->badGroup) {
                for (int checkX = -1; checkX <= 1; checkX++) {
                    for (int checkY = -1; checkY <= 1; checkY++) {
                        if (checkX == 0 && checkY == 0) {
                            continue;
                        }
                        if (auto badGroupPtr = checkAdjacent(gridNumber->gridX + checkX, gridNumber->gridY + checkY)) {
                            gridNumber->badGroup = badGroupPtr;
                            gridNumber->badGroup->numberIds.emplace_back(gridNumber->id);
                            break;
                        }
                    }
                    if (gridNumber->badGroup) {
                        break;
                    }
                }

                if (!gridNumber->badGroup) {
                    gridNumber->badGroup = std::make_shared<BadGroup>(badGroup++, std::vector{gridNumber->id}, randomNumber(0,4));
                    badGroups.emplace(badGroup, gridNumber->badGroup);
                }
            }
        }
    }

    int randomNumber(int min, int max) final
    {
        static std::random_device rd;
        static std::mt19937 gen(rd());
        std::uniform_int_distribution<> dist(min, max);
        return dist(gen);
    }

    static bool randomBool()
    {
        return rand() > (RAND_MAX / 2);
    }

};

std::shared_ptr<NumberGrid> createNumberGrid(int gridSize)
{
    return std::make_shared<NumberGridImpl>(gridSize);
}