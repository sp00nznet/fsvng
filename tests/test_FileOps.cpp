#include <gtest/gtest.h>

#include "core/FileOps.h"

#include <filesystem>
#include <fstream>

using namespace fsvng;
namespace fs = std::filesystem;

namespace {
fs::path scratch(const char* tag) {
    fs::path d = fs::temp_directory_path() / (std::string("fsvops_") + tag);
    fs::remove_all(d);
    fs::create_directories(d);
    return d;
}
void writeFile(const fs::path& p, const char* content = "x") {
    std::ofstream(p) << content;
}
} // namespace

TEST(FileOps, RenameMovesFile) {
    fs::path d = scratch("rename");
    writeFile(d / "old.txt");

    auto r = FileOps::rename((d / "old.txt").string(), "new.txt");
    EXPECT_TRUE(r.ok) << r.error;
    EXPECT_FALSE(fs::exists(d / "old.txt"));
    EXPECT_TRUE(fs::exists(d / "new.txt"));

    fs::remove_all(d);
}

TEST(FileOps, RenameRejectsSeparatorsAndDuplicates) {
    fs::path d = scratch("rename_bad");
    writeFile(d / "a.txt");
    writeFile(d / "b.txt");

    EXPECT_FALSE(FileOps::rename((d / "a.txt").string(), "sub/c.txt").ok);
    EXPECT_FALSE(FileOps::rename((d / "a.txt").string(), "").ok);
    EXPECT_FALSE(FileOps::rename((d / "a.txt").string(), "..").ok);
    // Target already exists.
    EXPECT_FALSE(FileOps::rename((d / "a.txt").string(), "b.txt").ok);
    // Original still intact after failed renames.
    EXPECT_TRUE(fs::exists(d / "a.txt"));

    fs::remove_all(d);
}

TEST(FileOps, CreateFolder) {
    fs::path d = scratch("mkdir");

    auto r = FileOps::createFolder(d.string(), "newdir");
    EXPECT_TRUE(r.ok) << r.error;
    EXPECT_TRUE(fs::is_directory(d / "newdir"));

    // Duplicate is rejected.
    EXPECT_FALSE(FileOps::createFolder(d.string(), "newdir").ok);
    // Bad names rejected.
    EXPECT_FALSE(FileOps::createFolder(d.string(), "a/b").ok);

    fs::remove_all(d);
}

TEST(FileOps, MoveIntoAnotherDirectory) {
    fs::path d = scratch("move");
    fs::create_directories(d / "src");
    fs::create_directories(d / "dst");
    writeFile(d / "src" / "f.txt", "hello");

    auto r = FileOps::moveInto((d / "src" / "f.txt").string(), (d / "dst").string());
    EXPECT_TRUE(r.ok) << r.error;
    EXPECT_FALSE(fs::exists(d / "src" / "f.txt"));
    EXPECT_TRUE(fs::exists(d / "dst" / "f.txt"));

    // Moving onto an existing name is rejected.
    writeFile(d / "src" / "f.txt");
    fs::create_directories(d / "dst2");
    writeFile(d / "dst2" / "f.txt");
    EXPECT_FALSE(FileOps::moveInto((d / "src" / "f.txt").string(),
                                   (d / "dst2").string()).ok);

    fs::remove_all(d);
}
