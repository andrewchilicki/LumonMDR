#pragma once
#include <memory>
#include <string>

class ImageDisplay;

enum class FileCompletion
{
    InProgress,
    JustCompleted,  // Finished during this visit to the file
    AlreadyComplete // Was already finished when opened
};

class NumbersPanel {
public:
    virtual void init() = 0;
    virtual void update() = 0;

    virtual void drawNumbersPanel() = 0;
    virtual void drawSettings() = 0;

    virtual void triggerLoadAnimation() = 0;

    // Switches to that file's grid and progress (creating it on first open) and shows its
    // name in the header in place of the saved header text
    virtual void openFile(const std::string& fileName) = 0;

    virtual FileCompletion getFileCompletion() const = 0;

    // Shortcut for testing: instantly refines every remaining group in the open file, so it
    // completes as soon as any numbers already flying land
    virtual void completeFile() = 0;

    virtual ~NumbersPanel() = default;
};

std::shared_ptr<NumbersPanel> createNumbersPanel(const std::shared_ptr<ImageDisplay>& imageDisplay);