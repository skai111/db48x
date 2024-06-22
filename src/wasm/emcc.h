#include <emscripten/bind.h>
using namespace emscripten;

EMSCRIPTEN_BINDINGS(db48x_module) {
    function("ui_battery", &ui_battery);
    function("ui_update_pixmap", &ui_update_pixmap);
    function("ui_run_rpl", &ui_run_rpl);
    function("ui_init", &ui_init);
    function("ui_push_key", &key_push);
}
