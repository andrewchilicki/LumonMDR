#include "NumbersPanel.h"

#include "Numbers/NumberGrid.h"
#include "ImageDisplay.h"
#include "Settings.h"
#include "../UIManager.h"

#include <cmath>
#include <imgui.h>
#include <imgui_internal.h>
#include <iostream>
#include <random>
#include <utility>
#include <json.hpp>
#include "PerlinNoise.hpp"

class NumbersPanelImpl : public NumbersPanel
{
public:
    explicit NumbersPanelImpl(std::shared_ptr<ImageDisplay> imageDisplay) : imageDisplay(std::move(imageDisplay))
    {
        numberGrid = createNumberGrid(gridSize);

        // Update max bad groups for each bin
        auto badGroups = numberGrid->getBadGroups();
        for (auto &[id, group] : badGroups) {
            bins[group->binIdx].maxBadGroups++;
        }

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

    void update() final
    {
        numberGrid->update();
    }

    void drawNumbersPanel() final
    {
        bool displaySizeChanged = updateDisplaySettings(displayPresets, displaySettings.globalScale);

        ImVec2 mousePos = ImGui::GetIO().MousePos;
        ImVec2 windowSize = ImGui::GetWindowSize();
        ImVec2 windowPos = ImGui::GetWindowPos();

        ImDrawList* draw_list = ImGui::GetWindowDrawList();

        // Update viewport
        bool viewportChanged = updateViewport(windowSize);

        // Draw Overlays
        drawGraphicOverlays(windowPos, windowSize, draw_list);

        // Draw Grid
        auto numberRefiningToBin = drawNumbersGrid(windowPos, windowSize, mousePos, viewportChanged || displaySizeChanged);

        // Draw Bins
        drawBins(windowPos, windowSize, draw_list, numberRefiningToBin);

    }

    void triggerLoadAnimation() final
    {
        // Reset 'regenerate scale' on all numbers
        for (auto &[x, gridY] : numberGrid->getGrid()) {
            for (auto &[y, gridNumber] : gridY) {
                gridNumber->regenerateScale = 0.f;
            }
        }
    }

private:
    std::optional<int> drawNumbersGrid(const ImVec2& windowPos, const ImVec2& windowSize, const ImVec2& mousePos, bool updateDisplayInfos)
    {
        std::optional<int> refiningToBin = std::nullopt;
        for (auto &[x, gridY] : numberGrid->getGrid()) {
            for (auto &[y, gridNumber] : gridY) {

                std::string numberToDraw = "numbers/" + std::to_string(gridNumber->num) + ".png";
                auto [width, height] = imageDisplay->getImageSize(numberToDraw);
                double badScale = gridNumber->badGroup ? gridNumber->badGroup->scale : 0.0;

                if (updateDisplayInfos) {
                    // Only need to update when viewport has changed
                    ImVec2 localNumberPos = ImVec2((x * displaySettings.gridSpacing + panelOffset.x)*panelScale, (y * displaySettings.gridSpacing + panelOffset.y)*panelScale);
                    gridNumber->displayInfos.centerX = localNumberPos.x + windowPos.x;
                    gridNumber->displayInfos.centerY = localNumberPos.y + windowPos.y;

                    double baseNumberScale = displaySettings.imageScale*panelScale;
                    double widthOffset = (baseNumberScale*width/2.f);
                    double heightOffset = (baseNumberScale*height/2.f);
                    gridNumber->displayInfos.isVisible = gridNumber->displayInfos.centerX + widthOffset < windowPos.x + windowSize.x && gridNumber->displayInfos.centerX - widthOffset > windowPos.x &&
                                                            gridNumber->displayInfos.centerY + heightOffset < windowPos.y + windowSize.y - displayPresets.numberWindowBufferBottom && gridNumber->displayInfos.centerY - heightOffset > windowPos.y + displayPresets.numberWindowBufferTop;
                }

                // Don't draw numbers out of viewport
                if (!gridNumber->displayInfos.isVisible) {
                    continue;
                }

                auto centerPos = ImVec2(gridNumber->displayInfos.centerX, gridNumber->displayInfos.centerY);

                // Animate number on screen
                float numberAlpha = 255;
                if (gridNumber->regenerateScale < 1.f) {
                    gridNumber->regenerateScale += numberGrid->randomNumber(0,10)*0.001f;
                    numberAlpha = static_cast<int>(std::clamp(gridNumber->regenerateScale*2.f*255.f, 0.f, 255.f));
                }

                // Offset from noise scale
                double noiseScale = perlin.noise3D((x * displaySettings.noiseScale), (y * displaySettings.noiseScale), t*displaySettings.noiseSpeed);
                if (gridNumber->displayInfos.horizontalOffset) {
                    centerPos.x += noiseScale*displaySettings.noiseScaleOffset;
                } else {
                    centerPos.y += noiseScale*displaySettings.noiseScaleOffset;
                }

                // Colour
                auto col = ColorValues::lumonBlue.Value;
                col.w = numberAlpha;
                if (revealMap && gridNumber->badGroup) {
                    col = gridNumber->badGroup->isActive ? ImVec4(255,255,0,numberAlpha) : ImVec4(255,0,0,255);
                }

                // Scale from mouse hovering
                auto numberScale = getScaleFromCursor(centerPos, mousePos);

                // Handle if part of bad group
                if (gridNumber->badGroup) {
                    if (gridNumber->badGroup->isActive) {
                        // Make number 'super active'
                        if (numberScale > 1.0f) {
                            gridNumber->badGroup->superActive = true;
                        }
                        // Mark as refined on 'LEFT CLICK'
                        if (!gridNumber->badGroup->refined && numberScale >= (0.5f + displaySettings.mouseScaleMultiplier) && ImGui::IsKeyDown(ImGuiKey_MouseLeft)) {
                            gridNumber->badGroup->refined = true;
                            bins[gridNumber->badGroup->binIdx].badGroupsRefined++;
                        }
                    }

                    // Add jitter to 'super active' bad numbers
                    if (gridNumber->badGroup->superActive) {
                        centerPos.x += numberGrid->randomNumber(-10, 10)*badScale;
                        centerPos.y += numberGrid->randomNumber(-10, 10)*badScale;
                    }

                    // Animate position if number has been refined
                    if (gridNumber->badGroup->refined) {
                        float startX = gridNumber->displayInfos.centerX;
                        float startY = gridNumber->displayInfos.centerY;

                        if (gridNumber->displayInfos.refinedX == -1) {
                            gridNumber->displayInfos.refinedX = startX;
                            gridNumber->displayInfos.refinedY = startY;
                        }

                        auto binIdx = gridNumber->badGroup->binIdx;
                        if (binIdx > 4) {
                            std::cout << "Error: Bin index greater than expected. Setting to max." << std::endl;
                            binIdx = 4;
                        }

                        float distX = bins[binIdx].pos.x - gridNumber->displayInfos.refinedX;
                        float distY = bins[binIdx].pos.y - gridNumber->displayInfos.refinedY;
                        float distance = sqrt(distX * distX + distY * distY);

                        if (distance > displaySettings.refinedToBinSpeed) {
                            float dirX = distX / distance;
                            float dirY = distY / distance;
                            gridNumber->displayInfos.refinedX += dirX * displaySettings.refinedToBinSpeed;
                            gridNumber->displayInfos.refinedY += dirY * displaySettings.refinedToBinSpeed;
                            centerPos = ImVec2(gridNumber->displayInfos.refinedX, gridNumber->displayInfos.refinedY);

                            refiningToBin = gridNumber->badGroup->binIdx;
                        } else {
                            gridNumber->badGroup.reset(); // No longer a bad number
                            gridNumber->num = numberGrid->randomNumber(0,9);
                            gridNumber->regenerateScale = 0.f;
                        }
                    }
                }

                // Draw number
                float combinedScale = gridNumber->regenerateScale*displaySettings.imageScale*numberScale*panelScale + badScale;
                ImGui::SetCursorPos(ImVec2(centerPos.x - ImGui::GetWindowPos().x - ((width*combinedScale)/2.f), centerPos.y - ImGui::GetWindowPos().y - ((height*combinedScale)/2.f)));
                imageDisplay->drawImGuiImage(numberToDraw, combinedScale, col);
            }
        }
        t += 1;

        return refiningToBin;
    }

    void drawBins(const ImVec2& windowPos, const ImVec2& windowSize, ImDrawList* drawList, std::optional<int> numberRefiningToBin)
    {
        std::string binPercentPath = "bins/bin-percent.png";
        auto [widthP, heightP] = imageDisplay->getImageSize(binPercentPath);
        for (auto &b : bins) {
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

            double percentD = double(b.badGroupsRefined) / double(b.maxBadGroups);
            int percentInt = lround(percentD * 100.f);
            std::string percentString = std::to_string(percentInt) + "%";
            drawList->AddText(font, displayPresets.fontSize, ImVec2(trCorner.x + 5.f, (trCorner.y + brCorner.y)/2.f - displayPresets.fontSize/2.f), ColorValues::lumonBlue, percentString.c_str());

            drawList->AddRectFilled(trCorner, ImVec2(trCorner.x + ((brCorner.x - trCorner.x)* percentD), brCorner.y), ImColor(ColorValues::lumonBlue.Value.x, ColorValues::lumonBlue.Value.y, ColorValues::lumonBlue.Value.z, 0.3f));

            // Animate bin open
            if (numberRefiningToBin && *numberRefiningToBin == b.id - 1) {
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
        ImVec2 headerTextPos = ImVec2(headerBoxMin.x + 25.f, (headerBoxMin.y+headerBoxMax.y)/2.f - displayPresets.fontSize/2.f);
        drawList->AddText(font, displayPresets.fontSize, headerTextPos, ColorValues::lumonBlue, displaySettings.headerText.c_str());

        // Lumon logo
        std::string logoPath = "lumon-logo.png";
        auto [widthH, heightH] = imageDisplay->getImageSize(logoPath);
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

    bool updateViewport(const ImVec2& windowSize)
    {
        static bool viewportInit = false;
        bool viewportChanged = !viewportInit;
        // Handle arrow key input
        if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow)) {
            panelOffset.x += controlSettings.arrowSensitivity;
            viewportChanged = true;
        } else if (ImGui::IsKeyPressed(ImGuiKey_RightArrow)) {
            panelOffset.x -= controlSettings.arrowSensitivity;
            viewportChanged = true;
        }

        if (ImGui::IsKeyPressed(ImGuiKey_UpArrow)) {
            panelOffset.y += controlSettings.arrowSensitivity;
            viewportChanged = true;
        } else if (ImGui::IsKeyPressed(ImGuiKey_DownArrow)) {
            panelOffset.y -= controlSettings.arrowSensitivity;
            viewportChanged = true;
        }

        // Handle zoom
        if (ImGui::IsKeyPressed(ImGuiKey_Comma)) {
            panelScale -= controlSettings.zoomSensitivity;
            viewportChanged = true;
        } else if (ImGui::IsKeyPressed(ImGuiKey_Period)) {
            panelScale += controlSettings.zoomSensitivity;
            viewportChanged = true;
        }
        panelScale = std::clamp(panelScale, displaySettings.minZoomScale, displaySettings.maxZoomScale);

        if (viewportChanged) {
            // Clamp movement to within grid boundaries
            float gridWidthScaled  = gridSize * displaySettings.gridSpacing * panelScale;
            float gridHeightScaled = gridSize * displaySettings.gridSpacing * panelScale;

            float minOffsetX = -gridWidthScaled + windowSize.x;
            float maxOffsetX = 0;
            float minOffsetY = -gridHeightScaled + windowSize.y - displayPresets.numberWindowBufferTop;
            float maxOffsetY = displayPresets.numberWindowBufferTop;

            if (!viewportInit) {
                // Start at center of grid
                panelOffset.x = (windowSize.x - gridWidthScaled) / 2.0f / panelScale;
                panelOffset.y = (windowSize.y - gridHeightScaled) / 2.0f / panelScale;
            }

            panelOffset.x = std::clamp(panelOffset.x, minOffsetX / panelScale, maxOffsetX / panelScale);
            panelOffset.y = std::clamp(panelOffset.y, minOffsetY / panelScale, maxOffsetY / panelScale);
        }

        viewportInit = true;
        return viewportChanged;
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
        ImGui::InputText("Header Text", &displaySettings.headerText[0], displaySettings.headerText.capacity() + 1);
        ImGui::Text("Noise:");
        ImGui::InputFloat("Noise Speed", &displaySettings.noiseSpeed);
        ImGui::InputFloat("Noise Scale", &displaySettings.noiseScale);
        ImGui::InputFloat("Noise Scale Offset", &displaySettings.noiseScaleOffset);
        ImGui::Text("Controls:");
        ImGui::InputFloat("Arrow Sensitivity", &controlSettings.arrowSensitivity);
        ImGui::InputFloat("Zoom Sensitivity", &controlSettings.zoomSensitivity);
        ImGui::Separator();
        ImGui::Text("Debug:");
        ImGui::Checkbox("revealMap", &revealMap);
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

    int gridSize = 100;
    std::shared_ptr<ImageDisplay> imageDisplay;
    std::shared_ptr<NumberGrid> numberGrid;

    ImFont* font;

    ImVec2 panelOffset = ImVec2(0,0);
    float panelScale = 0.3f;

    std::string settingsSavePath = "./settings.json";
    DisplaySettings displaySettings;
    ControlSettings controlSettings;

    PresetDisplaySettings displayPresets;

    siv::PerlinNoise perlin{ 555 };
    int t = 0;

    // Debug options
    bool revealMap = false;

    // TODO - Move to class
    struct Bin
    {
        int id;
        ImVec2 pos = ImVec2(0, 0);

        int badGroupsRefined = 0;
        int maxBadGroups = 0;

        ImVec2 updatePos(const ImVec2 &windowSize, const ImVec2 &windowPos, float offsetY) {
            pos = ImVec2(windowPos.x + (windowSize.x / 6.f)*id, windowPos.y + windowSize.y - offsetY);
            return pos;
        }
    };
    std::array<Bin, 5> bins = {Bin{1}, Bin{2}, Bin{3}, Bin{4}, Bin{5}};

    ImVec2 lastViewportSize = ImVec2(1280, 720);
    float lastGlobalScale = 1.f;
};

std::shared_ptr<NumbersPanel> createNumbersPanel(const std::shared_ptr<ImageDisplay>& imageDisplay)
{
    return std::make_shared<NumbersPanelImpl>(imageDisplay);
}