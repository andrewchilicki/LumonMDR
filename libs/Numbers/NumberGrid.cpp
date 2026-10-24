#include "NumberGrid.h"

#include "PerlinNoise.hpp"

#include <algorithm>
#include <cstdlib>
#include <map>
#include <random>
#include <set>
#include <vector>

namespace
{
    // Active groups swell to kMaxGroupScale, hold there, then shrink away if not refined in time
    constexpr double kMaxGroupScale = 0.23;
    constexpr float kGroupGrowDuration = 1.f;
    constexpr float kGroupHoldDuration = 4.f;
    constexpr float kSuperActiveHoldDuration = 8.f; // Once hovered, a group sticks around longer
    constexpr float kGroupShrinkDuration = 1.f;

    constexpr size_t kMaxActiveGroups = 3;
    constexpr float kMinGapBetweenGroups = 0.5f;
    constexpr float kMaxGapBetweenGroups = 1.5f;
    constexpr float kFirstGroupDelay = 2.f;

    // Once this few unrefined groups are left, they're highlighted: each activates as soon as it's
    // on screen (beyond kMaxActiveGroups) and holds at full size until refined
    constexpr size_t kEndgameGroupCount = 2;

    void deactivate(BadGroup& group)
    {
        group.isActive = false;
        group.superActive = false;
        group.reachedMax = false;
        group.scale = 0;
        group.holdTime = 0.f;
        group.cursorProximity = 0.f;
        group.agitation = 0.f;
    }

    // Returns false once the group has shrunk back to nothing
    bool advanceActiveGroup(BadGroup& group, float dt)
    {
        if (!group.reachedMax) {
            if (group.scale < kMaxGroupScale) {
                group.scale = std::min(kMaxGroupScale, group.scale + kMaxGroupScale * dt / kGroupGrowDuration);
            } else if (!group.highlighted) {
                group.holdTime += dt;
                float holdDuration = group.superActive ? kSuperActiveHoldDuration : kGroupHoldDuration;
                group.reachedMax = group.holdTime >= holdDuration;
            }
            return true;
        }

        group.scale -= kMaxGroupScale * dt / kGroupShrinkDuration;
        return group.scale > 0.0;
    }
}

class NumberGridImpl : public NumberGrid {
public:
    NumberGridImpl(int gridSize, unsigned int seed) : perlinBadNumbers(seed)
    {
        generateGrid(gridSize);
    }

    const std::map<int, std::map<int, NumberPtr>>& getGrid() const final
    {
        return grid;
    }

