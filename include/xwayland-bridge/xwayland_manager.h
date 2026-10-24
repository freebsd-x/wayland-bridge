#pragma once
#include <string>
#include <atomic>
#include <thread>
#include <mutex>

namespace fbx::xwayland {

// Manages the Xwayland process lifecycle.
// Spawned after the bridge Wayland display is ready.
// Provides a minimal X11 WM over the `wm_fd` socketpair (needed for rootless mode).
// Runs clipboard sync between X11 CLIPBOARD and Wayland clipboard in a background thread.
class XwaylandManager {
public:
    XwaylandManager() = default;
    ~XwaylandManager() noexcept;

    // Spawn Xwayland connected to `wayland_socket_name` (e.g. "wayland-0").
    // Blocks until Xwayland reports its display number via displayfd.
    // Returns true on success.
    bool spawn(const std::string& wayland_socket_name) noexcept;

    bool isRunning() const noexcept { return pid_ > 0; }
    const std::string& displayName() const noexcept { return display_name_; } // ":1"
    int pid() const noexcept { return static_cast<int>(pid_); }

    // Poll fd for the X11 WM socket (returns -1 if not running).
    // Must be added to the bridge's poll loop; call handleWmEvents() when readable.
    int wmFd() const noexcept { return wm_fd_; }

    // Read and process incoming X11 WM protocol events (MAP_REQUEST etc.).
    void handleWmEvents() noexcept;

    void stop() noexcept;

private:
    static void clipboardThread(const std::string& display_name,
                                std::atomic<bool>& stop_flag) noexcept;

    pid_t              pid_          = -1;
    int                wm_fd_        = -1;  // our end of WM socketpair
    std::string        display_name_;
    std::atomic<bool>  stop_flag_{false};
    std::thread        clipboard_thread_;
};

} // namespace fbx::xwayland
