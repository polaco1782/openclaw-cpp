/*
 * OpenClaw C++11 - Memory Tool Plugin
 * 
 * Provides memory_save, memory_search, memory_get, and task operations.
 */
#ifndef OPENCLAW_TOOLS_MEMORY_HPP
#define OPENCLAW_TOOLS_MEMORY_HPP

#include "../../plugin/plugin.hpp"
#include "../../memory/manager.hpp"
#include "../../core/json.hpp"
#include <memory>

namespace openclaw {

class MemoryTool : public Plugin {
public:
    MemoryTool();
    virtual ~MemoryTool();
    
    // Plugin interface
    const char* name() const override;
    const char* description() const override;
    const char* version() const override;
    
    bool init(const Config& cfg) override;
    void shutdown() override;
    
    // Tool schema for AI function calling
    Json get_tool_schema() const;
    
    // Execute tool call
    Json execute(const std::string& function_name, const Json& params);
    
    // Direct access to memory manager for testing
    MemoryManager* memory_manager();
    
private:
    std::unique_ptr<MemoryManager> manager_;
    MemoryConfig config_;
    
    // Tool implementations
    Json memory_save(const Json& params);
    Json memory_search(const Json& params);
    Json memory_get(const Json& params);
    Json memory_list(const Json& params);
    Json task_create(const Json& params);
    Json task_complete(const Json& params);
    Json task_list(const Json& params);
    
    Json make_error(const std::string& message);
    Json make_success(const std::string& message);
};

} // namespace openclaw

#endif // OPENCLAW_TOOLS_MEMORY_HPP
