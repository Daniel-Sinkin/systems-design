// app/main.cpp
#include <algorithm>
#include <array>              // IWYU pragma: keep
#include <atomic>             // IWYU pragma: keep
#include <chrono>             // IWYU pragma: keep
#include <cmath>
#include <concepts>           // IWYU pragma: keep
#include <condition_variable> // IWYU pragma: keep
#include <cstdarg>            // IWYU pragma: keep
#include <format>             // IWYU pragma: keep
#include <functional>
#include <limits> // IWYU pragma: keep
#include <mutex>  // IWYU pragma: keep
#include <numeric>
#include <optional>
#include <print>         // IWYU pragma: keep
#include <random>
#include <sstream>       // IWYU pragma: keep
#include <stdexcept>     // IWYU pragma: keep
#include <string>
#include <string_view>   // IWYU pragma: keep
#include <thread>        // IWYU pragma: keep
#include <type_traits>   // IWYU pragma: keep
#include <unordered_map> // IWYU pragma: keep
#include <unordered_set>
#include <utility>       // IWYU pragma: keep
#include <vector>

#include <cstddef>
#include <cstdint>

#if __has_include(<stdfloat>)
#include <stdfloat>
#endif

namespace ds_tn {

using usize = std::size_t;
using isize = std::ptrdiff_t;
using uptr = std::uintptr_t;
using iptr = std::intptr_t;

using i64 = std::int64_t;
using i32 = std::int32_t;
using i16 = std::int16_t;
using i8 = std::int8_t;

using u64 = std::uint64_t;
using u32 = std::uint32_t;
using u16 = std::uint16_t;
using u8 = std::uint8_t;

#if defined(__cpp_lib_stdfloat) && __cpp_lib_stdfloat >= 202207L
using f32 = std::float32_t;
using f64 = std::float64_t;
#else
using f32 = float;
using f64 = double;
#endif
static_assert(sizeof(f32) == 4);
static_assert(sizeof(f64) == 8);

class Hash {
public:
    explicit Hash(usize value) noexcept : value_(value) {}

    [[nodiscard]] auto value() const noexcept -> usize { return value_; }
    [[nodiscard]] auto operator==(Hash other) const noexcept -> bool { return value_ == other.value_; }
    [[nodiscard]] auto operator==(usize hash_value) const noexcept -> bool { return value_ == hash_value; }

private:
    usize value_{};
};

namespace literals {

[[nodiscard]] constexpr auto operator""_b(unsigned long long value) noexcept -> usize {
    return static_cast<usize>(value);
}

[[nodiscard]] constexpr auto operator""_kib(unsigned long long value) noexcept -> usize {
    return static_cast<usize>(value * 1024ULL);
}

[[nodiscard]] constexpr auto operator""_mib(unsigned long long value) noexcept -> usize {
    return static_cast<usize>(value * 1024ULL * 1024ULL);
}

} // namespace literals

template <typename T>
auto linear_search(const std::vector<T> &values, const T &target) -> std::optional<usize> {
    if (const auto it = std::ranges::find(values, target); it != values.end()) {
        return static_cast<usize>(it - values.begin());
    }
    return std::nullopt;
}

template <std::floating_point F>
class Tensor {
public:
    using Dimension = usize;

    Tensor(const Tensor &other) {
        copy_from(other);
    }
    Tensor &operator=(const Tensor &other) {
        if (this != &other) {
            copy_from(other);
        }
        return *this;
    }
    Tensor(Tensor &&other) {
        steal_from(std::move(other));
    }
    Tensor &operator=(Tensor &&other) {
        if (this != &other) {
            steal_from(std::move(other));
        }
        return *this;
    }

    explicit Tensor(std::vector<Dimension> dims) {
        auto names = make_auto_names(dims.size());
        initialize(std::move(dims), std::move(names));
    }
    Tensor(std::vector<Dimension> dims, std::vector<std::string> names) {
        initialize(std::move(dims), std::move(names));
    }
    Tensor(std::vector<std::string> names, std::vector<Dimension> dims) : Tensor(std::move(dims), std::move(names)) {
    }

