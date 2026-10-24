#pragma once
#include <string>
#include <vector>

struct FileInfo
{
    std::string name;
    int presetPercent = 0;  // Completion (0-100) the file starts at the first time it's opened
    unsigned int seed = 0;  // Grid layout seed; 0 uses a default based on the file's position in kFiles
};

// Files selectable from the picker, in wheel order: {name, preset %, layout seed}.
// Presets refine whole groups, so each seed is chosen so its layout's group count lands exactly on
// the preset % (noted as groups left / total). If you change a %, NumbersPanel finds a layout that
// fits at startup and logs the seed to put here instead
inline const std::vector<FileInfo> kFiles = {
    {"Cold Harbor", 0, 506},    // 28/28
    {"Allentown", 100, 507},    // 0/36
    {"Dranesville", 100, 508},  // 0/29
    {"Jesup", 100, 509},        // 0/25
    {"Kingsport", 35, 5510},    // 20/31
    {"Labrador", 50, 511},      // 14/28
    {"Le Mars", 100, 512},      // 0/34
    {"Siena", 80, 1008},        // 7/35
    {"Tumwater", 100, 514},     // 0/23
    {"Wellington", 65, 515},    // 10/29
    {"Bellingham", 100, 516},   // 0/28
    {"Cairns", 20, 1517},       // 20/25
    {"Coleman", 100, 518},      // 0/24
    {"Culpepper", 0, 519},      // 26/26
    {"Longbranch", 90, 520},    // 3/33
    {"Minsk", 45, 521},         // 22/40
    {"Lexington", 100, 522}     // 0/29
};
