#pragma once

#include <cstdio>
#include <imgui.h>
#include <iostream>

// The UI typeface, used everywhere. If it doesn't load, the caller falls back to ImGui's built-in
// font.
// Rajdhani is by the Indian Type Foundry under the SIL Open Font License 1.1; its license must stay
// alongside it (assets/Rajdhani-OFL.txt) wherever the font is distributed
namespace Fonts
{
    inline const char* kUiFontPath = "./assets/Rajdhani-Regular.ttf";

    // Size the default UI font is loaded at. Most screens draw it at a fraction of this (e.g. the
    // boot text and file picker at 0.5x), so raising it enlarges all of their text together
    constexpr float kUiFontSize = 58.f;

    // Adds the UI font at `size` px to the atlas; nullptr if it doesn't load
    inline ImFont* loadUiFont(float size)
    {
        // ImGui asserts on a missing file, so check it exists first
        if (FILE* file = std::fopen(kUiFontPath, "rb")) {
            std::fclose(file);
            if (ImFont* font = ImGui::GetIO().Fonts->AddFontFromFileTTF(kUiFontPath, size)) {
                return font;
            }
        }
        std::cerr << "Couldn't load font " << kUiFontPath << std::endl;
        return nullptr;
    }
}
