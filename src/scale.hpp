#pragma once

#include "compiler.hpp"

#include <cstdint>
#include <expected>
#include <optional>
#include <string>

namespace latch {

struct ScalePlan {
    int buffer_width = 0;
    int buffer_height = 0;
    int destination_width = 0;
    int destination_height = 0;
    // wl_surface.set_buffer_scale. Fractional scale keeps this at 1.
    int buffer_scale = 1;
    bool fractional = false;
};

// surface size is the configure size in logical pixels. fractional_120ths is
// the wp_fractional_scale_v1 numerator (denominator 120). When it is absent,
// integer_scale is the wl_output scale and must be at least 1.
std::expected<ScalePlan, std::string> plan_scale(int surface_width,
                                                 int surface_height,
                                                 std::optional<uint32_t> fractional_120ths,
                                                 int integer_scale);

} // namespace latch
