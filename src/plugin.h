struct plugin_t {
    void *libhandle;
    void (*function)();
};

void free_plugin (struct plugin_t *plugin);
int load_plugin (struct plugin_t *plugin, char *name);
void run_plugin (struct plugin_t *plugin);
