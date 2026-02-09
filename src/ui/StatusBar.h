#pragma once

#include <string>

namespace fsvng {

class StatusBar {
public:
    static StatusBar& instance();
    void draw();
    void setMessage(const std::string& left, const std::string& right = "");

private:
    StatusBar() = default;
    std::string leftMessage_;
    std::string rightMessage_;
};

} // namespace fsvng
