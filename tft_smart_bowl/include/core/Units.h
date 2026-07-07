#ifndef CORE_UNITS_H
#define CORE_UNITS_H

#include <stdint.h>

namespace Core {

// Represents mass/weight in grams
using grams_t = float;

// Represents time durations or timestamps in milliseconds
using ms_t = uint32_t;

// Represents voltage in millivolts (e.g. battery reading)
using mv_t = uint32_t;

// Represents percentage values (e.g., battery charge 0-100%)
using percent_t = uint8_t;

} // namespace Core

#endif // CORE_UNITS_H
