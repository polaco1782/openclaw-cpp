/*
 * OpenClaw C++11 - Personal AI Assistant Framework
 * 
 * A minimal, modular implementation inspired by OpenClaw
 * with dynamic plugin loading support.
 * 
 * Usage:
 *   ./openclaw [config.json]
 * 
 * All configuration is read from config.json file.
 */

#include <openclaw/core/types.hpp>
#include <openclaw/core/logger.hpp>
#include <openclaw/core/config.hpp>
#include <openclaw/core/json.hpp>
#include <openclaw/core/utils.hpp>
#include <openclaw/core/thread_pool.hpp>
#include <openclaw/core/registry.hpp>
#include <openclaw/core/loader.hpp>
#include <openclaw/core/channel.hpp>
#include <openclaw/core/tool.hpp>
#include <openclaw/ai/ai.hpp>
#include <openclaw/core/session.hpp>
#include <openclaw/core/rate_limiter.hpp>
#include <openclaw/core/agent.hpp>
#include <openclaw/core/browser_tool.hpp>
#include <openclaw/core/commands.hpp>
#include <openclaw/skills/manager.hpp>
#include <openclaw/skills/types.hpp>

#include <iostream>
#include <csignal>
#include <cstring>
#include <unistd.h>
#include <curl/curl.h>
#include <sstream>

namespace openclaw {

// ============================================================================
// Application Configuration
// ============================================================================

static const char* APP_VERSION = "0.5.0";
static const char* APP_NAME = "OpenClaw C++11";

// Default system prompt for the AI assistant
static const char* DEFAULT_SYSTEM_PROMPT = 
    "You are OpenClaw, a helpful AI assistant running on a minimal C++11 framework. "
    "You are friendly, concise, and helpful. Keep responses brief unless asked for detail. "
    "You can help with questions, coding, and general conversation.\n\n"
    "You have access to tools that let you browse the web and manage memory/tasks.";

static std::vector<std::string> split_message_chunks(const std::string& text, size_t max_len) {
    std::vector<std::string> chunks;
    if (text.size() <= max_len) {
        chunks.push_back(text);
        return chunks;
    }

    size_t start = 0;
    while (start < text.size()) {
        size_t remaining = text.size() - start;
        if (remaining <= max_len) {
            chunks.push_back(text.substr(start));
            break;
        }

        size_t end = start + max_len;
        size_t split_pos = text.rfind('\n', end);
        if (split_pos == std::string::npos || split_pos <= start) {
            std::string slice = text.substr(start, max_len);
            slice = truncate_safe(slice, max_len);
            if (slice.empty()) {
                break;
            }
            chunks.push_back(slice);
            start += slice.size();
        } else {
            chunks.push_back(text.substr(start, split_pos - start));
            start = split_pos + 1;
        }

        while (start < text.size() && text[start] == '\n') {
            ++start;
        }
    }

    return chunks;
}

// ============================================================================
// Application State
// ============================================================================

class Application {
public:
    // Singleton access
    static Application& instance() {
        static Application app;
        return app;
    }
    
    // Configuration
    Config& config() { return config_; }
    PluginLoader& loader() { return loader_; }
    PluginRegistry& registry() { return PluginRegistry::instance(); }
    SessionManager& sessions() { return SessionManager::instance(); }
    ThreadPool& thread_pool() { return thread_pool_; }
    SkillManager& skills() { return skill_manager_; }
    const std::vector<SkillEntry>& skill_entries() const { return skill_entries_; }
    const std::vector<SkillCommandSpec>& skill_commands() const { return skill_command_specs_; }
    Agent& agent() { return agent_; }
    
    // State
    bool is_running() const { return running_; }
    void stop() { running_ = false; }
    
    // Rate limiting
    KeyedRateLimiter& user_limiter() { return user_limiter_; }
    MessageDebouncer& debouncer() { return debouncer_; }
    TypingIndicator& typing() { return typing_; }
    
    // System prompt (can be customized via config)
    const std::string& system_prompt() const { return system_prompt_; }
    void set_system_prompt(const std::string& prompt) { system_prompt_ = prompt; }
    void rebuild_system_prompt();
    
    // Initialize the application
    bool init(int argc, char* argv[]);
    
