#pragma once
#include <memory>

// Shown in place of the number grid once a file is complete
class FileCompleteScreen {
public:
    enum class Kind
    {
        FirstCompletion, // The file was just finished
        AlreadyComplete  // The file was opened already finished
    };

    // Loads the large popup font; call after the default font is added, before the first frame
    virtual void init() = 0;

    // Starts showing the screen for `kind`, restarting its animation. With `outieMessage`, a first
    // or repeat completion glitches out into the outie's lines (the Labrador easter egg), then returns to the logo
    virtual void show(Kind kind, bool outieMessage) = 0;

    virtual void update() = 0;
    virtual void drawFileCompleteScreen() = 0;

    // True while the screen is a popup over the number grid, so the grid should still be updated
    // and drawn underneath it
    virtual bool showsGridBehind() const = 0;

    // True once the completion animation has played out and the app should go back to the file
    // picker, including after the outie's message. Never true for the plain messages ("File
    // complete", or the fallback shown without the animation), which stay until dismissed
    virtual bool isFinished() const = 0;

    virtual ~FileCompleteScreen() = default;
};

std::shared_ptr<FileCompleteScreen> createFileCompleteScreen();
