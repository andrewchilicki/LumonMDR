#include "NumbersPanel.h"

#include "Numbers/NumberGrid.h"
#include "Bin.h"
#include "Files.h"
#include "ImageDisplay.h"
#include "Settings.h"
#include "../UIManager.h"

#include <algorithm>
#include <array>
#include <cfloat>
#include <cmath>
#include <cstring>
#include <imgui.h>
#include <imgui_internal.h>
#include <iostream>
#include <map>
#include <optional>
#include <random>
#include <utility>
#include <vector>
#include <json.hpp>
#include "PerlinNoise.hpp"

namespace
{
    // Per-number fade-in: each number waits a random delay, then fades/grows in over a random duration
    constexpr float kFadeInMaxDelay = 3.f;
    constexpr float kFadeInMinDuration = 1.5f;
    constexpr float kFadeInMaxDuration = 3.5f;

    constexpr float kHeaderTextPadding = 25.f;

    // Seed for the grid shown before any file is opened; each file in kFiles offsets from it
    constexpr unsigned int kDefaultGridSeed = 505;

    // Numbers per side of each file's grid. 100 gave ~300 bad groups per file; 30 gives ~25-35
    constexpr int kGridSize = 30;

    // When a file's layout can't show its preset % exactly, later seeds are tried, kSeedSearchStride
    // apart (clear of the other files' default seeds), up to kMaxSeedSearchAttempts
    constexpr unsigned int kSeedSearchStride = 1000;
    constexpr int kMaxSeedSearchAttempts = 200;

    // Groups a preset refines: the fewest that reach `percent` of `total`
    int presetRefinedGroups(int total, int percent)
    {
        return (total * percent + 99) / 100;
    }

    // Whether refining whole groups lands exactly on `percent` as the header shows it (rounded down)
    bool presetIsExact(int total, int percent)
    {
        if (total == 0) {
            return percent == 0;
        }
        return presetRefinedGroups(total, percent) * 100 / total == percent;
    }

    // Scrolling: the view eases toward a target offset (higher kScrollSmoothing = snappier).
    // A tap of an arrow key moves one row; holding it past kHeldScrollDelay keeps scrolling.
    // Moving the mouse within kEdgeScrollMargin px of an edge scrolls that way, faster nearer the edge
    constexpr float kScrollSmoothing = 12.f;
    constexpr float kHeldScrollDelay = 0.3f;
    constexpr float kHeldScrollRowsPerSecond = 6.f;
    constexpr float kEdgeScrollMargin = 60.f;
    constexpr float kEdgeScrollRowsPerSecond = 6.f;

    // Zooming eases toward a target zoom (higher kZoomSmoothing = snappier), keeping whatever is
    // under the anchor point in place: the cursor for the mouse wheel, the grid's center for keys.
    // A tap of ','/'.' zooms by kZoomStepFactor; holding past kHeldZoomDelay keeps zooming
    constexpr float kZoomSmoothing = 10.f;
    constexpr float kZoomStepFactor = 1.25f;
    constexpr float kHeldZoomDelay = 0.3f;
    constexpr float kHeldZoomStepsPerSecond = 3.f;
    constexpr float kWheelZoomStepFactor = 1.15f;

    // Saved settings like noiseSpeed and refinedToBinSpeed were tuned as per-frame amounts at 60fps;
    // scaling them by elapsed time keeps them feeling the same at any frame rate
    constexpr float kReferenceFps = 60.f;

    // Max pixel jitter of a hovered group's numbers, scaled by the group's swell and agitation
    constexpr int kSuperActiveJitter = 10;

    // Zoom at which group swell and jitter look as tuned; both shrink in proportion when zoomed
    // further out (and grow when zoomed in), so active groups are harder to spot from far away
    constexpr float kCueReferenceZoom = 0.3f;

    // Active groups swell fully when the cursor is on them, down to kFarSwellFraction of that once
    // it's kSwellFalloffRadius px or more away. kProximitySmoothing sets how quickly the swell
    // follows the cursor (higher = snappier)
    constexpr float kFarSwellFraction = 0.35f;
    constexpr float kSwellFalloffRadius = 400.f;
    constexpr float kProximitySmoothing = 6.f;

    // Shaking ramps up over kAgitationRiseDuration while hovered and settles over kAgitationSettleDuration after
    constexpr float kAgitationRiseDuration = 0.15f;
    constexpr float kAgitationSettleDuration = 1.5f;

    // Highlighted (last few) groups always swell fully so they're easy to spot
    float swellScale(const BadGroup& group)
    {
        float proximity = group.highlighted ? 1.f : group.cursorProximity;
        return static_cast<float>(group.scale) * (kFarSwellFraction + (1.f - kFarSwellFraction) * proximity);
    }

    // After the last group is refined, how long to keep showing the grid (so its numbers can land
    // in their bins) before the file counts as just completed
    constexpr float kCompletionDelay = 1.5f;

    // Numbers fade out over their last kBorderFadeDistance px before the grid area's edge
    constexpr float kBorderFadeDistance = 40.f;

    // Off-screen highlighted groups get a pulsing arrow kHintInset px inside the grid area's edge,
    // pointing toward them
    constexpr float kHintInset = 30.f;
    constexpr float kHintSize = 14.f;
    constexpr float kHintPulseSpeed = 4.f; // Radians per second
    constexpr float kHintMinAlpha = 0.35f;
    constexpr float kHintFadeInDuration = 2.5f;
    constexpr float kHintFadeOutDuration = 0.4f;

