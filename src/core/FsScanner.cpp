#include "FsScanner.h"
#include "PlatformUtils.h"

#include <chrono>
#include <cstring>
#include <iostream>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace fsvng {

static constexpr int MAX_SCAN_DEPTH = 128;

std::unique_ptr<FsNode> FsScanner::scan(const std::string& rootPath,
                                          ScanProgressCallback progressCb) {
    progressCb_ = std::move(progressCb);
    nextId_ = 0;
    std::memset(&stats_, 0, sizeof(stats_));
    lastProgressTime_ = PlatformUtils::getTime();

    // Canonicalize the root path.
    std::error_code ec;
    std::filesystem::path canonRoot = std::filesystem::canonical(rootPath, ec);
    if (ec) {
        // Fall back to the raw path if canonical fails.
        canonRoot = std::filesystem::path(rootPath);
    }

    // Create metanode (invisible root of tree structure).
    auto metanode = std::make_unique<FsNode>();
    metanode->type = NODE_METANODE;
    metanode->id = nextId_++;
    metanode->name = "";

    // Create the root directory node.
    auto rootNode = std::make_unique<FsNode>();
    rootNode->type = NODE_DIRECTORY;
    rootNode->id = nextId_++;
    rootNode->name = canonRoot.string();

    // Stat the root directory itself.
    std::filesystem::file_status rootStatus = std::filesystem::status(canonRoot, ec);
    if (!ec) {
        populateStats(rootNode.get(), canonRoot, rootStatus);
    }
    stats_.nodeCounts[NODE_DIRECTORY]++;
    stats_.statCount++;

    // Report initial progress.
    if (progressCb_) {
        progressCb_(rootNode->name, stats_);
        lastProgressTime_ = PlatformUtils::getTime();
    }

    // Recursively scan.
    FsNode* rootRaw = rootNode.get();
    metanode->addChild(std::move(rootNode));
    processDir(canonRoot, rootRaw, 0);

    return metanode;
}

void FsScanner::processDir(const std::filesystem::path& dirPath, FsNode* parentNode, int depth) {
    if (depth >= MAX_SCAN_DEPTH || cancelRequested.load()) {
        return;
    }

    std::error_code ec;

    auto dirIt = std::filesystem::directory_iterator(dirPath,
        std::filesystem::directory_options::skip_permission_denied, ec);
    if (ec) {
        return;
    }

    while (dirIt != std::filesystem::directory_iterator()) {
        const auto& entry = *dirIt;

        // Get file status; skip entries that fail.
        std::filesystem::file_status status = entry.symlink_status(ec);
        if (ec) {
            ec.clear();
            dirIt.increment(ec);
            if (ec) break;
            continue;
        }

        // Create a new FsNode for this entry.
        auto node = std::make_unique<FsNode>();
        node->id = nextId_++;

        try {
            node->name = entry.path().filename().string();
        } catch (...) {
            dirIt.increment(ec);
            if (ec) break;
            continue;
        }

        node->type = classifyFileType(status.type());

        // Populate size, timestamps, permissions.
        populateStats(node.get(), entry.path(), status);

        // Update scan statistics.
        stats_.nodeCounts[node->type]++;
        stats_.sizeCounts[node->type] += node->size;
        stats_.statCount++;

        bool isDir = (node->type == NODE_DIRECTORY);
        FsNode* nodeRaw = parentNode->addChild(std::move(node));

        // Recurse into directories.
        if (isDir) {
            // Report progress periodically (every ~100ms).
            double now = PlatformUtils::getTime();
            if (progressCb_ && (now - lastProgressTime_) >= 0.1) {
                progressCb_(entry.path().string(), stats_);
                lastProgressTime_ = now;
            }

            processDir(entry.path(), nodeRaw, depth + 1);
        }

        // Advance iterator using non-throwing overload
        dirIt.increment(ec);
        if (ec) {
            break;
        }
    }
}

