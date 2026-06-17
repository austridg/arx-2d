#include "blackboard.h"

#include <fstream>

namespace arx {

void Blackboard::setInt(const std::string& k, int v)              { data[k] = std::to_string(v); }
void Blackboard::setFloat(const std::string& k, float v)          { data[k] = std::to_string(v); }
void Blackboard::setBool(const std::string& k, bool v)            { data[k] = v ? "1" : "0"; }
void Blackboard::setString(const std::string& k, const std::string& v) { data[k] = v; }

int Blackboard::getInt(const std::string& k, int def) const {
    auto it = data.find(k);
    if (it == data.end()) { return def; }
    try { return std::stoi(it->second); } catch (...) { return def; }
}

float Blackboard::getFloat(const std::string& k, float def) const {
    auto it = data.find(k);
    if (it == data.end()) { return def; }
    try { return std::stof(it->second); } catch (...) { return def; }
}

bool Blackboard::getBool(const std::string& k, bool def) const {
    auto it = data.find(k);
    if (it == data.end()) { return def; }
    return it->second == "1" || it->second == "true";
}

std::string Blackboard::getString(const std::string& k, const std::string& def) const {
    auto it = data.find(k);
    return it == data.end() ? def : it->second;
}

bool Blackboard::has(const std::string& k) const { return data.find(k) != data.end(); }
void Blackboard::remove(const std::string& k)    { data.erase(k); }
void Blackboard::clear()                         { data.clear(); }

bool Blackboard::save(const std::string& path) const {
    std::ofstream f(path);
    if (!f) { return false; }
    for (const auto& [k, v] : data) { f << k << '=' << v << '\n'; }
    return true;
}

bool Blackboard::load(const std::string& path) {
    std::ifstream f(path);
    if (!f) { return false; }
    data.clear();
    std::string line;
    while (std::getline(f, line)) {
        auto eq = line.find('=');
        if (eq == std::string::npos) { continue; }
        data[line.substr(0, eq)] = line.substr(eq + 1);
    }
    return true;
}

} // namespace arx
