#pragma once

#include <cstdint>

namespace presentation_time_event {

enum class Event : uint8_t {
  none,
  ordinary_hour,
  six_am,
  noon,
  six_pm,
  midnight,
};

inline bool isMajorSpecial(Event event) {
  return event == Event::six_am || event == Event::noon ||
         event == Event::six_pm || event == Event::midnight;
}

// Classifies civil-time presentation boundaries. A displayed xx:00 is not an
// event by itself: the presentation must first have observed a valid :59.
class Resolver {
 public:
  Event update(bool valid, uint8_t civil_hour, uint8_t civil_minute) {
    if (!valid) {
      reset();
      return Event::none;
    }

    const bool top_of_hour = has_previous_minute_ && previous_minute_ == 59U &&
                             civil_minute == 0U;
    previous_minute_ = civil_minute;
    has_previous_minute_ = true;

    if (!top_of_hour) {
      return Event::none;
    }

    switch (civil_hour) {
      case 0:
        return Event::midnight;
      case 6:
        return Event::six_am;
      case 12:
        return Event::noon;
      case 18:
        return Event::six_pm;
      default:
        return Event::ordinary_hour;
    }
  }

  void reset() {
    has_previous_minute_ = false;
    previous_minute_ = 0U;
  }

 private:
  bool has_previous_minute_ = false;
  uint8_t previous_minute_ = 0U;
};

}  // namespace presentation_time_event
