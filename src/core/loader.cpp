/*
 * OpenClaw C++11 - Dynamic Plugin Loader Implementation
 */
#include <openclaw/core/loader.hpp>
#include <openclaw/core/logger.hpp>
#include <dlfcn.h>
#include <dirent.h>
#include <sys/stat.h>
#include <cstring>

namespace openclaw {

PluginLoader::PluginLoader() {
    // Default search paths
    search_paths_.push_back("./plugins");
    search_paths_.push_back("/usr/lib/openclaw/plugins");
    search_paths_.push_back("/usr/local/lib/openclaw/plugins");
}

PluginLoader::~PluginLoader() {
    unload_all();
}

bool PluginLoader::load(const std::string& path) {
    // Check if it's a full path or just a name
    std::string full_path;
    struct stat st;
    
    if (path.find('/') != std::string::npos && stat(path.c_str(), &st) == 0) {
        full_path = path;
    } else {
        full_path = find_plugin(path);
        if (full_path.empty()) {
            set_error("Plugin not found: " + path);
            return false;
        }
    }
    
    LoadedPlugin plugin;
    if (!load_impl(full_path, plugin)) {
        return false;
    }
    
    // Check for duplicate
    if (name_index_.find(plugin.info.name) != name_index_.end()) {
        LOG_WARN("Plugin %s already loaded, skipping", plugin.info.name);
        if (plugin.destroy_func && plugin.instance) {
            plugin.destroy_func(plugin.instance);
        }
        dlclose(plugin.handle);
        return true;  // Not an error
    }
    
    name_index_[plugin.info.name] = plugins_.size();
    plugins_.push_back(plugin);
    
    LOG_INFO("Loaded plugin: %s v%s (%s)", 
             plugin.info.name, plugin.info.version, plugin.info.type);
    
    return true;
}

bool PluginLoader::load_impl(const std::string& path, LoadedPlugin& plugin) {
    // Open shared library
    void* handle = dlopen(path.c_str(), RTLD_NOW | RTLD_GLOBAL);
    if (!handle) {
        set_error(std::string("dlopen failed: ") + dlerror());
        return false;
    }
    
    plugin.handle = handle;
    plugin.path = path;
    
    // Get plugin info function
    GetPluginInfoFunc get_info = (GetPluginInfoFunc)dlsym(handle, "openclaw_get_plugin_info");
    if (!get_info) {
        set_error("Plugin missing openclaw_get_plugin_info: " + path);
        dlclose(handle);
        return false;
    }
    
    // Get create/destroy functions
    plugin.create_func = (CreatePluginFunc)dlsym(handle, "openclaw_create_plugin");
    plugin.destroy_func = (DestroyPluginFunc)dlsym(handle, "openclaw_destroy_plugin");
    
    if (!plugin.create_func) {
        set_error("Plugin missing openclaw_create_plugin: " + path);
        dlclose(handle);
        return false;
    }
    
    // Get plugin info
    plugin.info = get_info();
    
    // Create plugin instance
    plugin.instance = plugin.create_func();
    if (!plugin.instance) {
        set_error("Failed to create plugin instance: " + path);
        dlclose(handle);
        return false;
    }
    
    return true;
}

int PluginLoader::load_dir(const std::string& dir) {
    DIR* d = opendir(dir.c_str());
    if (!d) {
        return 0;
    }
    
    int count = 0;
    struct dirent* entry;
    
    while ((entry = readdir(d)) != nullptr) {
        std::string name = entry->d_name;
        
        // Skip hidden files and non-.so files
        if (name[0] == '.') continue;
        if (name.length() < 4) continue;
        if (name.substr(name.length() - 3) != ".so") continue;
        
        std::string path = dir + "/" + name;
        if (load(path)) {
            count++;
        }
    }
    
    closedir(d);
    return count;
}

int PluginLoader::load_from_config(const Config& config) {
    int count = 0;
    
    // Get plugin list from config
    const Json& plugins_config = config.get_section("plugins");
    
    if (plugins_config.is_array()) {
        // Array of plugin names/paths
        for (const auto& item : plugins_config) {
            if (item.is_string()) {
                std::string name = item.get<std::string>();
                if (!name.empty() && load(name)) {
                    count++;
                }
            }
        }
    } else if (plugins_config.is_object()) {
        // Object with plugin names as keys, config as values
        for (auto it = plugins_config.begin(); it != plugins_config.end(); ++it) {
            // Check if plugin is enabled
            const Json& plugin_cfg = it.value();
            if (plugin_cfg.is_object()) {
                bool enabled = true;
                if (plugin_cfg.contains("enabled")) {
                    enabled = plugin_cfg["enabled"].get<bool>();
                }
                if (!enabled) {
                    LOG_DEBUG("Plugin %s disabled in config", it.key().c_str());
                    continue;
                }
            }
            
            if (load(it.key())) {
                count++;
            }
        }
    }
    
    // Note: plugins_dir is used as a search path, not for auto-loading
    // Only plugins explicitly listed in "plugins" array/object will be loaded
    
    return count;
}

void PluginLoader::unload(const std::string& name) {
    std::map<std::string, size_t>::iterator it = name_index_.find(name);
    if (it == name_index_.end()) {
        return;
    }
    
    size_t idx = it->second;
    LoadedPlugin& plugin = plugins_[idx];
    
    // Shutdown and destroy plugin instance
    if (plugin.instance) {
        plugin.instance->shutdown();
        if (plugin.destroy_func) {
            plugin.destroy_func(plugin.instance);
        }
        plugin.instance = nullptr;
    }
    
    // Close shared library
    if (plugin.handle) {
        dlclose(plugin.handle);
        plugin.handle = nullptr;
    }
    
    LOG_INFO("Unloaded plugin: %s", name.c_str());
    
    // Remove from maps (mark as unloaded but keep in vector to preserve indices)
    name_index_.erase(it);
}

void PluginLoader::unload_all() {
    // Unload in reverse order
    for (size_t i = plugins_.size(); i > 0; --i) {
        LoadedPlugin& plugin = plugins_[i - 1];
        
        if (plugin.instance) {
            plugin.instance->shutdown();
            if (plugin.destroy_func) {
                plugin.destroy_func(plugin.instance);
            }
            plugin.instance = nullptr;
        }
        
        if (plugin.handle) {
            dlclose(plugin.handle);
            plugin.handle = nullptr;
        }
    }
    
    plugins_.clear();
    name_index_.clear();
}

Plugin* PluginLoader::get(const std::string& name) {
    std::map<std::string, size_t>::iterator it = name_index_.find(name);
    if (it == name_index_.end()) {
        return nullptr;
    }
    return plugins_[it->second].instance;
}

const std::vector<LoadedPlugin>& PluginLoader::plugins() const {
    return plugins_;
}

bool PluginLoader::is_loaded(const std::string& name) const {
    return name_index_.find(name) != name_index_.end();
}

std::string PluginLoader::last_error() const {
    return last_error_;
}

const std::vector<std::string>& PluginLoader::search_paths() const {
    return search_paths_;
}

void PluginLoader::add_search_path(const std::string& path) {
    search_paths_.insert(search_paths_.begin(), path);
}

std::string PluginLoader::find_plugin(const std::string& name) {
    // Try various name formats
    std::vector<std::string> candidates;
    candidates.push_back(name);
    candidates.push_back(name + ".so");
    candidates.push_back("lib" + name + ".so");
    candidates.push_back("openclaw_" + name + ".so");
    candidates.push_back("libopenclaw_" + name + ".so");
    
    struct stat st;
    for (size_t i = 0; i < search_paths_.size(); ++i) {
        for (size_t j = 0; j < candidates.size(); ++j) {
            std::string path = search_paths_[i] + "/" + candidates[j];
            if (stat(path.c_str(), &st) == 0 && S_ISREG(st.st_mode)) {
                return path;
            }
        }
    }
    
    return "";
}

void PluginLoader::set_error(const std::string& error) {
    last_error_ = error;
    LOG_ERROR("%s", error.c_str());
}

} // namespace openclaw
