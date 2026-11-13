#pragma once

#include "../../RoboXML.hpp"
#include "../../../RoboShared/RoboTypes.hpp"

#include <string>
#include <vector>

/* Stage C10. Audio is a decoded sound and its clock. It has no box. */

constexpr int ROBO_AUDIO_MAX_VOLUME = 100;
constexpr int ROBO_AUDIO_NSF_SONG_MS = 90000;
constexpr int ROBO_AUDIO_SPC_MS = 180000;

struct RoboAudioNode {
    std::string source;
    int time = 0;
    bool playing = false;
    int volume = ROBO_AUDIO_MAX_VOLUME;
};

struct RoboAudioClip {
    std::string format;
    int sampleRate = 0;
    int channels = 0;
    int duration = 0;
    int time = 0;
    int volume = ROBO_AUDIO_MAX_VOLUME;
    bool playing = false;
    bool live = false;
    bool pcm = false;
    int songs = 0;
    std::vector<std::string> entries;
    std::vector<RoboI16> samples;
};

struct RoboAudioOutput {
    std::string format;
    int sampleRate = 0;
    int channels = 0;
    int time = 0;
    int volume = ROBO_AUDIO_MAX_VOLUME;
    std::vector<RoboI16> samples;
};

/* Accepts source, time, playing, and volume. An unknown attribute or a bad value writes a diagnostic and returns false. */
bool RoboAudioRead(const RoboXMLNode& node, RoboAudioNode& audio, std::string& diagnostic);

/* The format named by a file name or extension. Empty when the name is not a supported audio format. */
const char* RoboAudioFormatOf(const std::string& source);

/* Opens every supported format. PCM containers fill samples. Other formats report duration from their header. */
bool RoboAudioDecode(RoboAudioClip& clip, const void* bytes, std::size_t size, const std::string& source);

/* Moves the clock. A live stream stays at 0. A seek past the end stops on the last millisecond. */
bool RoboAudioSeek(RoboAudioClip& clip, int timeMs);

/* Copies the node's clock onto a decoded clip. */
void RoboAudioBind(const RoboAudioNode& audio, RoboAudioClip& clip);

/* Plays from the current time through the host output. Volume scales the samples. */
bool RoboAudioPlay(const RoboAudioClip& clip, RoboAudioOutput& output);
