#pragma once

#include "imgui.h"

#include <memory>
#include <string>

// A video (any format FFmpeg reads; audio is ignored) whose current frame is uploaded to a single
// texture whenever it changes. A background thread decodes a few frames ahead of playback
class AnimatedVideo {
public:
    // Whether the first frame has been decoded. Must be called from the GL thread, as the first
    // true result creates the texture. Other calls do nothing until this returns true
    virtual bool isReady() = 0;

    // Advances playback by `dt` seconds
    virtual void update(float dt) = 0;
    // Jumps back to the first frame
    virtual void restart() = 0;
    // Whether a non-looping video has played to its end, leaving its last frame showing
    virtual bool hasEnded() const = 0;

    virtual ImTextureID getTexture() const = 0;
    virtual int getWidth() const = 0;
    virtual int getHeight() const = 0;
    // Length in seconds as the file reports it, once ready; 0 if it doesn't say
    virtual float getDuration() const = 0;

    virtual ~AnimatedVideo() = default;
};

// Starts decoding in the background and returns immediately. Without `loop`, playback stops on the
// last frame
std::shared_ptr<AnimatedVideo> loadAnimatedVideo(const std::string& filePath, bool loop = true);
