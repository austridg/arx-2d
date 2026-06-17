#pragma once

#include <string>

#include "raylib.h"
#include "entity/vec2.h"

namespace arx {

// rad2d::Text positions by its top-left and exposes no width, so centering needs a
// measurement. These wrap raylib's measuring with the same font/size/spacing you gave
// the Text. Pass the dereferenced font from your FontRegistry: measureText(*fonts.get(id), ...).
inline Vec2 measureText(const Font& font, const std::string& s, float fontSize, float spacing) {
    Vector2 m = MeasureTextEx(font, s.c_str(), fontSize, spacing);
    return { m.x, m.y };
}

// the top-left X/Y that centers text of the given size around a point
inline float centeredX(float centerX, float textWidth)  { return centerX - textWidth * 0.5f; }
inline float centeredY(float centerY, float textHeight) { return centerY - textHeight * 0.5f; }

} // namespace arx
