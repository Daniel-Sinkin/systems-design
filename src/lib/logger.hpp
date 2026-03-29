#pragma once

#include "common.hpp"

#include <cassert>
#include <concepts>
#include <exception>
#include <iostream>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace ds_sysdes {

template <typename T>
struct Logger {
public:
    static constexpr usize k_buffer_size{1}; // Should insta-flush

    Logger() : name_("Default") {
        msg_buffer_.reserve(k_buffer_size);
    }
    Logger(std::string_view name) : name_(std::string{name}) {
        msg_buffer_.reserve(k_buffer_size);
    }

    Logger(const Logger &other) = default;
    Logger &operator=(const Logger &other) = default;
    Logger(Logger &&other) = default;
    Logger &operator=(Logger &&other) = default;

    ~Logger() { flush_buffer(); }

    auto log(std::string_view msg) -> void {
        msg_buffer_.push_back(std::string{msg});
        if (msg_buffer_.size() >= k_buffer_size) {
            flush_buffer();
        }
    }

    auto flush_buffer() -> void {
        for (const auto &msg : msg_buffer_) {
            static_cast<T *>(this)->flush_one_(msg);
        }
        msg_buffer_.clear();
    }

    [[nodiscard]] auto name() const noexcept -> std::string_view { return name_; }
    [[maybe_unused]] auto flush_one_(const std::string &) -> void {}

private:
    std::string name_{};
    std::vector<std::string> msg_buffer_{};
};

template <typename T>
concept LoggerLike = std::derived_from<T, Logger<T>>;

struct TerminalLogger : Logger<TerminalLogger> {
    using Logger<TerminalLogger>::Logger;

    [[maybe_unused]] auto flush_one_(const std::string &msg) -> void {
        const std::scoped_lock lock{output_mutex_};
        std::cout << "[" << name() << "] " << msg << '\n';
    }

private:
    inline static std::mutex output_mutex_{};
};

template <LoggerLike LoggerT>
struct LoggerManager {
    static auto get_logger(const std::string &name) -> LoggerT & {
        const std::scoped_lock lock{mutex_};
        if (const auto it = loggers_.find(name); it != loggers_.end()) {
            return it->second;
        }
        const auto [it, inserted] = loggers_.emplace(name, LoggerT{name});
        assert(inserted && "Failed to create new logger");
        if (!inserted) {
            std::terminate();
        }
        return it->second;
    }

private:
    inline static std::mutex mutex_{};
    inline static std::unordered_map<std::string, LoggerT> loggers_{};
};

[[nodiscard]] inline auto get_terminal_logger(const std::string &name) -> TerminalLogger & {
    return LoggerManager<TerminalLogger>::get_logger(name);
}

} // namespace ds_sysdes
