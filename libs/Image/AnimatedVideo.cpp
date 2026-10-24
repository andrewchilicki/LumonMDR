#include "AnimatedVideo.h"

#include <GL/glew.h>
#include <condition_variable>
#include <deque>
#include <iostream>
#include <mutex>
#include <thread>
#include <utility>
#include <vector>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
}

namespace
{
    // Frames decoded ahead of playback. Each is width*height*4 bytes, so this caps memory use
    constexpr size_t kQueuedFrames = 8;
}

// A whole video is too big to keep decoded, so frames are decoded just ahead of playback. When
// looping, the decoder rewinds to the start on reaching the end so the loop is seamless
class AnimatedVideoImpl : public AnimatedVideo {
public:
    AnimatedVideoImpl(std::string filePath, bool loop) : filePath(std::move(filePath)), loop(loop)
    {
        decoder = std::thread([this] { decode(); });
    }

    ~AnimatedVideoImpl() override
    {
        {
            std::lock_guard<std::mutex> lock(mutex);
            stopping = true;
        }
        wake.notify_all();
        decoder.join();
        if (texture != 0) {
            glDeleteTextures(1, &texture);
        }
    }

    bool isReady() final
    {
        if (texture != 0) {
            return true;
        }
        std::lock_guard<std::mutex> lock(mutex);
        if (queue.empty()) {
            return false;
        }

        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, queue.front().pixels.data());
        return true;
    }

    void update(float dt) final
    {
        if (texture == 0) {
            return;
        }
        playTime += dt;

        std::unique_lock<std::mutex> lock(mutex);
        // Drop frames whose successor is already due. If decoding falls behind, the newest frame holds
        bool dropped = false;
        while (queue.size() >= 2 && queue[1].time <= playTime) {
            queue.pop_front();
            dropped = true;
        }
        if (dropped) {
            wake.notify_all();
        }
        if (!queue.empty() && queue.front().time <= playTime && queue.front().sequence != shownSequence) {
            shownSequence = queue.front().sequence;
            playedSinceRewind = true;
            glBindTexture(GL_TEXTURE_2D, texture);
            glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, queue.front().pixels.data());
        }
    }

    void restart() final
    {
        if (texture == 0) {
            return;
        }
        playTime = 0.f;
        shownSequence = -1;

        // The decoder already sits at the start until anything has played
        std::lock_guard<std::mutex> lock(mutex);
        if (playedSinceRewind) {
            queue.clear();
            rewindRequested = true;
            generation++;
            playedSinceRewind = false;
            wake.notify_all();
        }
    }

    bool hasEnded() const final
    {
        std::lock_guard<std::mutex> lock(mutex);
        return endTime >= 0.0 && playTime >= endTime;
    }

    ImTextureID getTexture() const final { return (ImTextureID)(intptr_t)texture; }
    int getWidth() const final { return width; }
    int getHeight() const final { return height; }
    float getDuration() const final { return duration; }

