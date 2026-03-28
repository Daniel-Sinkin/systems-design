#pragma once

#include "common.hpp"
#include "net.hpp"

#include <system_error>
#include <utility>

namespace ds_net {

inline auto send_exactly(FileDescriptor fd, const void* data, usize byte_count) -> void {
    const auto* bytes = static_cast<const std::byte*>(data);
    usize sent = 0;

    while (sent < byte_count) {
        const auto remaining = byte_count - sent;
        const auto n = send(fd, bytes + sent, remaining, 0);
        if (n == -1) {
            DS_THROW_ERRNO("send()");
        }
        sent += static_cast<usize>(n);
    }
}

class ClientSocket {
public:
    explicit ClientSocket(const AddressInfo& ai) {
        fd_ = socket(ai.family, ai.socket_type, ai.protocol);
        if (fd_ == k_invalid_fd) {
            DS_THROW_ERRNO("socket()");
        }

        const auto raw = to_raw_addr(ai.address);
        if (connect(fd_, reinterpret_cast<const sockaddr*>(&raw.storage), raw.len) == -1) {
            close(fd_);
            DS_THROW_ERRNO("connect()");
        }
    }

    ClientSocket(const ClientSocket&) = delete;
    ClientSocket& operator=(const ClientSocket&) = delete;
    ClientSocket(ClientSocket&& other) noexcept { steal_from(std::move(other)); }
    ClientSocket& operator=(ClientSocket&& other) noexcept {
        if (this != &other) {
            steal_from(std::move(other));
        }
        return *this;
    }
    ~ClientSocket() { destroy(); }

    auto send_i32(i32 value) const -> void {
        const auto wire_value = htonl(static_cast<u32>(value));
        send_exactly(fd_, &wire_value, sizeof(wire_value));
    }

private:
    FileDescriptor fd_{k_invalid_fd};

    auto steal_from(ClientSocket&& other) -> void {
        destroy();
        fd_ = std::exchange(other.fd_, k_invalid_fd);
    }

    auto destroy() -> void {
        if (fd_ != k_invalid_fd) {
            close(fd_);
            fd_ = k_invalid_fd;
        }
    }
};

} // namespace ds_net
