#pragma once

#include <errno.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <sys/signalfd.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cassert>
#include <cerrno>
#include <csignal>
#include <expected>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <system_error>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "coroutine.hpp"
#include "scheduler.hpp"
#include "signal-mask.hpp"
#include "socket.hpp"

namespace asy {
struct Epoll : Scheduler {
   private:
    struct StartupContext {
        sigset_t mask;
        net::UniqueFd signal_fd;
    };

    size_t event_buff_size_ = 125;
    net::UniqueFd server_sock_;
    net::UniqueFd epoll_sock_;
    net::UniqueFd stop_sock_;
    std::vector<epoll_event> events_;
    std::unordered_map<int, std::function<void()>> signals_;
    std::unordered_set<int> register_fds_;

    std::unordered_map<int, std::unique_ptr<Coroutine>> coroutines_;
    std::unordered_map<int, std::function<void(ssize_t)>> ticks_;
    std::unordered_map<int, uint64_t> coro_key_;
    std::unordered_map<int, uint64_t> pending_erase_;

    std::function<void(int, sockaddr, socklen_t)> on_accept_;
    std::unordered_map<int, std::function<void(int)>> on_read_handlers_;
    std::unordered_map<int, std::function<void(int)>> on_write_handlers_;

    bool stop_ = false;
    bool running_ = false;

    bool updateEpoll(int fd) {
        bool haves = register_fds_.contains(fd);

        decltype(epoll_event::events) flags{};

        if (on_read_handlers_.contains(fd)) {
            flags |= EPOLLIN;
        }
        if (on_write_handlers_.contains(fd)) {
            flags |= EPOLLOUT;
        }
        flags |= EPOLLET;

        if (!(flags & (EPOLLIN | EPOLLOUT))) {
            if (haves) {
                if (epoll_ctl(epoll_sock_.fd(), EPOLL_CTL_DEL, fd, nullptr) == -1) {
                    return false;
                }
                register_fds_.erase(fd);
            }

            return true;
        }

        epoll_event ev{};
        ev.events = flags;
        ev.data.fd = fd;

        int op = haves ? EPOLL_CTL_MOD : EPOLL_CTL_ADD;

        if (epoll_ctl(epoll_sock_.fd(), op, fd, &ev) == -1) {
            return false;
        }

        if (!haves) {
            register_fds_.insert(fd);
        }

        return true;
    }

    void readEvent(epoll_event event) {
        if (!on_read_handlers_.contains(event.data.fd)) {
            return;
        }

        std::invoke(on_read_handlers_[event.data.fd], event.data.fd);
    }

    void writeEvent(epoll_event event) {
        if (!on_write_handlers_.contains(event.data.fd)) {
            return;
        }

        std::invoke(on_write_handlers_[event.data.fd], event.data.fd);
    }

    bool onContinue() {
        return !stop_;
    }

    std::optional<StartupContext> initial() {
        epoll_sock_ = std::move(net::UniqueFd(epoll_create1(EPOLL_CLOEXEC)));

        if (!epoll_sock_) {
            return std::nullopt;
        }

        if (server_sock_) {
            if (!onRead(server_sock_, [&](int server_fd) {
                    sockaddr addr{};
                    socklen_t len = sizeof(addr);
                    while (true) {
                        int fd = ::accept(server_fd, &addr, &len);

                        if (fd == -1) {
                            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                                break;
                            }
                            if (errno == ECONNABORTED) {
                                continue;
                            }
                            break;
                        }

                        if (int flags = fcntl(fd, F_GETFL); flags >= 0) {
                            if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) {
                                close(fd);
                            }
                        } else {
                            close(fd);
                        }

                        std::invoke(on_accept_, fd, addr, len);
                    }
                })) {
                return std::nullopt;
            }
        }

        sigset_t mask;
        sigemptyset(&mask);
        for (auto& sg : signals_) {
            sigaddset(&mask, sg.first);
        }

        net::UniqueFd signal_fd;

        if (signals_.size()) {
            signal_fd = signalfd(-1, &mask, SFD_NONBLOCK | SFD_CLOEXEC);

            if (!onRead(signal_fd, [&](int fd) {
                    signalfd_siginfo info{};
                    ::read(fd, &info, sizeof(info));

                    auto it = signals_.find(info.ssi_signo);
                    if (it != signals_.end()) {
                        std::invoke(it->second);
                    }
                })) {
                return std::nullopt;
            }
        }

