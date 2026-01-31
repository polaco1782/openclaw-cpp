/*
 * OpenClaw C++11 - Memory Tool Plugin Implementation
 */
#include <openclaw/tools/memory/memory.hpp>
#include <openclaw/plugin/loader.hpp>
#include <sstream>

namespace openclaw {

MemoryTool::MemoryTool() {}

MemoryTool::~MemoryTool() {
    shutdown();
}

const char* MemoryTool::name() const {
    return "memory";
}

const char* MemoryTool::description() const {
    return "Memory and task management for persistent context";
}

const char* MemoryTool::version() const {
    return "1.0.0";
}

bool MemoryTool::init(const Config& cfg) {
    // Extract memory configuration
    config_.workspace_dir = cfg.get_string("workspace_dir", ".");
    config_.db_path = cfg.get_string("memory_db_path", "");
    config_.chunking.target_tokens = cfg.get_int("memory_chunk_tokens", 400);
    config_.chunking.overlap_tokens = cfg.get_int("memory_chunk_overlap", 80);
    config_.search.max_results = cfg.get_int("memory_max_results", 10);
    config_.search.min_score = 0.1;
    
    manager_.reset(new MemoryManager(config_));
    
    if (!manager_->initialize()) {
        return false;
    }
    
    // Perform initial sync
    manager_->sync();
    
    initialized_ = true;
    return true;
}

void MemoryTool::shutdown() {
    if (manager_) {
        manager_->shutdown();
        manager_.reset();
    }
}

Json MemoryTool::get_tool_schema() const {
    Json tools;
    tools.set_type(Json::Type::ARRAY);
    
    // memory_save tool
    {
        Json tool;
        tool.set("name", "memory_save");
        tool.set("description", "Save content to long-term memory. Use for important facts, preferences, or context that should persist across sessions.");
        
        Json params;
        params.set("type", "object");
        
        Json props;
        {
            Json content;
            content.set("type", "string");
            content.set("description", "The content to save to memory");
            props.set("content", content);
        }
        {
            Json filename;
            filename.set("type", "string");
            filename.set("description", "Optional filename (default: MEMORY.md or memory/YYYY-MM-DD.md for daily)");
            props.set("filename", filename);
        }
        {
            Json daily;
            daily.set("type", "boolean");
            daily.set("description", "If true, save to daily memory file (memory/YYYY-MM-DD.md)");
            props.set("daily", daily);
        }
        {
            Json append;
            append.set("type", "boolean");
            append.set("description", "If true, append to existing file instead of overwriting");
            props.set("append", append);
        }
        
        params.set("properties", props);
        
        Json required;
        required.set_type(Json::Type::ARRAY);
        required.push("content");
        params.set("required", required);
        
        tool.set("parameters", params);
        tools.push(tool);
    }
    
    // memory_search tool
    {
        Json tool;
        tool.set("name", "memory_search");
        tool.set("description", "Search through memories for relevant information. Returns snippets ranked by relevance.");
        
        Json params;
        params.set("type", "object");
        
        Json props;
        {
            Json query;
            query.set("type", "string");
            query.set("description", "Search query - keywords or natural language");
            props.set("query", query);
        }
        {
            Json max_results;
            max_results.set("type", "integer");
            max_results.set("description", "Maximum number of results (default: 10)");
            props.set("max_results", max_results);
        }
        
        params.set("properties", props);
        
        Json required;
        required.set_type(Json::Type::ARRAY);
        required.push("query");
        params.set("required", required);
        
        tool.set("parameters", params);
        tools.push(tool);
    }
    
    // memory_get tool
    {
        Json tool;
        tool.set("name", "memory_get");
        tool.set("description", "Get the full content of a specific memory file.");
        
        Json params;
        params.set("type", "object");
        
        Json props;
        {
            Json path;
            path.set("type", "string");
            path.set("description", "Path to the memory file (e.g., 'MEMORY.md' or 'memory/2024-01-15.md')");
            props.set("path", path);
        }
        
        params.set("properties", props);
        
        Json required;
        required.set_type(Json::Type::ARRAY);
        required.push("path");
        params.set("required", required);
        
        tool.set("parameters", params);
        tools.push(tool);
    }
    
    // memory_list tool
    {
        Json tool;
        tool.set("name", "memory_list");
        tool.set("description", "List all memory files.");
        
        Json params;
        params.set("type", "object");
        params.set("properties", Json());  // No properties
        
        tool.set("parameters", params);
        tools.push(tool);
    }
    
    // task_create tool
    {
        Json tool;
        tool.set("name", "task_create");
        tool.set("description", "Create a task or reminder for later.");
        
        Json params;
        params.set("type", "object");
        
        Json props;
        {
            Json content;
            content.set("type", "string");
            content.set("description", "The task description");
            props.set("content", content);
        }
        {
            Json context;
            context.set("type", "string");
            context.set("description", "Additional context or notes");
            props.set("context", context);
        }
        {
            Json due_at;
            due_at.set("type", "integer");
            due_at.set("description", "Due date as Unix timestamp in milliseconds (0 = no due date)");
            props.set("due_at", due_at);
        }
        
        params.set("properties", props);
        
        Json required;
        required.set_type(Json::Type::ARRAY);
        required.push("content");
        params.set("required", required);
        
        tool.set("parameters", params);
        tools.push(tool);
    }
    
    // task_complete tool
    {
        Json tool;
        tool.set("name", "task_complete");
        tool.set("description", "Mark a task as completed.");
        
        Json params;
        params.set("type", "object");
        
        Json props;
        {
            Json task_id;
            task_id.set("type", "string");
            task_id.set("description", "The task ID to complete");
            props.set("task_id", task_id);
        }
        
        params.set("properties", props);
        
        Json required;
        required.set_type(Json::Type::ARRAY);
        required.push("task_id");
        params.set("required", required);
        
        tool.set("parameters", params);
        tools.push(tool);
    }
    
    // task_list tool
    {
        Json tool;
        tool.set("name", "task_list");
        tool.set("description", "List all tasks.");
        
        Json params;
        params.set("type", "object");
        
        Json props;
        {
            Json include_completed;
            include_completed.set("type", "boolean");
            include_completed.set("description", "Whether to include completed tasks (default: false)");
            props.set("include_completed", include_completed);
        }
        
        params.set("properties", props);
        
        tool.set("parameters", params);
        tools.push(tool);
    }
    
    return tools;
}

Json MemoryTool::execute(const std::string& function_name, const Json& params) {
    if (!manager_) {
        return make_error("Memory tool not initialized");
    }
    
    if (function_name == "memory_save") {
        return memory_save(params);
    } else if (function_name == "memory_search") {
        return memory_search(params);
    } else if (function_name == "memory_get") {
        return memory_get(params);
    } else if (function_name == "memory_list") {
        return memory_list(params);
    } else if (function_name == "task_create") {
        return task_create(params);
    } else if (function_name == "task_complete") {
        return task_complete(params);
    } else if (function_name == "task_list") {
        return task_list(params);
    }
    
    return make_error("Unknown function: " + function_name);
}

MemoryManager* MemoryTool::memory_manager() {
    return manager_.get();
}

Json MemoryTool::memory_save(const Json& params) {
    std::string content = params.get_string("content", "");
    if (content.empty()) {
        return make_error("Content is required");
    }
    
    bool daily = params.get_bool("daily", false);
    bool append = params.get_bool("append", false);
    std::string filename = params.get_string("filename", "");
    
    bool ok;
    if (daily) {
        ok = manager_->save_daily_memory(content);
    } else if (append) {
        std::string target = filename.empty() ? "MEMORY.md" : filename;
        ok = manager_->append_to_memory(content, target);
    } else {
        ok = manager_->save_memory(content, filename);
    }
    
    if (ok) {
        return make_success("Memory saved successfully");
    } else {
        return make_error("Failed to save memory: " + manager_->last_error());
    }
}

Json MemoryTool::memory_search(const Json& params) {
    std::string query = params.get_string("query", "");
    if (query.empty()) {
        return make_error("Query is required");
    }
    
    MemorySearchConfig config;
    config.max_results = params.get_int("max_results", 10);
    
    std::vector<MemorySearchResult> results = manager_->search(query, config);
    
    Json response;
    response.set("success", true);
    
    Json items;
    items.set_type(Json::Type::ARRAY);
    
    for (const auto& r : results) {
        Json item;
        item.set("path", r.path);
        item.set("score", r.score);
        item.set("snippet", r.snippet);
        item.set("start_line", r.start_line);
        item.set("end_line", r.end_line);
        item.set("source", memory_source_to_string(r.source));
        items.push(item);
    }
    
    response.set("results", items);
    response.set("count", static_cast<int>(results.size()));
    
    return response;
}

Json MemoryTool::memory_get(const Json& params) {
    std::string path = params.get_string("path", "");
    if (path.empty()) {
        return make_error("Path is required");
    }
    
    std::string content = manager_->get_memory_content(path);
    
    if (content.empty()) {
        return make_error("File not found or empty: " + path);
    }
    
    Json response;
    response.set("success", true);
    response.set("path", path);
    response.set("content", content);
    
    return response;
}

Json MemoryTool::memory_list(const Json& params) {
    (void)params;  // Unused
    
    std::vector<std::string> files = manager_->list_memory_files();
    
    Json response;
    response.set("success", true);
    
    Json items;
    items.set_type(Json::Type::ARRAY);
    for (const auto& f : files) {
        items.push(f);
    }
    
    response.set("files", items);
    response.set("count", static_cast<int>(files.size()));
    
    return response;
}

Json MemoryTool::task_create(const Json& params) {
    std::string content = params.get_string("content", "");
    if (content.empty()) {
        return make_error("Content is required");
    }
    
    std::string context = params.get_string("context", "");
    std::string task_id = manager_->create_task(content, context);
    
    if (task_id.empty()) {
        return make_error("Failed to create task");
    }
    
    // Update due date if provided
    int64_t due_at = params.get_int("due_at", 0);
    if (due_at > 0) {
        manager_->update_task_due(task_id, due_at);
    }
    
    Json response;
    response.set("success", true);
    response.set("task_id", task_id);
    response.set("message", "Task created successfully");
    
    return response;
}

Json MemoryTool::task_complete(const Json& params) {
    std::string task_id = params.get_string("task_id", "");
    if (task_id.empty()) {
        return make_error("Task ID is required");
    }
    
    if (manager_->complete_task(task_id)) {
        return make_success("Task completed successfully");
    } else {
        return make_error("Failed to complete task");
    }
}

Json MemoryTool::task_list(const Json& params) {
    bool include_completed = params.get_bool("include_completed", false);
    
    std::vector<MemoryTask> tasks = manager_->list_tasks(include_completed);
    
    Json response;
    response.set("success", true);
    
    Json items;
    items.set_type(Json::Type::ARRAY);
    
    for (const auto& t : tasks) {
        Json item;
        item.set("id", t.id);
        item.set("content", t.content);
        item.set("context", t.context);
        item.set("created_at", static_cast<int>(t.created_at));
        item.set("due_at", static_cast<int>(t.due_at));
        item.set("completed", t.completed);
        if (t.completed) {
            item.set("completed_at", static_cast<int>(t.completed_at));
        }
        items.push(item);
    }
    
    response.set("tasks", items);
    response.set("count", static_cast<int>(tasks.size()));
    
    return response;
}

Json MemoryTool::make_error(const std::string& message) {
    Json response;
    response.set("success", false);
    response.set("error", message);
    return response;
}

Json MemoryTool::make_success(const std::string& message) {
    Json response;
    response.set("success", true);
    response.set("message", message);
    return response;
}

} // namespace openclaw

// Export plugin for dynamic loading
OPENCLAW_DECLARE_PLUGIN(openclaw::MemoryTool, "memory", "1.0.0", 
                        "Memory and task management tool", "tool")
