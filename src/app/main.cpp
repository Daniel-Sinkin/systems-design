// app/main.cpp
#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <type_traits>
#include <unordered_map>
#include <utility>

#include "common.hpp"
#include "logger.hpp"
#include "random_utils.hpp"
#include "spsc_queue.hpp"
#include "trading.hpp"

namespace ds_sysdes {

class Symbol {
public:
    enum class Id : u32 {
        Invalid = 0,
        AAPL,
        MSFT,
        BTCUSD,
        End
    };
    static constexpr auto Count = static_cast<usize>(std::to_underlying(Id::End) - 1);
    static constexpr auto Invalid = Id::Invalid;

    Symbol(Id id) {
        if (!is_valid(id)) {
            throw std::runtime_error(std::format("SymbolId '{}' is invalid", std::to_underlying(id)));
        }
        id_ = id;
    }
    Symbol(u32 id) : Symbol(Id{id}) {}

    [[nodiscard]] static auto is_valid(u32 id) noexcept -> bool { return is_valid(Id{id}); }
    [[nodiscard]] static auto is_valid(Id id) noexcept -> bool { return (id != Id::Invalid) and (id != Id::End); }
    [[nodiscard]] auto is_valid() noexcept -> bool { return is_valid(id_); }

    // Idx is 0 based, while the first symbol starts with index 0
    [[nodiscard]] static auto idx(Id id) noexcept -> usize { return static_cast<usize>(std::to_underlying(id)) - 1; }
    [[nodiscard]] auto idx() noexcept -> usize { return idx(id_); }
    [[nodiscard]] auto id() const noexcept -> Id { return id_; }
    [[nodiscard]] static auto from_idx(usize idx) -> Symbol {
        if (idx >= Count) {
            throw std::out_of_range(std::format("symbol idx = {} is OOB", idx));
        }
        return Symbol{static_cast<u32>(idx + 1)};
    }

private:
    Id id_{Id::Invalid};
};

[[nodiscard]] auto to_string(Symbol::Id symbol) -> std::string_view {
    switch (symbol) {
    case Symbol::Id::Invalid:
        return "Invalid";
    case Symbol::Id::AAPL:
        return "AAPL";
    case Symbol::Id::MSFT:
        return "MSFT";
    case Symbol::Id::BTCUSD:
        return "BTCUSD";
    case Symbol::Id::End:
        std::unreachable();
    }
    std::unreachable();
}

enum class UpdateType : u8 {
    Invalid = 0,
    Add = 1,
    Modify = 2,
    Delete = 3,
    Trade = 4
};

enum class TradeSide : u8 {
    Invalid = 0,
    Sell = 1,
    Buy = 2
};

struct Price {
    using Value = u32;
    Value value{0};
};
static_assert(sizeof(Price) == sizeof(Price::Value));

struct Quantity {
    using Value = u32;
    Value value{0};
};
static_assert(sizeof(Quantity) == sizeof(Quantity::Value));

struct MarketUpdate {
    Symbol::Id symbol{Symbol::Invalid};
    UpdateType type{UpdateType::Invalid};
    TradeSide side{TradeSide::Invalid};
    Price price{};
    Quantity qty{};
};
static_assert(sizeof(MarketUpdate) == 16);

template <TrivialMessage T, usize BufferSize = 1024>
    requires(is_power_of_2(BufferSize))
class MPSC_NonBlocking {
public:
    template <NothrowAssignableTo<T> U>
    [[nodiscard]] auto push(U &&msg) noexcept -> bool {
        u64 h = head_.load(std::memory_order_relaxed);

        while (true) {
            const auto t = tail_.load(std::memory_order_acquire);
            if ((h - t) == BufferSize) {
                return false;
            }

            if (head_.compare_exchange_weak(h, h + 1, std::memory_order_acq_rel, std::memory_order_relaxed)) {
                break;
            }
        }

        auto &slot = buffer_[h & (BufferSize - 1)];
        slot.value = std::forward<U>(msg);
        slot.ready.store(true, std::memory_order_release);
        return true;
    }

    [[nodiscard]] auto pop(T &out) noexcept -> bool {
        const auto t = tail_.load(std::memory_order_relaxed);
        auto &slot = buffer_[t & (BufferSize - 1)];
        if (!slot.ready.load(std::memory_order_acquire)) {
            return false;
        }

        out = slot.value;
        slot.ready.store(false, std::memory_order_relaxed);
        tail_.store(t + 1, std::memory_order_release);
        return true;
    }

private:
    struct Slot {
        T value{};
        std::atomic<bool> ready{false};
    };

