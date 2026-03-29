#pragma once

#include "common.hpp"
#include "logger.hpp"
#include "random_utils.hpp"
#include "spsc_queue.hpp"
#include "trading.hpp"

#include <array>
#include <atomic>
#include <cstring>
#include <exception>
#include <format>
#include <limits>
#include <thread>
#include <utility>
#include <vector>

namespace ds_sysdes {
using namespace ds_sysdes::literals;

using IngressErrorMessage = std::array<char, 122>;
[[maybe_unused]] constexpr IngressErrorMessage k_string_ingress_invalid_symbol{
    "Invalid SymbolId!"};
[[maybe_unused]] constexpr IngressErrorMessage k_string_ingress_full_queue{
    "Symbol Queue Full!"};

enum class SymbolId : u32 {
    AAPL,
    MSFT,
    BTCUSD,
    Count,
    Invalid = std::numeric_limits<u32>::max()
};

constexpr usize SymbolCount = static_cast<usize>(std::to_underlying(SymbolId::Count));

[[nodiscard]] inline auto to_string(SymbolId symbol) -> std::string_view {
    switch (symbol) {
    case SymbolId::AAPL:
        return "AAPL";
    case SymbolId::MSFT:
        return "MSFT";
    case SymbolId::BTCUSD:
        return "BTCUSD";
    case SymbolId::Count:
        std::unreachable();
    case SymbolId::Invalid:
        return "Invalid";
    }
    std::unreachable();
}

struct Price {
    Price() : value(0) {}
    Price(u64 price) : value(price) {}
    u64 value;
};
static_assert(sizeof(Price) == sizeof(u64));

struct Quantity {
    Quantity() : value(0) {}
    Quantity(u32 qty) : value(qty) {}
    u32 value;
};
static_assert(sizeof(Quantity) == sizeof(u32));

struct Order {
    enum class Flags : u8 {
        None = 0,
        IsBuy = 1 << 0,
        IsLimit = 1 << 1,
    };

    enum class Action : u8 {
        Invalid = 0,
        New,
        Cancel,
    };

    u64 client_order_id{};
    Price price{};
    Quantity qty{};
    Action action{Action::Invalid};
    Flags flags{Flags::None};

    [[nodiscard]] auto is_buy() const noexcept -> bool {
        return (std::to_underlying(flags) & std::to_underlying(Flags::IsBuy)) != 0;
    }

    [[nodiscard]] auto is_sell() const noexcept -> bool {
        return !is_buy();
    }

    [[nodiscard]] auto is_limit() const noexcept -> bool {
        return (std::to_underlying(flags) & std::to_underlying(Flags::IsLimit)) != 0;
    }
};
static_assert(sizeof(Order) == 24_b);

[[nodiscard]] inline auto to_string(Order::Action action) -> std::string_view {
    switch (action) {
    case Order::Action::Invalid:
        return "Invalid";
    case Order::Action::New:
        return "New";
    case Order::Action::Cancel:
        return "Cancel";
    }
    std::unreachable();
}

struct OrderMessage {
    u32 connection_id{};
    u32 seq_num{};
    SymbolId symbol{SymbolId::Invalid};
    std::byte _pad[4]{};
    Order order{};

    [[nodiscard]] auto to_string() const -> std::string {
        return std::format(
            "OrderMessage{{connection_id={}, seq_num={}, symbol={}, client_order_id={}, price_ticks={}, qty={}, action={}, side={}, type={}}}",
            connection_id,
            seq_num,
            ds_sysdes::to_string(symbol),
            order.client_order_id,
            order.price.value,
            order.qty.value,
            ds_sysdes::to_string(order.action),
            order.is_buy() ? "Buy" : "Sell",
            order.is_limit() ? "Limit" : "Market");
    }
};
static_assert(sizeof(OrderMessage) == 40_b);

constexpr usize k_queue_size_ingress{1024};
using IngressQueue = SPSC_NonBlocking<OrderMessage, k_queue_size_ingress>;
static_assert(sizeof(IngressQueue) == 40_kib + 128_b && "This is just for readability, change if outdated");

constexpr usize k_queue_size_per_symbol{1024};
using SymbolQueue = SPSC_NonBlocking<Order, k_queue_size_per_symbol>;

struct Response {
    enum class Type : u8 {
        Invalid = 0,
        Accept,
        Reject
    };

