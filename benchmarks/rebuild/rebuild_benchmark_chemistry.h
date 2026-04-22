// Reason: Keep Track 5 chemistry rebuild fixture seeding isolated so the
// rebuild benchmark entry stays small as mixed-capability datasets grow.

#pragma once

#include <filesystem>

namespace kernel::benchmarks::rebuild {

bool seed_chemistry_rebuild_fixture(
    const std::filesystem::path& vault_root,
    int chemistry_spectrum_count);

}  // namespace kernel::benchmarks::rebuild