    // Built once rather than per number per frame
    const std::array<std::string, 10> kNumberImagePaths = [] {
        std::array<std::string, 10> paths;
        for (int i = 0; i < 10; i++) {
            paths[i] = "numbers/" + std::to_string(i) + ".png";
        }
        return paths;
    }();
}

class NumbersPanelImpl : public NumbersPanel
{
    struct FileProgress;

public:
    explicit NumbersPanelImpl(std::shared_ptr<ImageDisplay> imageDisplay) : imageDisplay(std::move(imageDisplay))
    {
        openFile("");

        // Load settings
        if (auto loadedSettings = loadSettings(settingsSavePath)) {
            displaySettings = loadedSettings->displaySettings;
            controlSettings = loadedSettings->controlSettings;
            std::cout << "Successfully loaded settings from disk." << std::endl;
        }
    }

    void init() final
    {
        // Load custom font
        ImGuiIO& io = ImGui::GetIO();
        font = io.Fonts->AddFontFromFileTTF("./assets/Montserrat-Bold.ttf", 50.f);
        io.Fonts->Build();
        if (font == nullptr) {
            font = ImGui::GetDefaultFont();
            std::cerr << "Failed to load 'Montserrat-Bold' font." << std::endl;
        }
    }

    // Game logic. Works from positions cached by the last draw (number centers, visibility, bin
    // positions), and leaves per-number offsets/scales for the next draw to render.
    void update() final
    {
        float dt = ImGui::GetIO().DeltaTime;
        ImVec2 mousePos = ImGui::GetIO().MousePos;

        activeFile->numberGrid->update(dt);
        noiseTime += dt * kReferenceFps * displaySettings.noiseSpeed;
        updateGroupCursorResponse(dt, mousePos);
        updateHintFades(dt);

        if (fileCompletion == FileCompletion::InProgress && allGroupsRefined()) {
            completionTimer += dt;
            if (completionTimer >= kCompletionDelay) {
                fileCompletion = FileCompletion::JustCompleted;
            }
        }

        binReceiving.reset();
        for (const auto &[x, gridY] : activeFile->numberGrid->getGrid()) {
            for (const auto &[y, gridNumber] : gridY) {
                // Fade in off-screen numbers too, so they're already there when scrolled to
                advanceFadeIn(*gridNumber, dt);
                if (gridNumber->displayInfos.isVisible) {
                    updateNumber(*gridNumber, dt, mousePos);
                }
            }
        }
    }

    void drawNumbersPanel() final
    {
        bool displaySizeChanged = updateDisplaySettings(displayPresets, displaySettings.globalScale);

        ImVec2 windowSize = ImGui::GetWindowSize();
        ImVec2 windowPos = ImGui::GetWindowPos();

        ImDrawList* drawList = ImGui::GetWindowDrawList();

        // Update viewport, re-laying out numbers if it moved or a different file's grid is showing
        bool viewportChanged = updateViewport(windowPos, windowSize);
        if (viewportChanged || displaySizeChanged || fileChanged) {
            layoutNumbers(windowPos, windowSize);
            fileChanged = false;
        }

        drawGraphicOverlays(windowPos, windowSize, drawList);
        drawNumbers();
        drawOffscreenHints(windowPos, windowSize, drawList);
        drawBins(windowPos, windowSize, drawList);
    }

    void triggerLoadAnimation() final
    {
        // Restart the fade-in on all numbers, staggered so they don't all appear at once
        for (const auto &[x, gridY] : activeFile->numberGrid->getGrid()) {
            for (const auto &[y, gridNumber] : gridY) {
                startFadeIn(*gridNumber, kFadeInMaxDelay);
            }
        }
    }

    void openFile(const std::string& fileName_) final
    {
        fileName = fileName_;

        auto [it, isNew] = files.try_emplace(fileName);
        activeFile = &it->second;
        if (isNew) {
            createFileProgress(*activeFile, fileName);
        }

        // Cached number positions belong to whatever viewport this file was last drawn with
        fileChanged = true;
        triggerLoadAnimation();

        fileCompletion = allGroupsRefined() ? FileCompletion::AlreadyComplete : FileCompletion::InProgress;
        completionTimer = 0.f;
    }

    FileCompletion getFileCompletion() const final
    {
        return fileCompletion;
    }

