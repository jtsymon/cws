#include <string.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <stdio.h>

#include "plugin.h"

void free_plugin (struct plugin_t *plugin) {
    if (plugin && plugin->libhandle) {
        dlclose (plugin->libhandle);
        plugin->libhandle = NULL;
        plugin->function = NULL;
    }
}

#define PLUGIN_PREFIX "./lib/"
#define PLUGIN_SUFFIX ".so"
int load_plugin (struct plugin_t *plugin, char *name) {
    if (!plugin) {
        return 1;
    }
    int name_len = strlen (name);
    char *rel_name = malloc (sizeof (PLUGIN_PREFIX) + name_len + sizeof (PLUGIN_SUFFIX));
    char *tmp = rel_name;
    strcpy (tmp, PLUGIN_PREFIX);
    tmp += sizeof (PLUGIN_PREFIX) - 1;
    strcpy (tmp, name);
    tmp += name_len;
    strcpy (tmp, PLUGIN_SUFFIX);
    plugin->libhandle = dlopen(rel_name, RTLD_NOW);
    if (!plugin->libhandle) {
        fprintf(stderr, "Error loading DSO: %s\n", dlerror());
        return 1;
    }
    plugin->function = dlsym(plugin->libhandle, "run");
    if (!plugin->function) {
        fprintf(stderr, "Error loading function: %s\n", dlerror());
        free_plugin (plugin);
        return 1;
    }
    return 0;
}
