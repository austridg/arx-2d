#include "audio.h"

#include <iostream>

namespace arx {

void Audio::loadSound(int id, const std::string& path) {
    Sound s = LoadSound(path.c_str());
    if (s.frameCount == 0) { std::cerr << "[ERROR] - Audio: failed to load sound " << path << std::endl; return; }
    sounds[id] = s;
}

void Audio::playSound(int id, float volume, float pitch) {
    auto it = sounds.find(id);
    if (it == sounds.end()) { std::cerr << "[ERROR] - Audio: no sound with id " << id << std::endl; return; }
    SetSoundVolume(it->second, volume);
    SetSoundPitch(it->second, pitch);
    PlaySound(it->second);
}

void Audio::loadMusic(int id, const std::string& path) {
    Music m = LoadMusicStream(path.c_str());
    if (m.frameCount == 0) { std::cerr << "[ERROR] - Audio: failed to load music " << path << std::endl; return; }
    music[id] = m;
}

void Audio::playMusic(int id, bool loop) {
    auto it = music.find(id);
    if (it == music.end()) { std::cerr << "[ERROR] - Audio: no music with id " << id << std::endl; return; }
    if (currentMusic >= 0 && currentMusic != id) { StopMusicStream(music[currentMusic]); }
    it->second.looping = loop;
    SetMusicVolume(it->second, musicVolume);
    PlayMusicStream(it->second);
    currentMusic = id;
}

void Audio::stopMusic() {
    if (currentMusic >= 0) { StopMusicStream(music[currentMusic]); currentMusic = -1; }
}

void Audio::setMusicVolume(float v) {
    musicVolume = v;
    if (currentMusic >= 0) { SetMusicVolume(music[currentMusic], v); }
}

void Audio::update() {
    if (currentMusic >= 0) { UpdateMusicStream(music[currentMusic]); }
}

void Audio::unloadAll() {
    for (auto& [id, s] : sounds) { UnloadSound(s); }
    for (auto& [id, m] : music)  { UnloadMusicStream(m); }
    sounds.clear();
    music.clear();
    currentMusic = -1;
}

} // namespace arx
