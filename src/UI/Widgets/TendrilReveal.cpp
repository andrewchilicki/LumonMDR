#include "TendrilReveal.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <deque>
#include <queue>
#include <random>
#include <utility>

namespace
{
    constexpr float kCellSize = 4.f;            // Px per side of each revealed cell
    constexpr float kRevealDuration = 4.f;      // Until the reveal reaches the furthest cell
    constexpr float kCellFadeDuration = 0.2f;   // Each cell crossfades to the image over about this long...
    constexpr float kCellFadeJitter = 0.4f;     // ...give or take this fraction, so the edge isn't uniform

    // Cracks shoot out from the middle in straight segments with sharp kinks between them; the
    // initial ones run until they leave the area. They fork into shorter cracks, more often the
    // further out they are (kOuterForkChance at the edge), so the outskirts fill in too.
    // Fork lengths are fractions of the area's larger side; segment lengths are in cells
    constexpr int kInitialCracks = 4;
    constexpr int kMinSegmentLength = 6;
    constexpr int kMaxSegmentLength = 28;
    constexpr float kMinKinkAngle = 0.25f;      // Radians
    constexpr float kMaxKinkAngle = 0.5f;
    constexpr float kInnerForkChance = 0.008f;  // Per cell travelled
    constexpr float kOuterForkChance = 0.025f;
    constexpr float kMinForkAngle = 0.5f;
    constexpr float kMaxForkAngle = 1.2f;
    constexpr float kMinForkLength = 0.15f;
    constexpr float kMaxForkLength = 0.45f;
    constexpr int kMaxCracks = 40;

    // Afterwards, any area more than kMaxCrackGap px from a crack gets an extra crack sent into it
    // (running kGapCrackOvershoot times the distance), up to kMaxGapFills of them, so no
    // unconverted chunk is left to slowly fill in at the end
    constexpr float kMaxCrackGap = 280.f;
    constexpr float kGapCrackOvershoot = 1.3f;
    constexpr int kMaxGapFills = 40;

    // Each crack widens outward once its tip has passed, fastest near the middle and slowing to
    // kOuterWidenFraction of that at the edge. Speeds are relative to how fast tips travel, so the
    // cracks shoot out well ahead of the widening. Widening spreads in straight lines and right
    // angles (not circles), keeping the edges angular; each step takes a random kMinStepJitter to
    // kMaxStepJitter times as long, making the edges uneven
    constexpr float kBaseWidenSpeed = 0.1f;
    constexpr float kOuterWidenFraction = 0.75f;
    constexpr float kMinStepJitter = 0.5f;
    constexpr float kMaxStepJitter = 1.8f;

    // How much each cell's timing comes from converting the screen at an even rate (1) versus the
    // natural spread of the cracks (0), where the slowest pockets lag at the end
    constexpr float kEvenPacing = 0.5f;

    // Stray pixels: some convert up to kMaxJumpAhead seconds early, ahead of the edge; others
    // inside the converted area hold out up to kMaxHoldOut seconds late
    constexpr float kJumpAheadChance = 0.1f;
    constexpr float kMaxJumpAhead = 1.f;
    constexpr float kHoldOutChance = 0.1f;
    constexpr float kMaxHoldOut = 1.f;

    // Cheap, well-mixed hash to a value in [0, 1)
    float hash01(uint32_t a, uint32_t b)
    {
        uint32_t h = a * 0x9E3779B1u ^ (b + 0x7F4A7C15u) * 0x85EBCA77u;
        h ^= h >> 15;
        h *= 0x2C1B3C6Du;
        h ^= h >> 12;
        h *= 0x297A2D39u;
        h ^= h >> 15;
        return (h & 0xFFFFFF) / static_cast<float>(0x1000000);
    }

    ImU32 withAlpha(ImU32 color, float alpha)
    {
        ImVec4 c = ImGui::ColorConvertU32ToFloat4(color);
        c.w *= std::clamp(alpha, 0.f, 1.f);
        return ImGui::ColorConvertFloat4ToU32(c);
    }
}