        if (!onRead(stop_sock_, [&](int fd) {
                stop_ = true;
                uint64_t value;
                ::read(fd, &value, sizeof(value));
            })) {
            return std::nullopt;
        }

        stop_ = false;
        return StartupContext{mask, std::move(signal_fd)};
    }

   public:
    Epoll(net::UniqueFd server_sock, size_t event_buff_size = 125)
        : server_sock_(std::move(server_sock))
        , event_buff_size_(event_buff_size)
        , epoll_sock_(-1) {
        events_.resize(event_buff_size);
        stop_sock_ = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    }

    void run() {
        if (running_) {
            throw std::logic_error("Epoll::run() called twice");
        }
        running_ = true;

        auto context = initial();
        if (!context) {
            return;
        }
        sgmask::SignalMaskGuard signal_mask_guard(context->mask);

        while (onContinue()) {
            int count_events = epoll_wait(epoll_sock_, &events_[0], event_buff_size_, -1);

            if (count_events < 0) {
                if (errno == EINTR) {
                    continue;
                }
                break;
            }

            for (size_t index = 0; index < count_events; index++) {
                auto& event = events_[index];

                if (event.events & (EPOLLIN | EPOLLOUT)) {
                    if (event.events & EPOLLIN) {
                        readEvent(event);
                    }
                    if (event.events & EPOLLOUT) {
                        writeEvent(event);
                    }
                }
            }

            for (auto fd : pending_erase_) {
                if (coro_key_[fd.first] == fd.second) {
                    ticks_.erase(fd.first);
                    coroutines_.erase(fd.first);
                }
            }
            pending_erase_.clear();
        }
    }

    // ==========================================
    // Event
    // ==========================================

    void onSignal(int signal, std::function<void()> handler) {
        signals_[signal] = std::move(handler);
    }

    void onAccept(std::function<void(int, sockaddr, socklen_t)> handler) {
        on_accept_ = std::move(handler);
    }

    std::expected<void, int> onRead(int fd, std::function<void(int)> handler) override {
        on_read_handlers_[fd] = std::move(handler);

        if (!updateEpoll(fd)) {
            int code = errno;
            on_read_handlers_.erase(fd);
            return std::unexpected(code);
        }

        return {};
    }

    std::expected<void, int> onWrite(int fd, std::function<void(int)> handler) override {
        on_write_handlers_[fd] = std::move(handler);

        if (!updateEpoll(fd)) {
            int code = errno;
            on_write_handlers_.erase(fd);
            return std::unexpected(code);
        }

        return {};
    }

    void noWriteReaction(int fd) {
        on_write_handlers_.erase(fd);

        updateEpoll(fd);
    }

    void noReadReaction(int fd) {
        on_read_handlers_.erase(fd);

        updateEpoll(fd);
    }

    void cancel(int fd) override {
        on_write_handlers_.erase(fd);
        on_read_handlers_.erase(fd);
        bool ok = updateEpoll(fd);
        assert(ok && "EPOLL_CTL_DEL failed - fd was never registered");
    }

    void spawn(int fd, std::unique_ptr<Coroutine> coro) {
        auto* ptr = coro.get();
        coroutines_[fd] = std::move(coro);
        uint64_t key = ++coro_key_[fd];

        ticks_[fd] = [this, fd, key, ptr](ssize_t last_result) {
            if (coro_key_[fd] != key) {
                return;
            }
            auto awt = ptr->resume(last_result);

            if (!awt) {
                cancel(fd);
                pending_erase_[fd] = key;
                return;
            }

            awt->subscribe(*this, ticks_[fd]);
        };

        std::invoke(ticks_[fd], 0);
    }

    // ==========================================
    // Reaction
    // ==========================================

    size_t eventBuffSize() {
        return event_buff_size_;
    }

    void setEventBuffSize(size_t event_buff_size) {
        event_buff_size_ = event_buff_size;
        events_.resize(event_buff_size);
    }

    int serverFd() {
        return server_sock_;
    }

    void setServerFd(int fd) {
        server_sock_ = fd;
    }

    void stop() {
        uint64_t value;
        ::write(stop_sock_, &value, sizeof(value));
    }

    const auto& signals() const {
        return signals_;
    }
};
};  // namespace asy