#pragma once

namespace fsvng {

class Toolbar {
public:
    static Toolbar& instance();
    void draw();

private:
    Toolbar() = default;
};

} // namespace fsvng