    void drawSettings() final
    {
        ImGui::SetWindowFontScale(displayPresets.settingsFontScale);
        if (ImGui::Button("Save Settings")) {
            saveSettings(Settings{displaySettings, controlSettings}, settingsSavePath);
        }
        ImGui::Separator();
        ImGui::Text("Display:");
        ImGui::InputFloat("Global Scale", &displaySettings.globalScale);
        ImGui::InputFloat("Numbers Image Scale", &displaySettings.imageScale);
        ImGui::InputFloat("Numbers Grid Spacing", &displaySettings.gridSpacing);
        ImGui::InputFloat("Mouse Scale Radius", &displaySettings.mouseScaleRadius);
        ImGui::InputFloat("Mouse Scale Multiplier", &displaySettings.mouseScaleMultiplier);
        ImGui::InputFloat("Min Scale Multiplier", &displaySettings.minZoomScale);
        ImGui::InputFloat("Max Scale Multiplier", &displaySettings.maxZoomScale);
        ImGui::InputFloat("Refined to Bin Speed", &displaySettings.refinedToBinSpeed);

        // Edit through a fixed buffer; ImGui can't resize a std::string itself
        char headerTextBuffer[128];
        std::strncpy(headerTextBuffer, displaySettings.headerText.c_str(), sizeof(headerTextBuffer) - 1);
        headerTextBuffer[sizeof(headerTextBuffer) - 1] = '\0';
        if (ImGui::InputText("Header Text", headerTextBuffer, sizeof(headerTextBuffer))) {
            displaySettings.headerText = headerTextBuffer;
        }

        ImGui::Text("Noise:");
        ImGui::InputFloat("Noise Speed", &displaySettings.noiseSpeed);
        ImGui::InputFloat("Noise Scale", &displaySettings.noiseScale);
        ImGui::InputFloat("Noise Scale Offset", &displaySettings.noiseScaleOffset);
        ImGui::Text("Controls:");
        ImGui::Separator();
        ImGui::Text("Debug:");
        ImGui::Checkbox("revealMap", &revealMap);
    }

private:
    void createFileProgress(FileProgress& progress, const std::string& name)
    {
        unsigned int seed = kDefaultGridSeed;
        int presetPercent = 0;
        for (size_t i = 0; i < kFiles.size(); i++) {
            if (kFiles[i].name == name) {
                seed = kFiles[i].seed != 0 ? kFiles[i].seed : kDefaultGridSeed + static_cast<unsigned int>(i) + 1;
                presetPercent = kFiles[i].presetPercent;
            }
        }

        // Seeds in Files.h are chosen so the preset shows exactly. If one doesn't (e.g. its % was
        // changed), fall back to the first later layout that does, so the header never disagrees
        // with Files.h, and log the seed to put there
        presetPercent = std::clamp(presetPercent, 0, 100);
        for (int attempt = 0; ; attempt++) {
            unsigned int attemptSeed = seed + attempt * kSeedSearchStride;
            progress.numberGrid = createNumberGrid(kGridSize, attemptSeed);
            int total = static_cast<int>(progress.numberGrid->getBadGroups().size());
            if (presetIsExact(total, presetPercent)) {
                if (attempt > 0) {
                    std::cerr << "Seed " << seed << " can't show exactly " << presetPercent << "% for '" << name << "'; using " << attemptSeed << " instead. Set that seed in Files.h." << std::endl;
                }
                break;
            }
            if (attempt + 1 >= kMaxSeedSearchAttempts) {
                int shownPercent = total > 0 ? presetRefinedGroups(total, presetPercent) * 100 / total : 0;
                std::cerr << "No grid layout shows exactly " << presetPercent << "% for '" << name << "'; it will show " << shownPercent << "%." << std::endl;
                break;
            }
        }

        // Update max bad groups for each bin
        for (const auto &[id, group] : progress.numberGrid->getBadGroups()) {
            progress.bins[group->binIdx].maxBadGroups++;
        }

        applyPresetCompletion(progress, presetPercent);
    }

    // Instantly refines enough random bad groups to reach `percent` overall (rounded up), always
    // taking from whichever bin is proportionally least full so the bins stay roughly even
    void applyPresetCompletion(FileProgress& progress, int percent)
    {
        std::array<std::vector<BadGroupPtr>, 5> unrefined;
        int total = 0;
        for (const auto &[id, group] : progress.numberGrid->getBadGroups()) {
            unrefined[group->binIdx].push_back(group);
            total++;
        }
        for (auto &groups : unrefined) {
            std::shuffle(groups.begin(), groups.end(), rng);
        }

        auto &bins = progress.bins;
        int target = presetRefinedGroups(total, percent);
        for (int n = 0; n < target; n++) {
            int best = -1;
            for (int b = 0; b < static_cast<int>(bins.size()); b++) {
                if (unrefined[b].empty()) {
                    continue;
                }
                // refined[b] / max[b] < refined[best] / max[best], cross-multiplied
                if (best < 0 || bins[b].badGroupsRefined * bins[best].maxBadGroups < bins[best].badGroupsRefined * bins[b].maxBadGroups) {
                    best = b;
                }
            }
            if (best < 0) {
                break;
            }

            BadGroupPtr group = unrefined[best].back();
            unrefined[best].pop_back();

            // Detach its numbers so they become ordinary; the grid drops the emptied group on its next update
            for (int numberId : group->numberIds) {
                progress.numberGrid->getGridNumber(numberId)->badGroup.reset();
            }
            group->numberIds.clear();
            group->refined = true;
            bins[best].badGroupsRefined++;
        }
    }

    void startFadeIn(Number& number, float maxDelay)
    {
        number.regenerateScale = 0.f;
        number.regenerateDelay = std::uniform_real_distribution<float>(0.f, maxDelay)(rng);
        number.regenerateDuration = std::uniform_real_distribution<float>(kFadeInMinDuration, kFadeInMaxDuration)(rng);
    }

    // Recomputes every number's on-screen center and whether it's inside the grid window
    void layoutNumbers(const ImVec2& windowPos, const ImVec2& windowSize)
    {
        for (const auto &[x, gridY] : activeFile->numberGrid->getGrid()) {
            for (const auto &[y, gridNumber] : gridY) {
                auto &info = gridNumber->displayInfos;
                auto [width, height] = imageDisplay->getImageSize(kNumberImagePaths[gridNumber->num]);

                info.centerX = (x * displaySettings.gridSpacing + panelOffset.x) * panelScale + windowPos.x;
                info.centerY = (y * displaySettings.gridSpacing + panelOffset.y) * panelScale + windowPos.y;

                float baseNumberScale = displaySettings.imageScale * panelScale;
                float widthOffset = baseNumberScale * width / 2.f;
                float heightOffset = baseNumberScale * height / 2.f;

                // How far the number sits inside the grid area at its closest edge; it's only drawn
                // while fully inside, fading out as it approaches the edge
                float inside = std::min({info.centerX - widthOffset - windowPos.x,
                                         windowPos.x + windowSize.x - (info.centerX + widthOffset),
                                         info.centerY - heightOffset - (windowPos.y + displayPresets.numberWindowBufferTop),
                                         windowPos.y + windowSize.y - displayPresets.numberWindowBufferBottom - (info.centerY + heightOffset)});
                info.isVisible = inside > 0.f;
                info.edgeAlpha = std::clamp(inside / kBorderFadeDistance, 0.f, 1.f);
            }
        }
    }

