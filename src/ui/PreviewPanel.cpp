#include "ui/PreviewPanel.h"

#include <imgui.h>
#include <glad/gl.h>

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <fstream>
#include <filesystem>
#include <vector>

#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_STDIO
#include <stb_image.h>

#include "core/FsNode.h"
#include "ui/MainWindow.h"

namespace fsvng {

namespace {

constexpr int64_t kMaxTextBytes = 1 * 1024 * 1024;    // 1 MB text cap
constexpr int64_t kMaxImageBytes = 32 * 1024 * 1024;  // 32 MB image cap

std::string lowerExt(const std::string& name) {
    auto dot = name.find_last_of('.');
    if (dot == std::string::npos) return "";
    std::string ext = name.substr(dot + 1);
    std::transform(ext.begin(), ext.end(), ext.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return ext;
}

bool isImageExt(const std::string& ext) {
    static const char* kExts[] = {"png", "jpg", "jpeg", "bmp", "gif",
                                  "tga", "psd", "hdr", "pic", "ppm", "pgm"};
    for (const char* e : kExts) {
        if (ext == e) return true;
    }
    return false;
}

// Heuristic: treat a byte buffer as binary if it contains a NUL or a high
// fraction of non-printable, non-whitespace bytes in its leading window.
bool looksBinary(const std::vector<char>& buf) {
    size_t window = std::min<size_t>(buf.size(), 8192);
    size_t suspicious = 0;
    for (size_t i = 0; i < window; ++i) {
        unsigned char c = static_cast<unsigned char>(buf[i]);
        if (c == 0) return true;
        bool printable = (c >= 0x20 && c < 0x7f) || c == '\n' || c == '\r' ||
                         c == '\t' || c >= 0x80;  // allow UTF-8 high bytes
        if (!printable) ++suspicious;
    }
    return window > 0 && (suspicious * 100 / window) > 10;
}

} // namespace

PreviewPanel& PreviewPanel::instance() {
    static PreviewPanel s;
    return s;
}

void PreviewPanel::releaseState(SceneState& st) {
    if (st.texture_ != 0) {
        glDeleteTextures(1, &st.texture_);
        st.texture_ = 0;
    }
}

void PreviewPanel::clear() {
    if (s_->texture_ != 0) {
        glDeleteTextures(1, &s_->texture_);
        s_->texture_ = 0;
    }
    s_->kind_ = Kind::None;
    s_->text_.clear();
    s_->truncated_ = false;
    s_->imgW_ = s_->imgH_ = 0;
    s_->info_.clear();
}

void PreviewPanel::loadFor(FsNode* node) {
    clear();
    s_->currentNode_ = node;

    if (!node || node->isDir() || node->isMetanode()) {
        s_->kind_ = Kind::None;
        return;
    }

    const std::string path = node->absName();
    const std::string ext = lowerExt(node->name);

    // ---- Image branch ----------------------------------------------------
    if (isImageExt(ext)) {
        if (node->size > kMaxImageBytes) {
            s_->kind_ = Kind::TooLarge;
            s_->info_ = "Image too large to preview";
            return;
        }
        std::ifstream f(path, std::ios::binary);
        if (!f) { s_->kind_ = Kind::Unreadable; s_->info_ = "Cannot open file"; return; }
        std::vector<unsigned char> raw((std::istreambuf_iterator<char>(f)),
                                       std::istreambuf_iterator<char>());
        int w = 0, h = 0, comp = 0;
        stbi_uc* pixels = stbi_load_from_memory(raw.data(),
                                                static_cast<int>(raw.size()),
                                                &w, &h, &comp, 4);
        if (!pixels) {
            s_->kind_ = Kind::Binary;
            s_->info_ = "Unsupported or corrupt image";
            return;
        }
        glGenTextures(1, &s_->texture_);
        glBindTexture(GL_TEXTURE_2D, s_->texture_);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA,
                     GL_UNSIGNED_BYTE, pixels);
        glBindTexture(GL_TEXTURE_2D, 0);
        stbi_image_free(pixels);

        s_->imgW_ = w;
        s_->imgH_ = h;
        s_->kind_ = Kind::Image;
        s_->info_ = std::to_string(w) + " x " + std::to_string(h);
        return;
    }

    // ---- Text branch -----------------------------------------------------
    std::ifstream f(path, std::ios::binary);
    if (!f) { s_->kind_ = Kind::Unreadable; s_->info_ = "Cannot open file"; return; }

    int64_t readLen = std::min<int64_t>(node->size > 0 ? node->size : kMaxTextBytes,
                                        kMaxTextBytes);
    std::vector<char> buf(static_cast<size_t>(readLen));
    f.read(buf.data(), static_cast<std::streamsize>(buf.size()));
    buf.resize(static_cast<size_t>(f.gcount()));

    if (looksBinary(buf)) {
        s_->kind_ = Kind::Binary;
        s_->info_ = "Binary file (no preview)";
        return;
    }

    s_->text_.assign(buf.begin(), buf.end());
    s_->truncated_ = (node->size > kMaxTextBytes);
    s_->kind_ = Kind::Text;
}

void PreviewPanel::draw() {
    ImGui::Begin("Preview");

    FsNode* node = MainWindow::instance().getCurrentNode();
    if (node != s_->currentNode_) {
        loadFor(node);
    }

    switch (s_->kind_) {
        case Kind::None:
            ImGui::TextDisabled("Select a file to preview");
            break;

        case Kind::Text: {
            ImGui::Text("%s", s_->currentNode_ ? s_->currentNode_->name.c_str() : "");
            if (s_->truncated_) {
                ImGui::SameLine();
                ImGui::TextDisabled("(truncated)");
            }
            ImGui::Separator();
            ImGui::BeginChild("##previewtext", ImVec2(0, 0), false,
                              ImGuiWindowFlags_HorizontalScrollbar);
            ImGui::TextUnformatted(s_->text_.c_str(), s_->text_.c_str() + s_->text_.size());
            ImGui::EndChild();
            break;
        }

        case Kind::Image: {
            ImGui::Text("%s  (%s)", s_->currentNode_ ? s_->currentNode_->name.c_str() : "",
                        s_->info_.c_str());
            ImGui::Separator();
            if (s_->texture_ != 0 && s_->imgW_ > 0 && s_->imgH_ > 0) {
                ImVec2 avail = ImGui::GetContentRegionAvail();
                float scale = std::min(avail.x / s_->imgW_, avail.y / s_->imgH_);
                if (scale > 1.0f) scale = 1.0f;        // don't upscale
                if (scale <= 0.0f) scale = 1.0f;
                ImVec2 size(s_->imgW_ * scale, s_->imgH_ * scale);
                ImGui::Image(static_cast<ImTextureID>(s_->texture_), size);
            }
            break;
        }

        case Kind::Binary:
        case Kind::TooLarge:
        case Kind::Unreadable:
            ImGui::Text("%s", s_->currentNode_ ? s_->currentNode_->name.c_str() : "");
            ImGui::Separator();
            ImGui::TextDisabled("%s", s_->info_.c_str());
            break;
    }

    ImGui::End();
}

} // namespace fsvng
