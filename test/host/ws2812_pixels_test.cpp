#include <cassert>
#include <cstdlib>
#include "ws2812_pixels.hpp"

int main() {
    using namespace ws2812_display;
    Pixels pixels;
    assert(sizeof(Pixels) == 768);
    assert(logical_width == 32 && logical_height == 8);

    assert(default_brightness == 2);
    assert(display_appearance::transferMode() ==
           display_appearance::TransferMode::off);
    assert(scaleChannel(173, 0) == 0);
    assert(scaleChannel(173, 255) == 173);
    assert(scaleChannel(255, 6) == 6);
    assert(scaleChannel(96, 6) == 2);
    assert(scaleChannel(1, 6) == 0);
    assert(scaleChannel(255, 127) <= 255);
    const Rgb amber{255, 96, 0};
    assert(pixels.set(0, amber));
    const Rgb preserved = pixels.at(0);
    const uint32_t amber_at_default = pixels.grbWord(0, default_brightness);
    assert(amber_at_default == ((1u << 24) | (2u << 16)));
    assert(pixels.grbWord(0, display_appearance::reference_amber_brightness) ==
           ((2u << 24) | (6u << 16)));
    assert(pixels.grbWord(0, default_brightness,
           display_appearance::TransferMode::off) == amber_at_default);
    assert(pixels.at(0).red == preserved.red && pixels.at(0).green == preserved.green &&
           pixels.at(0).blue == preserved.blue);

    BrightnessRefreshTracker refresh_tracker;
    assert(!refresh_tracker.consumeChange());
    setBrightness(16);
    assert(refresh_tracker.consumeChange()); // Same HH:MM still schedules a retransmit.
    const uint32_t after_change = brightnessGeneration();
    assert(brightness() == 16 && brightnessGeneration() == after_change);
    assert(!refresh_tracker.consumeChange());
    setBrightness(default_brightness);
    assert(brightnessGeneration() != after_change); // Driver observes this and dirties output.

    bool seen[pixel_count] = {};
    for (int y = 0; y < logical_height; ++y) {
        for (int x = 0; x < logical_width; ++x) {
            uint16_t raw_index = 0xffff;
            assert(logicalToRaw(x, y, raw_index));
            assert(raw_index < pixel_count);
            assert(!seen[raw_index]);
            seen[raw_index] = true;
        }
    }
    for (bool present : seen) assert(present);

    // The observed panel starts at top-left and snakes vertically by column.
    assert(panel_mapping.origin == Origin::top_left);
    assert(panel_mapping.serpentine_columns && panel_mapping.first_column_top_to_bottom);
    uint16_t corner_tl, corner_tr, corner_br, corner_bl;
    assert(logicalToRaw(0, 0, corner_tl) && corner_tl == 0);
    assert(logicalToRaw(31, 0, corner_tr) && corner_tr == 255);
    assert(logicalToRaw(31, 7, corner_br) && corner_br == 248);
    assert(logicalToRaw(0, 7, corner_bl) && corner_bl == 7);
    assert(corner_tl != corner_tr && corner_tl != corner_br &&
           corner_tl != corner_bl && corner_tr != corner_br &&
           corner_tr != corner_bl && corner_br != corner_bl);
    assert(logicalToRaw(0, 1, corner_tl) && corner_tl == 1);
    assert(logicalToRaw(1, 0, corner_tl) && corner_tl == 15);
    assert(logicalToRaw(1, 7, corner_tl) && corner_tl == 8);
    assert(logicalToRaw(2, 0, corner_tl) && corner_tl == 16);

    assert(perimeter_pixel_count == 76);
    bool perimeter_seen[logical_height][logical_width] = {};
    int first_x = -1;
    int first_y = -1;
    int previous_x = -1;
    int previous_y = -1;
    for (std::size_t step = 0; step < perimeter_pixel_count; ++step) {
        int x = -1;
        int y = -1;
        assert(perimeterStepToXY(step, x, y));
        assert(x >= 0 && x < logical_width && y >= 0 && y < logical_height);
        assert(x == 0 || x == logical_width - 1 ||
               y == 0 || y == logical_height - 1);
        assert(!perimeter_seen[y][x]);
        perimeter_seen[y][x] = true;
        if (step == 0) {
            first_x = x;
            first_y = y;
            assert(x == 0 && y == 0);
        }
        if (step == 31) assert(x == 31 && y == 0);
        if (step == 38) assert(x == 31 && y == 7);
        if (step == 69) assert(x == 0 && y == 7);
        if (step == 75) {
            assert(x == 0 && y == 1);
            assert(std::abs(x - first_x) + std::abs(y - first_y) == 1);
        }
        if (previous_x >= 0)
            assert(std::abs(x - previous_x) + std::abs(y - previous_y) == 1);
        previous_x = x;
        previous_y = y;
    }
    int invalid_x = 123;
    int invalid_y = 456;
    assert(!perimeterStepToXY(perimeter_pixel_count, invalid_x, invalid_y));
    assert(invalid_x == 123 && invalid_y == 456);
    assert(!perimeterStepToXY(perimeter_pixel_count + 1, invalid_x, invalid_y));
    assert(invalid_x == 123 && invalid_y == 456);

    const Rgb scan_color{255, 255, 255};
    uint8_t row_coverage[logical_height][logical_width] = {};
    for (int row = 0; row < logical_height; ++row) {
        assert(setLogicalRowFrame(pixels, row, scan_color));
        unsigned lit_pixels = 0;
        bool row_raw_seen[pixel_count] = {};
        for (int y = 0; y < logical_height; ++y) {
            for (int x = 0; x < logical_width; ++x) {
                uint16_t raw_index = 0xffff;
                assert(logicalToRaw(x, y, raw_index) && raw_index < pixel_count);
                const Rgb actual = pixels.at(raw_index);
                const bool lit = actual.red == 255 && actual.green == 255 &&
                                 actual.blue == 255;
                assert(lit == (y == row));
                if (y == row) {
                    assert(!row_raw_seen[raw_index]);
                    row_raw_seen[raw_index] = true;
                    ++row_coverage[y][x];
                    ++lit_pixels;
                }
            }
        }
        assert(lit_pixels == 32);
    }
    for (int y = 0; y < logical_height; ++y)
        for (int x = 0; x < logical_width; ++x)
            assert(row_coverage[y][x] == 1);
    assert(!setLogicalRowFrame(pixels, -1, scan_color));
    assert(!setLogicalRowFrame(pixels, logical_height, scan_color));

    uint8_t column_coverage[logical_height][logical_width] = {};
    for (int column = 0; column < logical_width; ++column) {
        assert(setLogicalColumnFrame(pixels, column, scan_color));
        unsigned lit_pixels = 0;
        bool column_raw_seen[pixel_count] = {};
        for (int y = 0; y < logical_height; ++y) {
            for (int x = 0; x < logical_width; ++x) {
                uint16_t raw_index = 0xffff;
                assert(logicalToRaw(x, y, raw_index) && raw_index < pixel_count);
                const Rgb actual = pixels.at(raw_index);
                const bool lit = actual.red == 255 && actual.green == 255 &&
                                 actual.blue == 255;
                assert(lit == (x == column));
                if (x == column) {
                    assert(!column_raw_seen[raw_index]);
                    column_raw_seen[raw_index] = true;
                    ++column_coverage[y][x];
                    ++lit_pixels;
                }
            }
        }
        assert(lit_pixels == 8);
    }
    for (int y = 0; y < logical_height; ++y)
        for (int x = 0; x < logical_width; ++x)
            assert(column_coverage[y][x] == 1);
    assert(!setLogicalColumnFrame(pixels, -1, scan_color));
    assert(!setLogicalColumnFrame(pixels, logical_width, scan_color));
    pixels.clear();

    uint16_t untouched_index = 0xa5a5;
    assert(!logicalToRaw(-1, 0, untouched_index) && untouched_index == 0xa5a5);
    assert(!logicalToRaw(32, 0, untouched_index) && untouched_index == 0xa5a5);
    assert(!logicalToRaw(0, -1, untouched_index) && untouched_index == 0xa5a5);
    assert(!logicalToRaw(0, 8, untouched_index) && untouched_index == 0xa5a5);

    for (std::size_t i = 0; i < pixel_count; ++i)
        assert(pixels.grbWord(i) == 0);

    assert(pixels.set(0, {255, 128, 64}));
    assert(pixels.grbWord(0) == 0x80ff4000u); // GRB bytes, MSB first.
    assert(pixels.grbWord(0, 8) == 0x04080200u);
    assert(pixels.at(0).red == 255); // Scaling does not alter stored RGB.

    assert(!pixels.set(pixel_count, {1, 2, 3}));
    assert(pixels.grbWord(pixel_count) == 0);
    assert(pixels.at(pixel_count).red == 0);
    assert(pixels.set(255, {1, 2, 3}));
    assert(pixels.grbWord(255) == 0x02010300u);

    pixels.fill({17, 18, 19});
    assert(!pixels.setLogical(-1, 0, {255, 0, 0}));
    assert(!pixels.setLogical(32, 0, {255, 0, 0}));
    assert(!pixels.setLogical(0, -1, {255, 0, 0}));
    assert(!pixels.setLogical(0, 8, {255, 0, 0}));
    for (std::size_t i = 0; i < pixel_count; ++i)
        assert(pixels.at(i).red == 17 && pixels.at(i).green == 18 &&
               pixels.at(i).blue == 19);

    pixels.clear();
    assert(pixels.setLogical(0, 0, {255, 0, 0}));
    assert(pixels.at(0).red == 255);
    assert(pixels.setLogical(31, 0, {0, 255, 0}));
    assert(pixels.at(255).green == 255);
    assert(pixels.setLogical(31, 7, {0, 0, 255}));
    assert(pixels.at(248).blue == 255);
    assert(pixels.setLogical(0, 7, {255, 255, 0}));
    assert(pixels.at(7).red == 255 && pixels.at(7).green == 255);

    pixels.fill({255, 0, 0});
    assert(pixels.grbWord(0, 8) == 0x00080000u);
    assert(pixels.grbWord(255, 8) == 0x00080000u);
    pixels.fill({0, 255, 0});
    assert(pixels.grbWord(127, 8) == 0x08000000u);
    pixels.fill({0, 0, 255});
    assert(pixels.grbWord(128, 8) == 0x00000800u);
    pixels.fill({255, 255, 255});
    assert(pixels.grbWord(255, 8) == 0x08080800u);
    pixels.clear();
    for (std::size_t i = 0; i < pixel_count; ++i)
        assert(pixels.grbWord(i) == 0);
}
