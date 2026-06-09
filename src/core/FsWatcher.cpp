#include "FsWatcher.h"

#include <system_error>

#if defined(__linux__)
#include <sys/inotify.h>
#include <poll.h>
#include <unistd.h>
#include <cerrno>
#include <cstring>
#include <filesystem>
#include <vector>
#endif

namespace fsvng {

FsWatcher::~FsWatcher() {
    stop();
}

void FsWatcher::stop() {
    stopRequested_.store(true);
    if (thread_.joinable()) {
        thread_.join();
    }
    stopRequested_.store(false);
    running_.store(false);
}

void FsWatcher::start(const std::string& rootPath) {
    stop();
    changeCount_.store(0);
    if (rootPath.empty()) {
        return;
    }
    running_.store(true);
    thread_ = std::thread(&FsWatcher::threadFunc, this, rootPath);
}

#if defined(__linux__)

void FsWatcher::threadFunc(std::string rootPath) {
    inotifyFd_ = inotify_init1(IN_NONBLOCK);
    if (inotifyFd_ < 0) {
        running_.store(false);
        return;
    }

    const uint32_t mask = IN_CREATE | IN_DELETE | IN_MOVED_FROM |
                          IN_MOVED_TO | IN_MODIFY | IN_ATTRIB | IN_MOVE_SELF |
                          IN_DELETE_SELF;

    // Recursively add watches, capped to avoid exhausting the inotify limit
    // on very large trees.
    const size_t kMaxWatches = 16384;
    size_t watchCount = 0;
    {
        std::error_code ec;
        inotify_add_watch(inotifyFd_, rootPath.c_str(), mask);
        ++watchCount;
        std::filesystem::recursive_directory_iterator it(
            rootPath,
            std::filesystem::directory_options::skip_permission_denied, ec);
        std::filesystem::recursive_directory_iterator end;
        for (; it != end && watchCount < kMaxWatches; it.increment(ec)) {
            if (ec) { ec.clear(); continue; }
            std::error_code dec;
            if (it->is_directory(dec) && !dec) {
                if (inotify_add_watch(inotifyFd_, it->path().c_str(), mask) >= 0) {
                    ++watchCount;
                }
            }
        }
    }

    constexpr size_t kBufLen = 64 * 1024;
    std::vector<char> buf(kBufLen);

    while (!stopRequested_.load()) {
        struct pollfd pfd;
        pfd.fd = inotifyFd_;
        pfd.events = POLLIN;
        int pr = poll(&pfd, 1, 200 /* ms */);
        if (pr <= 0) {
            continue; // timeout or EINTR: re-check stop flag
        }
        if (!(pfd.revents & POLLIN)) {
            continue;
        }

        // Drain all currently-available events; count this as one batch so a
        // burst of changes triggers a single downstream rescan.
        bool sawEvent = false;
        for (;;) {
            ssize_t n = read(inotifyFd_, buf.data(), buf.size());
            if (n <= 0) {
                break; // EAGAIN (drained) or error
            }
            sawEvent = true;
            // We don't inspect individual events; presence is enough.
        }
        if (sawEvent) {
            changeCount_.fetch_add(1);
        }
    }

    close(inotifyFd_);
    inotifyFd_ = -1;
    running_.store(false);
}

#else // non-Linux: no-op watcher

void FsWatcher::threadFunc(std::string rootPath) {
    (void)rootPath;
    // No filesystem-change backend on this platform; just idle until stopped.
    running_.store(false);
}

#endif

} // namespace fsvng
