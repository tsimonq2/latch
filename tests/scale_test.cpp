#include "check.hpp"
#include "scale.hpp"

#include <iostream>

int main()
{
    const auto fractional = latch::plan_scale(100, 50, 180, 1);
    CHECK(fractional.has_value());
    CHECK(fractional->buffer_width == 150);
    CHECK(fractional->buffer_height == 75);
    CHECK(fractional->destination_width == 100);
    CHECK(fractional->destination_height == 50);
    CHECK(fractional->buffer_scale == 1);

    const auto integer = latch::plan_scale(80, 40, std::nullopt, 2);
    CHECK(integer.has_value());
    CHECK(integer->buffer_width == 160);
    CHECK(integer->buffer_height == 80);
    CHECK(integer->buffer_scale == 2);

    CHECK(!latch::plan_scale(10, 10, 0, 1).has_value());
    CHECK(!latch::plan_scale(10, 10, std::nullopt, 0).has_value());

    if (latch_failures != 0) {
        std::cerr << latch_failures << " checks failed\n";
        return 1;
    }
    return 0;
}
