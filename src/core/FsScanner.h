#pragma once

#include "FsNode.h"
#include "Types.h"

#include <atomic>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>

namespace fsvng {

// ============================================================================
// Scan statistics
// ============================================================================

struct ScanStats {
    int nodeCounts[NUM_NODE_TYPES] = {};
    int64_t sizeCounts[NUM_NODE_TYPES] = {};
    int statCount = 0;
};

// Callback invoked periodically during scanning to report progress.
using ScanProgressCallback = std::function<void(const std::string& currentDir, const ScanStats& stats)>;

// ============================================================================
// FsScanner - filesystem scanner using std::filesystem
// ============================================================================

class FsScanner {
public:
    // Scan a directory tree rooted at rootPath.
    // Returns a metanode whose first child is the scanned root directory.
    // The caller takes ownership of the returned tree.
    std::unique_ptr<FsNode> scan(const std::string& rootPath,
                                  ScanProgressCallback progressCb = nullptr);

    // Set to true from another thread to cancel a running scan
    std::atomic<bool> cancelRequested{false};

private:
    // Recursively process a directory, adding children to parentNode.
    void processDir(const std::filesystem::path& dirPath, FsNode* parentNode, int depth);

    // Map std::filesystem::file_type to our NodeType enum.
    NodeType classifyFileType(std::filesystem::file_type ft) const;

    // Populate stat fields (size, timestamps, etc.) from filesystem status.
    void populateStats(FsNode* node, const std::filesystem::path& entryPath,
                       const std::filesystem::file_status& status);

    ScanStats stats_{};
    ScanProgressCallback progressCb_;
    unsigned int nextId_ = 0;
    double lastProgressTime_ = 0.0;
};

} // namespace fsvng
