#include "server.hpp"

int main() {
    using namespace ds_net;

    auto& log = Logger::get_logger("server");

    const auto ai = get_ipv4_tcp_address_info(k_default_port);
    if (!ai) {
        std::cerr << "Failed to get address info\n";
        return 1;
    }

    ServerSocket server{*ai};
    log.logln("Listening on port " + std::to_string(k_default_port));

    auto connection = server.accept_connection();
    log.logln("Accepted one client connection");

    const auto value = connection.receive_i32();
    if (!value) {
        log.logln("Client disconnected before sending an integer");
        return 1;
    }

    log.logln("Received integer: " + std::to_string(*value));
    return 0;
}
