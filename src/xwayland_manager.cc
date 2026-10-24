#include "xwayland-bridge/xwayland_manager.h"
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <sys/poll.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstdio>
#include <cstdlib>
#include <csignal>
#include <thread>
#include <atomic>
#include <string>
#include <iostream>

namespace fbx::xwayland {

bool XwaylandManager::spawn(const std::string& wayland_socket_name) noexcept {
    int wm_fds[2] = {-1, -1};
    int display_fds[2] = {-1, -1};

    if (socketpair(AF_UNIX, SOCK_STREAM, 0, wm_fds) < 0) {
        std::cerr << "[XwaylandManager] socketpair failed\n";
        return false;
    }

    if (pipe(display_fds) < 0) {
        std::cerr << "[XwaylandManager] pipe failed\n";
        close(wm_fds[0]);
        close(wm_fds[1]);
        return false;
    }

    const pid_t forked_pid = fork();
    if (forked_pid < 0) {
        std::cerr << "[XwaylandManager] fork failed\n";
        close(wm_fds[0]); close(wm_fds[1]);
        close(display_fds[0]); close(display_fds[1]);
        return false;
    }

    if (forked_pid == 0) {
        // Child: clear close-on-exec for inherited fds
        fcntl(wm_fds[1], F_SETFD, 0);
        fcntl(display_fds[1], F_SETFD, 0);

        // Close the parent-side ends
        close(wm_fds[0]);
        close(display_fds[0]);

        setenv("WAYLAND_DISPLAY", wayland_socket_name.c_str(), 1);

        char wm_s[8], dfd_s[8];
        snprintf(wm_s,  sizeof(wm_s),  "%d", wm_fds[1]);
        snprintf(dfd_s, sizeof(dfd_s), "%d", display_fds[1]);

        execl("/usr/local/bin/Xwayland", "Xwayland",
              "-rootless", "-wm", wm_s, "-displayfd", dfd_s, nullptr);
        // Fallback path
        execl("/usr/bin/Xwayland", "Xwayland",
              "-rootless", "-wm", wm_s, "-displayfd", dfd_s, nullptr);
        _exit(1);
    }

    // Parent
    close(wm_fds[1]);
    close(display_fds[1]);

    // Read display number with 5s timeout
    char buf[16] = {};
    struct pollfd pfd{ display_fds[0], POLLIN, 0 };
    int r = poll(&pfd, 1, 5000);
    if (r > 0) {
        (void)read(display_fds[0], buf, sizeof(buf) - 1);
    }
    close(display_fds[0]);

    int display_num = atoi(buf);
    if (display_num <= 0) display_num = 1;

    display_name_ = ":" + std::to_string(display_num);
    wm_fd_ = wm_fds[0];
    pid_   = forked_pid;

    setenv("DISPLAY", display_name_.c_str(), 1);

    // Start clipboard sync thread
    stop_flag_ = false;
    clipboard_thread_ = std::thread(clipboardThread, display_name_, std::ref(stop_flag_));

    std::cout << "[XwaylandManager] Xwayland pid=" << pid_
              << " display=" << display_name_ << "\n";
    return true;
}

void XwaylandManager::handleWmEvents() noexcept {
    // Drain available data — for a minimal WM we just read and ignore
    // to keep the fd non-blocking. In a full implementation this would
    // parse X11 protocol events and handle MapRequest/ConfigureRequest.
    char buf[4096];
    while (read(wm_fd_, buf, sizeof(buf)) > 0) {}
}

void XwaylandManager::stop() noexcept {
    stop_flag_ = true;
    if (clipboard_thread_.joinable()) clipboard_thread_.join();
    if (pid_ > 0) {
        kill(pid_, SIGTERM);
        int status;
        waitpid(pid_, &status, 0);
        pid_ = -1;
    }
    if (wm_fd_ >= 0) { close(wm_fd_); wm_fd_ = -1; }
}

XwaylandManager::~XwaylandManager() noexcept { stop(); }

/*static*/ void XwaylandManager::clipboardThread(const std::string& display_name,
                                                   std::atomic<bool>& stop_flag) noexcept {
    // Use xclip or xsel CLI tools as a simple clipboard bridge.
    // This runs in a background thread and performs periodic sync.
    (void)display_name;
    while (!stop_flag) {
        // Sleep 100ms between polls to avoid busy-looping
        struct timespec ts{0, 100'000'000};
        nanosleep(&ts, nullptr);
        // Clipboard sync via XCB would go here in a full implementation.
        // For now, X11 apps share the clipboard via Xwayland's built-in
        // PRIMARY/CLIPBOARD selection mechanisms.
    }
}

} // namespace fbx::xwayland
