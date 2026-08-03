#pragma once

#include <sys/types.h>
#include <unistd.h>

#include <cstddef>
#include <cstdio>
#include <functional>
#include <memory>
#include <stdexcept>

#include "scheduler.hpp"

namespace asy {
struct Awaitable {
    virtual void subscribe(Scheduler& loop, std::function<void(ssize_t)> tick) = 0;
    virtual ~Awaitable() = default;
};

struct ReadAwaitable : Awaitable {
    int fd_;
    char* buff_;
    size_t size_;

    ReadAwaitable(int fd, char* buff, size_t size)
        : fd_(fd)
        , buff_(buff)
        , size_(size) {}

    void subscribe(Scheduler& loop, std::function<void(ssize_t)> tick) override {
        auto result = loop.onRead(fd_, [buff = buff_, size = size_, tick](int fd) {
            ssize_t total = 0;

            while (total < static_cast<ssize_t>(size)) {
                ssize_t getting = ::read(fd, buff + total, size - total);

                if (getting == -1) {
                    if (errno == EINTR) {
                        continue;
                    }
                    if (errno == EAGAIN) {
                        if (total == 0) {
                            // have many threaded problem
                            return;
                        }
                        break;
                    }
                    std::invoke(tick, -1);
                    return;
                }
                if (getting == 0) {
                    std::invoke(tick, total > 0 ? total : -1);
                    return;
                }

                total += getting;
            }

            if (total == 0) {
                return;
            }
            std::invoke(tick, total);
        });

        if (!result) {
            std::invoke(tick, -1);
        }
    }

    ~ReadAwaitable() {}
};

struct WriteAwaitable : Awaitable {
    int fd_;
    char* buff_;
    size_t size_;

    WriteAwaitable(int fd, char* buff, size_t size)
        : fd_(fd)
        , buff_(buff)
        , size_(size) {}

    void subscribe(Scheduler& loop, std::function<void(ssize_t)> tick) override {
        auto result = loop.onWrite(fd_, [buff = buff_, size = size_, tick](int fd) {
            ssize_t total = 0;

            while (total < static_cast<ssize_t>(size)) {
                ssize_t getting = ::write(fd, buff + total, size - total);

                if (getting == -1) {
                    if (errno == EINTR)
                        continue;
                    if (errno == EAGAIN)
                        break;
                    std::invoke(tick, -1);
                    return;
                }

                total += getting;
            }

            std::invoke(tick, total);
        });

        if (!result) {
            std::invoke(tick, -1);
        }
    }

    ~WriteAwaitable() {}
};

struct Coroutine {
    virtual std::unique_ptr<Awaitable> resume(ssize_t last_result) = 0;
    virtual ~Coroutine() = default;
};
}  // namespace asy