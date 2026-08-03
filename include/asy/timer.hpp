#pragma once

#include <sys/timerfd.h>

#include <ctime>

#include "socket.hpp"

namespace asy {
inline net::UniqueFd createTimer(int ms) {
    net::UniqueFd fd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);

    

    return std::move(fd);
}
}  // namespace asy