    // Run the main loop
    int run();
    
    // Cleanup
    void shutdown();

private:
    Application() 
        : running_(true)
        , user_limiter_(KeyedRateLimiter::TOKEN_BUCKET, 10, 2)
        , debouncer_(5)
        , thread_pool_(8)  // 8 worker threads for better concurrency tolerance
        , system_prompt_(DEFAULT_SYSTEM_PROMPT) {}
    
    Application(const Application&);
    Application& operator=(const Application&);
    
    bool running_;
    Config config_;
    PluginLoader loader_;
    KeyedRateLimiter user_limiter_;
    MessageDebouncer debouncer_;
    TypingIndicator typing_;
    ThreadPool thread_pool_;
    std::string system_prompt_;
    SkillManager skill_manager_;
    std::vector<SkillEntry> skill_entries_;
    std::vector<SkillCommandSpec> skill_command_specs_;
    Agent agent_;
};

// Global signal handler
static void signal_handler(int sig) {
    (void)sig;
    Application::instance().stop();
    LOG_INFO("Received shutdown signal");
}

// ============================================================================
// Helper Functions
// ============================================================================

// Helper to notify plugins about outgoing messages
void notify_outgoing_message(const std::string& channel_id, const std::string& to,
                              const std::string& text, const std::string& reply_to = "") {
    Application& app = Application::instance();
    
    // Create a pseudo-message for the outgoing response
    Message out_msg;
    out_msg.id = "bot-" + std::to_string(std::time(nullptr));
    out_msg.channel = channel_id;
    out_msg.from = "bot";
    out_msg.from_name = "OpenClaw Bot";
    out_msg.to = to;
    out_msg.text = text;
    out_msg.chat_type = "private";
    out_msg.timestamp = std::time(nullptr);
    out_msg.reply_to_id = reply_to;
    
    // Notify all plugins (especially gateway)
    const std::vector<Plugin*>& plugins = app.registry().plugins();
    for (size_t i = 0; i < plugins.size(); ++i) {
        if (plugins[i] && plugins[i]->is_initialized()) {
            plugins[i]->on_incoming_message(out_msg);
        }
    }
}

// ============================================================================
// Message Handler (Orchestrator)
// ============================================================================

void process_message(const Message msg) {
    Application& app = Application::instance();
    
    LOG_DEBUG("Processing message from %s: %s", msg.from_name.c_str(), msg.text.c_str());
    
    ChannelPlugin* channel = app.registry().get_channel(msg.channel);
    if (!channel || msg.text.empty()) {
        return;
    }
    
    std::string response;
    
    // Get session for this message
    Session& session = app.sessions().get_session_for_message(msg);
    
    // Handle commands (start with /)
    if (msg.text[0] == '/') {
        // Parse command and arguments
        std::string cmd_text = msg.text;
        
        // Handle @botname suffix (e.g., /help@mybot)
        size_t at_pos = cmd_text.find('@');
        if (at_pos != std::string::npos) {
            size_t space_pos = cmd_text.find(' ');
            if (space_pos == std::string::npos || at_pos < space_pos) {
                cmd_text = cmd_text.substr(0, at_pos) + 
                          (space_pos != std::string::npos ? cmd_text.substr(space_pos) : "");
            }
        }
        
        // Check if this is a skill command
        std::pair<const SkillCommandSpec*, std::string> skill_cmd = 
            app.skills().resolve_skill_command_invocation(cmd_text, app.skill_commands());
        
        LOG_DEBUG("[Skills] Skill command resolution result: %s", 
                  skill_cmd.first ? "MATCHED" : "NO MATCH");
        
        if (skill_cmd.first) {
            // This is a skill command!
            const SkillCommandSpec* spec = skill_cmd.first;
            const std::string& skill_args = skill_cmd.second;
            
            LOG_INFO("Skill command invoked: %s (args: %s)", spec->skill_name.c_str(), skill_args.c_str());
            
            if (!spec->dispatch.empty() && spec->dispatch.kind == "tool") {
                // Direct tool dispatch - not implemented yet
                response = "⚠️ Direct tool dispatch not yet implemented for skill '" + spec->skill_name + "'";
                LOG_WARN("Direct tool dispatch requested but not implemented");
            } else {
                // AI-mediated dispatch via agentic loop
                LOG_DEBUG("AI-mediated skill dispatch via agentic loop: %s", spec->skill_name.c_str());
                
                // Find the skill entry to get SKILL.md location
                const SkillEntry* entry = app.skills().find_skill_by_name(spec->skill_name, app.skill_entries());
                LOG_DEBUG("[Skills] Skill entry lookup: %s", entry ? "FOUND" : "NOT FOUND");
                
                if (entry) {
                    // Rewrite prompt: "Use skill X. Read SKILL.md, then: <user args>"
                    std::string rewritten = "Use the '" + spec->skill_name + "' skill. ";
                    rewritten += "First, read its instructions using the read tool: ";
                    rewritten += entry->skill.file_path + ". ";
                    rewritten += "Then follow those instructions to complete this task: ";
                    rewritten += skill_args;
                    
                    LOG_DEBUG("Rewritten prompt: %s", rewritten.c_str());
                    LOG_DEBUG("System prompt size: %zu chars", app.system_prompt().size());
                    
                    // Route to AI via agentic loop
                    AIPlugin* ai = app.registry().get_default_ai();
                    LOG_DEBUG("[Skills] AI provider: %s", ai ? (ai->is_configured() ? "READY" : "NOT CONFIGURED") : "NULL");
                    
                    if (ai && ai->is_configured()) {
                        app.typing().start_typing(msg.to);
                        
                        LOG_INFO("[Skills] Starting agentic loop for skill: %s", spec->skill_name.c_str());
                        
                        AgentConfig agent_config;
                        agent_config.max_iterations = 15;
                        
                        AgentResult agent_result = app.agent().run(
                            ai, 
                            rewritten, 
                            session.history(), 
                            app.system_prompt(),
                            agent_config
                        );
                        
                        LOG_DEBUG("[Skills] Agent loop completed: success=%s, iterations=%d, tool_calls=%d",
                                  agent_result.success ? "yes" : "no", 
                                  agent_result.iterations, 
                                  agent_result.tool_calls_made);
                        
                        if (agent_result.success) {
                            response = agent_result.final_response;
                            LOG_INFO("Skill executed via %d tool calls", agent_result.tool_calls_made);
                        } else {
                            response = "❌ Skill execution failed: " + agent_result.error;
                            LOG_ERROR("Skill execution failed: %s", agent_result.error.c_str());
                        }
                        
                        app.typing().stop_typing(msg.to);
                    } else {
                        response = "⚠️ AI not configured for skill execution. Check your config.json.";
                        LOG_WARN("Cannot execute skill - AI not configured");
                    }
                } else {
                    response = "⚠️ Skill '" + spec->skill_name + "' not found in loaded entries";
                    LOG_WARN("Skill command matched but skill entry not found: %s", spec->skill_name.c_str());
                }
            }
            
            // Send response
            LOG_DEBUG("[Skills] Sending response (%zu chars)", response.size());
            if (!response.empty()) {
                SendResult result = channel->send_message(msg.to, response, msg.id);
                if (result.success) {
                    notify_outgoing_message(msg.channel, msg.to, response, msg.id);
                } else {
                    LOG_ERROR("Failed to send skill response: %s", result.error.c_str());
                }
            }
            return;
        }
        
        // Not a skill command - extract command name and arguments
        size_t space_pos = cmd_text.find(' ');
        std::string command = (space_pos != std::string::npos) ? 
            cmd_text.substr(0, space_pos) : cmd_text;
        std::string args = (space_pos != std::string::npos) ? 
            cmd_text.substr(space_pos + 1) : "";
        
        // Handle built-in /skills command
        if (command == "/skills") {
            response = "**Available Skills:**\n\n";
            response += app.skills().list_skills_for_display(app.skill_entries(), true);
            response += "\n💡 Use `/skillname <args>` or `/skill skillname <args>` to invoke a skill";

            std::vector<std::string> chunks = split_message_chunks(response, 3500);
            for (size_t i = 0; i < chunks.size(); ++i) {
                const std::string& chunk = chunks[i];
                if (chunk.empty()) {
                    continue;
                }

                SendResult result;
                if (i == 0) {
                    result = channel->send_message(msg.to, chunk, msg.id);
                } else {
                    result = channel->send_message(msg.to, chunk);
                }

                if (result.success) {
                    notify_outgoing_message(msg.channel, msg.to, chunk, msg.id);
                } else {
                    LOG_ERROR("Failed to send skills list: %s", result.error.c_str());
                    break;
                }
            }
            return;
        }
        
        // Route to registered command handler
        response = app.registry().execute_command(command, msg, session, args);
        
        // Unknown command - don't respond
        if (response.empty()) {
            return;
        }
    } else {
        // Regular message - route to AI via agentic loop
        AIPlugin* ai = app.registry().get_default_ai();
        if (!ai || !ai->is_configured()) {
            response = "🤖 AI not configured. Set claude.api_key in config.json to enable Claude.\n"
                       "Type /help for available commands.";
        } else {
            // Start typing indicator
            app.typing().start_typing(msg.to);
            
            LOG_DEBUG("[AI] === Processing user message via agentic loop ===");
            LOG_DEBUG("[AI] User: %s", msg.from_name.c_str());
            LOG_DEBUG("[AI] Message: %s", msg.text.c_str());
            LOG_DEBUG("[AI] Session history size: %zu messages", session.history().size());
            LOG_DEBUG("[AI] System prompt length: %zu chars", app.system_prompt().size());
            LOG_DEBUG("[AI] Registered tools: %zu", app.agent().tools().size());
            
            // Run the agentic loop
            AgentConfig agent_config;
            agent_config.max_iterations = 15;
            agent_config.max_consecutive_errors = 3;
            
            AgentResult agent_result = app.agent().run(
                ai, 
                msg.text, 
                session.history(), 
                app.system_prompt(),
                agent_config
            );
            
            LOG_DEBUG("[AI] === Agent loop complete ===");
            LOG_DEBUG("[AI] Success: %s", agent_result.success ? "yes" : "no");
            LOG_DEBUG("[AI] Iterations: %d", agent_result.iterations);
            LOG_DEBUG("[AI] Tool calls: %d", agent_result.tool_calls_made);
            LOG_DEBUG("[AI] Response length: %zu chars", agent_result.final_response.size());
            
            if (agent_result.success) {
                response = agent_result.final_response;
                
                // Log tools used
                if (!agent_result.tools_used.empty()) {
                    std::string tools_str;
                    for (size_t i = 0; i < agent_result.tools_used.size(); ++i) {
                        if (i > 0) tools_str += ", ";
                        tools_str += agent_result.tools_used[i];
                    }
                    LOG_INFO("[AI] Tools used: %s", tools_str.c_str());
                }
            } else {
                response = "❌ Agent error: " + agent_result.error;
            }
            
            // Stop typing indicator
            app.typing().stop_typing(msg.to);
        }
    }
    
    // Send response
    if (!response.empty()) {
        SendResult result = channel->send_message(msg.to, response, msg.id);
        if (result.success) {
            // Notify plugins (especially gateway) about outgoing message
            notify_outgoing_message(msg.channel, msg.to, response, msg.id);
        } else {
            LOG_ERROR("Failed to send response: %s", result.error.c_str());
            // Log thread pool status for debugging
            size_t pending = app.thread_pool().pending();
            if (pending > 4) {
                LOG_WARN("Thread pool has %zu pending tasks - system may be overloaded", pending);
            }
        }
    }
}

void on_message(const Message& msg) {
    Application& app = Application::instance();
    
    // Deduplicate messages
    if (!app.debouncer().should_process(msg.id)) {
        LOG_DEBUG("Skipping duplicate message: %s", msg.id.c_str());
        return;
    }
    
    LOG_INFO("[%s] Message from %s: %s", 
             msg.channel.c_str(), msg.from_name.c_str(), msg.text.c_str());
    
    // Notify all plugins about incoming message (for gateway, logging, etc.)
    const std::vector<Plugin*>& plugins = app.registry().plugins();
    for (size_t i = 0; i < plugins.size(); ++i) {
        if (plugins[i] && plugins[i]->is_initialized()) {
            plugins[i]->on_incoming_message(msg);
        }
    }
    
    // Check rate limit for user
    RateLimitResult rate_result = app.user_limiter().check(msg.from);
    if (!rate_result.allowed) {
        LOG_WARN("Rate limit exceeded for user %s, retry in %lldms", 
                 msg.from.c_str(), (long long)rate_result.retry_after_ms);
        return;
    }
    
    // Process message in thread pool (non-blocking)
    app.thread_pool().enqueue([msg]() {
        process_message(msg);
    });
}

void on_error(const std::string& channel, const std::string& error) {
    LOG_ERROR("Channel %s error: %s", channel.c_str(), error.c_str());
}

// ============================================================================
// Application Implementation
// ============================================================================

void print_usage(const char* prog) {
    std::cout << APP_NAME << " - Personal AI Assistant Framework\n\n"
              << "Usage: " << prog << " [options] [config.json]\n\n"
              << "Options:\n"
              << "  -h, --help     Show this help message\n"
              << "  -v, --version  Show version\n\n"
              << "Config file format (JSON):\n"
              << "  {\n"
              << "    \"plugins\": [\"telegram\", \"claude\", \"browser\", \"memory\"],\n"
              << "    \"plugins_dir\": \"./plugins\",\n"
              << "    \"log_level\": \"info\",\n"
              << "    \"system_prompt\": \"You are a helpful assistant.\",\n"
              << "    \"telegram\": { \"bot_token\": \"...\" },\n"
              << "    \"claude\": { \"api_key\": \"...\" }\n"
              << "  }\n\n"
              << "Example:\n"
              << "  " << prog << " config.json\n";
}

void print_version() {
    std::cout << APP_NAME << " v" << APP_VERSION << " (dynamic plugins)\n"
              << "Available plugins: telegram, whatsapp, browser, claude, memory\n";
}

bool Application::init(int argc, char* argv[]) {
    const char* config_file = NULL;
    
    // Initialize libcurl globally (must be done before any threads start)
    curl_global_init(CURL_GLOBAL_ALL);
    
    // Parse command line arguments
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return false;
        }
        if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--version") == 0) {
            print_version();
            return false;
        }
        config_file = argv[i];
    }
    
    // Setup signal handlers
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    LOG_INFO("%s v%s starting...", APP_NAME, APP_VERSION);
    
    // Load configuration
    if (config_file) {
        if (!config_.load_file(config_file)) {
            LOG_WARN("Failed to load config from %s, using defaults", config_file);
        } else {
            LOG_INFO("Loaded config from %s", config_file);
        }
    }
    
    // Configure logging from config
    std::string log_level = config_.get_string("log_level", "info");
    if (log_level == "debug") {
        Logger::instance().set_level(LogLevel::DEBUG);
    } else if (log_level == "warn") {
        Logger::instance().set_level(LogLevel::WARN);
    } else if (log_level == "error") {
        Logger::instance().set_level(LogLevel::ERROR);
    }
    
    // Load custom system prompt from config if present
    std::string custom_prompt = config_.get_string("system_prompt", "");
    if (!custom_prompt.empty()) {
        system_prompt_ = custom_prompt;
        LOG_DEBUG("Using custom system prompt from config");
    }
    
    // Setup skills system
    LOG_INFO("Initializing skills system...");
    SkillsConfig skills_config;
    skills_config.workspace_dir = config_.get_string("workspace_dir", ".");
    skills_config.bundled_skills_dir = config_.get_string("skills.bundled_dir", "");
    skills_config.managed_skills_dir = config_.get_string("skills.managed_dir", "");
    
    skill_manager_.set_config(skills_config);
    
    // Load and filter skills
    std::vector<SkillEntry> entries = skill_manager_.load_workspace_skill_entries();
    std::vector<SkillEntry> eligible = skill_manager_.filter_skill_entries(entries, NULL);
    
    LOG_INFO("Loaded %zu skills (%zu eligible for this environment)", entries.size(), eligible.size());
    
    // Store skill entries and command specs for use in dispatching
    skill_entries_ = eligible;
    skill_command_specs_ = skill_manager_.build_workspace_skill_command_specs(&entries, NULL, NULL);
    
    LOG_DEBUG("Built %zu skill command specs:", skill_command_specs_.size());
    for (size_t i = 0; i < skill_command_specs_.size(); ++i) {
        LOG_DEBUG("  /%s -> skill '%s' (%s)", 
                  skill_command_specs_[i].name.c_str(),
                  skill_command_specs_[i].skill_name.c_str(),
                  skill_command_specs_[i].description.c_str());
    }
    
    // Build full skills section (instructions + XML) and append to system prompt
    if (!eligible.empty()) {
        std::string skills_section = skill_manager_.build_skills_section(&entries);
        if (!skills_section.empty()) {
            system_prompt_ += "\n\n" + skills_section;
            LOG_DEBUG("Appended skills section to system prompt");
            
            // Warn if system prompt is getting large (could consume many tokens)
            size_t prompt_size = system_prompt_.size();
            if (prompt_size > 20000) {
                LOG_WARN("System prompt is very large (%zu chars, ~%zu tokens). This may consume significant context window.",
                         prompt_size, prompt_size / 4);
            } else if (prompt_size > 10000) {
                LOG_WARN("System prompt is large (%zu chars, ~%zu tokens).", prompt_size, prompt_size / 4);
            }
            LOG_DEBUG("Final system prompt size: %zu characters (~%zu tokens)", prompt_size, prompt_size / 4);
        }
    }
    
    // Configure session manager
    sessions().set_max_history(static_cast<size_t>(
        config_.get_int("session.max_history", 20)));
    
    // Initialize agent with built-in tools
    LOG_INFO("Initializing agent tools...");
    std::string workspace_dir = config_.get_string("workspace_dir", ".");
    int bash_timeout = config_.get_int("agent.bash_timeout", 20);  // Default 20 seconds
    
    agent_.register_tool(builtin_tools::create_read_tool(workspace_dir));
    agent_.register_tool(builtin_tools::create_bash_tool(workspace_dir, bash_timeout));
    agent_.register_tool(builtin_tools::create_list_dir_tool(workspace_dir));
    agent_.register_tool(builtin_tools::create_write_tool(workspace_dir));
    
    // Register content chunking tools (for handling large responses)
    agent_.register_tool(builtin_tools::create_content_chunk_tool(agent_.chunker()));
    agent_.register_tool(builtin_tools::create_content_search_tool(agent_.chunker()));
    
    LOG_INFO("Registered %zu agent tools", agent_.tools().size());
    
    // Configure plugin search paths
    loader_.add_search_path("./bin/plugins");
    loader_.add_search_path("./plugins");
    
    std::string plugins_dir = config_.get_string("plugins_dir", "");
    if (!plugins_dir.empty()) {
        loader_.add_search_path(plugins_dir);
    }
    
    // Load plugins from config
    int loaded = loader_.load_from_config(config_);
    
    LOG_INFO("Loaded %d plugins", loaded);
    
    // Register plugins with registry
    const std::vector<LoadedPlugin>& plugins = loader_.plugins();
    for (size_t i = 0; i < plugins.size(); ++i) {
        if (plugins[i].instance) {
            registry().register_plugin(plugins[i].instance);
            LOG_DEBUG("Registered plugin: %s (%s)", 
                      plugins[i].info.name, plugins[i].info.type);
        }
    }

    LOG_INFO("Registered %zu plugins (%zu channels, %zu tools, %zu AI providers)",
             registry().plugins().size(), 
             registry().channels().size(),
             registry().tools().size(),
             registry().ai_providers().size());
    
    // Register core commands (built-ins)
    register_core_commands(config_, registry());

    // Initialize all plugins (this is where plugins register their commands)
    registry().init_all(config_);
    
    LOG_INFO("Registered %zu commands", registry().commands().size());

    // Register tool plugins with the agent (so AI can invoke them)
    const std::vector<ToolProvider*>& tool_plugins = registry().tools();
    for (size_t i = 0; i < tool_plugins.size(); ++i) {
        ToolProvider* tool = tool_plugins[i];
        if (!tool || !tool->is_initialized()) {
            continue;
        }

        // Get detailed agent tools from the provider
        std::vector<AgentTool> agent_tools = tool->get_agent_tools();
        
        for (size_t j = 0; j < agent_tools.size(); ++j) {
            agent_.register_tool(agent_tools[j]);
            LOG_DEBUG("Registered tool: %s", agent_tools[j].name.c_str());
        }
    }

    if (!tool_plugins.empty()) {
        LOG_INFO("Registered %zu plugin tool(s) with agent", tool_plugins.size());
    }
    
    // Set callbacks on channels
    const std::vector<ChannelPlugin*>& channels = registry().channels();
    for (size_t i = 0; i < channels.size(); ++i) {
        if (channels[i]->is_initialized()) {
            channels[i]->set_message_callback(on_message);
            channels[i]->set_error_callback(on_error);
        }
    }
    
    // Start channels
    int started_count = 0;
    for (size_t i = 0; i < channels.size(); ++i) {
        if (channels[i]->is_initialized() && channels[i]->start()) {
            LOG_INFO("Started channel: %s", channels[i]->channel_id());
            started_count++;
        }
    }
    
    // Check if gateway plugin is loaded
    Plugin* gateway = registry().get_plugin("gateway");
    bool has_gateway = (gateway != NULL && gateway->is_initialized());
    
    if (started_count == 0 && !has_gateway) {
        LOG_ERROR("No channels or gateway started. Configure at least one:");
        LOG_ERROR("  1. Set telegram.bot_token in config.json for Telegram");
        LOG_ERROR("  2. Or enable gateway with gateway.port in config.json");
        return false;
    }
    
    if (has_gateway) {
        LOG_INFO("Gateway service available - will start on first poll");
    }
    
    // Log AI status
    AIPlugin* ai = registry().get_default_ai();
    if (ai && ai->is_configured()) {
        LOG_INFO("AI provider: %s (%s)", ai->provider_id().c_str(), ai->default_model().c_str());
    } else {
        LOG_WARN("No AI provider configured. Set claude.api_key in config.json for Claude.");
    }
    
    LOG_INFO("%d channel(s) started, ready to receive messages", started_count);
    return true;
}