private:
    struct Frame
    {
        double time;          // Seconds from the start of playback, counting loops
        long long sequence;   // Distinguishes frames, so each is uploaded once
        std::vector<uint8_t> pixels;
    };

    // Runs on the decoder thread
    void decode()
    {
        AVFormatContext* format = nullptr;
        AVCodecContext* codec = nullptr;
        SwsContext* scaler = nullptr;
        AVPacket* packet = av_packet_alloc();
        AVFrame* frame = av_frame_alloc();

        if (open(format, codec)) {
            AVStream* stream = format->streams[streamIndex];
            double timeBase = av_q2d(stream->time_base);
            int64_t startPts = stream->start_time != AV_NOPTS_VALUE ? stream->start_time : 0;
            AVRational rate = stream->avg_frame_rate.num > 0 ? stream->avg_frame_rate : AVRational{30, 1};
            double frameDuration = av_q2d(av_inv_q(rate));

            double loopOffset = 0.0;     // Added to this pass's timestamps, so loops follow on
            double lastFrameTime = -1.0; // Within this pass
            long long sequence = 0;
            bool draining = false;
            bool atEnd = false;          // Played through without looping; waits for a restart

            auto rewind = [&] {
                av_seek_frame(format, streamIndex, startPts, AVSEEK_FLAG_BACKWARD);
                avcodec_flush_buffers(codec);
                draining = false;
                lastFrameTime = -1.0;
            };

            while (true) {
                long long frameGeneration;
                {
                    std::unique_lock<std::mutex> lock(mutex);
                    wake.wait(lock, [&] { return stopping || rewindRequested || (!atEnd && queue.size() < kQueuedFrames); });
                    if (stopping) {
                        break;
                    }
                    if (rewindRequested) {
                        rewindRequested = false;
                        atEnd = false;
                        loopOffset = 0.0;
                        lock.unlock();
                        rewind();
                        lock.lock();
                    }
                    frameGeneration = generation;
                }

                int result = avcodec_receive_frame(codec, frame);
                if (result == AVERROR(EAGAIN)) {
                    // Needs more input: feed the next video packet, or flush once the file runs out
                    if (draining) {
                        continue;
                    }
                    while ((result = av_read_frame(format, packet)) >= 0 && packet->stream_index != streamIndex) {
                        av_packet_unref(packet);
                    }
                    if (result < 0) {
                        avcodec_send_packet(codec, nullptr);
                        draining = true;
                    } else {
                        avcodec_send_packet(codec, packet);
                        av_packet_unref(packet);
                    }
                    continue;
                }
                if (result == AVERROR_EOF) {
                    // End of the video: loop back to the start
                    if (lastFrameTime < 0.0) {
                        std::cerr << "Video " << filePath << " has no frames" << std::endl;
                        break;
                    }
                    if (!loop) {
                        std::lock_guard<std::mutex> lock(mutex);
                        endTime = loopOffset + lastFrameTime + frameDuration;
                        atEnd = true;
                        continue;
                    }
                    loopOffset += lastFrameTime + frameDuration;
                    rewind();
                    continue;
                }
                if (result < 0) {
                    std::cerr << "Failed to decode video " << filePath << std::endl;
                    break;
                }

                double passTime = frame->best_effort_timestamp != AV_NOPTS_VALUE
                    ? (frame->best_effort_timestamp - startPts) * timeBase
                    : lastFrameTime + frameDuration;
                lastFrameTime = passTime;

                scaler = sws_getCachedContext(scaler, frame->width, frame->height, static_cast<AVPixelFormat>(frame->format),
                                              width, height, AV_PIX_FMT_RGBA, SWS_BILINEAR, nullptr, nullptr, nullptr);
                setColorConversion(scaler, *frame);
                Frame decoded{loopOffset + passTime, sequence++, std::vector<uint8_t>(static_cast<size_t>(width) * height * 4)};
                uint8_t* destination[1] = {decoded.pixels.data()};
                int destinationStride[1] = {width * 4};
                sws_scale(scaler, frame->data, frame->linesize, 0, frame->height, destination, destinationStride);
                av_frame_unref(frame);

                std::lock_guard<std::mutex> lock(mutex);
                // Drop a frame decoded across a restart; it belongs to the old playback
                if (generation == frameGeneration && !rewindRequested) {
                    queue.push_back(std::move(decoded));
                }
            }
        }

        sws_freeContext(scaler);
        av_frame_free(&frame);
        av_packet_free(&packet);
        avcodec_free_context(&codec);
        avformat_close_input(&format);
    }

    // swscale otherwise assumes SD (BT.601) colours, which shifts the hues of HD (BT.709) video.
    // Uses the frame's tagged colour space and range, guessing from its height when untagged
    static void setColorConversion(SwsContext* scaler, const AVFrame& frame)
    {
        int colorSpace = frame.colorspace;
        if (colorSpace == AVCOL_SPC_UNSPECIFIED) {
            colorSpace = frame.height >= 720 ? AVCOL_SPC_BT709 : AVCOL_SPC_SMPTE170M;
        }
        bool fullRange = frame.color_range == AVCOL_RANGE_JPEG;
        // AVColorSpace values double as SWS_CS_* ids
        const int* coefficients = sws_getCoefficients(colorSpace);
        sws_setColorspaceDetails(scaler, coefficients, fullRange, sws_getCoefficients(SWS_CS_DEFAULT), 1, 0, 1 << 16, 1 << 16);
    }

    // Opens the file and its video decoder, setting width/height/streamIndex
    bool open(AVFormatContext*& format, AVCodecContext*& codec)
    {
        if (avformat_open_input(&format, filePath.c_str(), nullptr, nullptr) < 0) {
            std::cerr << "Failed to open video " << filePath << std::endl;
            return false;
        }
        if (avformat_find_stream_info(format, nullptr) < 0) {
            std::cerr << "Failed to read video " << filePath << std::endl;
            return false;
        }
        const AVCodec* decoderCodec = nullptr;
        streamIndex = av_find_best_stream(format, AVMEDIA_TYPE_VIDEO, -1, -1, &decoderCodec, 0);
        if (streamIndex < 0 || decoderCodec == nullptr) {
            std::cerr << "No playable video stream in " << filePath << std::endl;
            return false;
        }
        codec = avcodec_alloc_context3(decoderCodec);
        avcodec_parameters_to_context(codec, format->streams[streamIndex]->codecpar);
        codec->thread_count = 0; // Let FFmpeg pick, so decoding keeps up on slower machines
        if (avcodec_open2(codec, decoderCodec, nullptr) < 0) {
            std::cerr << "Failed to open the decoder for " << filePath << std::endl;
            return false;
        }

        // Read by the main thread only once a frame is queued, which happens after this under the mutex
        width = codec->width;
        height = codec->height;
        AVStream* stream = format->streams[streamIndex];
        if (stream->duration != AV_NOPTS_VALUE) {
            duration = static_cast<float>(stream->duration * av_q2d(stream->time_base));
        } else if (format->duration != AV_NOPTS_VALUE) {
            duration = static_cast<float>(format->duration) / AV_TIME_BASE;
        }
        std::cout << "Opened video " << filePath << " (" << width << "x" << height << ")" << std::endl;
        return true;
    }

    std::string filePath;
    bool loop;
    std::thread decoder;
    int streamIndex = -1;
    int width = 0, height = 0;
    float duration = 0.f;

    // Shared with the decoder thread
    mutable std::mutex mutex;
    std::condition_variable wake;
    std::deque<Frame> queue;
    double endTime = -1.0; // Once known, when a non-looping video's last frame finishes
    bool stopping = false;
    bool rewindRequested = false;
    long long generation = 0;

    // Main thread only
    GLuint texture = 0;
    float playTime = 0.f;
    long long shownSequence = -1;
    bool playedSinceRewind = false;
};

std::shared_ptr<AnimatedVideo> loadAnimatedVideo(const std::string& filePath, bool loop)
{
    return std::make_shared<AnimatedVideoImpl>(filePath, loop);
}
