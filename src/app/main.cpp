// app/main.cpp
#include "common.hpp"  // IWYU pragma: keep
#include "trading.hpp" // IWYU pragma: keep
#include <print>
#include <thread>

using namespace ds_sysdes;

namespace ds_sysdes {

enum class SymbolId : u32 {
    Invalid = 0,
    AAPL,
    MSFT,
    BTCUSD,
};
struct Order {
    enum class Flag : u8 {
        is_buy = 1 << 0,
        is_limit = 1 << 1,
    };
    enum class Action : u8 {
        New,
        Cancel,
    };

    u64 client_order_id{};
    u64 price_ticks{};
    u32 qty{};
    Action action{Action::New};
    Flag flags{};
};
static_assert(sizeof(Order) == 24);

struct OrderMessage {
    u32 connection_id{};
    u32 seq_num{};
    SymbolId symbol{SymbolId::Invalid};
    std::byte _pad[4]{};
    Order order{};
};
static_assert(sizeof(OrderMessage) == 40);

} // namespace ds_sysdes

int main() {
    std::println("Hello, from main2!");

    std::jthread t{[] { std::println("Hello, From Thread!"); }};

    t.join();
}
