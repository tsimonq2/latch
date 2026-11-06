#include "contrast.hpp"

#include <cmath>
#include <cstdlib>

namespace latch {
namespace {

int hex_nibble(char c)
{
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }
    return -1;
}

double channel(int value)
{
    const double s = static_cast<double>(value) / 255.0;
    if (s <= 0.04045) {
        return s / 12.92;
    }
    return std::pow((s + 0.055) / 1.055, 2.4);
}

double luminance(Rgb color)
{
    return 0.2126 * channel(color.r) + 0.7152 * channel(color.g) + 0.0722 * channel(color.b);
}

} // namespace

std::expected<Rgb, std::string> parse_color(std::string_view text)
{
    if (text.size() != 7 || text[0] != '#') {
        return std::unexpected("color must be #rrggbb");
    }
    int bytes[3] = {};
    for (int i = 0; i < 3; ++i) {
        const int hi = hex_nibble(text[1 + i * 2]);
        const int lo = hex_nibble(text[2 + i * 2]);
        if (hi < 0 || lo < 0) {
            return std::unexpected("color must be #rrggbb");
        }
        bytes[i] = hi * 16 + lo;
    }
    return Rgb{bytes[0], bytes[1], bytes[2]};
}

double contrast_ratio(Rgb foreground, Rgb background)
{
    double lighter = luminance(foreground);
    double darker = luminance(background);
    if (lighter < darker) {
        const double swap = lighter;
        lighter = darker;
        darker = swap;
    }
    return (lighter + 0.05) / (darker + 0.05);
}

} // namespace latch
