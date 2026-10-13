#!/bin/sh
set -eu
cd "$(dirname "$0")/../.."
test_dir=$(mktemp -d /tmp/clock-host-tests.XXXXXX)
trap 'rm -rf "$test_dir"' EXIT HUP INT TERM
c++ -std=c++11 -Wall -Wextra -Werror -I include \
    test/host/watchdog_test.cpp -o "$test_dir/watchdog"
"$test_dir/watchdog"
c++ -std=c++11 -Wall -Wextra -Werror -DAAC_BUILD_PROFILE_PRODUCTION \
    -I include \
    test/host/gps_source_selection_test.cpp -o "$test_dir/gps_source_production"
"$test_dir/gps_source_production"
c++ -std=c++11 -Wall -Wextra -Werror -I include \
    test/host/pps_byte_association_test.cpp -o "$test_dir/pps_byte_association"
"$test_dir/pps_byte_association"
c++ -std=c++11 -Wall -Wextra -Werror -I include \
    src/clock_state.cpp test/host/timebase_poll_order_test.cpp \
    -o "$test_dir/timebase_poll_order"
"$test_dir/timebase_poll_order"
c++ -std=c++11 -Wall -Wextra -Werror -I include \
    src/dev_console.cpp src/clock_state.cpp \
    test/host/dev_console_test.cpp -o "$test_dir/dev_console"
"$test_dir/dev_console"
c++ -std=c++11 -Wall -Wextra -Werror -DAAC_BUILD_PROFILE_DEVELOPMENT \
    -I include \
    src/nmea_rmc.cpp src/nmea_gga.cpp src/clock_state.cpp src/gps_input_simulated.cpp \
    test/host/simulated_gps_test.cpp -o "$test_dir/simulated_gps"
"$test_dir/simulated_gps"
c++ -std=c++11 -Wall -Wextra -Werror -DAAC_BUILD_PROFILE_DEVELOPMENT \
    -I include \
    src/nmea_rmc.cpp src/nmea_gga.cpp src/clock_state.cpp src/gps_input_simulated.cpp \
    test/host/simulated_fault_test.cpp -o "$test_dir/simulated_fault"
"$test_dir/simulated_fault"
echo "Simulated GPS/PPS fault tests passed"
c++ -std=c++11 -Wall -Wextra -Werror -I test/host/stubs -I include \
    test/host/display_hardware_test.cpp -o "$test_dir/display_hardware"
"$test_dir/display_hardware"
c++ -std=c++11 -Wall -Wextra -Werror -DAAC_DISPLAY_VFD -I include \
    src/nmea_rmc.cpp src/nmea_gga.cpp src/clock_state.cpp src/display_time.cpp src/clock_display.cpp \
    test/host/timebase_test.cpp -o "$test_dir/timebase"
"$test_dir/timebase"
c++ -std=c++11 -Wall -Wextra -Werror -DAAC_DISPLAY_VFD -I test/host/stubs -I include \
    src/pd2200.cpp src/clock_state.cpp src/display_time.cpp src/clock_display.cpp test/host/vfd_test.cpp -o "$test_dir/vfd"
"$test_dir/vfd"
c++ -std=c++11 -Wall -Wextra -Werror -DAAC_DISPLAY_VFD -I test/host/stubs -I include \
    src/pd2200.cpp src/clock_state.cpp src/display_time.cpp src/clock_display.cpp src/clock_vfd.cpp \
    test/host/hh_test.cpp -o "$test_dir/hh"
"$test_dir/hh"
c++ -std=c++11 -Wall -Wextra -Werror -DAAC_DISPLAY_VFD -I test/host/stubs -I include \
    src/clock_state.cpp src/display_time.cpp src/clock_display.cpp \
    src/pd2200.cpp src/clock_vfd.cpp test/host/timezone_test.cpp -o "$test_dir/timezone"
