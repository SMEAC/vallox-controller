#pragma once
#include <stdint.h>

// Operating modes of the Vallox unit as the controller sees them.
//   Home / Away / Auto  -> "basic" modes, register 4609 = 0 / 1 / 2
//   Boost / Fireplace   -> timed modes (registers 4612 / 4613 hold the minutes)
//   Programmable        -> timed mode started by a digital input of the unit
//                          (register 4614); shown, but never set by this controller
enum class Mode : uint8_t { Home, Away, Auto, Boost, Fireplace, Programmable };

inline const char* modeName(Mode m) {
  switch (m) {
    case Mode::Home:         return "HOME";
    case Mode::Away:         return "AWAY";
    case Mode::Auto:         return "AUTO";
    case Mode::Boost:        return "BOOST";
    case Mode::Fireplace:    return "FIRE";
    case Mode::Programmable: return "EXTERNAL";
  }
  return "?";
}