void TendrilReveal::build(const ImVec2& size, unsigned int seed)
{
    builtSize = size;
    cols = std::max(1, static_cast<int>(std::ceil(size.x / kCellSize)));
    rows = std::max(1, static_cast<int>(std::ceil(size.y / kCellSize)));
    int cellCount = cols * rows;
    revealSeed = seed;

    std::mt19937 rng(seed);
    auto uniform = [&](float min, float max) { return std::uniform_real_distribution<float>(min, max)(rng); };
    auto randomSign = [&] { return uniform(0.f, 1.f) < 0.5f ? -1.f : 1.f; };

    // Arrival times are in 'tip time': a crack tip crosses one cell per unit. Each cell also gets
    // the widening speed of whatever reaches it, which it passes on
    arrival.assign(cellCount, INFINITY);
    std::vector<float> widenSpeed(cellCount, kBaseWidenSpeed);

    // Grow the cracks, recording when each tip passes each cell and how fast that point widens
    struct Crack { float x, y, heading, length, birthTime; };
    float centerX = cols / 2.f;
    float centerY = rows / 2.f;
    float longestSide = static_cast<float>(std::max(cols, rows));
    float maxRadius = std::sqrt(centerX * centerX + centerY * centerY);
    // Grown oldest first, so every crack gets its forks before the forks' forks use up kMaxCracks
    std::deque<Crack> pending;
    for (int i = 0; i < kInitialCracks; i++) {
        float heading = (i + uniform(-0.35f, 0.35f)) * 6.2831853f / kInitialCracks;
        // Long enough to leave the area however much it kinks
        pending.push_back({centerX, centerY, heading, longestSide * 4.f, 0.f});
    }

    auto growCrack = [&](const Crack &crack) {
        float x = crack.x;
        float y = crack.y;
        float heading = crack.heading;
        int segmentLeft = static_cast<int>(uniform(kMinSegmentLength, kMaxSegmentLength));
        for (float travelled = 1.f; travelled <= crack.length; travelled += 1.f) {
            if (--segmentLeft <= 0) {
                heading += randomSign() * uniform(kMinKinkAngle, kMaxKinkAngle);
                segmentLeft = static_cast<int>(uniform(kMinSegmentLength, kMaxSegmentLength));
            }
            x += std::cos(heading);
            y += std::sin(heading);

            int cx = static_cast<int>(x);
            int cy = static_cast<int>(y);
            if (cx < 0 || cy < 0 || cx >= cols || cy >= rows) {
                break;
            }

            int cell = cy * cols + cx;
            float time = crack.birthTime + travelled;
            float outward = std::min(1.f, std::hypot(x - centerX, y - centerY) / maxRadius);
            if (time < arrival[cell]) {
                arrival[cell] = time;
                widenSpeed[cell] = kBaseWidenSpeed * (1.f - (1.f - kOuterWidenFraction) * outward);
            }

            float remaining = crack.length - travelled;
            float forkChance = kInnerForkChance + (kOuterForkChance - kInnerForkChance) * outward;
            if (uniform(0.f, 1.f) < forkChance && remaining > kMinSegmentLength) {
                float forkLength = std::min(remaining, longestSide * uniform(kMinForkLength, kMaxForkLength));
                pending.push_back({x, y, heading + randomSign() * uniform(kMinForkAngle, kMaxForkAngle), forkLength, time});
            }
        }
    };

    int cracksGrown = 0;
    auto growPending = [&] {
        while (!pending.empty() && cracksGrown < kMaxCracks) {
            growCrack(pending.front());
            pending.pop_front();
            cracksGrown++;
        }
        pending.clear();
    };
    growPending();

    // Random forking can still miss areas, which would be left as a slow-filling chunk at the end.
    // So repeatedly send a crack from the nearest existing crack toward whichever cell is furthest
    // from all of them, until none is more than kMaxCrackGap px away
    int maxGapCells = static_cast<int>(kMaxCrackGap / kCellSize);
    std::vector<int> gapDistance(cellCount);
    std::vector<int> nearestCrack(cellCount);
    std::vector<int> queue;
    queue.reserve(cellCount);
    for (int fill = 0; fill < kMaxGapFills; fill++) {
        // Distance (in cells, diagonals counting as one) from every cell to its nearest crack cell
        std::fill(gapDistance.begin(), gapDistance.end(), -1);
        queue.clear();
        for (int cell = 0; cell < cellCount; cell++) {
            if (arrival[cell] < INFINITY) {
                gapDistance[cell] = 0;
                nearestCrack[cell] = cell;
                queue.push_back(cell);
            }
        }
        int furthest = -1;
        for (size_t i = 0; i < queue.size(); i++) {
            int cell = queue[i];
            if (furthest < 0 || gapDistance[cell] > gapDistance[furthest]) {
                furthest = cell;
            }
            int cx = cell % cols;
            int cy = cell / cols;
            for (int dy = -1; dy <= 1; dy++) {
                for (int dx = -1; dx <= 1; dx++) {
                    int nx = cx + dx;
                    int ny = cy + dy;
                    if (nx < 0 || ny < 0 || nx >= cols || ny >= rows) {
                        continue;
                    }
                    int neighbour = ny * cols + nx;
                    if (gapDistance[neighbour] < 0) {
                        gapDistance[neighbour] = gapDistance[cell] + 1;
                        nearestCrack[neighbour] = nearestCrack[cell];
                        queue.push_back(neighbour);
                    }
                }
            }
        }
        if (furthest < 0 || gapDistance[furthest] <= maxGapCells) {
            break;
        }

        int source = nearestCrack[furthest];
        float sourceX = source % cols + 0.5f;
        float sourceY = source / cols + 0.5f;
        float targetX = furthest % cols + 0.5f;
        float targetY = furthest / cols + 0.5f;
        float heading = std::atan2(targetY - sourceY, targetX - sourceX);
        float length = std::hypot(targetX - sourceX, targetY - sourceY) * kGapCrackOvershoot;
        growCrack({sourceX, sourceY, heading, length, arrival[source]});
        growPending();
    }

    // Widen every crack outward at once: each cell is reached by whichever crack gets there first,
    // and inherits that crack's widening speed. Diagonal steps cost the same as straight ones, so
    // the widening spreads in squares rather than circles
    std::vector<float> stepJitter(cellCount);
    for (float &jitter : stepJitter) {
        jitter = uniform(kMinStepJitter, kMaxStepJitter);
    }
    using Entry = std::pair<float, int>;
    std::priority_queue<Entry, std::vector<Entry>, std::greater<>> frontier;
    for (int cell = 0; cell < cellCount; cell++) {
        if (arrival[cell] < INFINITY) {
            frontier.emplace(arrival[cell], cell);
        }
    }
    int center = static_cast<int>(centerY) * cols + static_cast<int>(centerX);
    if (frontier.empty()) {
        arrival[center] = 0.f;
        frontier.emplace(0.f, center);
    }

    while (!frontier.empty()) {
        auto [time, cell] = frontier.top();
        frontier.pop();
        if (time > arrival[cell]) {
            continue;
        }
        int cx = cell % cols;
        int cy = cell / cols;
        for (int dy = -1; dy <= 1; dy++) {
            for (int dx = -1; dx <= 1; dx++) {
                int nx = cx + dx;
                int ny = cy + dy;
                if ((dx == 0 && dy == 0) || nx < 0 || ny < 0 || nx >= cols || ny >= rows) {
                    continue;
                }
                int neighbour = ny * cols + nx;
                float next = time + stepJitter[neighbour] / widenSpeed[cell];
                if (next < arrival[neighbour]) {
                    arrival[neighbour] = next;
                    widenSpeed[neighbour] = widenSpeed[cell];
                    frontier.emplace(next, neighbour);
                }
            }
        }
    }

    // Rescale so the last cell is reached at kRevealDuration. Partly re-timed by each cell's rank
    // so the screen converts at a steadier rate, rather than the slowest pockets trickling in at
    // the end. The order cells convert in is unchanged
    std::vector<int> byArrival(cellCount);
    for (int cell = 0; cell < cellCount; cell++) {
        byArrival[cell] = cell;
    }
    std::sort(byArrival.begin(), byArrival.end(), [&](int a, int b) { return arrival[a] < arrival[b]; });
    float latest = arrival[byArrival.back()];
    for (int rank = 0; rank < cellCount; rank++) {
        float &a = arrival[byArrival[rank]];
        float natural = latest > 0.f ? a / latest : 0.f;
        float even = cellCount > 1 ? static_cast<float>(rank) / (cellCount - 1) : 0.f;
        a = kRevealDuration * (kEvenPacing * even + (1.f - kEvenPacing) * natural);
    }

    // Stray pixels that jump ahead or hold out
    for (float &a : arrival) {
        float roll = uniform(0.f, 1.f);
        if (roll < kJumpAheadChance) {
            a = std::max(0.f, a - uniform(0.f, kMaxJumpAhead));
        } else if (roll < kJumpAheadChance + kHoldOutChance) {
            a += uniform(0.f, kMaxHoldOut);
        }
    }
}

