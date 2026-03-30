#pragma once

#include "common.hpp"
#include "trading.hpp"

#include <array>
#include <atomic>
#include <concepts>
#include <type_traits>
#include <utility>

namespace ds_sysdes {

template <typename T>
concept TrivialMessage =
    std::is_default_constructible_v<T> and
    std::is_trivially_copyable_v<T> and
    std::is_trivially_destructible_v<T>;

template <typename U, typename T>
concept NothrowAssignableTo =
    std::assignable_from<T &, U &&> and
    std::is_nothrow_assignable_v<T &, U &&>;

template <std::unsigned_integral T>
[[nodiscard]] constexpr auto is_power_of_2(T x) noexcept -> bool {
    return (x > 0) and ((x & (x - 1)) == 0);
}

template <TrivialMessage T, usize BufferSize = 1024>
    requires(is_power_of_2(BufferSize))
class SPSC_NonBlocking {
public:
    template <NothrowAssignableTo<T> U>
    [[nodiscard]] auto push(U &&msg) noexcept -> bool {
        const auto h = head_.load(std::memory_order_relaxed);
        const auto t = tail_.load(std::memory_order_acquire);
        if (full_(h, t)) {
            return false;
        }

        buffer_[h & (BufferSize - 1)] = std::forward<U>(msg);
        head_.store(h + 1, std::memory_order_release);
        return true;
    }

    [[nodiscard]] auto pop(T &out) noexcept -> bool {
        const auto h = head_.load(std::memory_order_acquire);
        const auto t = tail_.load(std::memory_order_relaxed);
        if (empty_(h, t)) {
            return false;
        }

        out = buffer_[t & (BufferSize - 1)];
        tail_.store(t + 1, std::memory_order_release);
        return true;
    }

    [[nodiscard]] auto empty() const noexcept -> bool {
        const auto h = head_.load(std::memory_order_relaxed);
        const auto t = tail_.load(std::memory_order_relaxed);
        return empty_(h, t);
    }

    [[nodiscard]] auto full() const noexcept -> bool {
        const auto h = head_.load(std::memory_order_relaxed);
        const auto t = tail_.load(std::memory_order_relaxed);
        return full_(h, t);
    }

    [[nodiscard]] auto size() const noexcept -> usize {
        const auto h = head_.load(std::memory_order_acquire);
        const auto t = tail_.load(std::memory_order_acquire);
        return static_cast<usize>(h - t);
    }

private:
    std::array<T, BufferSize> buffer_{};
    alignas(k_cacheline_size) std::atomic<u64> head_{};
    alignas(k_cacheline_size) std::atomic<u64> tail_{};

    [[nodiscard]] auto empty_(u64 h, u64 t) const noexcept -> bool {
        return h == t;
    }

    [[nodiscard]] auto full_(u64 h, u64 t) const noexcept -> bool {
        return (h - t) == BufferSize;
    }
};

} // namespace ds_sysdes
