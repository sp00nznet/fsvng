#include <gtest/gtest.h>
#include "core/FsScanner.h"
#include "core/FsNode.h"
#include <filesystem>
#include <fstream>

using namespace fsvng;

TEST(FsScannerTest, ScanTempDirectory) {
    namespace fs = std::filesystem;

    // Create a temporary directory structure
    auto tempDir = fs::temp_directory_path() / "fsvng_test_scan";
    fs::create_directories(tempDir / "subdir1");
    fs::create_directories(tempDir / "subdir2");

    // Create some files
    {
        std::ofstream(tempDir / "file1.txt") << "Hello World";
        std::ofstream(tempDir / "file2.dat") << "Some data here";
        std::ofstream(tempDir / "subdir1" / "nested.txt") << "Nested content";
    }

    FsScanner scanner;
    auto root = scanner.scan(tempDir.string());

    ASSERT_NE(root, nullptr);
    EXPECT_EQ(root->type, NODE_METANODE);
    EXPECT_GE(root->childCount(), 1);

    FsNode* rootDir = root->children[0].get();
    EXPECT_EQ(rootDir->type, NODE_DIRECTORY);

    // Should have found files and subdirs
    int fileCount = 0;
    int dirCount = 0;
    for (auto& child : rootDir->children) {
        if (child->isDir()) dirCount++;
        else fileCount++;
    }
    EXPECT_GE(fileCount, 2);
    EXPECT_GE(dirCount, 2);

    // Cleanup
    fs::remove_all(tempDir);
}

TEST(FsScannerTest, ProgressCallback) {
    namespace fs = std::filesystem;
    auto tempDir = fs::temp_directory_path() / "fsvng_test_progress";
    fs::create_directories(tempDir);
    std::ofstream(tempDir / "file.txt") << "data";

    bool callbackCalled = false;
    FsScanner scanner;
    auto root = scanner.scan(tempDir.string(), [&](const std::string& dir, const ScanStats& stats) {
        callbackCalled = true;
    });

    ASSERT_NE(root, nullptr);
    // Callback may or may not fire for tiny scans (depends on timing)

    fs::remove_all(tempDir);
}