"$test_dir/timezone"
c++ -std=c++11 -Wall -Wextra -Werror -DAAC_DISPLAY_VFD -I test/host/stubs -I include \
    src/clock_state.cpp src/display_time.cpp src/clock_display.cpp src/pd2200.cpp \
    src/clock_vfd.cpp test/host/hh_zero_test.cpp -o "$test_dir/hh_zero"
"$test_dir/hh_zero"
c++ -std=c++11 -Wall -Wextra -Werror -DAAC_DISPLAY_VFD -I test/host/stubs -I include \
    src/clock_state.cpp src/display_time.cpp src/clock_display.cpp src/pd2200.cpp \
    src/clock_vfd.cpp test/host/hh_pair_test.cpp -o "$test_dir/hh_pair"
"$test_dir/hh_pair"
c++ -std=c++11 -Wall -Wextra -Werror -I test/host/stubs -I include \
    test/host/display_selection_test.cpp -o "$test_dir/display_selection_default"
"$test_dir/display_selection_default"
c++ -std=c++11 -Wall -Wextra -Werror -DAAC_DISPLAY_VFD -I test/host/stubs -I include \
    test/host/display_selection_test.cpp -o "$test_dir/display_selection_vfd"
"$test_dir/display_selection_vfd"
c++ -std=c++11 -Wall -Wextra -Werror -DAAC_DISPLAY_STDOUT -DAAC_DISPLAY_VFD \
    -I test/host/stubs -I include test/host/display_selection_test.cpp \
    -o "$test_dir/display_selection_both"
"$test_dir/display_selection_both"
c++ -std=c++11 -Wall -Wextra -Werror -DAAC_DISPLAY_WS2812 -I test/host/stubs -I include \
    test/host/display_selection_test.cpp -o "$test_dir/display_selection_ws2812"
"$test_dir/display_selection_ws2812"
c++ -std=c++11 -Wall -Wextra -Werror -DAAC_DISPLAY_STDOUT -DAAC_DISPLAY_WS2812 \
    -I test/host/stubs -I include test/host/display_selection_test.cpp \
    -o "$test_dir/display_selection_stdout_ws2812"
"$test_dir/display_selection_stdout_ws2812"
c++ -std=c++11 -Wall -Wextra -Werror -DAAC_DISPLAY_VFD -DAAC_DISPLAY_WS2812 \
    -I test/host/stubs -I include test/host/display_selection_test.cpp \
    -o "$test_dir/display_selection_vfd_ws2812"
"$test_dir/display_selection_vfd_ws2812"
c++ -std=c++11 -Wall -Wextra -Werror -DAAC_DISPLAY_STDOUT -DAAC_DISPLAY_VFD -DAAC_DISPLAY_WS2812 \
    -I test/host/stubs -I include test/host/display_selection_test.cpp \
    -o "$test_dir/display_selection_all"
"$test_dir/display_selection_all"
c++ -std=c++11 -Wall -Wextra -Werror -I include src/clock_state.cpp src/display_time.cpp \
    src/stdout_display.cpp test/host/stdout_display_test.cpp -o "$test_dir/stdout_display"
"$test_dir/stdout_display"
c++ -std=c++11 -Wall -Wextra -Werror -I include \
    test/host/display_framework_test.cpp -o "$test_dir/display_framework"
"$test_dir/display_framework"
c++ -std=c++11 -Wall -Wextra -Werror -DAAC_BUILD_PROFILE_PRODUCTION -I include \
    test/host/ws2812_pixels_test.cpp -o "$test_dir/ws2812_pixels"
"$test_dir/ws2812_pixels"
c++ -std=c++11 -Wall -Wextra -Werror -I include \
    test/host/display_appearance_test.cpp -o "$test_dir/display_appearance"
"$test_dir/display_appearance"
c++ -std=c++11 -Wall -Wextra -Werror -I include \
    test/host/display_compositor_test.cpp -o "$test_dir/display_compositor"
"$test_dir/display_compositor"
c++ -std=c++11 -Wall -Wextra -Werror -I include \
    src/clock_state.cpp src/display_time.cpp \
    test/host/clock_recenter_slide_test.cpp -o "$test_dir/clock_recenter_slide"