    [[nodiscard]] auto size() const noexcept -> usize { return data_.size(); }
    [[nodiscard]] auto size_bytes() const noexcept -> usize { return data_.size() * sizeof(F); }

    [[nodiscard]] auto hash_name(const std::string &name) const noexcept -> Hash { return Hash{std::hash<std::string>{}(name)}; }
    [[nodiscard]] auto get_idx(Hash hash) const noexcept -> std::optional<usize> {
        return linear_search(hashes_, hash);
    }
    [[nodiscard]] auto get_idx(const std::string &name) const noexcept -> std::optional<usize> {
        return linear_search(names_, name);
    }
    [[nodiscard]] auto get_dim(Hash hash) const noexcept -> std::optional<usize> {
        if (const auto idx = get_idx(hash); idx) {
            return dims_[*idx];
        }
        return std::nullopt;
    }
    [[nodiscard]] auto get_dim(const std::string &name) const noexcept -> std::optional<usize> {
        if (const auto idx = get_idx(name); idx) {
            return dims_[*idx];
        }
        return std::nullopt;
    }

    [[nodiscard]] static auto uniform(std::vector<Dimension> dims, F min = F{0}, F max = F{1}) -> Tensor {
        Tensor tensor(std::move(dims));
        tensor.fill_uniform(min, max);
        return tensor;
    }
    [[nodiscard]] static auto uniform(
        std::vector<std::string> names,
        std::vector<Dimension> dims,
        F min = F{0},
        F max = F{1}
    ) -> Tensor {
        Tensor tensor(std::move(dims), std::move(names));
        tensor.fill_uniform(min, max);
        return tensor;
    }
    [[nodiscard]] static auto normal(std::vector<Dimension> dims, F mean = F{0}, F variance = F{1}) -> Tensor {
        Tensor tensor(std::move(dims));
        tensor.fill_normal(mean, variance);
        return tensor;
    }
    [[nodiscard]] static auto normal(
        std::vector<std::string> names,
        std::vector<Dimension> dims,
        F mean = F{0},
        F variance = F{1}
    ) -> Tensor {
        Tensor tensor(std::move(dims), std::move(names));
        tensor.fill_normal(mean, variance);
        return tensor;
    }

    [[nodiscard]] auto auto_name_by_idx(usize idx) const -> std::string_view {
        return names_.at(idx);
    }
    [[nodiscard]] auto get_single_value(usize i) const -> F { return data_.at(i); }
    auto print() const -> void {
        validate_storage();

        if (dims_.size() == 1) {
            std::println("{}", format_slice(0, dims_[0]));
        } else if (dims_.size() == 2) {
            print_matrix(0, dims_[0], dims_[1]);
        } else if (dims_.size() == 3) {
            const auto matrix_count = dims_[0];
            const auto rows = dims_[1];
            const auto cols = dims_[2];
            const auto matrix_size = rows * cols;

            for (auto matrix_idx = 0zu; matrix_idx < matrix_count; ++matrix_idx) {
                if (matrix_idx != 0zu) {
                    std::println("");
                }
                print_matrix(matrix_idx * matrix_size, rows, cols);
            }
        } else {
            throw std::runtime_error(std::format("Not Implemented. Printing only works tensors of rank 1, 2, 3 but is {}", dims_.size()));
        }
    }

private:
    std::vector<F> data_{};
    std::vector<Dimension> dims_{};
    std::vector<std::string> names_{};
    std::vector<Hash> hashes_{};

