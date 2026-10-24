#pragma once
#include <string>
#include <vector>

struct FileInfo
{
    std::string name;
    int presetPercent = 0; // Completion (0-100) the file starts at the first time it's opened
};

// Files selectable from the picker, in wheel order
inline const std::vector<FileInfo> kFiles = {
    {"Cold Harbor", 0},
    {"Allentown", 0},
    {"Dranesville", 0},
    {"Jesup", 0},
    {"Kingsport", 0},
    {"Labrador", 0},
    {"Le Mars", 0},
    {"Siena", 0},
    {"Tumwater", 0},
    {"Wellington", 0},
    {"Bellingham", 0},
    {"Cairns", 0},
    {"Coleman", 0},
    {"Culpepper", 0},
    {"Longbranch", 0},
    {"Minsk", 0},
    {"Lexington", 0}
};