"$test_dir/clock_recenter_slide"
c++ -std=c++11 -Wall -Wextra -Werror -I include \
    test/host/clock_presentation_transition_test.cpp \
    -o "$test_dir/clock_presentation_transition"
"$test_dir/clock_presentation_transition"
c++ -std=c++11 -Wall -Wextra -Werror -I include \
    test/host/clock_six_pm_effect_test.cpp \
    -o "$test_dir/clock_six_pm_effect"
"$test_dir/clock_six_pm_effect"
c++ -std=c++11 -Wall -Wextra -Werror -I include \
    test/host/clock_six_am_effect_test.cpp \
    -o "$test_dir/clock_six_am_effect"
"$test_dir/clock_six_am_effect"
c++ -std=c++11 -Wall -Wextra -Werror -DAAC_BUILD_PROFILE_DEVELOPMENT \
    -DAAC_DISPLAY_WS2812 -I include src/dev_console.cpp src/clock_state.cpp \
    src/display_time.cpp test/host/informational_overlay_test.cpp \
    -o "$test_dir/informational_overlay"
"$test_dir/informational_overlay"
c++ -std=c++11 -Wall -Wextra -Werror -I include \
    src/clock_state.cpp src/display_time.cpp test/host/holdover_output_test.cpp \
    -o "$test_dir/holdover_output"
"$test_dir/holdover_output"
c++ -std=c++11 -Wall -Wextra -Werror -DAAC_BUILD_PROFILE_DEVELOPMENT \
    -DAAC_DISPLAY_WS2812 -I include src/dev_console.cpp src/clock_state.cpp \
    test/host/ws2812_transfer_test.cpp -o "$test_dir/ws2812_transfer"
"$test_dir/ws2812_transfer"
c++ -std=c++11 -Wall -Wextra -Werror -DAAC_DISPLAY_WS2812 -I include \
    src/dev_console.cpp src/clock_state.cpp test/host/ws2812_brightness_console_test.cpp \
    -o "$test_dir/ws2812_brightness_console"
"$test_dir/ws2812_brightness_console"
c++ -std=c++11 -Wall -Wextra -Werror -DAAC_DISPLAY_WS2812 -I include \
    src/dev_console.cpp src/clock_state.cpp test/host/ws2812_clock_color_console_test.cpp \
    -o "$test_dir/ws2812_clock_color_console"
"$test_dir/ws2812_clock_color_console"
c++ -std=c++11 -Wall -Wextra -Werror -I include \
    test/host/numeric_font_5x7_test.cpp -o "$test_dir/numeric_font_5x7"
"$test_dir/numeric_font_5x7"
c++ -std=c++11 -Wall -Wextra -Werror -I include -I test/host \
    test/host/numeric_clock_font_6x8_test.cpp -o "$test_dir/numeric_clock_font_6x8"
"$test_dir/numeric_clock_font_6x8"
c++ -std=c++11 -Wall -Wextra -Werror -I include \
    test/host/numeric_clock_font_compact_test.cpp -o "$test_dir/numeric_clock_font_compact"
"$test_dir/numeric_clock_font_compact"
c++ -std=c++11 -Wall -Wextra -Werror -I include \
    test/host/numeric_clock_font_soft_test.cpp -o "$test_dir/numeric_clock_font_soft"
"$test_dir/numeric_clock_font_soft"
c++ -std=c++11 -Wall -Wextra -Werror -I include \
    test/host/compact_clock_font_test.cpp -o "$test_dir/compact_clock_font"
"$test_dir/compact_clock_font"
c++ -std=c++11 -Wall -Wextra -Werror -DAAC_BUILD_PROFILE_DEVELOPMENT -I include \
    src/clock_state.cpp src/display_time.cpp test/host/clock_text_renderer_test.cpp \
    -o "$test_dir/clock_text_renderer"
"$test_dir/clock_text_renderer"
