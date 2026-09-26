#pragma once

#include <expected>
#include <string>

namespace game_face {

struct Error {
    std::string message;
};

template <class T>
using Result = std::expected<T, Error>;

inline std::unexpected<Error> failure(std::string message)
{
    return std::unexpected<Error>(Error{std::move(message)});
}

} // namespace game_face
