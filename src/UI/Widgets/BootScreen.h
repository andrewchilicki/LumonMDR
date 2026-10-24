#pragma once
#include <memory>

class ImageDisplay;

class BootScreen {
public:
    virtual void update() = 0;
    virtual void drawBootScreen() = 0;

    virtual bool isComplete() const = 0;

    // Plays just the logo animation again, completing once it fades out
    virtual void replayLogo() = 0;

    virtual ~BootScreen() = default;
};

std::shared_ptr<BootScreen> createBootScreen(const std::shared_ptr<ImageDisplay>& imageDisplay);
