#pragma once

#include <atomic>
#include <cstdint>
#include <string>
#include <thread>

namespace fsvng {

// ============================================================================
// FsWatcher - watch a directory tree for changes.
//
// Linux: backed by inotify (recursive watches added at start). Other
// platforms: a no-op stub (changeCount() never advances). The watcher reports
// activity via a monotonically increasing change counter rather than detailed
// events; callers debounce and re-scan when the counter moves.
// ============================================================================
class FsWatcher {
public:
    FsWatcher() = default;
    ~FsWatcher();

    FsWatcher(const FsWatcher&) = delete;
    FsWatcher& operator=(const FsWatcher&) = delete;

    // Begin watching rootPath (recursively). Replaces any existing watch.
    void start(const std::string& rootPath);

    // Stop watching and join the background thread.
    void stop();

    // Number of change batches observed since the watch started. Compare
    // against a previously-saved value to detect new activity.
    uint64_t changeCount() const { return changeCount_.load(); }

    // True while a watch thread is active.
    bool isRunning() const { return running_.load(); }

private:
    void threadFunc(std::string rootPath);

    std::thread thread_;
    std::atomic<bool> running_{false};
    std::atomic<bool> stopRequested_{false};
    std::atomic<uint64_t> changeCount_{0};
    int inotifyFd_ = -1;
};

} // namespace fsvng
