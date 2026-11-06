#pragma once

#include <expected>
#include <string>
#include <string_view>

namespace latch {

struct Rgb {
    int r = 0;
    int g = 0;
    int b = 0;
};

std::expected<Rgb, std::string> parse_color(std::string_view text);

// WCAG 2.2 relative-luminance contrast. The lighter color is the numerator.
double contrast_ratio(Rgb foreground, Rgb background);

} // namespace latch
