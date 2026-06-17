#pragma once

#include <unordered_map>
#include <string>

#include "raylib.h"

namespace arx {

// The engine's audio hub (rad2d deliberately stays out of sound). Owns short SFX
// (fully loaded into memory) and streamed Music (decoded on the fly), each keyed by
// your own int ids. The Game owns one of these, inits/closes the audio device with
// the window, and calls update() every frame so the active music stream is fed.
class Audio {
private:
    std::unordered_map<int, Sound> sounds;
    std::unordered_map<int, Music> music;
    int   currentMusic = -1;     // id of the stream being updated, or -1
    float musicVolume = 1.0f;
public:
    // --- sound effects ---
    void loadSound(int id, const std::string& path);
    void playSound(int id, float volume = 1.0f, float pitch = 1.0f);

    // --- music (one track plays at a time) ---
    void loadMusic(int id, const std::string& path);
    void playMusic(int id, bool loop = true);
    void stopMusic();
    void setMusicVolume(float v);

    void update();      // feed the active music stream (call once per frame)
    void unloadAll();   // free every loaded sound/music (call before closing the device)
};

} // namespace arx
