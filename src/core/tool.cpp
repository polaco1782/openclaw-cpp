/*
 * OpenClaw C++11 - Tool Provider Implementation
 */
#include <openclaw/core/tool.hpp>
#include <openclaw/core/agent.hpp>

namespace openclaw {

std::vector<AgentTool> ToolProvider::get_agent_tools() const {
    // Default implementation: create a generic wrapper for each action
    // Subclasses should override this to provide detailed descriptions
    std::vector<AgentTool> tools;
    
    const std::vector<std::string> action_list = actions();
    const std::string id = tool_id();
    const std::string desc = description();
    
    for (size_t i = 0; i < action_list.size(); ++i) {
        AgentTool tool;
        tool.name = id + "_" + action_list[i];
        tool.description = desc + " - " + action_list[i] + " action";
        tool.params.push_back(ToolParamSchema("params", "object", "Action parameters", false));
        tools.push_back(tool);
    }
    
    return tools;
}

} // namespace openclaw