bool TendrilReveal::isBuiltFor(const ImVec2& size) const
{
    return !arrival.empty() && builtSize.x == size.x && builtSize.y == size.y;
}

float TendrilReveal::totalDuration()
{
    return kRevealDuration + kMaxHoldOut + kCellFadeDuration * (1.f + kCellFadeJitter);
}

void TendrilReveal::draw(ImDrawList* drawList, const ImVec2& areaMin, float time, ImTextureID image, const ImVec2& imageMin, const ImVec2& imageMax, ImU32 background) const
{
    ImVec2 imageSize(imageMax.x - imageMin.x, imageMax.y - imageMin.y);
    auto uvAt = [&](float x, float y) { return ImVec2((x - imageMin.x) / imageSize.x, (y - imageMin.y) / imageSize.y); };

    // Draws min..max with the image where it overlaps the image and the background elsewhere,
    // over whatever's already there at `alpha`. A cell straddling the image edge needs both
    auto drawRevealed = [&](ImVec2 min, ImVec2 max, float alpha) {
        ImVec2 inMin(std::max(min.x, imageMin.x), std::max(min.y, imageMin.y));
        ImVec2 inMax(std::min(max.x, imageMax.x), std::min(max.y, imageMax.y));
        bool overlapsImage = inMin.x < inMax.x && inMin.y < inMax.y;
        bool insideImage = overlapsImage && inMin.x == min.x && inMin.y == min.y && inMax.x == max.x && inMax.y == max.y;
        if (!insideImage) {
            drawList->AddRectFilled(min, max, withAlpha(background, alpha));
        }
        if (overlapsImage) {
            drawList->AddImage(image, inMin, inMax, uvAt(inMin.x, inMin.y), uvAt(inMax.x, inMax.y), IM_COL32(255, 255, 255, static_cast<int>(std::clamp(alpha, 0.f, 1.f) * 255.f)));
        }
    };

    for (int y = 0; y < rows; y++) {
        float top = areaMin.y + y * kCellSize;
        float bottom = std::min(top + kCellSize, areaMin.y + builtSize.y);

        // Fully revealed cells are merged into horizontal runs to keep the draw count down
        bool inRun = false;
        float runStart = 0.f;
        auto flushRun = [&](float runEnd) {
            if (inRun) {
                drawRevealed(ImVec2(runStart, top), ImVec2(runEnd, bottom), 1.f);
                inRun = false;
            }
        };

        for (int x = 0; x < cols; x++) {
            int cell = y * cols + x;
            float left = areaMin.x + x * kCellSize;
            float right = std::min(left + kCellSize, areaMin.x + builtSize.x);

            float fadeDuration = kCellFadeDuration * (1.f + kCellFadeJitter * (2.f * hash01(static_cast<uint32_t>(cell), revealSeed) - 1.f));
            float progress = (time - arrival[cell]) / fadeDuration;
            if (progress >= 1.f) {
                if (!inRun) {
                    inRun = true;
                    runStart = left;
                }
                continue;
            }

            flushRun(left);
            if (progress > 0.f) {
                // Crossfade straight from whatever's behind to the image
                drawRevealed(ImVec2(left, top), ImVec2(right, bottom), progress);
            }
        }
        flushRun(areaMin.x + builtSize.x);
    }
}