    std::array<Slot, BufferSize> buffer_{};
    alignas(k_cacheline_size) std::atomic<u64> head_{0};
    alignas(k_cacheline_size) std::atomic<u64> tail_{0};
};

enum class StreamType : u8 {
    Invalid = 0,
    Incremental = 1,
    Snapshot = 2
};
struct FeedPacket {
    static constexpr usize MarketUpdatesPerFeedPacket{16};
    using Data = std::array<MarketUpdate, MarketUpdatesPerFeedPacket>;

    struct MetadataSnapshot {
        u32 snapshot_id{};
        u16 chunk_id{};
        u16 num_chunks{};
    };
    struct Metadata {
        u64 seq_num{};
        u32 num_updates{};
        StreamType type{StreamType::Invalid};
        MetadataSnapshot snapshot_metadata{};
    };
    static_assert(sizeof(Metadata) == 24);

    Metadata metadata{};
    Data data{};
};
using IngressQueue = SPSC_NonBlocking<FeedPacket>;
using ExchangeQueue = MPSC_NonBlocking<MarketUpdate>;

class Exchange {
public:
    explicit Exchange(IngressQueue *q)
        : log_(&get_terminal_logger("Exchange")),
          q_(q),
          exchange_thread_([this](std::stop_token stop_token) {
              using namespace std::chrono_literals;

              while (!stop_token.stop_requested()) {
                  step();
                  std::this_thread::sleep_for(100ms);
              }
          }) {}

    auto subscribe_to_symbol(Symbol symbol, IngressQueue *q) -> void {
        (void)q;
        const auto id = symbol.id();
        if (subscription_threads_.contains(id)) {
            log_->log(std::format("Subscription for symbol '{}' already exists.", to_string(id)));
            return;
        }

        subscription_threads_.emplace(
            id,
            std::jthread([this, symbol](std::stop_token stop_token) {
                using namespace std::chrono_literals;
                auto &log = get_terminal_logger(std::format("Prod-{}", to_string(symbol.id())));

                while (!stop_token.stop_requested()) {
                    const auto pushed = send_data_(symbol);
                    log.log(pushed ? "Enqueued one market update." : "Failed to enqueue one market update.");
                    std::this_thread::sleep_for(random_duration_between(50ms, 700ms));
                }
            }));
        log_->log(std::format("Subscribed to symbol '{}'.", to_string(id)));
    }

    auto unsubscribe_to_symbol(Symbol symbol) -> void {
        const auto id = symbol.id();
        if (const auto it = subscription_threads_.find(id); it != subscription_threads_.end()) {
            it->second.request_stop();
            subscription_threads_.erase(it);
            log_->log(std::format("Unsubscribed from symbol '{}'.", to_string(id)));
        } else {
            log_->log(std::format("No active subscription exists for symbol '{}'.", to_string(id)));
        }
    }

private:
    [[nodiscard]] auto generate_market_update_(Symbol symbol) const noexcept -> MarketUpdate {
        const auto base_price = [&symbol] {
            switch (symbol.id()) {
            case Symbol::Id::AAPL:
                return 10000u;
            case Symbol::Id::MSFT:
                return 30000u;
            case Symbol::Id::BTCUSD:
                return 4'000'000u;
            case Symbol::Id::Invalid:
            case Symbol::Id::End:
                std::unreachable();
            }
            std::unreachable();
        }();

        return MarketUpdate{
            .symbol = symbol.id(),
            .type = static_cast<UpdateType>(random_between<u8>(1, 4)),
            .side = random_bool() ? TradeSide::Buy : TradeSide::Sell,
            .price = Price{base_price + random_between<u32>(0, 250)},
            .qty = Quantity{random_between<u32>(1, 500)},
        };
    }

    auto send_data_(Symbol symbol) -> bool {
        return exchange_queue_.push(generate_market_update_(symbol));
    }

    auto step() -> void {
        FeedPacket out{};
        out.metadata = {
            .seq_num = next_seq_number_++,
            .num_updates = 0,
            .type = StreamType::Incremental,
        };

        for (; out.metadata.num_updates < FeedPacket::MarketUpdatesPerFeedPacket; ++out.metadata.num_updates) {
            if (!exchange_queue_.pop(out.data[out.metadata.num_updates])) {
                break;
            }
        }

        if (out.metadata.num_updates == 0) {
            return;
        }

        if (!q_->push(out)) {
            log_->log("Failed to publish one feed packet.");
        }
    }

    u64 next_seq_number_{0};
    std::unordered_map<Symbol::Id, std::jthread> subscription_threads_{};
    TerminalLogger *log_{};
    IngressQueue *q_{};
    std::jthread exchange_thread_{};
    ExchangeQueue exchange_queue_{};
};

} // namespace ds_sysdes
