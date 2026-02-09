#include "app/App.h"

int main(int argc, char* argv[]) {
    fsvng::App app;

    if (!app.init(argc, argv)) {
        return 1;
    }

    app.run();
    app.shutdown();

    return 0;
}
