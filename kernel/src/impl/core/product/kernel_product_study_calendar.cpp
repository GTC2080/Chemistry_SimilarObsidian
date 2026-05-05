// Reason: Own study calendar math separately from progression and heatmap rules.

#include "core/kernel_product_study_calendar.h"

#include <cstdio>
#include <cstdint>
#include <limits>
#include <string>

namespace kernel::core::product {

std::int64_t study_floor_div(const std::int64_t value, const std::int64_t divisor) {
  std::int64_t quotient = value / divisor;
  const std::int64_t remainder = value % divisor;
  if (remainder != 0 && ((remainder < 0) != (divisor < 0))) {
    --quotient;
  }
  return quotient;
}

std::int64_t study_positive_mod(const std::int64_t value, const std::int64_t modulus) {
  const std::int64_t remainder = value % modulus;
  return remainder < 0 ? remainder + modulus : remainder;
}

bool subtract_study_days(
    const std::int64_t epoch_secs,
    const std::int64_t days,
    std::int64_t* out_epoch_secs) {
  if (out_epoch_secs == nullptr || days < 0) {
    return false;
  }
  if (days > std::numeric_limits<std::int64_t>::max() / kStudySecsPerDay) {
    return false;
  }

  const std::int64_t delta = days * kStudySecsPerDay;
  if (epoch_secs < std::numeric_limits<std::int64_t>::min() + delta) {
    return false;
  }
  *out_epoch_secs = epoch_secs - delta;
  return true;
}

std::string format_study_date_from_epoch_secs(const std::int64_t epoch_secs) {
  const std::int64_t days = study_floor_div(epoch_secs, kStudySecsPerDay);
  const std::int64_t z = days + 719468;
  const std::int64_t era = (z >= 0 ? z : z - 146096) / 146097;
  const std::uint64_t doe = static_cast<std::uint64_t>(z - era * 146097);
  const std::uint64_t yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
  std::int64_t year = static_cast<std::int64_t>(yoe) + era * 400;
  const std::uint64_t doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
  const std::uint64_t mp = (5 * doy + 2) / 153;
  const std::uint64_t day = doy - (153 * mp + 2) / 5 + 1;
  const std::uint64_t month = mp < 10 ? mp + 3 : mp - 9;
  if (month <= 2) {
    ++year;
  }

  char buffer[16]{};
  std::snprintf(
      buffer,
      sizeof(buffer),
      "%04lld-%02llu-%02llu",
      static_cast<long long>(year),
      static_cast<unsigned long long>(month),
      static_cast<unsigned long long>(day));
  return std::string(buffer);
}

}  // namespace kernel::core::product
