#include "FileOps.h"

#include <filesystem>
#include <system_error>

#if !defined(_WIN32)
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <cstdlib>
#include <cstring>
#endif

namespace fsvng {
namespace FileOps {

namespace fs = std::filesystem;

namespace {

// True if `name` contains a path separator (rejected for rename targets).
bool hasSeparator(const std::string& name) {
    return name.find('/') != std::string::npos ||
           name.find('\\') != std::string::npos;
}

#if !defined(_WIN32)
// Search $PATH for an executable named `tool`.
bool toolOnPath(const char* tool) {
    const char* path = std::getenv("PATH");
    if (!path) return false;
    std::string p(path);
    size_t start = 0;
    while (start <= p.size()) {
        size_t colon = p.find(':', start);
        std::string dir = p.substr(start, colon == std::string::npos
                                              ? std::string::npos
                                              : colon - start);
        if (!dir.empty()) {
            std::error_code ec;
            fs::path candidate = fs::path(dir) / tool;
            if (fs::exists(candidate, ec) && !ec) {
                return true;
            }
        }
        if (colon == std::string::npos) break;
        start = colon + 1;
    }
    return false;
}

// Run argv synchronously and return its exit status (-1 on spawn failure).
int runSync(const char* const argv[]) {
    pid_t pid = fork();
    if (pid < 0) {
        return -1;
    }
    if (pid == 0) {
        execvp(argv[0], const_cast<char* const*>(argv));
        _exit(127);
    }
    int status = 0;
    if (waitpid(pid, &status, 0) < 0) {
        return -1;
    }
    if (WIFEXITED(status)) {
        return WEXITSTATUS(status);
    }
    return -1;
}
#endif

} // namespace

bool trashAvailable() {
#if defined(_WIN32) || defined(__APPLE__)
    // No trash backend wired up on these platforms yet.
    return false;
#else
    return toolOnPath("gio");
#endif
}

Result moveToTrash(const std::string& path) {
    if (path.empty()) {
        return {false, "empty path"};
    }
#if defined(_WIN32) || defined(__APPLE__)
    return {false, "trash not supported on this platform"};
#else
    if (!trashAvailable()) {
        return {false, "the 'gio' tool is required to move files to Trash"};
    }
    const char* argv[] = {"gio", "trash", "--", path.c_str(), nullptr};
    int rc = runSync(argv);
    if (rc == 0) {
        return {true, ""};
    }
    return {false, "failed to move to Trash (gio exit " + std::to_string(rc) + ")"};
#endif
}

Result rename(const std::string& path, const std::string& newName) {
    if (newName.empty()) {
        return {false, "name cannot be empty"};
    }
    if (hasSeparator(newName) || newName == "." || newName == "..") {
        return {false, "invalid file name"};
    }
    fs::path src(path);
    fs::path dst = src.parent_path() / newName;
    std::error_code ec;
    if (fs::exists(dst, ec)) {
        return {false, "a file named '" + newName + "' already exists"};
    }
    fs::rename(src, dst, ec);
    if (ec) {
        return {false, ec.message()};
    }
    return {true, ""};
}

Result createFolder(const std::string& parentDir, const std::string& name) {
    if (name.empty()) {
        return {false, "name cannot be empty"};
    }
    if (hasSeparator(name) || name == "." || name == "..") {
        return {false, "invalid folder name"};
    }
    fs::path dir = fs::path(parentDir) / name;
    std::error_code ec;
    if (fs::exists(dir, ec)) {
        return {false, "a file or folder named '" + name + "' already exists"};
    }
    if (!fs::create_directory(dir, ec) || ec) {
        return {false, ec ? ec.message() : std::string("could not create folder")};
    }
    return {true, ""};
}

Result moveInto(const std::string& src, const std::string& destDir) {
    fs::path s(src);
    fs::path d = fs::path(destDir) / s.filename();
    std::error_code ec;

    if (fs::equivalent(s.parent_path(), fs::path(destDir), ec) && !ec) {
        return {false, "source is already in the destination"};
    }
    ec.clear();
    if (fs::exists(d, ec)) {
        return {false, "an entry named '" + s.filename().string() +
                       "' already exists in the destination"};
    }
    // Try a fast rename first; fall back to copy+remove across filesystems.
    fs::rename(s, d, ec);
    if (!ec) {
        return {true, ""};
    }
    ec.clear();
    fs::copy(s, d, fs::copy_options::recursive, ec);
    if (ec) {
        return {false, ec.message()};
    }
    fs::remove_all(s, ec);
    if (ec) {
        return {false, "moved, but could not remove original: " + ec.message()};
    }
    return {true, ""};
}

} // namespace FileOps
} // namespace fsvng
