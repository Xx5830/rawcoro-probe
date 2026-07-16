#pragma once

#include <fcntl.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#include <vector>

#include "socket.hpp"

namespace asy {
struct Epoll {
   private:
    size_t event_buff_size_ = 125;
    net::Socket server_sock_;
    net::Socket epoll_sock_;
    std::vector<epoll_event> events_;

    void acceptEvent(epoll_event event) {
        sockaddr addr{};
        socklen_t len = sizeof(addr);
        for (int fd = ::accept(server_sock_, &addr, &len); fd != -1; fd = ::accept(server_sock_, &addr, &len)) {
            if (int flags = fcntl(fd, F_GETFL); flags >= 0) {
                if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) {
                    close(fd);
                }
            } else {
                close(fd);
            }

            epoll_event event_info;
            event_info.data.fd = fd;
            event_info.events = EPOLLIN | EPOLLET;
            epoll_ctl(epoll_sock_, EPOLL_CTL_ADD, fd, &event_info);
        }
    }

    void connectEvent(epoll_event event) {}

   public:
    Epoll(net::Socket server_sock, size_t event_buff_size = 125)
        : server_sock_(std::move(server_sock))
        , event_buff_size_(event_buff_size)
        , epoll_sock_(-1) {
        events_.resize(event_buff_size);
    }

    size_t eventBuffSize() {
        return event_buff_size_;
    }

    void setEventBuffSize(size_t event_buff_size) {
        event_buff_size_ = event_buff_size;
        events_.resize(event_buff_size);
    }

    bool run() {
        if (!server_sock_) {
            return false;
        }

        epoll_sock_ = std::move(net::Socket(epoll_create1(EPOLL_CLOEXEC)));

        if (!epoll_sock_) {
            return false;
        }

        {
            epoll_event ev{};
            ev.events = EPOLLIN | EPOLLET;
            ev.data.fd = server_sock_.fd();
            if (epoll_ctl(epoll_sock_.fd(), EPOLL_CTL_ADD, server_sock_.fd(), &ev) == -1) {
                return false;
            }
        }

        while (true) {
            int count_events = epoll_wait(epoll_sock_, &events_[0], event_buff_size_, -1);

            if (count_events < 0) {
                if (errno == EINTR) {
                    continue;
                }
                break;
            }

            for (size_t index = 0; index < count_events; index++) {
                auto& event = events_[index];

                if (event.data.fd == server_sock_) {
                    acceptEvent(event);
                } else {
                    connectEvent(event);
                }
            }
        }

        return true;
    }

    size_t serverFd() {
        return server_sock_;
    }

    void setServerFd(int fd) {
        server_sock_ = fd;
    }
};
};  // namespace asy