    // How close the cursor is to each active group, which drives how much it swells and shakes.
    // Uses number positions from the previous update, which is a frame behind at most
    void updateGroupCursorResponse(float dt, const ImVec2& mousePos)
    {
        for (const auto &[id, group] : activeFile->numberGrid->getBadGroups()) {
            if (!group->isActive || group->refined) {
                continue;
            }

            float nearest = FLT_MAX;
            for (int numberId : group->numberIds) {
                const auto &info = activeFile->numberGrid->getGridNumber(numberId)->displayInfos;
                if (info.isVisible) {
                    float distX = mousePos.x - (info.centerX + info.offsetX);
                    float distY = mousePos.y - (info.centerY + info.offsetY);
                    nearest = std::min(nearest, std::sqrt(distX * distX + distY * distY));
                }
            }

            float targetProximity = std::clamp(1.f - nearest / kSwellFalloffRadius, 0.f, 1.f);
            group->cursorProximity += (targetProximity - group->cursorProximity) * std::min(1.f, dt * kProximitySmoothing);

            bool hovered = nearest < displaySettings.mouseScaleRadius;
            if (hovered) {
                group->superActive = true;
                group->agitation = std::min(1.f, group->agitation + dt / kAgitationRiseDuration);
            } else {
                group->agitation = std::max(0.f, group->agitation - dt / kAgitationSettleDuration);
            }
        }
    }

    // Wait out the fade-in delay, then fade/grow in over the duration
    void advanceFadeIn(Number& number, float dt)
    {
        if (number.regenerateScale >= 1.f) {
            return;
        }
        if (number.regenerateDelay > 0.f) {
            number.regenerateDelay -= dt;
        } else {
            number.regenerateScale = std::min(1.f, number.regenerateScale + dt / number.regenerateDuration);
        }
    }

    void updateNumber(Number& number, float dt, const ImVec2& mousePos)
    {
        auto &info = number.displayInfos;

        // Drift along one axis with the noise field
        float drift = static_cast<float>(perlin.noise3D(number.gridX * displaySettings.noiseScale, number.gridY * displaySettings.noiseScale, noiseTime)) * displaySettings.noiseScaleOffset;
        info.offsetX = info.horizontalOffset ? drift : 0.f;
        info.offsetY = info.horizontalOffset ? 0.f : drift;

        info.hoverScale = getScaleFromCursor(ImVec2(info.centerX + info.offsetX, info.centerY + info.offsetY), mousePos);

        BadGroupPtr group = number.badGroup;
        if (!group) {
            return;
        }

        if (group->refined) {
            flyToBin(number, *group, dt);
            return;
        }

        // Clicking right on top of an active group refines it
        if (group->isActive && info.hoverScale >= 0.5f + displaySettings.mouseScaleMultiplier && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
            group->refined = true;
            activeFile->bins[group->binIdx].badGroupsRefined++;
            return;
        }

        if (group->agitation > 0.f) {
            std::uniform_int_distribution<int> jitter(-kSuperActiveJitter, kSuperActiveJitter);
            float amount = group->agitation * static_cast<float>(group->scale) * zoomCueFactor();
            info.offsetX += jitter(rng) * amount;
            info.offsetY += jitter(rng) * amount;
        }
    }

    void flyToBin(Number& number, BadGroup& group, float dt)
    {
        auto &info = number.displayInfos;
        if (info.refinedX == -1.f) {
            info.refinedX = info.centerX;
            info.refinedY = info.centerY;
        }

        int binIdx = std::clamp(group.binIdx, 0, 4);
        const Bin &bin = activeFile->bins[binIdx];
        float distX = bin.pos.x - info.refinedX;
        float distY = bin.pos.y - info.refinedY;
        float distance = std::sqrt(distX * distX + distY * distY);
        float step = displaySettings.refinedToBinSpeed * kReferenceFps * dt;

        if (distance > step) {
            info.refinedX += distX / distance * step;
            info.refinedY += distY / distance * step;
            binReceiving = binIdx;
            return;
        }

        // Landed: becomes an ordinary number again. Removing it from the group lets the grid drop
        // the group once all its numbers are in
        auto &ids = group.numberIds;
        ids.erase(std::remove(ids.begin(), ids.end(), number.id), ids.end());
        number.badGroup.reset();
        number.num = std::uniform_int_distribution<int>(0, 9)(rng);
        info.refinedX = -1.f;
        info.refinedY = -1.f;
        startFadeIn(number, 0.f);
    }

