// app/main.cpp
#include "common.hpp"
#include <limits>

using namespace ds_sysdes;

namespace ds_sysdes {
constexpr usize k_cacheline_width{64};
constexpr u64 k_invalid_id64{std::numeric_limits<u64>::max()};

struct UserId {
    u64 id{k_invalid_id64};

    [[nodiscard]] auto is_valid() const noexcept -> bool {
        return id != k_invalid_id64;
    }
};
static_assert(sizeof(UserId) == 8);

struct FollowsEntry {
    UserId follower_id{k_invalid_id64};
    UserId followee_id{k_invalid_id64};
};
static_assert(sizeof(FollowsEntry) == 16);

// Each post takes up 3 Cache Lines, 1. CL is Metadata, 2nd+3rd taken by text
struct PostsEntry {
    u64 id{};                              // 8 byte
    UserId sender_id{};                    // 8 byte
    u64 timestamp{};                       // 8 byte
    std::array<std::byte, 40> _padding;    // 40 Byte
    std::array<unsigned char, 128> text{}; // 128 Byte
};
static_assert(sizeof(PostsEntry) == 3 * k_cacheline_width);

struct UsersEntry {
    UserId id{}; //
    std::array<unsigned char, 32> screen_name{};
    u64 profile_image_id{k_invalid_id64};
};
} // namespace ds_sysdes

int main() {
    std::println("Hello, World!");
    return 0;
}
