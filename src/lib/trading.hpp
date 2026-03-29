#pragma once

// lib/trading.hpp

#include "common.hpp"
#include <limits>

namespace ds_sysdes {
constexpr u64 k_invalid_id_64{std::numeric_limits<u64>::max()};
constexpr u32 k_invalid_id_32{std::numeric_limits<u32>::max()};

constexpr usize k_cacheline_size{64zu};

} // namespace ds_sysdes