    Type type{Type::Invalid};
    std::byte _pad[3]{};
    u32 connection_id{};
    u64 client_order_id{};
    std::array<char, 112> msg{};
};
static_assert(sizeof(Response) == 128_b);

[[nodiscard]] inline auto to_string(Response::Type response_type) -> std::string_view {
    switch (response_type) {
    case Response::Type::Invalid:
        return "Invalid";
    case Response::Type::Accept:
        return "Accept";
    case Response::Type::Reject:
        return "Reject";
    }
    return "Unknown";
}

constexpr usize k_queue_size_error_event{1024};
using ResponseEventQueue = SPSC_NonBlocking<Response, k_queue_size_error_event>;
static_assert(sizeof(ResponseEventQueue) == 128_kib + 128_b && "This is just for readability, change if outdated");

class User {
public:
    static constexpr std::string_view k_log_name_format_user = "User-{}";

    User(SymbolId symbol) : id_(next_id_.fetch_add(1, std::memory_order_relaxed)), symbol_(symbol) {
        if (symbol == SymbolId::Invalid) {
            assert(false && "Invalid symbol for user");
            std::terminate();
        }
        log_ = &get_terminal_logger(std::format(k_log_name_format_user, id_));
    }

    auto submit_order(IngressQueue &queue, Price price, Quantity qty, Order::Action action, Order::Flags flags) -> void {
        const OrderMessage msg{
            .connection_id = id_,
            .seq_num = 0,
            .symbol = symbol_,
            .order = {
                .client_order_id = order_id_++,
                .price = price,
                .qty = qty,
                .action = action,
                .flags = flags}};
        const auto pushed = queue.push(msg);
        log_->log(std::format("{}{}", msg.to_string(), pushed ? "" : " [queue full]"));
    }

    [[nodiscard]] auto id() const noexcept -> u32 { return id_; }
    [[nodiscard]] auto order_id() const noexcept -> u32 { return order_id_; }
    [[nodiscard]] auto symbol() const noexcept -> SymbolId { return symbol_; }

private:
    inline static std::atomic<u32> next_id_{0};

    u32 id_{};
    u32 order_id_{};
    SymbolId symbol_{};