    void drawNumbers()
    {
        ImVec2 windowPos = ImGui::GetWindowPos();
        for (const auto &[x, gridY] : activeFile->numberGrid->getGrid()) {
            for (const auto &[y, gridNumber] : gridY) {
                const auto &info = gridNumber->displayInfos;
                if (!info.isVisible) {
                    continue;
                }

                const BadGroupPtr &group = gridNumber->badGroup;
                bool flying = group && group->refined && info.refinedX != -1.f;
                ImVec2 pos = flying ? ImVec2(info.refinedX, info.refinedY) : ImVec2(info.centerX + info.offsetX, info.centerY + info.offsetY);
                float badScale = group ? swellScale(*group) * zoomCueFactor() : 0.f;

                ImVec4 col = ColorValues::lumonBlue.Value;
                col.w = gridNumber->regenerateScale;
                if (revealMap && group) {
                    col = group->isActive ? ImVec4(1.f, 1.f, 0.f, col.w) : ImVec4(1.f, 0.f, 0.f, 1.f);
                }
                if (!flying) {
                    col.w *= info.edgeAlpha;
                }

                const std::string &imagePath = kNumberImagePaths[gridNumber->num];
                auto [width, height] = imageDisplay->getImageSize(imagePath);
                float scale = gridNumber->regenerateScale * displaySettings.imageScale * info.hoverScale * panelScale + badScale;
                ImGui::SetCursorPos(ImVec2(pos.x - windowPos.x - width * scale / 2.f, pos.y - windowPos.y - height * scale / 2.f));
                imageDisplay->drawImGuiImage(imagePath, scale, col);
            }
        }
    }

    // Off-screen arrows fade in slowly while a highlighted group has no numbers on screen, and
    // fade out quickly once it does
    void updateHintFades(float dt)
    {
        for (const auto &[id, group] : activeFile->numberGrid->getBadGroups()) {
            bool anyVisible = false;
            for (int numberId : group->numberIds) {
                anyVisible |= activeFile->numberGrid->getGridNumber(numberId)->displayInfos.isVisible;
            }

            if (group->highlighted && !group->refined && !anyVisible) {
                group->hintAlpha = std::min(1.f, group->hintAlpha + dt / kHintFadeInDuration);
            } else {
                group->hintAlpha = std::max(0.f, group->hintAlpha - dt / kHintFadeOutDuration);
            }
        }
    }

    // Pulsing arrows at the edge of the grid area pointing toward highlighted groups with no
    // numbers on screen
    void drawOffscreenHints(const ImVec2& windowPos, const ImVec2& windowSize, ImDrawList* drawList)
    {
        ImVec2 areaMin(windowPos.x + kHintInset, windowPos.y + displayPresets.numberWindowBufferTop + kHintInset);
        ImVec2 areaMax(windowPos.x + windowSize.x - kHintInset, windowPos.y + windowSize.y - displayPresets.numberWindowBufferBottom - kHintInset);
        ImVec2 areaCenter((areaMin.x + areaMax.x) / 2.f, (areaMin.y + areaMax.y) / 2.f);
        ImVec2 areaHalf((areaMax.x - areaMin.x) / 2.f, (areaMax.y - areaMin.y) / 2.f);

        float pulse = 0.5f + 0.5f * std::sin(static_cast<float>(ImGui::GetTime()) * kHintPulseSpeed);
        float pulseAlpha = kHintMinAlpha + (1.f - kHintMinAlpha) * pulse;

        for (const auto &[id, group] : activeFile->numberGrid->getBadGroups()) {
            if (group->hintAlpha <= 0.f || group->numberIds.empty()) {
                continue;
            }

            // Group's center from the laid-out number positions
            ImVec2 target(0.f, 0.f);
            for (int numberId : group->numberIds) {
                const auto &info = activeFile->numberGrid->getGridNumber(numberId)->displayInfos;
                target.x += info.centerX;
                target.y += info.centerY;
            }
            target.x /= group->numberIds.size();
            target.y /= group->numberIds.size();

            // Where the line from the area's center to the group crosses the inset edge
            ImVec2 dir(target.x - areaCenter.x, target.y - areaCenter.y);
            float length = std::sqrt(dir.x * dir.x + dir.y * dir.y);
            if (length < 1.f) {
                continue;
            }
            float t = std::min(dir.x != 0.f ? areaHalf.x / std::abs(dir.x) : FLT_MAX, dir.y != 0.f ? areaHalf.y / std::abs(dir.y) : FLT_MAX);
            ImVec2 edge(areaCenter.x + dir.x * t, areaCenter.y + dir.y * t);

            ImVec2 forward(dir.x / length, dir.y / length);
            ImVec2 side(-forward.y, forward.x);
            ImVec2 tip(edge.x + forward.x * kHintSize, edge.y + forward.y * kHintSize);
            ImVec2 baseLeft(edge.x - forward.x * kHintSize * 0.5f + side.x * kHintSize * 0.7f, edge.y - forward.y * kHintSize * 0.5f + side.y * kHintSize * 0.7f);
            ImVec2 baseRight(edge.x - forward.x * kHintSize * 0.5f - side.x * kHintSize * 0.7f, edge.y - forward.y * kHintSize * 0.5f - side.y * kHintSize * 0.7f);
            ImU32 color = ImGui::ColorConvertFloat4ToU32(ImVec4(ColorValues::lumonBlue.Value.x, ColorValues::lumonBlue.Value.y, ColorValues::lumonBlue.Value.z, pulseAlpha * group->hintAlpha));
            drawList->AddTriangleFilled(tip, baseLeft, baseRight, color);
        }
    }

