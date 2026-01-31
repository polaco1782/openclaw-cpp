#ifndef OPENCLAW_PLUGIN_REGISTRY_HPP
#define OPENCLAW_PLUGIN_REGISTRY_HPP

#include "plugin.hpp"
#include "channel.hpp"
#include "tool.hpp"
#include "../ai/ai.hpp"
#include <vector>
#include <map>
#include <string>

namespace openclaw {

// Plugin registry - manages all plugins
class PluginRegistry {
public:
    static PluginRegistry& instance() {
        static PluginRegistry registry;
        return registry;
    }
    
    // Register a plugin
    void register_plugin(Plugin* plugin) {
        if (!plugin) return;
        plugins_.push_back(plugin);
        plugin_map_[plugin->name()] = plugin;
        
        // Also track channel plugins separately
        ChannelPlugin* channel = dynamic_cast<ChannelPlugin*>(plugin);
        if (channel) {
            channels_.push_back(channel);
            channel_map_[channel->channel_id()] = channel;
        }
        
        // Also track tool plugins
        ToolPlugin* tool = dynamic_cast<ToolPlugin*>(plugin);
        if (tool) {
            tools_.push_back(tool);
            tool_map_[tool->tool_id()] = tool;
        }
        
        // Also track AI plugins
        AIPlugin* ai = dynamic_cast<AIPlugin*>(plugin);
        if (ai) {
            ai_providers_.push_back(ai);
            ai_map_[ai->provider_id()] = ai;
        }
    }
    
    // Get plugin by name
    Plugin* get_plugin(const std::string& name) {
        std::map<std::string, Plugin*>::iterator it = plugin_map_.find(name);
        return it != plugin_map_.end() ? it->second : NULL;
    }
    
    // Get channel by ID
    ChannelPlugin* get_channel(const std::string& channel_id) {
        std::map<std::string, ChannelPlugin*>::iterator it = channel_map_.find(channel_id);
        return it != channel_map_.end() ? it->second : NULL;
    }
    
    // Get tool by ID
    ToolPlugin* get_tool(const std::string& tool_id) {
        std::map<std::string, ToolPlugin*>::iterator it = tool_map_.find(tool_id);
        return it != tool_map_.end() ? it->second : NULL;
    }
    
    // Get AI provider by ID
    AIPlugin* get_ai(const std::string& provider_id) {
        std::map<std::string, AIPlugin*>::iterator it = ai_map_.find(provider_id);
        return it != ai_map_.end() ? it->second : NULL;
    }
    
    // Get first available AI provider
    AIPlugin* get_default_ai() {
        for (size_t i = 0; i < ai_providers_.size(); ++i) {
            if (ai_providers_[i]->is_initialized() && ai_providers_[i]->is_configured()) {
                return ai_providers_[i];
            }
        }
        return NULL;
    }
    
    // Get all plugins
    const std::vector<Plugin*>& plugins() const { return plugins_; }
    
    // Get all channels
    const std::vector<ChannelPlugin*>& channels() const { return channels_; }
    
    // Get all tools
    const std::vector<ToolPlugin*>& tools() const { return tools_; }
    
    // Get all AI providers
    const std::vector<AIPlugin*>& ai_providers() const { return ai_providers_; }
    
    // Initialize all plugins
    bool init_all(const Config& cfg) {
        bool all_ok = true;
        for (size_t i = 0; i < plugins_.size(); ++i) {
            if (!plugins_[i]->init(cfg)) {
                all_ok = false;
            }
        }
        return all_ok;
    }
    
    // Shutdown all plugins
    void shutdown_all() {
        // Shutdown in reverse order
        for (size_t i = plugins_.size(); i > 0; --i) {
            plugins_[i - 1]->shutdown();
        }
    }
    
    // Start all channels
    bool start_all_channels() {
        bool all_ok = true;
        for (size_t i = 0; i < channels_.size(); ++i) {
            if (!channels_[i]->start()) {
                all_ok = false;
            }
        }
        return all_ok;
    }
    
    // Stop all channels
    void stop_all_channels() {
        for (size_t i = 0; i < channels_.size(); ++i) {
            channels_[i]->stop();
        }
    }
    
    // Poll all channels
    void poll_all_channels() {
        for (size_t i = 0; i < channels_.size(); ++i) {
            channels_[i]->poll();
        }
    }
    
    // Execute a tool action
    ToolResult execute_tool(const std::string& tool_id, 
                            const std::string& action,
                            const Json& params) {
        ToolPlugin* tool = get_tool(tool_id);
        if (!tool) {
            return ToolResult::fail("Tool not found: " + tool_id);
        }
        if (!tool->supports(action)) {
            return ToolResult::fail("Tool " + tool_id + " does not support action: " + action);
        }
        return tool->execute(action, params);
    }

private:
    PluginRegistry() {}
    PluginRegistry(const PluginRegistry&);
    PluginRegistry& operator=(const PluginRegistry&);
    
    std::vector<Plugin*> plugins_;
    std::vector<ChannelPlugin*> channels_;
    std::vector<ToolPlugin*> tools_;
    std::vector<AIPlugin*> ai_providers_;
    std::map<std::string, Plugin*> plugin_map_;
    std::map<std::string, ChannelPlugin*> channel_map_;
    std::map<std::string, ToolPlugin*> tool_map_;
    std::map<std::string, AIPlugin*> ai_map_;
};

// Helper macro for static plugin registration
#define REGISTER_PLUGIN(PluginClass) \
    namespace { \
        static PluginClass plugin_instance_##PluginClass; \
        struct PluginRegistrar_##PluginClass { \
            PluginRegistrar_##PluginClass() { \
                openclaw::PluginRegistry::instance().register_plugin(&plugin_instance_##PluginClass); \
            } \
        }; \
        static PluginRegistrar_##PluginClass registrar_##PluginClass; \
    }

} // namespace openclaw

#endif // OPENCLAW_PLUGIN_REGISTRY_HPP
