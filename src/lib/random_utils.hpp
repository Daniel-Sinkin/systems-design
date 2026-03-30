#pragma once

#include "common.hpp"

#include <cassert>
#include <chrono>
#include <concepts>
#include <random>

namespace ds_sysdes {

namespace detail {

[[nodiscard]] inline auto random_engine() -> std::mt19937& {
    thread_local std::mt19937 rng{std::random_device{}()};
    return rng;
}

} // namespace detail

template <typename T>
    requires std::integral<T>
[[nodiscard]] auto random_between(T left, T right) -> T {
    std::uniform_int_distribution<T> dist{left, right};
    return dist(detail::random_engine());
}

template <typename T>
    requires std::floating_point<T>
[[nodiscard]] auto random_between(T left, T right) -> T {
    std::uniform_real_distribution<T> dist{left, right};
    return dist(detail::random_engine());
}

[[nodiscard]] inline auto random_bool(f64 true_weight = 0.5) -> bool {
    assert(0.0 <= true_weight && true_weight <= 1.0);
    std::bernoulli_distribution dist{true_weight};
    return dist(detail::random_engine());
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
