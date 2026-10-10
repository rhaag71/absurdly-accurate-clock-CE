#pragma once

#include <cstddef>
#include <cstdint>
#include "display_time.hpp"

// Compile-time product and presentation choices. Profile selection is
// independent from display backend selection (see display_backend.hpp).
#if defined(AAC_BUILD_PROFILE_PRODUCTION) && defined(AAC_BUILD_PROFILE_DEVELOPMENT)
#error "Select exactly one AAC-CE build profile"
#elif defined(AAC_BUILD_PROFILE_PRODUCTION)
#define AAC_CE_SELECTED_PROFILE_PRODUCTION 1
#elif defined(AAC_BUILD_PROFILE_DEVELOPMENT)
#define AAC_CE_SELECTED_PROFILE_DEVELOPMENT 1
#else
#error "Select AAC_BUILD_PROFILE_PRODUCTION or AAC_BUILD_PROFILE_DEVELOPMENT"
#endif

namespace ce_config {
enum class BuildProfile : uint8_t { production, development };
enum class DisplayBackend : uint8_t {
    pd2200,
    ws2812,
    max7219_matrix,
    max7219_seven_segment,
    spi_tft,
    character_vfd,
};
enum class HourFormat : uint8_t { twelve_hour, twenty_four_hour };
enum class DstRule : uint8_t { none, contemporary_us };
enum class Theme : uint8_t { default_theme };

#if defined(AAC_CE_SELECTED_PROFILE_PRODUCTION)
constexpr BuildProfile build_profile = BuildProfile::production;
#else
constexpr BuildProfile build_profile = BuildProfile::development;
#endif

// The selector is separate from the profile selector. Only PD-2200 currently
// has a driver; the other enum values reserve typed configuration choices.
#if defined(AAC_DISPLAY_BACKEND_PD2200)
constexpr DisplayBackend display_backend = DisplayBackend::pd2200;
#else
#error "Select a supported AAC-CE display backend"
#endif

constexpr unsigned long gps_baud = 9600;

struct CalendarDate {
    uint8_t month;
    uint8_t day;
};
struct CalendarDates {
    const CalendarDate* values;
    size_t count;
};

// Personal dates are intentionally empty until supplied by the user.
constexpr CalendarDates birthdays = {nullptr, 0};
constexpr CalendarDates holidays = {nullptr, 0};

struct AppearanceDefaults {
    Theme theme;
    bool brightness_limit_configured;
    uint8_t brightness_limit_percent;
    bool current_limit_configured;
    uint16_t current_limit_ma;
};

// Limits remain unset until the display hardware and power budget are measured.
constexpr AppearanceDefaults appearance = {Theme::default_theme, false, 0, false, 0};

struct SimulatedGpsPpsDefaults {
    bool enabled;
    bool initial_utc_configured;
    int64_t initial_utc_seconds;
    uint32_t pps_period_us;
    uint32_t rmc_after_pps_us;
};
constexpr SimulatedGpsPpsDefaults no_simulated_input = {false, false, 0, 0, 0};

struct ProfileSettings {
    presentation::DisplayZone civil_timezone;
    DstRule dst_rule;
    HourFormat hour_format;
    CalendarDates birthdays;
    CalendarDates holidays;
    AppearanceDefaults appearance;
    SimulatedGpsPpsDefaults simulated_gps_pps;
};

constexpr ProfileSettings production_settings = {
    presentation::DisplayZone::utc, DstRule::contemporary_us,
    HourFormat::twenty_four_hour, birthdays, holidays, appearance,
    no_simulated_input,
};
constexpr ProfileSettings development_settings = {
    presentation::DisplayZone::utc, DstRule::contemporary_us,
    HourFormat::twenty_four_hour, birthdays, holidays, appearance,
    no_simulated_input,
};
constexpr ProfileSettings active_settings =
    build_profile == BuildProfile::production ? production_settings : development_settings;
constexpr presentation::DisplayZone civil_timezone = active_settings.civil_timezone;
constexpr DstRule dst_rule = active_settings.dst_rule;
constexpr HourFormat hour_format = active_settings.hour_format;

// Stage 2B may add the simulator to development builds. No simulated source is
// compiled or active in either profile in Stage 2A.
constexpr bool development_profile_allows_simulated_input =
    build_profile == BuildProfile::development;
constexpr bool simulated_input_implemented = false;
}

#undef AAC_CE_SELECTED_PROFILE_PRODUCTION
#undef AAC_CE_SELECTED_PROFILE_DEVELOPMENT
