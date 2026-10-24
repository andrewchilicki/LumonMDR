#pragma once
#include <memory>
#include <string>

class FileSelectScreen {
public:
    virtual void update() = 0;
    virtual void drawFileSelectScreen() = 0;

    virtual bool isComplete() const = 0;
    virtual const std::string& getSelectedFile() const = 0;

    virtual void reset() = 0;

    virtual ~FileSelectScreen() = default;
};

std::shared_ptr<FileSelectScreen> createFileSelectScreen();
