#include "xwayland-bridge/wayland_bridge.h"
#include "xwayland-bridge/xwayland_manager.h"
#include "shared/protocol.h"
#include <iostream>
#include <vector>
#include <array>
#include <sys/poll.h>
#include <sys/socket.h>
#include <unistd.h>
#include <csignal>
#include <cstring>
#include <time.h>

using namespace fbx::xwayland;

namespace {
    bool running = true;
    void handle_signal(int sig) {
        if (sig == SIGINT || sig == SIGTERM) running = false;
    }
}

int main() {
    std::cout << "Starting xwayland-bridge...\n";

    std::signal(SIGINT,  handle_signal);
    std::signal(SIGTERM, handle_signal);

    auto bridge_res = WaylandBridge::create();
    if (!bridge_res.has_value()) {
        std::cerr << "Fatal: Failed to initialize Wayland Bridge.\n";
        return 1;
    }

    auto bridge = std::move(bridge_res.value());
    std::cout << "[xwayland-bridge] Ready to accept standard Wayland applications.\n";

    // Spawn Xwayland connected to the bridge's Wayland socket
    fbx::xwayland::XwaylandManager xwl_mgr;
    if (xwl_mgr.spawn(bridge->getSocketName())) {
        std::cout << "[xwayland-bridge] Xwayland running on "
                  << xwl_mgr.displayName() << "\n";
        // Notify compositor of DISPLAY via XBus
        if (bridge->ipcFd() >= 0) {
            fbx::shared::XBusMessage xmsg{};
            xmsg.magic_ = fbx::shared::XBUS_MAGIC;
            std::strncpy(xmsg.service_name_, "com.freebsdx.XWayland",
                         sizeof(xmsg.service_name_) - 1);
            xmsg.opcode_ = static_cast<uint32_t>(fbx::shared::XWaylandOpcode::StatusChanged);
            fbx::shared::XWaylandStatus st{};
            st.running_ = 1;
            st.pid_     = xwl_mgr.pid();
            std::strncpy(st.display_, xwl_mgr.displayName().c_str(), sizeof(st.display_) - 1);
            std::memcpy(xmsg.payload_, &st, sizeof(st));
            const uint8_t* p = reinterpret_cast<const uint8_t*>(&xmsg);
            // send() with MSG_EOR, not write(): the compositor socket is
            // SOCK_SEQPACKET and needs an explicit end-of-record marker, or this
            // message coalesces with adjacent ones (see IpcSocket::sendMessage).
            // write() has no flags argument and so can never frame the record.
            (void)send(bridge->ipcFd(), p, sizeof(xmsg), MSG_EOR);
        }
    } else {
        std::cerr << "[xwayland-bridge] Warning: Xwayland not available\n";
    }

    // fds[0] = Wayland event loop fd (incoming client requests)
    // fds[1] = IPC fd to freebsd-x compositor (SurfaceReady, input events)
    // fds[2] = X11 WM socket fd (Xwayland rootless WM events)
    std::array<pollfd, 3> fds{};
    fds[0].fd     = bridge->getEventFd();
    fds[0].events = POLLIN;
    fds[1].fd     = bridge->ipcFd(); // -1 if compositor not connected → poll ignores it
    fds[1].events = POLLIN;
    fds[2].fd     = xwl_mgr.wmFd(); // -1 if Xwayland not running → poll ignores it
    fds[2].events = POLLIN;

    while (running) {
        const int ret = poll(fds.data(), fds.size(), 100);

        if (ret < 0) {
            if (errno == EINTR) continue;
            break;
        }

        if (ret > 0) {
            // Drain all pending Wayland client requests
            if (fds[0].revents & POLLIN) {
                bridge->dispatch();
            }

            // Handle X11 WM events from Xwayland
            if (fds[2].fd >= 0 && (fds[2].revents & POLLIN)) {
                xwl_mgr.handleWmEvents();
            }

            // Drain all pending messages from the compositor
            if (fds[1].fd >= 0 && (fds[1].revents & POLLIN)) {
                // IpcSocket::receiveMessage() is used indirectly; call bridge helper
                // We drain in a loop so a burst of SurfaceReady events doesn't lag
                while (true) {
                    // Use the socket directly via the stored fd so we don't need to
                    // expose receiveMessage() from WaylandBridge's private IpcSocket.
                    // Instead, we read the raw bytes and hand them to handleIpcMessage().
                    char buf[4096];
                    char ctrl[CMSG_SPACE(sizeof(int))]{};

                    struct iovec iov{ .iov_base = buf, .iov_len = sizeof(buf) };
                    struct msghdr mh{};
                    mh.msg_iov        = &iov;
                    mh.msg_iovlen     = 1;
                    mh.msg_control    = ctrl;
                    mh.msg_controllen = sizeof(ctrl);

                    ssize_t n = recvmsg(fds[1].fd, &mh, MSG_DONTWAIT);
                    if (n <= 0) break;

                    int received_fd = -1;
                    if (auto* cmsg = CMSG_FIRSTHDR(&mh);
                        cmsg && cmsg->cmsg_level == SOL_SOCKET &&
                        cmsg->cmsg_type == SCM_RIGHTS) {
                        std::memcpy(&received_fd, CMSG_DATA(cmsg), sizeof(int));
                    }

                    std::vector<uint8_t> data(
                        reinterpret_cast<uint8_t*>(buf),
                        reinterpret_cast<uint8_t*>(buf) + n);
                    bridge->handleIpcMessage(data, received_fd);
                    if (received_fd >= 0) close(received_fd);
                }
            }
        }

        // Poll idle notifications with current monotonic time
        {
            struct timespec ts{};
            clock_gettime(CLOCK_MONOTONIC, &ts);
            uint32_t now_ms = static_cast<uint32_t>(
                (uint64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
            bridge->pollIdleNotifications(now_ms);
        }

        bridge->flush();
    }

    std::cout << "\nxwayland-bridge shutting down.\n";
    return 0;
}
