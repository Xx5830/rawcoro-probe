#pragma once

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#include <string_view>

namespace net {
struct UniqueFd {
   private:
    int fd_ = -1;

   public:
    UniqueFd(int fd) {
        fd_ = fd;
    }
    UniqueFd() {
        fd_ = -1;
    }

    static UniqueFd socket(int domain, int type, int protocol) {
        return ::socket(domain, type, protocol);
    }

    UniqueFd(const UniqueFd& other) = delete;
    UniqueFd(UniqueFd&& other) {
        *this = std::move(other);
    }

    UniqueFd& operator=(const UniqueFd& other) = delete;
    UniqueFd& operator=(UniqueFd&& other) {
        fd_ = other.fd();
        other.fd_ = -1;
        return *this;
    }

    bool isValid() const {
        return fd_ >= 0;
    }

    int fd() const {
        return fd_;
    }

    int release() {
        int fd = fd_;
        fd_ = -1;
        return fd;
    }

    explicit operator bool() const {
        return isValid();
    }

    operator int() const {
        return fd_;
    }

    ~UniqueFd() {
        if (fd_ != -1) {
            ::close(fd_);
        }
    }
};

inline UniqueFd tcpListen(std::string_view ip, uint16_t port) {
    UniqueFd sock = UniqueFd::socket(AF_INET, SOCK_STREAM, 0);

    if (!sock) {
        return -1;
    }

    int opt = 1;
    if (setsockopt(sock.fd(), SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
        return -1;
    }

    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);

    if (inet_pton(AF_INET, ip.data(), &addr.sin_addr) != 1) {
        return -1;
    }

    if (::bind(sock.fd(), reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == -1) {
        return -1;
    }

    if (int flags = fcntl(sock.fd(), F_GETFL); flags >= 0) {
        if (fcntl(sock.fd(), F_SETFL, flags | O_NONBLOCK) < 0) {
            return -1;
        }
    } else {
        return -1;
    }

    if (::listen(sock.fd(), SOMAXCONN) == -1) {
        return -1;
    }

    return sock;
}

inline UniqueFd tcpConnect(std::string_view ip, uint16_t port) {
    UniqueFd sock = UniqueFd::socket(AF_INET, SOCK_STREAM, 0);

    if (!sock) {
        return -1;
    }

    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    if (inet_pton(AF_INET, ip.data(), &addr.sin_addr) != 1) {
        return -1;
    }

    if (int flags = fcntl(sock.fd(), F_GETFL); flags >= 0) {
        if (fcntl(sock.fd(), F_SETFL, flags | O_NONBLOCK) < 0) {
            return -1;
        }
    } else {
        return -1;
    }

    if (::connect(sock.fd(), reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == -1) {
        if (errno != EINPROGRESS) {
            return -1;
        }
    }

    return sock;
}

}  // namespace net