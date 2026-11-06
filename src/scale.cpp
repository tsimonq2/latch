#include "scale.hpp"

namespace latch {
namespace {

int ceil_div(int value, int numerator, int denominator)
{
    return static_cast<int>((static_cast<long long>(value) * numerator + (denominator - 1)) / denominator);
}

} // namespace

std::expected<ScalePlan, std::string> plan_scale(int surface_width,
                                                 int surface_height,
                                                 std::optional<uint32_t> fractional_120ths,
                                                 int integer_scale)
{
    if (surface_width <= 0 || surface_height <= 0) {
        return std::unexpected("surface size must be positive");
    }
    ScalePlan plan;
    plan.destination_width = surface_width;
    plan.destination_height = surface_height;
    if (fractional_120ths) {
        if (*fractional_120ths == 0) {
            return std::unexpected("fractional scale must not be 0");
        }
        plan.fractional = true;
        plan.buffer_scale = 1;
        plan.buffer_width = ceil_div(surface_width, static_cast<int>(*fractional_120ths), 120);
        plan.buffer_height = ceil_div(surface_height, static_cast<int>(*fractional_120ths), 120);
        return plan;
    }
    if (integer_scale <= 0) {
        return std::unexpected("integer scale must be at least 1");
    }
    plan.buffer_scale = integer_scale;
    plan.buffer_width = surface_width * integer_scale;
    plan.buffer_height = surface_height * integer_scale;
    return plan;
}

} // namespace latch
