// Reason: Share study day-bucket/date math across study progression and heatmap rules.

#pragma once

#include <cstdint>
#include <string>

namespace kernel::core::product {

constexpr std::int64_t kStudySecsPerDay = 86400;

std::int64_t study_floor_div(std::int64_t value, std::int64_t divisor);
std::int64_t study_positive_mod(std::int64_t value, std::int64_t modulus);
bool subtract_study_days(
    std::int64_t epoch_secs,
    std::int64_t days,
    std::int64_t* out_epoch_secs);
std::string format_study_date_from_epoch_secs(std::int64_t epoch_secs);

}  // namespace kernel::core::product
