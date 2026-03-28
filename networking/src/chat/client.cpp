#include "client.hpp"

int main() {
    using namespace ds_net;

    auto& log = Logger::get_logger("client");

    const auto ai = get_ipv4_tcp_connect_info("127.0.0.1", k_default_port);
    if (!ai) {
        std::cerr << "Failed to resolve connect address\n";
        return 1;
    }

    ClientSocket client{*ai};
    constexpr i32 value_to_send = 42;
    client.send_i32(value_to_send);

    log.logln("Connected to server");
    log.logln("Sent integer: 42");

    return 0;
}