void Application::rebuild_system_prompt() {
    // Rebuild system prompt with current skills
    std::string base_prompt = config_.get_string("system_prompt", DEFAULT_SYSTEM_PROMPT);
    system_prompt_ = base_prompt;
    
    // Append skills
    std::vector<SkillEntry> entries = skill_manager_.load_workspace_skill_entries();
    std::vector<SkillEntry> eligible = skill_manager_.filter_skill_entries(entries, NULL);
    
    if (!eligible.empty()) {
        SkillSnapshot snapshot = skill_manager_.build_workspace_skill_snapshot(&entries, NULL);
        if (!snapshot.prompt.empty()) {
            system_prompt_ += "\n\n" + snapshot.prompt;
        }
    }
    
    LOG_DEBUG("System prompt rebuilt with %zu eligible skills", eligible.size());
}

int Application::run() {
    LOG_INFO("Entering main loop");
    
    while (running_) {
        // Poll all plugins (including gateway, channels, etc.)
        registry().poll_all();
        sleep_ms(100);
        
        // Periodic cleanup (every ~10 seconds)
        static int cleanup_counter = 0;
        if (++cleanup_counter >= 100) {
            cleanup_counter = 0;
            sessions().cleanup_inactive(3600);  // Remove sessions older than 1 hour
            user_limiter_.cleanup(3600);
        }
    }
    
    return 0;
}

void Application::shutdown() {
    LOG_INFO("Shutting down...");
    
    // Stop thread pool first (wait for pending tasks)
    thread_pool_.shutdown();
    
    registry().stop_all_channels();
    registry().shutdown_all();
    loader_.unload_all();
    
    // Cleanup libcurl global state
    curl_global_cleanup();
    
    LOG_INFO("Goodbye!");
}

} // namespace openclaw

// ============================================================================
// Main Entry Point
// ============================================================================

int main(int argc, char* argv[]) {
    openclaw::Application& app = openclaw::Application::instance();
    
    if (!app.init(argc, argv)) {
        // init returns false for --help/--version or fatal errors
        return app.is_running() ? 1 : 0;
    }
    
    int result = app.run();
    app.shutdown();
    
    return result;
}