    void drawBins(const ImVec2& windowPos, const ImVec2& windowSize, ImDrawList* drawList)
    {
        std::string binPercentPath = "bins/bin-percent.png";
        auto [widthP, heightP] = imageDisplay->getImageSize(binPercentPath);
        for (auto &b : activeFile->bins) {
            // Scale based on viewport size
            auto pos = b.updatePos(windowSize, windowPos, displayPresets.numberWindowBufferBottom - displayPresets.binOffset);

            // Draw bin images
            std::string binPath = "bins/bin0" + std::to_string(b.id) + ".png";
            auto [width, height] = imageDisplay->getImageSize(binPath);
            ImGui::SetCursorPos(ImVec2(pos.x - windowPos.x - (width*displayPresets.binImageScale/2.f), pos.y - windowPos.y - (height*displayPresets.binImageScale/2.f)));
            imageDisplay->drawImGuiImage(binPath, displayPresets.binImageScale, ColorValues::lumonBlue);

            auto percentPos = ImVec2(pos.x, pos.y + displayPresets.binPercentBarOffset);
            ImGui::SetCursorPos(ImVec2(percentPos.x - windowPos.x - (widthP*displayPresets.binImageScale/2.f), percentPos.y - windowPos.y - (heightP*displayPresets.binImageScale/2.f)));
            imageDisplay->drawImGuiImage(binPercentPath, displayPresets.binImageScale, ColorValues::lumonBlue);

            // Draw percentage bar and text
            ImVec2 trCorner = ImVec2(percentPos.x - (widthP*displayPresets.binImageScale/2.f), percentPos.y - (heightP*displayPresets.binImageScale/2.f));
            ImVec2 brCorner = ImVec2(percentPos.x + (widthP*displayPresets.binImageScale/2.f), percentPos.y + (heightP*displayPresets.binImageScale/2.f));

            double percentD = b.fractionRefined();
            int percentInt = lround(percentD * 100.f);
            std::string percentString = std::to_string(percentInt) + "%";
            drawList->AddText(font, displayPresets.fontSize, ImVec2(trCorner.x + 5.f, (trCorner.y + brCorner.y)/2.f - displayPresets.fontSize/2.f), ColorValues::lumonBlue, percentString.c_str());

            drawList->AddRectFilled(trCorner, ImVec2(trCorner.x + ((brCorner.x - trCorner.x)* percentD), brCorner.y), ImColor(ColorValues::lumonBlue.Value.x, ColorValues::lumonBlue.Value.y, ColorValues::lumonBlue.Value.z, 0.3f));

            // Animate bin open
            if (binReceiving && *binReceiving == b.id - 1) {
                std::string binOpenPath = "bins/bin-open.png";
                auto [widthO, heightO] = imageDisplay->getImageSize(binOpenPath);
                ImGui::SetCursorPos(ImVec2(pos.x - windowPos.x - (widthO*displayPresets.binImageScale/2.f), pos.y - windowPos.y - (heightO*displayPresets.binImageScale/2.f) - (height*displayPresets.binImageScale)));
                imageDisplay->drawImGuiImage(binOpenPath, displayPresets.binImageScale, ColorValues::lumonBlue);
            }
        }
    }

    void drawGraphicOverlays(const ImVec2& windowPos, const ImVec2& windowSize, ImDrawList* drawList)
    {
        // Header box
        ImVec2 headerBoxMin = ImVec2(windowPos.x + displayPresets.headerBoxBufferX, windowPos.y + displayPresets.headerBoxBufferY);
        ImVec2 headerBoxMax = ImVec2(windowPos.x + windowSize.x - displayPresets.headerBoxBufferX - displayPresets.headerImageOffsetX, windowPos.y + displayPresets.numberWindowBufferTop - displayPresets.lineGraphicsSpacing - displayPresets.headerBoxBufferY);
        drawList->AddRect(headerBoxMin, headerBoxMax, ColorValues::lumonBlue);
        float headerTextY = (headerBoxMin.y+headerBoxMax.y)/2.f - displayPresets.fontSize/2.f;
        const std::string& headerText = fileName.empty() ? displaySettings.headerText : fileName;
        drawList->AddText(font, displayPresets.fontSize, ImVec2(headerBoxMin.x + kHeaderTextPadding, headerTextY), ColorValues::lumonBlue, headerText.c_str());

        std::string logoPath = "lumon-logo.png";
        auto [widthH, heightH] = imageDisplay->getImageSize(logoPath);

        // Overall completion, right-aligned against the logo (which straddles the box's right edge)
        std::string completeText = std::to_string(getCompletionPercent()) + "% Complete";
        float completeTextWidth = font->CalcTextSizeA(displayPresets.fontSize, FLT_MAX, 0.f, completeText.c_str()).x;
        float completeTextX = headerBoxMax.x - (widthH*displayPresets.headerImageScale)/2.f - kHeaderTextPadding - completeTextWidth;
        drawList->AddText(font, displayPresets.fontSize, ImVec2(completeTextX, headerTextY), ColorValues::lumonBlue, completeText.c_str());

        // Lumon logo
        ImGui::SetCursorPos(ImVec2(headerBoxMax.x - (widthH*displayPresets.headerImageScale)/2.f, (headerBoxMax.y + headerBoxMin.y)/2.f - (heightH*displayPresets.headerImageScale)/2.f));
        imageDisplay->drawImGuiImage(logoPath, displayPresets.headerImageScale, ColorValues::lumonBlue);

        // Horizontal lines
        auto drawLine = [&](const float y) {
            drawList->AddLine(ImVec2(windowPos.x, y), ImVec2(windowPos.x + windowSize.x, y), ColorValues::lumonBlue, displayPresets.lineThickness);
        };

        float topLineY = windowPos.y + displayPresets.numberWindowBufferTop;
        drawLine(topLineY);
        drawLine(topLineY - displayPresets.lineGraphicsSpacing);

        float bottomLineY = windowPos.y + windowSize.y - displayPresets.numberWindowBufferBottom;
        drawLine(bottomLineY);
        drawLine(bottomLineY + displayPresets.lineGraphicsSpacing);
    }