NodeType FsScanner::classifyFileType(std::filesystem::file_type ft) const {
    switch (ft) {
        case std::filesystem::file_type::directory:  return NODE_DIRECTORY;
        case std::filesystem::file_type::regular:    return NODE_REGFILE;
        case std::filesystem::file_type::symlink:    return NODE_SYMLINK;
        case std::filesystem::file_type::fifo:       return NODE_FIFO;
        case std::filesystem::file_type::socket:     return NODE_SOCKET;
        case std::filesystem::file_type::character:  return NODE_CHARDEV;
        case std::filesystem::file_type::block:      return NODE_BLOCKDEV;
        default:                                     return NODE_UNKNOWN;
    }
}

void FsScanner::populateStats(FsNode* node, const std::filesystem::path& entryPath,
                               const std::filesystem::file_status& status) {
    std::error_code ec;

    // File size - only meaningful for regular files, but query anyway.
    if (status.type() == std::filesystem::file_type::regular) {
        node->size = static_cast<int64_t>(std::filesystem::file_size(entryPath, ec));
        if (ec) {
            node->size = 0;
            ec.clear();
        }
    } else {
        node->size = 0;
    }

    // Allocated size (on-disk size).
#ifdef _WIN32
    // On Windows, use GetCompressedFileSizeW to get actual disk usage.
    if (status.type() == std::filesystem::file_type::regular) {
        DWORD highDword = 0;
        DWORD lowDword = GetCompressedFileSizeW(entryPath.c_str(), &highDword);
        if (lowDword != INVALID_FILE_SIZE || GetLastError() == NO_ERROR) {
            node->sizeAlloc = static_cast<int64_t>(
                (static_cast<uint64_t>(highDword) << 32) | lowDword);
        } else {
            node->sizeAlloc = node->size;
        }
    } else {
        node->sizeAlloc = 0;
    }
#else
    // On POSIX, we would use stat() and st_blocks * 512.
    // std::filesystem doesn't expose block counts, so we fall back to file_size.
    node->sizeAlloc = node->size;
#endif

    // Permissions (lower 12 bits of mode on POSIX; approximate on Windows).
    std::filesystem::perms p = status.permissions();
    uint16_t mode = 0;
    if ((p & std::filesystem::perms::owner_read)   != std::filesystem::perms::none) mode |= 0400;
    if ((p & std::filesystem::perms::owner_write)  != std::filesystem::perms::none) mode |= 0200;
    if ((p & std::filesystem::perms::owner_exec)   != std::filesystem::perms::none) mode |= 0100;
    if ((p & std::filesystem::perms::group_read)   != std::filesystem::perms::none) mode |= 0040;
    if ((p & std::filesystem::perms::group_write)  != std::filesystem::perms::none) mode |= 0020;
    if ((p & std::filesystem::perms::group_exec)   != std::filesystem::perms::none) mode |= 0010;
    if ((p & std::filesystem::perms::others_read)  != std::filesystem::perms::none) mode |= 0004;
    if ((p & std::filesystem::perms::others_write) != std::filesystem::perms::none) mode |= 0002;
    if ((p & std::filesystem::perms::others_exec)  != std::filesystem::perms::none) mode |= 0001;
    if ((p & std::filesystem::perms::set_uid)      != std::filesystem::perms::none) mode |= 04000;
    if ((p & std::filesystem::perms::set_gid)      != std::filesystem::perms::none) mode |= 02000;
    if ((p & std::filesystem::perms::sticky_bit)   != std::filesystem::perms::none) mode |= 01000;
    node->perms = mode;

    // Timestamps.
    auto lwt = std::filesystem::last_write_time(entryPath, ec);
    if (!ec) {
        auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
            lwt - std::filesystem::file_time_type::clock::now() +
            std::chrono::system_clock::now());
        node->mtime = std::chrono::system_clock::to_time_t(sctp);
    } else {
        node->mtime = 0;
        ec.clear();
    }
    node->atime = node->mtime;
    node->ctime = node->mtime;

    node->userId = 0;
    node->groupId = 0;
}

} // namespace fsvng
