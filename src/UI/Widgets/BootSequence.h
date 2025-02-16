#pragma once
#include <memory>

class ImageDisplay;

class BootSequence
{
public:
    virtual void update() = 0;
    virtual void drawBootSequence() = 0;
    virtual void init() = 0;

    virtual ~BootSequence() = default;

    enum class BootState
    {
        Text,
        Logo,
        FolderName,
        EndState,
    };

    virtual BootState getState() const = 0;
};

std::shared_ptr<BootSequence> createBootSequence(const std::shared_ptr<ImageDisplay> &imageDisplay);