    TerminalLogger *log_{};
};

enum class UserType : u8 {
    Invalid = 0,
    Buyer,
    Seller,
    Mixed,
    Inactive,
    Malformed,
    Count,
};

struct UserTask {
    SymbolId symbol{SymbolId::Invalid};
    UserType type{UserType::Invalid};
    u64 price{0};
    u32 qty{0};
};

[[nodiscard]] constexpr auto make_default_user_tasks() -> std::array<UserTask, 4> {
    return std::to_array<UserTask>({
        {SymbolId::AAPL, UserType::Buyer, 10000, 100},
        {SymbolId::AAPL, UserType::Seller, 1000, 150},
        {SymbolId::MSFT, UserType::Buyer, 100, 10},
        {SymbolId::BTCUSD, UserType::Seller, 100000, 500},
    });
}

struct OrderGatewayContext {
    IngressQueue ingress_queue{};
    std::atomic<bool> quit_signal{false};
    std::vector<SymbolQueue> symbol_queues{SymbolCount};
    ResponseEventQueue response_event_queue{};
};

inline auto send_reject(
    ResponseEventQueue &response_event_queue,
    TerminalLogger &log,
    const OrderMessage &rejected_order,
    const IngressErrorMessage &msg) -> void {
    Response resp{
        .type = Response::Type::Reject,
        .connection_id = rejected_order.connection_id,
        .client_order_id = rejected_order.order.client_order_id,
    };
    std::memcpy(resp.msg.data(), msg.data(), sizeof(resp.msg));
    if (!response_event_queue.push(resp)) {
        log.log("CRITICAL FAILURE! Can't submit error to error event queue. Must terminate.");
        std::terminate();
    }
}

[[nodiscard]] inline auto make_user_task_worker(OrderGatewayContext &context, UserTask task) -> std::jthread {
    return std::jthread(
        [&context, task] {
            using namespace std::chrono_literals;

            User user{task.symbol};
            while (true) {
                if (context.quit_signal.load(std::memory_order_relaxed)) [[unlikely]] {
                    break;
                }

                user.submit_order(
                    context.ingress_queue,
                    task.price,
                    task.qty,
                    Order::Action::New,
                    (task.type == UserType::Buyer) ? Order::Flags::IsBuy : Order::Flags::None);
                std::this_thread::sleep_for(random_duration_between(200ms, 3000ms));
            }
        });
}

[[nodiscard]] inline auto make_ingress_worker(OrderGatewayContext &context) -> std::jthread {
    return std::jthread(
        [&context] {
            auto &log = get_terminal_logger("Ingress-Worker");

            while (true) {
                if (context.quit_signal.load(std::memory_order_relaxed)) [[unlikely]] {
                    break;
                }

                OrderMessage tmp{};
                if (context.ingress_queue.pop(tmp)) [[likely]] {
                    if (tmp.symbol == SymbolId::Invalid or tmp.symbol >= SymbolId::Count) [[unlikely]] {
                        send_reject(context.response_event_queue, log, tmp, k_string_ingress_invalid_symbol);
                    } else {
                        const auto idx = static_cast<usize>(std::to_underlying(tmp.symbol));
                        if (context.symbol_queues[idx].push(tmp.order)) [[likely]] {
                            log.log(std::format(
                                "Successfully enqueued an order for symbol '{}' there are now {} orders in there.",
                                to_string(tmp.symbol),
                                context.symbol_queues[idx].size()));
                        } else {
                            send_reject(context.response_event_queue, log, tmp, k_string_ingress_full_queue);
                        }
                    }
                }
            }
        });
}

[[nodiscard]] inline auto make_symbol_worker(OrderGatewayContext &context, usize symbol_index) -> std::jthread {
    return std::jthread(
        [&context, symbol_index] {
            using namespace std::chrono_literals;

            const auto symbol = static_cast<SymbolId>(symbol_index);
            auto &log = get_terminal_logger(std::format("Symbol-Worker({})", to_string(symbol)));
            while (true) {
                if (context.quit_signal.load(std::memory_order_relaxed)) [[unlikely]] {
                    break;
                }

                Order tmp{};
                if (context.symbol_queues[symbol_index].pop(tmp)) {
                    log.log("Processing new order.");
                }
                std::this_thread::sleep_for(200ms);
            }
        });
}

[[nodiscard]] inline auto make_response_worker(OrderGatewayContext &context) -> std::jthread {
    return std::jthread(
        [&context] {
            using namespace std::chrono_literals;

            auto &log = get_terminal_logger("Response-Worker");
            while (true) {
                if (context.quit_signal.load(std::memory_order_relaxed)) [[unlikely]] {
                    break;
                }

                Response tmp{};
                if (context.response_event_queue.pop(tmp)) [[likely]] {
                    log.log(std::format(
                        "Sending response to connection '{}' for order_id '{}' of type '{}' with message '{}'",
                        tmp.connection_id,
                        tmp.client_order_id,
                        to_string(tmp.type),
                        std::string_view{tmp.msg.data()}));
                }
                std::this_thread::sleep_for(125ms);
            }
        });
}

class OrderGatewayExperiment {
public:
    OrderGatewayExperiment() {
        const auto user_tasks = make_default_user_tasks();

        user_task_workers_.reserve(user_tasks.size());
        for (const auto &task : user_tasks) {
            user_task_workers_.push_back(make_user_task_worker(context_, task));
        }

        ingress_worker_ = make_ingress_worker(context_);

        symbol_workers_.reserve(SymbolCount);
        for (auto i = 0zu; i < SymbolCount; ++i) {
            symbol_workers_.push_back(make_symbol_worker(context_, i));
        }

        response_worker_ = make_response_worker(context_);
    }

private:
    OrderGatewayContext context_{};
    std::vector<std::jthread> user_task_workers_{};
    std::jthread ingress_worker_{};
    std::vector<std::jthread> symbol_workers_{};
    std::jthread response_worker_{};
};

} // namespace ds_sysdes