    void update(float dt) final
    {
        // Drop groups whose numbers have all landed in bins
        for (auto it = badGroups.begin(); it != badGroups.end(); ) {
            if (it->second->numberIds.empty()) {
                activeBadGroups.erase(it->first);
                it = badGroups.erase(it);
            } else {
                ++it;
            }
        }

        // Highlight the last few unrefined groups
        size_t unrefinedCount = std::count_if(badGroups.begin(), badGroups.end(), [](const auto &entry) { return !entry.second->refined; });
        bool endgame = unrefinedCount <= kEndgameGroupCount;
        for (const auto &[groupId, badGroup] : badGroups) {
            badGroup->highlighted = endgame && !badGroup->refined;
        }

        // Unrefined groups with at least one number on screen
        visibleBadGroups.clear();
        for (const auto &[groupId, badGroup] : badGroups) {
            if (badGroup->refined) {
                continue;
            }
            for (int numId : badGroup->numberIds) {
                if (numberIdMap.at(numId)->displayInfos.isVisible) {
                    visibleBadGroups.insert(groupId);
                    break;
                }
            }
        }

        // Advance active groups, retiring any that were refined, scrolled away or have shrunk back down
        for (auto it = activeBadGroups.begin(); it != activeBadGroups.end(); ) {
            BadGroup& badGroup = *badGroups.at(*it);
            bool finished = badGroup.refined || visibleBadGroups.count(*it) == 0 || !advanceActiveGroup(badGroup, dt);
            if (finished) {
                deactivate(badGroup);
                it = activeBadGroups.erase(it);
            } else {
                ++it;
            }
        }

        // Activate another visible group after a short gap, up to kMaxActiveGroups at once
        nextGroupCountdown -= dt;
        if (nextGroupCountdown <= 0.f && activeBadGroups.size() < kMaxActiveGroups) {
            std::vector<int> candidates;
            for (int groupId : visibleBadGroups) {
                if (activeBadGroups.count(groupId) == 0) {
                    candidates.push_back(groupId);
                }
            }

            if (!candidates.empty()) {
                int groupId = candidates[randomNumber(0, static_cast<int>(candidates.size()) - 1)];
                badGroups.at(groupId)->isActive = true;
                activeBadGroups.insert(groupId);
                nextGroupCountdown = std::uniform_real_distribution<float>(kMinGapBetweenGroups, kMaxGapBetweenGroups)(gen);
            }
        }

        // Highlighted groups activate as soon as they're on screen, regardless of the gap or cap
        for (int groupId : visibleBadGroups) {
            if (badGroups.at(groupId)->highlighted && activeBadGroups.insert(groupId).second) {
                badGroups.at(groupId)->isActive = true;
            }
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

    const std::map<int, BadGroupPtr>& getBadGroups() const final
    {
        return badGroups;
    }

    int randomNumber(int min, int max) final
    {
        std::uniform_int_distribution<> dist(min, max);
        return dist(gen);
    }

private:
    std::mt19937 gen{std::random_device{}()};

    std::map<int, std::map<int, NumberPtr>> grid;
    std::map<int, NumberPtr> numberIdMap;

    std::map<int, BadGroupPtr> badGroups;

    std::set<int> visibleBadGroups;
    std::set<int> activeBadGroups;
    float nextGroupCountdown = kFirstGroupDelay;

    siv::PerlinNoise perlinBadNumbers;
    float badScale = 0.4f;
    float badThresh = 0.7f;

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

        // Assign 'bad groups': flood fill so each group is exactly one blob of bad numbers
        // connected orthogonally (not diagonally), each sent to a random bin
        constexpr int kNeighbourOffsets[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};

        int nextGroupId = 0;
        for (int startId : badNumbers) {
            NumberPtr start = numberIdMap.at(startId);
            if (start->badGroup) {
                continue;
            }

            int groupId = nextGroupId++;
            auto group = std::make_shared<BadGroup>(groupId, std::vector<int>{}, randomNumber(0,4));
            badGroups.emplace(groupId, group);

            start->badGroup = group;
            std::vector<NumberPtr> toVisit{start};
            while (!toVisit.empty()) {
                NumberPtr number = toVisit.back();
                toVisit.pop_back();
                group->numberIds.push_back(number->id);

                for (const auto &offset : kNeighbourOffsets) {
                    NumberPtr neighbour = getGridNumber(number->gridX + offset[0], number->gridY + offset[1]);
                    if (neighbour && !neighbour->badGroup && badNumbers.count(neighbour->id) > 0) {
                        neighbour->badGroup = group;
                        toVisit.push_back(neighbour);
                    }
                }
            }
        }

        // No single-number groups: grow each to two by claiming a random free orthogonal neighbour
        // that doesn't touch another group (which would have merged them), or drop it if none can be
        auto touchesOtherGroup = [&](const NumberPtr &number, const BadGroupPtr &group) {
            for (const auto &offset : kNeighbourOffsets) {
                NumberPtr neighbour = getGridNumber(number->gridX + offset[0], number->gridY + offset[1]);
                if (neighbour && neighbour->badGroup && neighbour->badGroup != group) {
                    return true;
                }
            }
            return false;
        };

        for (auto it = badGroups.begin(); it != badGroups.end(); ) {
            const BadGroupPtr &group = it->second;
            if (group->numberIds.size() > 1) {
                ++it;
                continue;
            }

            NumberPtr single = numberIdMap.at(group->numberIds.front());
            std::vector<NumberPtr> candidates;
            for (const auto &offset : kNeighbourOffsets) {
                NumberPtr neighbour = getGridNumber(single->gridX + offset[0], single->gridY + offset[1]);
                if (neighbour && !neighbour->badGroup && !touchesOtherGroup(neighbour, group)) {
                    candidates.push_back(neighbour);
                }
            }

            if (candidates.empty()) {
                single->badGroup.reset();
                it = badGroups.erase(it);
                continue;
            }

            NumberPtr partner = candidates[randomNumber(0, static_cast<int>(candidates.size()) - 1)];
            partner->badGroup = group;
            group->numberIds.push_back(partner->id);
            ++it;
        }
    }

    static bool randomBool()
    {
        return rand() > (RAND_MAX / 2);
    }

};

std::shared_ptr<NumberGrid> createNumberGrid(int gridSize, unsigned int seed)
{
    return std::make_shared<NumberGridImpl>(gridSize, seed);
}
