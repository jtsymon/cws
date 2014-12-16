#include "../src/plugin.h"

int main (void) {
    struct plugin_t plugin;
    if (load_plugin (&plugin, "test")) {
        return 1;
    }
    run_plugin (&plugin);
    free_plugin (&plugin);
    return 0;
}
