#ifndef OPENCLAW_PLUGIN_TOOL_HPP
#define OPENCLAW_PLUGIN_TOOL_HPP

#include "plugin.hpp"
#include "../core/json.hpp"
#include <string>
#include <vector>

namespace openclaw {

// Tool result structure
struct ToolResult {
    bool success;
    Json data;
    std::string error;
    
    ToolResult() : success(false) {}
    
    static ToolResult ok(const Json& result) {
        ToolResult r;
        r.success = true;
        r.data = result;
        return r;
    }
    
    static ToolResult fail(const std::string& err) {
        ToolResult r;
        r.success = false;
        r.error = err;
        return r;
    }
};

// Tool plugin interface - for agent tools (browser, search, etc.)
class ToolPlugin : public Plugin {
public:
    virtual ~ToolPlugin() {}
    
    // Tool metadata
    virtual const char* tool_id() const = 0;
    virtual std::vector<std::string> actions() const = 0;
    
    // Execute an action
    virtual ToolResult execute(const std::string& action, const Json& params) = 0;
    
    // Check if action is supported
    bool supports(const std::string& action) const {
        std::vector<std::string> acts = actions();
        for (size_t i = 0; i < acts.size(); ++i) {
            if (acts[i] == action) return true;
        }
        return false;
    }
};

} // namespace openclaw

#endif // OPENCLAW_PLUGIN_TOOL_HPP