    [[nodiscard]] static auto storage_size_from_dims(const std::vector<Dimension> &dims) -> usize {
        if (dims.empty()) {
            return 0zu;
        }

        auto size = 1zu;
        for (const auto dim : dims) {
            if (size != 0zu && dim > (std::numeric_limits<usize>::max() / size)) {
                throw std::runtime_error("Tensor shape overflows addressable storage");
            }
            size *= dim;
        }
        return size;
    }

    auto validate_storage() const -> void {
        const auto expected_size = storage_size_from_dims(dims_);
        if (data_.size() != expected_size) {
            throw std::runtime_error(std::format("Tensor storage size mismatch. Expected {}, found {}", expected_size, data_.size()));
        }
    }

    [[nodiscard]] static auto make_auto_names(usize rank) -> std::vector<std::string> {
        auto names = std::vector<std::string>{};
        names.reserve(rank);
        for (auto i = 0zu; i < rank; ++i) {
            names.push_back(std::format("_{}", i));
        }
        return names;
    }

    static auto validate_names(const std::vector<std::string> &names, usize rank) -> void {
        if (names.size() != rank) {
            throw std::runtime_error(std::format(
                "Tensor dimension names must match rank. Expected {}, found {}",
                rank,
                names.size()
            ));
        }

        auto seen = std::unordered_set<std::string>{};
        seen.reserve(names.size());
        for (const auto &name : names) {
            if (!seen.insert(name).second) {
                throw std::runtime_error(std::format("Duplicate tensor dimension name '{}'", name));
            }
        }
    }

    auto initialize(std::vector<Dimension> dims, std::vector<std::string> names) -> void {
        if (dims.empty()) {
            throw std::runtime_error("Can't construct tensor without dimensions");
        }

        validate_names(names, dims.size());

        dims_ = std::move(dims);
        data_.resize(storage_size_from_dims(dims_));
        names_ = std::move(names);

        hashes_.clear();
        hashes_.reserve(names_.size());
        for (const auto &name : names_) {
            hashes_.push_back(hash_name(name));
        }
    }

    [[nodiscard]] static auto random_engine() -> std::mt19937 & {
        thread_local std::mt19937 engine{std::random_device{}()};
        return engine;
    }

    auto fill_uniform(F min, F max) -> void {
        if (min > max) {
            throw std::runtime_error(std::format("Uniform range must satisfy min <= max but got [{}, {}]", min, max));
        }

        auto distribution = std::uniform_real_distribution<F>{min, max};
        for (auto &value : data_) {
            value = distribution(random_engine());
        }
    }

    auto fill_normal(F mean, F variance) -> void {
        if (variance < F{0}) {
            throw std::runtime_error(std::format("Normal variance must be non-negative but got {}", variance));
        }

        auto distribution = std::normal_distribution<F>{mean, static_cast<F>(std::sqrt(variance))};
        for (auto &value : data_) {
            value = distribution(random_engine());
        }
    }

    [[nodiscard]] auto format_slice(usize offset, usize count) const -> std::string {
        std::string formatted{"["};
        for (auto i = 0zu; i < count; ++i) {
            if (i != 0zu) {
                formatted += ", ";
            }
            formatted += std::format("{}", data_.at(offset + i));
        }
        formatted += "]";
        return formatted;
    }

    auto print_matrix(usize offset, usize rows, usize cols) const -> void {
        std::println("[");
        for (auto row = 0zu; row < rows; ++row) {
            std::println("  {}", format_slice(offset + (row * cols), cols));
        }
        std::println("]");
    }

    auto copy_from(const Tensor &other) {
        data_ = other.data_;
        dims_ = other.dims_;
        names_ = other.names_;
        hashes_ = other.hashes_;
    }
    auto steal_from(Tensor &&other) {
        data_ = std::exchange(other.data_, {});
        dims_ = std::exchange(other.dims_, {});
        names_ = std::exchange(other.names_, {});
        hashes_ = std::exchange(other.hashes_, {});
    }
};
} // namespace ds_tn

int main() {
    std::println("Hello, World!");
    return 0;
}