    // Steps this frame from a forward/back key pair: a tap is one whole step, and holding past
    // `holdDelay` keeps stepping smoothly at `heldStepsPerSecond`
    static float keySteps(ImGuiKey forwardKey, ImGuiKey backKey, float dt, float holdDelay, float heldStepsPerSecond)
    {
        float steps = 0.f;
        for (auto [key, direction] : {std::pair{forwardKey, 1.f}, std::pair{backKey, -1.f}}) {
            if (ImGui::IsKeyPressed(key, false)) {
                steps += direction;
            } else if (ImGui::IsKeyDown(key) && ImGui::GetKeyData(key)->DownDuration > holdDelay) {
                steps += direction * heldStepsPerSecond * dt;
            }
        }
        return steps;
    }

    // Rows to scroll this frame from the mouse sitting near the edges of the number grid area:
    // the window's sides horizontally, and the header/footer lines vertically. Past an edge
    // (e.g. over the header) scrolls at full speed
    ImVec2 edgeScrollRows(const ImVec2& windowPos, const ImVec2& windowSize, float dt) const
    {
        // Not while the mouse is over another window, e.g. the settings panel
        if (!ImGui::IsMousePosValid() || !ImGui::IsWindowHovered()) {
            return ImVec2(0.f, 0.f);
        }

        ImVec2 mouse(ImGui::GetIO().MousePos.x - windowPos.x, ImGui::GetIO().MousePos.y - windowPos.y);
        float gridTop = displayPresets.numberWindowBufferTop;
        float gridBottom = windowSize.y - displayPresets.numberWindowBufferBottom;

        auto pull = [](float distanceInsideEdge) { return std::clamp(1.f - distanceInsideEdge / kEdgeScrollMargin, 0.f, 1.f); };
        float speed = kEdgeScrollRowsPerSecond * dt;
        return ImVec2((pull(windowSize.x - mouse.x) - pull(mouse.x)) * speed, (pull(gridBottom - mouse.y) - pull(mouse.y - gridTop)) * speed);
    }

    // Clamps one axis of the panel offset into [min, max], centering if the grid is smaller than the window
    static float clampOffset(float offset, float min, float max)
    {
        return min > max ? (min + max) / 2.f : std::clamp(offset, min, max);
    }

    bool updateViewport(const ImVec2& windowPos, const ImVec2& windowSize)
    {
        float dt = ImGui::GetIO().DeltaTime;
        bool viewportChanged = !viewportInitialized;

        // Scroll the target by whole rows; the offset moves opposite to the view
        float row = displaySettings.gridSpacing;
        ImVec2 edgeRows = edgeScrollRows(windowPos, windowSize, dt);
        scrollTarget.x -= (keySteps(ImGuiKey_RightArrow, ImGuiKey_LeftArrow, dt, kHeldScrollDelay, kHeldScrollRowsPerSecond) + edgeRows.x) * row;
        scrollTarget.y -= (keySteps(ImGuiKey_DownArrow, ImGuiKey_UpArrow, dt, kHeldScrollDelay, kHeldScrollRowsPerSecond) + edgeRows.y) * row;

        // Zoom input, in multiplicative steps so each step feels the same at any zoom level
        if (!viewportInitialized) {
            panelScale = std::clamp(panelScale, displaySettings.minZoomScale, displaySettings.maxZoomScale);
            zoomTarget = panelScale;
        }
        float zoomSteps = keySteps(ImGuiKey_Period, ImGuiKey_Comma, dt, kHeldZoomDelay, kHeldZoomStepsPerSecond);
        if (zoomSteps != 0.f) {
            zoomTarget *= std::pow(kZoomStepFactor, zoomSteps);
            zoomAnchor = ImVec2(windowSize.x / 2.f, (displayPresets.numberWindowBufferTop + windowSize.y - displayPresets.numberWindowBufferBottom) / 2.f);
        }
        float wheel = ImGui::GetIO().MouseWheel;
        if (wheel != 0.f && ImGui::IsWindowHovered()) {
            zoomTarget *= std::pow(kWheelZoomStepFactor, wheel);
            zoomAnchor = ImVec2(ImGui::GetIO().MousePos.x - windowPos.x, ImGui::GetIO().MousePos.y - windowPos.y);
        }
        zoomTarget = std::clamp(zoomTarget, displaySettings.minZoomScale, displaySettings.maxZoomScale);

        // Ease the zoom, shifting the offsets so the grid point under the anchor stays put
        // (screen = (grid + offset) * scale, so offset changes by anchor * (1/newScale - 1/oldScale))
        float previousScale = panelScale;
        panelScale += (zoomTarget - panelScale) * (1.f - std::exp(-kZoomSmoothing * dt));
        if (std::abs(zoomTarget - panelScale) < 0.0005f) {
            panelScale = zoomTarget;
        }
        if (panelScale != previousScale) {
            ImVec2 shift(zoomAnchor.x * (1.f / panelScale - 1.f / previousScale), zoomAnchor.y * (1.f / panelScale - 1.f / previousScale));
            panelOffset.x += shift.x;
            panelOffset.y += shift.y;
            scrollTarget.x += shift.x;
            scrollTarget.y += shift.y;
            viewportChanged = true;
        }

        float gridWidthScaled  = kGridSize * displaySettings.gridSpacing * panelScale;
        float gridHeightScaled = kGridSize * displaySettings.gridSpacing * panelScale;

        if (!viewportInitialized) {
            // Start at center of grid
            panelOffset.x = (windowSize.x - gridWidthScaled) / 2.0f / panelScale;
            panelOffset.y = (windowSize.y - gridHeightScaled) / 2.0f / panelScale;
            scrollTarget = panelOffset;
        }

        // Keep the target within grid boundaries
        float minOffsetX = (-gridWidthScaled + windowSize.x) / panelScale;
        float maxOffsetX = 0.f;
        float minOffsetY = (-gridHeightScaled + windowSize.y - displayPresets.numberWindowBufferTop) / panelScale;
        float maxOffsetY = displayPresets.numberWindowBufferTop / panelScale;
        scrollTarget.x = clampOffset(scrollTarget.x, minOffsetX, maxOffsetX);
        scrollTarget.y = clampOffset(scrollTarget.y, minOffsetY, maxOffsetY);

        // Ease toward the target, snapping once within a fraction of a pixel
        ImVec2 previousOffset = panelOffset;
        float ease = 1.f - std::exp(-kScrollSmoothing * dt);
        panelOffset.x += (scrollTarget.x - panelOffset.x) * ease;
        panelOffset.y += (scrollTarget.y - panelOffset.y) * ease;
        if (std::abs(scrollTarget.x - panelOffset.x) * panelScale < 0.1f && std::abs(scrollTarget.y - panelOffset.y) * panelScale < 0.1f) {
            panelOffset = scrollTarget;
        }
        viewportChanged |= panelOffset.x != previousOffset.x || panelOffset.y != previousOffset.y;

        viewportInitialized = true;
        return viewportChanged;
    }

