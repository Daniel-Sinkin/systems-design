#pragma once

#include "common.hpp"

#include <chrono>
#include <concepts>
#include <random>

namespace ds_sysdes {

template <typename T>
    requires std::integral<T>
[[nodiscard]] auto random_between(T left, T right) -> T {
    thread_local std::mt19937 rng{std::random_device{}()};
    std::uniform_int_distribution<T> dist{left, right};
    return dist(rng);
}

template <typename T>
    requires std::floating_point<T>
[[nodiscard]] auto random_between(T left, T right) -> T {
    thread_local std::mt19937 rng{std::random_device{}()};
    std::uniform_real_distribution<T> dist{left, right};
    return dist(rng);
}

template <typename Rep, typename Period>
[[nodiscard]] auto random_duration_between(
    std::chrono::duration<Rep, Period> min,
    std::chrono::duration<Rep, Period> max
) -> std::chrono::duration<Rep, Period> {
    using duration_type = std::chrono::duration<Rep, Period>;
    using rep_type = typename duration_type::rep;

    return duration_type{random_between<rep_type>(min.count(), max.count())};
}

} // namespace ds_sysdes
