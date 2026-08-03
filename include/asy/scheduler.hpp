#pragma once

#include <expected>
#include <functional>

namespace asy {
struct Scheduler {
    virtual std::expected<void, int> onWrite(int fd, std::function<void(int)> handler) = 0;
    virtual std::expected<void, int> onRead(int fd, std::function<void(int)> handler) = 0;
    virtual void cancel(int fd) = 0;

    virtual ~Scheduler() = default;
};
};  // namespace asy