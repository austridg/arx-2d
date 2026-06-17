#pragma once

#include <string>
#include <unordered_map>

namespace arx {

// A small key -> value store for global, non-entity state: score, settings, unlocks,
// save data. Values are kept as strings (human-readable on disk) with typed accessors
// that convert. The Game owns one; persist it with save()/load() to a plain text file.
class Blackboard {
private:
    std::unordered_map<std::string, std::string> data;
public:
    void setInt(const std::string& key, int v);
    void setFloat(const std::string& key, float v);
    void setBool(const std::string& key, bool v);
    void setString(const std::string& key, const std::string& v);

    int         getInt(const std::string& key, int def = 0) const;
    float       getFloat(const std::string& key, float def = 0.0f) const;
    bool        getBool(const std::string& key, bool def = false) const;
    std::string getString(const std::string& key, const std::string& def = "") const;

    bool has(const std::string& key) const;
    void remove(const std::string& key);
    void clear();

    bool save(const std::string& path) const;   // one "key=value" per line
    bool load(const std::string& path);          // replaces current contents
};

} // namespace arx