    bool updateDisplaySettings(PresetDisplaySettings &settings, float globalScale)
    {
        // Want preset display settings to scale proportionally to the window size
        auto viewportSize = ImGui::GetMainViewport()->Size;
        if (lastViewportSize.x == viewportSize.x && lastViewportSize.y == viewportSize.y && lastGlobalScale == globalScale) {
            return false;
        }

        float displaySizeScalePrev = lastViewportSize.x/1280.f * lastGlobalScale;
        float displaySizeScale = viewportSize.x/1280.f * globalScale;
        float newScale = displaySizeScale / displaySizeScalePrev;
        settings.numberWindowBufferTop *= newScale;
        settings.numberWindowBufferBottom *= newScale;
        settings.headerImageScale *= newScale;
        settings.headerImageOffsetX *= newScale;
        settings.headerBoxBufferX *= newScale;
        settings.headerBoxBufferY *= newScale;
        settings.binImageScale *= newScale;
        settings.binOffset *= newScale;
        settings.binPercentBarOffset *= newScale;
        settings.fontSize *= newScale;
        settings.settingsFontScale *= newScale;
        settings.lineGraphicsSpacing *= newScale;
        settings.lineThickness *= newScale;

        lastViewportSize = viewportSize;
        lastGlobalScale = globalScale;

        return true;
    }

    // Helpers
    bool allGroupsRefined() const
    {
        return std::all_of(activeFile->bins.begin(), activeFile->bins.end(), [](const Bin &b) { return b.badGroupsRefined >= b.maxBadGroups; });
    }

    float zoomCueFactor() const
    {
        return panelScale / kCueReferenceZoom;
    }

    // Refined bad groups across all bins, rounded down so 100% only shows once everything is refined
    int getCompletionPercent() const
    {
        int refined = 0;
        int total = 0;
        for (const auto &b : activeFile->bins) {
            refined += b.badGroupsRefined;
            total += b.maxBadGroups;
        }
        return total > 0 ? refined * 100 / total : 0;
    }

    float getScaleFromCursor(const ImVec2& globalNumberPos, const ImVec2& mousePos) const
    {
        float distX = mousePos.x - globalNumberPos.x;
        float distY = mousePos.y - globalNumberPos.y;
        float distance = sqrt(distX * distX + distY * distY);

        if (distance < displaySettings.mouseScaleRadius) {
            return 1.0f + (displaySettings.mouseScaleRadius - distance) / displaySettings.mouseScaleRadius * displaySettings.mouseScaleMultiplier;
        }
        return 1.0f;
    }

    std::shared_ptr<ImageDisplay> imageDisplay;

    ImFont* font;

    ImVec2 panelOffset = ImVec2(0,0);
    ImVec2 scrollTarget = ImVec2(0,0);
    float panelScale = 0.3f;
    float zoomTarget = 0.3f;
    ImVec2 zoomAnchor = ImVec2(0,0); // Window-relative point kept fixed while zooming
    bool viewportInitialized = false;

    std::string settingsSavePath = "./settings.json";
    DisplaySettings displaySettings;
    ControlSettings controlSettings;

    PresetDisplaySettings displayPresets;

    siv::PerlinNoise perlin{ 555 };
    double noiseTime = 0.0;

    // Bin (0-based) a refined number is currently flying into, which shows its lid open
    std::optional<int> binReceiving;

    // Debug options
    bool revealMap = false;

    // Each file keeps its own grid and bin progress, created the first time it's opened
    struct FileProgress
    {
        std::shared_ptr<NumberGrid> numberGrid;
        std::array<Bin, 5> bins = {Bin{1}, Bin{2}, Bin{3}, Bin{4}, Bin{5}};
    };
    std::map<std::string, FileProgress> files;
    FileProgress* activeFile = nullptr;
    bool fileChanged = false;

    FileCompletion fileCompletion = FileCompletion::InProgress;
    float completionTimer = 0.f;

    ImVec2 lastViewportSize = ImVec2(1280, 720);
    float lastGlobalScale = 1.f;

    std::string fileName;

    std::mt19937 rng{std::random_device{}()};
};

std::shared_ptr<NumbersPanel> createNumbersPanel(const std::shared_ptr<ImageDisplay>& imageDisplay)
{
    return std::make_shared<NumbersPanelImpl>(imageDisplay);
}
