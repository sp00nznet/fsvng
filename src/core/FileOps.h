#pragma once

#include <string>

namespace fsvng {

// ============================================================================
// FileOps - mutating filesystem operations used by the UI.
//
// Deletes go to the desktop trash (recoverable), never an unconditional
// unlink. If no trash backend is available, trashAvailable() returns false and
// the UI should disable delete rather than fall back to permanent removal.
// ============================================================================
namespace FileOps {

struct Result {
    bool ok = false;
    std::string error;
};

// True if a trash backend is available (Linux: the `gio` tool on PATH).
bool trashAvailable();

// Move a file or directory to the desktop trash.
Result moveToTrash(const std::string& path);

// Rename an entry in place. newName must be a bare filename (no separators).
Result rename(const std::string& path, const std::string& newName);

// Create a new directory `name` inside parentDir.
Result createFolder(const std::string& parentDir, const std::string& name);

// Move src (file or directory) into destDir, keeping its basename. Used by
// drag-and-drop between panes.
Result moveInto(const std::string& src, const std::string& destDir);

} // namespace FileOps
} // namespace fsvng
