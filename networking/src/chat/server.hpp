#pragma once

#include "common.hpp"
#include "net.hpp"

#include <optional>
#include <system_error>
#include <utility>

namespace ds_net {

inline auto recv_exactly(FileDescriptor fd, void* data, usize byte_count) -> bool {
    auto* bytes = static_cast<std::byte*>(data);
    usize received = 0;

    while (received < byte_count) {
        const auto remaining = byte_count - received;
        const auto n = recv(fd, bytes + received, remaining, 0);
        if (n == 0) {
            if (received == 0) {
                return false;
            }
            throw std::runtime_error("Peer disconnected mid-message");
        }
        if (n == -1) {
            DS_THROW_ERRNO("recv()");
        }
        received += static_cast<usize>(n);
    }

    return true;
}

class ConnectedSocket {
public:
    explicit ConnectedSocket(FileDescriptor fd) : fd_(fd) {}

    ConnectedSocket(const ConnectedSocket&) = delete;
    ConnectedSocket& operator=(const ConnectedSocket&) = delete;
    ConnectedSocket(ConnectedSocket&& other) noexcept { steal_from(std::move(other)); }
    ConnectedSocket& operator=(ConnectedSocket&& other) noexcept {
        if (this != &other) {
            steal_from(std::move(other));
        }
        return *this;
    }
    ~ConnectedSocket() { destroy(); }

    [[nodiscard]] auto receive_i32() const -> std::optional<i32> {
        u32 wire_value = 0;
        if (!recv_exactly(fd_, &wire_value, sizeof(wire_value))) {
            return std::nullopt;
        }
        return static_cast<i32>(ntohl(wire_value));
    }

private:
    FileDescriptor fd_{k_invalid_fd};

    auto steal_from(ConnectedSocket&& other) -> void {
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

class ServerSocket {
public:
    explicit ServerSocket(const AddressInfo& ai) {
        fd_ = socket(ai.family, ai.socket_type, ai.protocol);
        if (fd_ == k_invalid_fd) {
            DS_THROW_ERRNO("socket()");
        }

        int reuse_addr = 1;
        if (setsockopt(fd_, SOL_SOCKET, SO_REUSEADDR, &reuse_addr, sizeof(reuse_addr)) == -1) {
            close(fd_);
            DS_THROW_ERRNO("setsockopt()");
        }

        const auto raw = to_raw_addr(ai.address);
        if (bind(fd_, reinterpret_cast<const sockaddr*>(&raw.storage), raw.len) == -1) {
            close(fd_);
            DS_THROW_ERRNO("bind()");
        }
        if (listen(fd_, k_backlog) == -1) {
            close(fd_);
            DS_THROW_ERRNO("listen()");
        }
    }

    ServerSocket(const ServerSocket&) = delete;
    ServerSocket& operator=(const ServerSocket&) = delete;
    ServerSocket(ServerSocket&& other) noexcept { steal_from(std::move(other)); }
    ServerSocket& operator=(ServerSocket&& other) noexcept {
        if (this != &other) {
            steal_from(std::move(other));
        }
        return *this;
    }
    ~ServerSocket() { destroy(); }

    [[nodiscard]] auto accept_connection() const -> ConnectedSocket {
        sockaddr_storage client_addr{};
        socklen_t addr_len = sizeof(client_addr);
        const auto client_fd = accept(fd_, reinterpret_cast<sockaddr*>(&client_addr), &addr_len);
        if (client_fd == k_invalid_fd) {
            DS_THROW_ERRNO("accept()");
        }
        return ConnectedSocket{client_fd};
    }

private:
    static constexpr int k_backlog = 5;
    FileDescriptor fd_{k_invalid_fd};

    auto steal_from(ServerSocket&& other) -> void {
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
